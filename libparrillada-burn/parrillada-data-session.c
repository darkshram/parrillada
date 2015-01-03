/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "parrillada-media-private.h"

#include "scsi-device.h"

#include "parrillada-drive.h"
#include "parrillada-medium.h"
#include "parrillada-medium-monitor.h"
#include "burn-volume.h"

#include "parrillada-burn-lib.h"

#include "parrillada-data-session.h"
#include "parrillada-data-project.h"
#include "parrillada-file-node.h"
#include "parrillada-io.h"

#include "libparrillada-marshal.h"

typedef struct _ParrilladaDataSessionPrivate ParrilladaDataSessionPrivate;
struct _ParrilladaDataSessionPrivate
{
	ParrilladaIOJobBase *load_dir;

	/* Multisession drives that are inserted */
	GSList *media;

	/* Drive whose session is loaded */
	ParrilladaMedium *loaded;

	/* Nodes from the loaded session in the tree */
	GSList *nodes;
};

#define PARRILLADA_DATA_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DATA_SESSION, ParrilladaDataSessionPrivate))

G_DEFINE_TYPE (ParrilladaDataSession, parrillada_data_session, PARRILLADA_TYPE_DATA_PROJECT);

enum {
	AVAILABLE_SIGNAL,
	LOADED_SIGNAL,
	LAST_SIGNAL
};

static gulong parrillada_data_session_signals [LAST_SIGNAL] = { 0 };

/**
 * to evaluate the contents of a medium or image async
 */
struct _ParrilladaIOImageContentsData {
	ParrilladaIOJob job;
	gchar *dev_image;

	gint64 session_block;
	gint64 block;
};
typedef struct _ParrilladaIOImageContentsData ParrilladaIOImageContentsData;

static void
parrillada_io_image_directory_contents_destroy (ParrilladaAsyncTaskManager *manager,
					     gboolean cancelled,
					     gpointer callback_data)
{
	ParrilladaIOImageContentsData *data = callback_data;

	g_free (data->dev_image);
	parrillada_io_job_free (cancelled, PARRILLADA_IO_JOB (data));
}

static ParrilladaAsyncTaskResult
parrillada_io_image_directory_contents_thread (ParrilladaAsyncTaskManager *manager,
					    GCancellable *cancel,
					    gpointer callback_data)
{
	ParrilladaIOImageContentsData *data = callback_data;
	ParrilladaDeviceHandle *handle;
	GList *children, *iter;
	GError *error = NULL;
	ParrilladaVolSrc *vol;

	handle = parrillada_device_handle_open (data->job.uri, FALSE, NULL);
	if (!handle) {
		GError *error;

		error = g_error_new (PARRILLADA_BURN_ERROR,
		                     PARRILLADA_BURN_ERROR_GENERAL,
		                     _("The drive is busy"));

		parrillada_io_return_result (data->job.base,
					  data->job.uri,
					  NULL,
					  error,
					  data->job.callback_data);
		return PARRILLADA_ASYNC_TASK_FINISHED;
	}

	vol = parrillada_volume_source_open_device_handle (handle, &error);
	if (!vol) {
		parrillada_device_handle_close (handle);
		parrillada_io_return_result (data->job.base,
					  data->job.uri,
					  NULL,
					  error,
					  data->job.callback_data);
		return PARRILLADA_ASYNC_TASK_FINISHED;
	}

	children = parrillada_volume_load_directory_contents (vol,
							   data->session_block,
							   data->block,
							   &error);
	parrillada_volume_source_close (vol);
	parrillada_device_handle_close (handle);

	for (iter = children; iter; iter = iter->next) {
		ParrilladaVolFile *file;
		GFileInfo *info;

		file = iter->data;

		info = g_file_info_new ();
		g_file_info_set_file_type (info, file->isdir? G_FILE_TYPE_DIRECTORY:G_FILE_TYPE_REGULAR);
		g_file_info_set_name (info, PARRILLADA_VOLUME_FILE_NAME (file));

		if (file->isdir)
			g_file_info_set_attribute_int64 (info,
							 PARRILLADA_IO_DIR_CONTENTS_ADDR,
							 file->specific.dir.address);
		else
			g_file_info_set_size (info, PARRILLADA_VOLUME_FILE_SIZE (file));

		parrillada_io_return_result (data->job.base,
					  data->job.uri,
					  info,
					  NULL,
					  data->job.callback_data);
	}

	g_list_foreach (children, (GFunc) parrillada_volume_file_free, NULL);
	g_list_free (children);

	return PARRILLADA_ASYNC_TASK_FINISHED;
}

static const ParrilladaAsyncTaskType image_contents_type = {
	parrillada_io_image_directory_contents_thread,
	parrillada_io_image_directory_contents_destroy
};

static void
parrillada_io_load_image_directory (const gchar *dev_image,
				 gint64 session_block,
				 gint64 block,
				 const ParrilladaIOJobBase *base,
				 ParrilladaIOFlags options,
				 gpointer user_data)
{
	ParrilladaIOImageContentsData *data;
	ParrilladaIOResultCallbackData *callback_data = NULL;

	if (user_data) {
		callback_data = g_new0 (ParrilladaIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	data = g_new0 (ParrilladaIOImageContentsData, 1);
	data->block = block;
	data->session_block = session_block;

	parrillada_io_set_job (PARRILLADA_IO_JOB (data),
			    base,
			    dev_image,
			    options,
			    callback_data);

	parrillada_io_push_job (PARRILLADA_IO_JOB (data),
			     &image_contents_type);

}

void
parrillada_data_session_remove_last (ParrilladaDataSession *self)
{
	ParrilladaDataSessionPrivate *priv;
	GSList *iter;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	if (!priv->nodes)
		return;

	/* go through the top nodes and remove all the imported nodes */
	for (iter = priv->nodes; iter; iter = iter->next) {
		ParrilladaFileNode *node;

		node = iter->data;
		parrillada_data_project_destroy_node (PARRILLADA_DATA_PROJECT (self), node);
	}

	g_slist_free (priv->nodes);
	priv->nodes = NULL;

	g_signal_emit (self,
		       parrillada_data_session_signals [LOADED_SIGNAL],
		       0,
		       priv->loaded,
		       FALSE);

	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}
}

static void
parrillada_data_session_load_dir_destroy (GObject *object,
				       gboolean cancelled,
				       gpointer data)
{
	gint reference;
	ParrilladaFileNode *parent;

	/* reference */
	reference = GPOINTER_TO_INT (data);
	if (reference <= 0)
		return;

	parent = parrillada_data_project_reference_get (PARRILLADA_DATA_PROJECT (object), reference);
	if (parent)
		parrillada_data_project_directory_node_loaded (PARRILLADA_DATA_PROJECT (object), parent);

	parrillada_data_project_reference_free (PARRILLADA_DATA_PROJECT (object), reference);
}

static void
parrillada_data_session_load_dir_result (GObject *owner,
				      GError *error,
				      const gchar *dev_image,
				      GFileInfo *info,
				      gpointer data)
{
	ParrilladaDataSessionPrivate *priv;
	ParrilladaFileNode *parent;
	ParrilladaFileNode *node;
	gint reference;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (owner);

	if (!info) {
		g_signal_emit (owner,
			       parrillada_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       FALSE);

		/* FIXME: tell the user the error message */
		return;
	}

	reference = GPOINTER_TO_INT (data);
	if (reference > 0)
		parent = parrillada_data_project_reference_get (PARRILLADA_DATA_PROJECT (owner),
							     reference);
	else
		parent = NULL;

	/* add all the files/folders at the root of the session */
	node = parrillada_data_project_add_imported_session_file (PARRILLADA_DATA_PROJECT (owner),
							       info,
							       parent);
	if (!node) {
		/* This is not a problem, it could be simply that the user did 
		 * not want to overwrite, so do not do the following (reminder):
		g_signal_emit (owner,
			       parrillada_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       (priv->nodes != NULL));
		*/
		return;
 	}

	/* Only if we're exploring root directory */
	if (!parent) {
		priv->nodes = g_slist_prepend (priv->nodes, node);

		if (g_slist_length (priv->nodes) == 1) {
			/* Only tell when the first top node is successfully loaded */
			g_signal_emit (owner,
				       parrillada_data_session_signals [LOADED_SIGNAL],
				       0,
				       priv->loaded,
				       TRUE);
		}
	}
}

static gboolean
parrillada_data_session_load_directory_contents_real (ParrilladaDataSession *self,
						   ParrilladaFileNode *node,
						   GError **error)
{
	ParrilladaDataSessionPrivate *priv;
	goffset session_block;
	const gchar *device;
	gint reference = -1;

	if (node && !node->is_fake)
		return TRUE;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);
	device = parrillada_drive_get_device (parrillada_medium_get_drive (priv->loaded));
	parrillada_medium_get_last_data_track_address (priv->loaded,
						    NULL,
						    &session_block);

	if (!priv->load_dir)
		priv->load_dir = parrillada_io_register (G_OBJECT (self),
						      parrillada_data_session_load_dir_result,
						      parrillada_data_session_load_dir_destroy,
						      NULL);

	/* If there aren't any node then that's root */
	if (node) {
		reference = parrillada_data_project_reference_new (PARRILLADA_DATA_PROJECT (self), node);
		node->is_exploring = TRUE;
	}

	parrillada_io_load_image_directory (device,
					 session_block,
					 PARRILLADA_FILE_NODE_IMPORTED_ADDRESS (node),
					 priv->load_dir,
					 PARRILLADA_IO_INFO_URGENT,
					 GINT_TO_POINTER (reference));

	if (node)
		node->is_fake = FALSE;

	return TRUE;
}

gboolean
parrillada_data_session_load_directory_contents (ParrilladaDataSession *self,
					      ParrilladaFileNode *node,
					      GError **error)
{
	if (node == NULL)
		return FALSE;

	return parrillada_data_session_load_directory_contents_real (self, node, error);
}

gboolean
parrillada_data_session_add_last (ParrilladaDataSession *self,
			       ParrilladaMedium *medium,
			       GError **error)
{
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	if (priv->nodes)
		return FALSE;

	priv->loaded = medium;
	g_object_ref (medium);

	return parrillada_data_session_load_directory_contents_real (self, NULL, error);
}

gboolean
parrillada_data_session_has_available_media (ParrilladaDataSession *self)
{
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	return priv->media != NULL;
}

GSList *
parrillada_data_session_get_available_media (ParrilladaDataSession *self)
{
	GSList *retval;
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	retval = g_slist_copy (priv->media);
	g_slist_foreach (retval, (GFunc) g_object_ref, NULL);

	return retval;
}

ParrilladaMedium *
parrillada_data_session_get_loaded_medium (ParrilladaDataSession *self)
{
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);
	if (!priv->media || !priv->nodes)
		return NULL;

	return priv->loaded;
}

static gboolean
parrillada_data_session_is_valid_multi (ParrilladaMedium *medium)
{
	ParrilladaMedia media;
	ParrilladaMedia media_status;

	media = parrillada_medium_get_status (medium);
	media_status = parrillada_burn_library_get_media_capabilities (media);

	return (media_status & PARRILLADA_MEDIUM_WRITABLE) &&
	       (media & PARRILLADA_MEDIUM_HAS_DATA) &&
	       (parrillada_medium_get_last_data_track_address (medium, NULL, NULL) != -1);
}

static void
parrillada_data_session_disc_added_cb (ParrilladaMediumMonitor *monitor,
				    ParrilladaMedium *medium,
				    ParrilladaDataSession *self)
{
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	if (!parrillada_data_session_is_valid_multi (medium))
		return;

	g_object_ref (medium);
	priv->media = g_slist_prepend (priv->media, medium);

	g_signal_emit (self,
		       parrillada_data_session_signals [AVAILABLE_SIGNAL],
		       0,
		       medium,
		       TRUE);
}

static void
parrillada_data_session_disc_removed_cb (ParrilladaMediumMonitor *monitor,
				      ParrilladaMedium *medium,
				      ParrilladaDataSession *self)
{
	GSList *iter;
	GSList *next;
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	/* see if that's the current loaded one */
	if (priv->loaded && priv->loaded == medium)
		parrillada_data_session_remove_last (self);

	/* remove it from our list */
	for (iter = priv->media; iter; iter = next) {
		ParrilladaMedium *iter_medium;

		iter_medium = iter->data;
		next = iter->next;

		if (medium == iter_medium) {
			g_signal_emit (self,
				       parrillada_data_session_signals [AVAILABLE_SIGNAL],
				       0,
				       medium,
				       FALSE);

			priv->media = g_slist_remove (priv->media, iter_medium);
			g_object_unref (iter_medium);
		}
	}
}

static void
parrillada_data_session_init (ParrilladaDataSession *object)
{
	GSList *iter, *list;
	ParrilladaMediumMonitor *monitor;
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (object);

	monitor = parrillada_medium_monitor_get_default ();
	g_signal_connect (monitor,
			  "medium-added",
			  G_CALLBACK (parrillada_data_session_disc_added_cb),
			  object);
	g_signal_connect (monitor,
			  "medium-removed",
			  G_CALLBACK (parrillada_data_session_disc_removed_cb),
			  object);

	list = parrillada_medium_monitor_get_media (monitor,
						 PARRILLADA_MEDIA_TYPE_WRITABLE|
						 PARRILLADA_MEDIA_TYPE_REWRITABLE);
	g_object_unref (monitor);

	/* check for a multisession medium already in */
	for (iter = list; iter; iter = iter->next) {
		ParrilladaMedium *medium;

		medium = iter->data;
		if (parrillada_data_session_is_valid_multi (medium)) {
			g_object_ref (medium);
			priv->media = g_slist_prepend (priv->media, medium);
		}
	}
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

static void
parrillada_data_session_stop_io (ParrilladaDataSession *self)
{
	ParrilladaDataSessionPrivate *priv;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (self);

	if (priv->load_dir) {
		parrillada_io_cancel_by_base (priv->load_dir);
		parrillada_io_job_base_free (priv->load_dir);
		priv->load_dir = NULL;
	}
}

static void
parrillada_data_session_reset (ParrilladaDataProject *project,
			    guint num_nodes)
{
	parrillada_data_session_stop_io (PARRILLADA_DATA_SESSION (project));

	/* chain up this function except if we invalidated the node */
	if (PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_session_parent_class)->reset)
		PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_session_parent_class)->reset (project, num_nodes);
}

static void
parrillada_data_session_finalize (GObject *object)
{
	ParrilladaDataSessionPrivate *priv;
	ParrilladaMediumMonitor *monitor;

	priv = PARRILLADA_DATA_SESSION_PRIVATE (object);

	monitor = parrillada_medium_monitor_get_default ();
	g_signal_handlers_disconnect_by_func (monitor,
	                                      parrillada_data_session_disc_added_cb,
	                                      object);
	g_signal_handlers_disconnect_by_func (monitor,
	                                      parrillada_data_session_disc_removed_cb,
	                                      object);
	g_object_unref (monitor);

	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

	if (priv->nodes) {
		g_slist_free (priv->nodes);
		priv->nodes = NULL;
	}

	/* NOTE no need to clean up size_changed_sig since it's connected to 
	 * ourselves. It disappears with use. */

	parrillada_data_session_stop_io (PARRILLADA_DATA_SESSION (object));

	/* don't care about the nodes since they will be automatically
	 * destroyed */

	G_OBJECT_CLASS (parrillada_data_session_parent_class)->finalize (object);
}


static void
parrillada_data_session_class_init (ParrilladaDataSessionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaDataProjectClass *project_class = PARRILLADA_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDataSessionPrivate));

	object_class->finalize = parrillada_data_session_finalize;

	project_class->reset = parrillada_data_session_reset;

	parrillada_data_session_signals [AVAILABLE_SIGNAL] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	parrillada_data_session_signals [LOADED_SIGNAL] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
}

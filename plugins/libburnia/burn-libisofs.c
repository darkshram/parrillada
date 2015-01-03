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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <libisofs/libisofs.h>
#include <libburn/libburn.h>

#include "burn-libburnia.h"
#include "burn-job.h"
#include "parrillada-units.h"
#include "parrillada-plugin-registration.h"
#include "burn-libburn-common.h"
#include "parrillada-track-data.h"
#include "parrillada-track-image.h"


#define PARRILLADA_TYPE_LIBISOFS         (parrillada_libisofs_get_type ())
#define PARRILLADA_LIBISOFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_LIBISOFS, ParrilladaLibisofs))
#define PARRILLADA_LIBISOFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_LIBISOFS, ParrilladaLibisofsClass))
#define PARRILLADA_IS_LIBISOFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_LIBISOFS))
#define PARRILLADA_IS_LIBISOFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_LIBISOFS))
#define PARRILLADA_LIBISOFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_LIBISOFS, ParrilladaLibisofsClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaLibisofs, parrillada_libisofs, PARRILLADA_TYPE_JOB, ParrilladaJob);

struct _ParrilladaLibisofsPrivate {
	struct burn_source *libburn_src;

	/* that's for multisession */
	ParrilladaLibburnCtx *ctx;

	GError *error;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	guint thread_id;

	guint cancel:1;
};
typedef struct _ParrilladaLibisofsPrivate ParrilladaLibisofsPrivate;

#define PARRILLADA_LIBISOFS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_LIBISOFS, ParrilladaLibisofsPrivate))

static GObjectClass *parent_class = NULL;

static gboolean
parrillada_libisofs_thread_finished (gpointer data)
{
	ParrilladaLibisofs *self = data;
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		parrillada_job_error (PARRILLADA_JOB (self), error);
		return FALSE;
	}

	if (parrillada_job_get_fd_out (PARRILLADA_JOB (self), NULL) != PARRILLADA_BURN_OK) {
		ParrilladaTrackImage *track = NULL;
		gchar *output = NULL;
		goffset blocks = 0;

		/* Let's make a track */
		track = parrillada_track_image_new ();
		parrillada_job_get_image_output (PARRILLADA_JOB (self),
					      &output,
					      NULL);
		parrillada_track_image_set_source (track,
						output,
						NULL,
						PARRILLADA_IMAGE_FORMAT_BIN);

		parrillada_job_get_session_output_size (PARRILLADA_JOB (self), &blocks, NULL);
		parrillada_track_image_set_block_num (track, blocks);

		parrillada_job_add_track (PARRILLADA_JOB (self), PARRILLADA_TRACK (track));
		g_object_unref (track);
	}

	parrillada_job_finished_track (PARRILLADA_JOB (self));
	return FALSE;
}

static ParrilladaBurnResult
parrillada_libisofs_write_sector_to_fd (ParrilladaLibisofs *self,
				     int fd,
				     gpointer buffer,
				     gint bytes_remaining)
{
	gint bytes_written = 0;
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 ((gchar *) buffer) + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			break;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
                                int errsv = errno;

				/* unrecoverable error */
				priv->error = g_error_new (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_GENERAL,
							   _("Data could not be written (%s)"),
							   g_strerror (errsv));
				return PARRILLADA_BURN_ERR;
			}

			g_thread_yield ();
		}

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_libisofs_write_image_to_fd_thread (ParrilladaLibisofs *self)
{
	const gint sector_size = 2048;
	ParrilladaLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	ParrilladaBurnResult result;
	guchar buf [sector_size];
	int read_bytes;
	int fd = -1;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	parrillada_job_set_nonblocking (PARRILLADA_JOB (self), NULL);

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);
	parrillada_job_get_fd_out (PARRILLADA_JOB (self), &fd);

	PARRILLADA_JOB_LOG (self, "Writing to pipe");
	read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	while (priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size) == sector_size) {
		if (priv->cancel)
			break;

		result = parrillada_libisofs_write_sector_to_fd (self,
							      fd,
							      buf,
							      sector_size);
		if (result != PARRILLADA_BURN_OK)
			break;

		written_sectors ++;
		parrillada_job_set_written_track (PARRILLADA_JOB (self), written_sectors << 11);

		read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	}

	if (read_bytes == -1 && !priv->error)
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("Volume could not be created"));
}

static void
parrillada_libisofs_write_image_to_file_thread (ParrilladaLibisofs *self)
{
	const gint sector_size = 2048;
	ParrilladaLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	guchar buf [sector_size];
	int read_bytes;
	gchar *output;
	FILE *file;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	parrillada_job_get_image_output (PARRILLADA_JOB (self), &output, NULL);
	file = fopen (output, "w");
	if (!file) {
		int errnum = errno;

		if (errno == EACCES)
			priv->error = g_error_new_literal (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_PERMISSION,
							   _("You do not have the required permission to write at this location"));
		else
			priv->error = g_error_new_literal (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_GENERAL,
							   g_strerror (errnum));
		return;
	}

	PARRILLADA_JOB_LOG (self, "writing to file %s", output);

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);
	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);

	read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	while (read_bytes == sector_size) {
		if (priv->cancel)
			break;

		if (fwrite (buf, 1, sector_size, file) != sector_size) {
                        int errsv = errno;

			priv->error = g_error_new (PARRILLADA_BURN_ERROR,
						   PARRILLADA_BURN_ERROR_GENERAL,
						   _("Data could not be written (%s)"),
						   g_strerror (errsv));
			break;
		}

		if (priv->cancel)
			break;

		written_sectors ++;
		parrillada_job_set_written_track (PARRILLADA_JOB (self), written_sectors << 11);

		read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	}

	if (read_bytes == -1 && !priv->error)
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("Volume could not be created"));

	fclose (file);
	file = NULL;
}

static gpointer
parrillada_libisofs_thread_started (gpointer data)
{
	ParrilladaLibisofsPrivate *priv;
	ParrilladaLibisofs *self;

	self = PARRILLADA_LIBISOFS (data);
	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Entering thread");
	if (parrillada_job_get_fd_out (PARRILLADA_JOB (self), NULL) == PARRILLADA_BURN_OK)
		parrillada_libisofs_write_image_to_fd_thread (self);
	else
		parrillada_libisofs_write_image_to_file_thread (self);

	PARRILLADA_JOB_LOG (self, "Getting out thread");

	/* End thread */
	g_mutex_lock (priv->mutex);

	if (!priv->cancel)
		priv->thread_id = g_idle_add (parrillada_libisofs_thread_finished, self);

	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static ParrilladaBurnResult
parrillada_libisofs_create_image (ParrilladaLibisofs *self,
			       GError **error)
{
	ParrilladaLibisofsPrivate *priv;
	GError *thread_error = NULL;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	if (priv->thread)
		return PARRILLADA_BURN_RUNNING;

	if (iso_init () < 0) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("libisofs could not be initialized."));
		return PARRILLADA_BURN_ERR;
	}

	iso_set_msgs_severities ("NEVER", "ALL", "parrillada (libisofs)");

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (parrillada_libisofs_thread_started,
					self,
					FALSE,
					&thread_error);
	g_mutex_unlock (priv->mutex);

	/* Reminder: this is not necessarily an error as the thread may have finished */
	//if (!priv->thread)
	//	return PARRILLADA_BURN_ERR;

	if (thread_error) {
		g_propagate_error (error, thread_error);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static gboolean
parrillada_libisofs_create_volume_thread_finished (gpointer data)
{
	ParrilladaLibisofs *self = data;
	ParrilladaLibisofsPrivate *priv;
	ParrilladaJobAction action;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		parrillada_job_error (PARRILLADA_JOB (self), error);
		return FALSE;
	}

	parrillada_job_get_action (PARRILLADA_JOB (self), &action);
	if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		ParrilladaBurnResult result;
		GError *error = NULL;

		result = parrillada_libisofs_create_image (self, &error);
		if (error)
		parrillada_job_error (PARRILLADA_JOB (self), error);
		else
			return FALSE;
	}

	parrillada_job_finished_track (PARRILLADA_JOB (self));
	return FALSE;
}

static gint
parrillada_libisofs_sort_graft_points (gconstpointer a, gconstpointer b)
{
	const ParrilladaGraftPt *graft_a, *graft_b;
	gint len_a, len_b;

	graft_a = a;
	graft_b = b;

	/* we only want to know if:
	 * - a is a parent of b (a > b, retval < 0) 
	 * - b is a parent of a (b > a, retval > 0). */
	len_a = strlen (graft_a->path);
	len_b = strlen (graft_b->path);

	return len_a - len_b;
}

static int 
parrillada_libisofs_import_read (IsoDataSource *src, uint32_t lba, uint8_t *buffer)
{
	struct burn_drive *d;
	off_t data_count;
	gint result;

	d = (struct burn_drive*)src->data;

	result = burn_read_data(d,
				(off_t) lba * (off_t) 2048,
				(char*)buffer, 
				2048,
				&data_count,
				0);
	if (result < 0 )
		return -1; /* error */

	return 1;
}

static int
parrillada_libisofs_import_open (IsoDataSource *src)
{
	return 1;
}

static int
parrillada_libisofs_import_close (IsoDataSource *src)
{
	return 1;
}
    
static void 
parrillada_libisofs_import_free (IsoDataSource *src)
{ }

static ParrilladaBurnResult
parrillada_libisofs_import_last_session (ParrilladaLibisofs *self,
				      IsoImage *image,
				      IsoWriteOpts *wopts,
				      GError **error)
{
	int result;
	IsoReadOpts *opts;
	ParrilladaMedia media;
	IsoDataSource *src;
	goffset start_block;
	goffset session_block;
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	priv->ctx = parrillada_libburn_common_ctx_new (PARRILLADA_JOB (self), FALSE, error);
	if (!priv->ctx)
		return PARRILLADA_BURN_ERR;

	result = iso_read_opts_new (&opts, 0);
	if (result < 0) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("Read options could not be created"));
		return PARRILLADA_BURN_ERR;
	}

	src = g_new0 (IsoDataSource, 1);
	src->version = 0;
	src->refcount = 1;
	src->read_block = parrillada_libisofs_import_read;
	src->open = parrillada_libisofs_import_open;
	src->close = parrillada_libisofs_import_close;
	src->free_data = parrillada_libisofs_import_free;
	src->data = priv->ctx->drive;

	parrillada_job_get_last_session_address (PARRILLADA_JOB (self), &session_block);
	iso_read_opts_set_start_block (opts, session_block);

	/* import image */
	result = iso_image_import (image, src, opts, NULL);
	iso_data_source_unref (src);
	iso_read_opts_free (opts);

	/* release the drive */
	if (priv->ctx) {
		/* This may not be a good idea ...*/
		parrillada_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (result < 0) {
		PARRILLADA_JOB_LOG (self, "Import failed 0x%x", result);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_IMAGE_LAST_SESSION,
			     _("Last session import failed"));	
		return PARRILLADA_BURN_ERR;
	}

	/* check is this is a DVD+RW */
	parrillada_job_get_next_writable_address (PARRILLADA_JOB (self), &start_block);

	parrillada_job_get_media (PARRILLADA_JOB (self), &media);
	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS)
	||  PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_RESTRICTED)
	||  PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS_DL)) {
		/* This is specific to overwrite media; the start address is the
		 * size of all the previous data written */
		PARRILLADA_JOB_LOG (self, "Growing image (start %i)", start_block);
	}

	/* set the start block for the multisession image */
	iso_write_opts_set_ms_block (wopts, start_block);
	iso_write_opts_set_appendable (wopts, 1);

	iso_tree_set_replace_mode (image, ISO_REPLACE_ALWAYS);
	return PARRILLADA_BURN_OK;
}

static gpointer
parrillada_libisofs_create_volume_thread (gpointer data)
{
	ParrilladaLibisofs *self = PARRILLADA_LIBISOFS (data);
	ParrilladaLibisofsPrivate *priv;
	ParrilladaTrack *track = NULL;
	IsoWriteOpts *opts = NULL;
	IsoImage *image = NULL;
	ParrilladaBurnFlag flags;
	GSList *grafts = NULL;
	gchar *label = NULL;
	gchar *publisher;
	GSList *excluded;
	GSList *iter;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	if (priv->libburn_src) {
		burn_source_free (priv->libburn_src);
		priv->libburn_src = NULL;
	}

	PARRILLADA_JOB_LOG (self, "creating volume");

	/* create volume */
	parrillada_job_get_data_label (PARRILLADA_JOB (self), &label);
	if (!iso_image_new (label, &image)) {
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("Volume could not be created"));
		g_free (label);
		goto end;
	}

	iso_write_opts_new (&opts, 2);
	iso_write_opts_set_relaxed_vol_atts(opts, 1);

	parrillada_job_get_flags (PARRILLADA_JOB (self), &flags);
	if (flags & PARRILLADA_BURN_FLAG_MERGE) {
		ParrilladaBurnResult result;

		result = parrillada_libisofs_import_last_session (self,
							       image,
							       opts,
							       &priv->error);
		if (result != PARRILLADA_BURN_OK) {
			g_free (label);
			goto end;
		}
	}
	else if (flags & PARRILLADA_BURN_FLAG_APPEND) {
		goffset start_block;

		parrillada_job_get_next_writable_address (PARRILLADA_JOB (self), &start_block);
		iso_write_opts_set_ms_block (opts, start_block);
	}

	/* set label but set it after merging so the
	 * new does not get replaced by the former */
	publisher = g_strdup_printf ("Parrillada-%i.%i.%i",
				     PARRILLADA_MAJOR_VERSION,
				     PARRILLADA_MINOR_VERSION,
				     PARRILLADA_SUB);

	if (label)
		iso_image_set_volume_id (image, label);

	iso_image_set_publisher_id (image, publisher);
	iso_image_set_data_preparer_id (image, g_get_real_name ());

	g_free (publisher);
	g_free (label);

	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);

	/* copy the list as we're going to reorder it */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	grafts = parrillada_track_data_get_grafts (PARRILLADA_TRACK_DATA (track));
	grafts = g_slist_copy (grafts);
	grafts = g_slist_sort (grafts, parrillada_libisofs_sort_graft_points);

	/* add global exclusions */
	for (excluded = parrillada_track_data_get_excluded_list (PARRILLADA_TRACK_DATA (track));
	     excluded; excluded = excluded->next) {
		gchar *uri, *local;

		uri = excluded->data;
		local = g_filename_from_uri (uri, NULL, NULL);
		iso_tree_add_exclude (image, local);
		g_free (local);
	}

	for (iter = grafts; iter; iter = iter->next) {
		ParrilladaGraftPt *graft;
		gboolean is_directory;
		gchar *path_parent;
		gchar *path_name;
		IsoNode *parent;

		if (priv->cancel)
			goto end;

		graft = iter->data;

		PARRILLADA_JOB_LOG (self,
				 "Adding graft disc path = %s, URI = %s",
				 graft->path,
				 graft->uri);

		/* search for parent node.
		 * NOTE: because of mkisofs/genisoimage, we add a "/" at the end
		 * directories. So make sure there isn't one when getting the 
		 * parent path or g_path_get_dirname () will return the same
		 * exact name */
		if (g_str_has_suffix (graft->path, G_DIR_SEPARATOR_S)) {
			gchar *tmp;

			/* remove trailing "/" */
			tmp = g_strdup (graft->path);
			tmp [strlen (tmp) - 1] = '\0';
			path_parent = g_path_get_dirname (tmp);
			path_name = g_path_get_basename (tmp);
			g_free (tmp);

			is_directory = TRUE;
		}
		else {
			path_parent = g_path_get_dirname (graft->path);
			path_name = g_path_get_basename (graft->path);
			is_directory = FALSE;
		}

		iso_tree_path_to_node (image, path_parent, &parent);
		g_free (path_parent);

		if (!parent) {
			/* an error has occurred, possibly libisofs hasn't been
			 * able to find a parent for this node */
			g_free (path_name);
			priv->error = g_error_new (PARRILLADA_BURN_ERROR,
						   PARRILLADA_BURN_ERROR_GENERAL,
						   /* Translators: %s is the path */
						   _("No parent could be found in the tree for the path \"%s\""),
						   graft->path);
			goto end;
		}

		PARRILLADA_JOB_LOG (self, "Found parent");

		/* add the file/directory to the volume */
		if (graft->uri) {
			gchar *local_path;
			IsoDirIter *sibling;

			/* graft->uri can be a path or a URI */
			if (graft->uri [0] == '/')
				local_path = g_strdup (graft->uri);
			else if (g_str_has_prefix (graft->uri, "file://"))
				local_path = g_filename_from_uri (graft->uri, NULL, NULL);
			else
				local_path = NULL;

			if (!local_path){
				priv->error = g_error_new (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_FILE_NOT_LOCAL,
							   _("The file is not stored locally"));
				g_free (path_name);
				goto end;
			}

			/* see if the node exists with the same name among the 
			 * children of the parent directory. If there is a
			 * sibling destroy it. */
			sibling = NULL;
			iso_dir_get_children (ISO_DIR (parent), &sibling);

			IsoNode *node;
			while (iso_dir_iter_next (sibling, &node) == 1) {
				const gchar *iso_name;

				/* check if it has the same name */
				iso_name = iso_node_get_name (node);
				if (iso_name && !strcmp (iso_name, path_name))
					PARRILLADA_JOB_LOG (self,
							 "Found sibling for %s: removing %x",
							 path_name,
							 iso_dir_iter_remove (sibling));
			}

			if  (is_directory) {
				int result;
				IsoDir *directory;

				/* add directory node */
				result = iso_tree_add_new_dir (ISO_DIR (parent), path_name, &directory);
				if (result < 0) {
					PARRILLADA_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 result);
					priv->error = g_error_new (PARRILLADA_BURN_ERROR,
								   PARRILLADA_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while creating directory \"%s\""),
								   graft->path);
					g_free (path_name);
					goto end;
				}

				/* add contents */
				result = iso_tree_add_dir_rec (image, directory, local_path);
				if (result < 0) {
					PARRILLADA_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 result);
					priv->error = g_error_new (PARRILLADA_BURN_ERROR,
								   PARRILLADA_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while adding contents to directory \"%s\" (%x)"),
								   graft->path,
								   result);
					g_free (path_name);
					goto end;
				}
			}
			else {
				IsoNode *node;
				int err;

				err = iso_tree_add_new_node (image,
							 ISO_DIR (parent),
				                         path_name,
							 local_path,
							 &node);
				if (err < 0) {
					PARRILLADA_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 err);
					priv->error = g_error_new (PARRILLADA_BURN_ERROR,
								   PARRILLADA_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while adding file at path \"%s\""),
								   graft->path);
					g_free (path_name);
					goto end;
				}

				if (iso_node_get_name (node)
				&&  strcmp (iso_node_get_name (node), path_name)) {
					err = iso_node_set_name (node, path_name);
					if (err < 0) {
						PARRILLADA_JOB_LOG (self,
								 "ERROR %s %x",
								 path_name,
								 err);
						priv->error = g_error_new (PARRILLADA_BURN_ERROR,
									   PARRILLADA_BURN_ERROR_GENERAL,
									   _("libisofs reported an error while adding file at path \"%s\""),
									   graft->path);
						g_free (path_name);
						goto end;
					}
				}
			}

			g_free (local_path);
		}
		else if (iso_tree_add_new_dir (ISO_DIR (parent), path_name, NULL) < 0) {
			priv->error = g_error_new (PARRILLADA_BURN_ERROR,
						   PARRILLADA_BURN_ERROR_GENERAL,
						   _("libisofs reported an error while creating directory \"%s\""),
						   graft->path);
			g_free (path_name);
			goto end;

		}

		g_free (path_name);
	}


end:

	if (grafts)
		g_slist_free (grafts);

	if (!priv->error && !priv->cancel) {
		gint64 size;
		ParrilladaImageFS image_fs;

		image_fs = parrillada_track_data_get_fs (PARRILLADA_TRACK_DATA (track));

		if ((image_fs & PARRILLADA_IMAGE_FS_ISO)
		&&  (image_fs & PARRILLADA_IMAGE_ISO_FS_LEVEL_3))
			iso_write_opts_set_iso_level (opts, 3);
		else
			iso_write_opts_set_iso_level (opts, 2);

		iso_write_opts_set_rockridge (opts, 1);
		iso_write_opts_set_joliet (opts, (image_fs & PARRILLADA_IMAGE_FS_JOLIET) != 0);
		iso_write_opts_set_allow_deep_paths (opts, (image_fs & PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY) != 0);

		if (iso_image_create_burn_source (image, opts, &priv->libburn_src) >= 0) {
			size = priv->libburn_src->get_size (priv->libburn_src);
			parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (self),
								       PARRILLADA_BYTES_TO_SECTORS (size, 2048),
								       size);
		}
	}

	if (opts)
		iso_write_opts_free (opts);

	if (image)
		iso_image_unref (image);

	/* End thread */
	g_mutex_lock (priv->mutex);

	/* It is important that the following is done inside the lock; indeed,
	 * if the main loop is idle then that parrillada_libisofs_stop_real () can
	 * be called immediatly to stop the plugin while priv->thread is not
	 * NULL.
	 * As in this callback we check whether the thread is running (which
	 * means that we were cancelled) in some cases it would mean that we
	 * would cancel the libburn_src object and create crippled images. */
	if (!priv->cancel)
		priv->thread_id = g_idle_add (parrillada_libisofs_create_volume_thread_finished, self);

	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static ParrilladaBurnResult
parrillada_libisofs_create_volume (ParrilladaLibisofs *self, GError **error)
{
	ParrilladaLibisofsPrivate *priv;
	GError *thread_error = NULL;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);
	if (priv->thread)
		return PARRILLADA_BURN_RUNNING;

	if (iso_init () < 0) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("libisofs could not be initialized."));
		return PARRILLADA_BURN_ERR;
	}

	iso_set_msgs_severities ("NEVER", "ALL", "parrillada (libisofs)");
	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (parrillada_libisofs_create_volume_thread,
					self,
					FALSE,
					&thread_error);
	g_mutex_unlock (priv->mutex);

	/* Reminder: this is not necessarily an error as the thread may have finished */
	//if (!priv->thread)
	//	return PARRILLADA_BURN_ERR;
	if (thread_error) {
		g_propagate_error (error, thread_error);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_libisofs_clean_output (ParrilladaLibisofs *self)
{
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	if (priv->libburn_src) {
		burn_source_free (priv->libburn_src);
		priv->libburn_src = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}

static ParrilladaBurnResult
parrillada_libisofs_start (ParrilladaJob *job,
			GError **error)
{
	ParrilladaLibisofs *self;
	ParrilladaJobAction action;
	ParrilladaLibisofsPrivate *priv;

	self = PARRILLADA_LIBISOFS (job);
	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	parrillada_job_get_action (job, &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		/* do this to avoid a problem when using
		 * DUMMY flag. libisofs would not generate
		 * a second time. */
		parrillada_libisofs_clean_output (PARRILLADA_LIBISOFS (job));
		parrillada_job_set_current_action (PARRILLADA_JOB (self),
						PARRILLADA_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		return parrillada_libisofs_create_volume (self, error);
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* we need the source before starting anything */
	if (!priv->libburn_src)
		return parrillada_libisofs_create_volume (self, error);

	return parrillada_libisofs_create_image (self, error);
}

static void
parrillada_libisofs_stop_real (ParrilladaLibisofs *self)
{
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (self);

	/* Check whether we properly shut down or if we were cancelled */
	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		/* NOTE: this can only happen when we're preparing the volumes
		 * for a multi session disc. At this point we're only running
		 * to get the size of the future volume and we can't race with
		 * libburn plugin that isn't operating at this stage. */
		if (priv->ctx) {
			parrillada_libburn_common_ctx_free (priv->ctx);
			priv->ctx = NULL;
		}

		/* A thread is running. In this context we are probably cancelling */
		if (priv->libburn_src)
			priv->libburn_src->cancel (priv->libburn_src);

		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}
}

static ParrilladaBurnResult
parrillada_libisofs_stop (ParrilladaJob *job,
		       GError **error)
{
	ParrilladaLibisofs *self;

	self = PARRILLADA_LIBISOFS (job);
	parrillada_libisofs_stop_real (self);
	return PARRILLADA_BURN_OK;
}

static void
parrillada_libisofs_class_init (ParrilladaLibisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaLibisofsPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = parrillada_libisofs_finalize;

	job_class->start = parrillada_libisofs_start;
	job_class->stop = parrillada_libisofs_stop;
}

static void
parrillada_libisofs_init (ParrilladaLibisofs *obj)
{
	ParrilladaLibisofsPrivate *priv;

	priv = PARRILLADA_LIBISOFS_PRIVATE (obj);
	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
parrillada_libisofs_finalize (GObject *object)
{
	ParrilladaLibisofs *cobj;
	ParrilladaLibisofsPrivate *priv;

	cobj = PARRILLADA_LIBISOFS (object);
	priv = PARRILLADA_LIBISOFS_PRIVATE (object);

	parrillada_libisofs_stop_real (cobj);
	parrillada_libisofs_clean_output (cobj);

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	/* close libisofs library */
	iso_finish ();

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_libisofs_export_caps (ParrilladaPlugin *plugin)
{
	GSList *output;
	GSList *input;

	parrillada_plugin_define (plugin,
			       "libisofs",
	                       NULL,
			       _("Creates disc images from a file selection"),
			       "Philippe Rouquier",
			       3);

	parrillada_plugin_set_flags (plugin,
				  PARRILLADA_MEDIUM_CDR|
				  PARRILLADA_MEDIUM_CDRW|
				  PARRILLADA_MEDIUM_DVDR|
				  PARRILLADA_MEDIUM_DVDRW|
				  PARRILLADA_MEDIUM_DUAL_L|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_HAS_AUDIO|
				  PARRILLADA_MEDIUM_HAS_DATA,
				  PARRILLADA_BURN_FLAG_APPEND|
				  PARRILLADA_BURN_FLAG_MERGE,
				  PARRILLADA_BURN_FLAG_NONE);

	parrillada_plugin_set_flags (plugin,
				  PARRILLADA_MEDIUM_DVDRW_PLUS|
				  PARRILLADA_MEDIUM_RESTRICTED|
				  PARRILLADA_MEDIUM_DUAL_L|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_CLOSED|
				  PARRILLADA_MEDIUM_HAS_DATA,
				  PARRILLADA_BURN_FLAG_APPEND|
				  PARRILLADA_BURN_FLAG_MERGE,
				  PARRILLADA_BURN_FLAG_NONE);

	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					 PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					 PARRILLADA_IMAGE_FORMAT_BIN);

	input = parrillada_caps_data_new (PARRILLADA_IMAGE_FS_ISO|
				       PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       PARRILLADA_IMAGE_ISO_FS_LEVEL_3|
				       PARRILLADA_IMAGE_FS_JOLIET);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = parrillada_caps_data_new (PARRILLADA_IMAGE_FS_ISO|
				       PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       PARRILLADA_IMAGE_ISO_FS_LEVEL_3|
				       PARRILLADA_IMAGE_FS_SYMLINK);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	g_slist_free (output);

	parrillada_plugin_register_group (plugin, _(LIBBURNIA_DESCRIPTION));
}

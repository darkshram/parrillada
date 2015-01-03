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

/* This is for getline() and isblank() */
#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <gmodule.h>

#include "burn-job.h"
#include "parrillada-plugin-registration.h"
#include "parrillada-xfer.h"
#include "parrillada-track-image.h"


#define PARRILLADA_TYPE_LOCAL_TRACK         (parrillada_local_track_get_type ())
#define PARRILLADA_LOCAL_TRACK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_LOCAL_TRACK, ParrilladaLocalTrack))
#define PARRILLADA_LOCAL_TRACK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_LOCAL_TRACK, ParrilladaLocalTrackClass))
#define PARRILLADA_IS_LOCAL_TRACK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_LOCAL_TRACK))
#define PARRILLADA_IS_LOCAL_TRACK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_LOCAL_TRACK))
#define PARRILLADA_LOCAL_TRACK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_LOCAL_TRACK, ParrilladaLocalTrackClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaLocalTrack, parrillada_local_track, PARRILLADA_TYPE_JOB, ParrilladaJob);

struct _ParrilladaLocalTrackPrivate {
	GCancellable *cancel;
	ParrilladaXferCtx *xfer_ctx;

	gchar *checksum;
	gchar *checksum_path;
	GChecksumType gchecksum_type;
	ParrilladaChecksumType checksum_type;

	GHashTable *nonlocals;

	guint thread_id;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	GSList *src_list;
	GSList *dest_list;

	GError *error;

	guint download_checksum:1;
};
typedef struct _ParrilladaLocalTrackPrivate ParrilladaLocalTrackPrivate;

#define PARRILLADA_LOCAL_TRACK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_LOCAL_TRACK, ParrilladaLocalTrackPrivate))

static GObjectClass *parent_class = NULL;

static ParrilladaBurnResult
parrillada_local_track_clock_tick (ParrilladaJob *job)
{
	ParrilladaLocalTrackPrivate *priv;
	goffset written = 0;
	goffset total = 0;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (job);

	if (!priv->xfer_ctx)
		return PARRILLADA_BURN_OK;

	parrillada_job_start_progress (job, FALSE);

	parrillada_xfer_get_progress (priv->xfer_ctx,
				   &written,
				   &total);
	parrillada_job_set_progress (job, (gdouble) written / (gdouble) total);

	return PARRILLADA_BURN_OK;
}

/**
 * That's for URI translation ...
 */

static gchar *
parrillada_local_track_translate_uri (ParrilladaLocalTrack *self,
				   const gchar *uri)
{
	gchar *newuri;
	gchar *parent;
	ParrilladaLocalTrackPrivate *priv;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	if (!uri)
		return NULL;

	/* see if it is a local file */
	if (g_str_has_prefix (uri, "file://"))
		return g_strdup (uri);

	/* see if it was downloaded itself */
	newuri = g_hash_table_lookup (priv->nonlocals, uri);
	if (newuri) {
		/* we copy this string as it will be freed when freeing 
		 * downloaded GSList */
		return g_strdup (newuri);
	}

	/* see if one of its parent will be downloaded */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		gchar *tmp;

		tmp = g_hash_table_lookup (priv->nonlocals, parent);
		if (tmp) {
			newuri = g_build_path (G_DIR_SEPARATOR_S,
					       tmp,
					       uri + strlen (parent),
					       NULL);
			g_free (parent);
			return newuri;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}

	/* that should not happen */
	PARRILLADA_JOB_LOG (self, "Can't find a downloaded parent for %s", uri);

	g_free (parent);
	return NULL;
}

static ParrilladaBurnResult
parrillada_local_track_read_checksum (ParrilladaLocalTrack *self)
{
	gint size;
	FILE *file;
	gchar *name;
	gchar *source;
	gchar *line = NULL;
	size_t line_len = 0;
	ParrilladaTrack *track = NULL;
	ParrilladaLocalTrackPrivate *priv;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	/* get the file_checksum from the checksum file */
	file = fopen (priv->checksum_path, "r");

	/* get the name of the file that was downloaded */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	source = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), TRUE);
	name = g_path_get_basename (source);
	g_free (source);

	if (!file) {
		g_free (name);
		PARRILLADA_JOB_LOG (self, "Impossible to open the downloaded checksum file");
		return PARRILLADA_BURN_ERR;
	}

	/* find a line with the name of our file (there could be several ones) */
	PARRILLADA_JOB_LOG (self, "Searching %s in file", name);
	while ((size = getline (&line, &line_len, file)) > 0) {
		if (strstr (line, name)) {
			gchar *ptr;

			/* Skip blanks */
			ptr = line;
			while (isblank (*ptr)) { ptr ++; size --; }

			if (g_checksum_type_get_length (priv->checksum_type) * 2 > size) {
				g_free (line);
				line = NULL;
				continue;
			}
	
			ptr [g_checksum_type_get_length (priv->gchecksum_type) * 2] = '\0';
			priv->checksum = g_strdup (ptr);
			g_free (line);
			PARRILLADA_JOB_LOG (self, "Found checksum %s", priv->checksum);
			break;
		}

		g_free (line);
		line = NULL;
	}
	fclose (file);

	return (priv->checksum? PARRILLADA_BURN_OK:PARRILLADA_BURN_ERR);
}

static ParrilladaBurnResult
parrillada_local_track_download_checksum (ParrilladaLocalTrack *self)
{
	ParrilladaLocalTrackPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaTrack *track;
	gchar *checksum_src;
	GFile *src, *dest;
	GFile *tmp;
	gchar *uri;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Copying checksum file");

	/* generate a unique name for destination */
	result = parrillada_job_get_tmp_file (PARRILLADA_JOB (self),
					   NULL,
					   &priv->checksum_path,
					   NULL);
	if (result != PARRILLADA_BURN_OK)
		goto error;

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_FILE_COPY,
					_("Copying checksum file"),
					TRUE);

	/* This is an image. See if there is any checksum sitting in the same
	 * directory to check our download integrity.
	 * Try all types of checksum. */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	uri = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), TRUE);
	dest = g_file_new_for_path (priv->checksum_path);

	/* Try with three difference sources */
	checksum_src = g_strdup_printf ("%s.md5", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = parrillada_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == PARRILLADA_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_MD5;
		priv->checksum_type = PARRILLADA_CHECKSUM_MD5;
		goto end;
	}

	checksum_src = g_strdup_printf ("%s.sha1", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = parrillada_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == PARRILLADA_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_SHA1;
		priv->checksum_type = PARRILLADA_CHECKSUM_SHA1;
		goto end;
	}

	checksum_src = g_strdup_printf ("%s.sha256", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = parrillada_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == PARRILLADA_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_SHA256;
		priv->checksum_type = PARRILLADA_CHECKSUM_SHA256;
		goto end;
	}

	/* There are also ftp sites that includes all images checksum in one big
	 * SHA1 file. */
	tmp = g_file_new_for_uri (uri);
	src = g_file_get_parent (tmp);
	g_object_unref (tmp);

	tmp = src;
	src = g_file_get_child_for_display_name (tmp, "SHA1SUM", NULL);
	g_object_unref (tmp);

	result = parrillada_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result != PARRILLADA_BURN_OK) {
		g_free (uri);
		g_object_unref (dest);
		goto error;
	}

	priv->gchecksum_type = G_CHECKSUM_SHA1;
	priv->checksum_type = PARRILLADA_CHECKSUM_SHA1;

end:

	g_object_unref (dest);
	g_free (uri);

	return result;

error:

	/* we give up */
	PARRILLADA_JOB_LOG (self, "No checksum file available");
	g_free (priv->checksum_path);
	priv->checksum_path = NULL;
	return result;
}

static ParrilladaBurnResult
parrillada_local_track_update_track (ParrilladaLocalTrack *self)
{
	ParrilladaTrack *track = NULL;
	ParrilladaTrack *current = NULL;
	ParrilladaLocalTrackPrivate *priv;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	/* now we update all the track with the local uris in retval */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &current);

	/* make a copy of the tracks instead of modifying them */
	if (PARRILLADA_IS_TRACK_DATA (current)) {
		GSList *next;
		GSList *grafts;
		GSList *unreadable;
		guint64 file_num = 0;

		track = PARRILLADA_TRACK (parrillada_track_data_new ());
		parrillada_track_tag_copy_missing (PARRILLADA_TRACK (track), current);

		parrillada_track_data_add_fs (PARRILLADA_TRACK_DATA (track), parrillada_track_data_get_fs (PARRILLADA_TRACK_DATA (current)));

		parrillada_track_data_get_file_num (PARRILLADA_TRACK_DATA (current), &file_num);
		parrillada_track_data_set_file_num (PARRILLADA_TRACK_DATA (track), file_num);

		grafts = parrillada_track_data_get_grafts (PARRILLADA_TRACK_DATA (current));
		for (; grafts; grafts = grafts->next) {
			ParrilladaGraftPt *graft;
			gchar *uri;

			graft = grafts->data;
			uri = parrillada_local_track_translate_uri (self, graft->uri);
			if (uri) {
				g_free (graft->uri);
				graft->uri = uri;
			}
		}

		PARRILLADA_JOB_LOG (self, "Translating unreadable");

		/* Translate the globally excluded.
		 * NOTE: if we can't find a parent for an excluded URI that
		 * means it shouldn't be included. */
		unreadable = parrillada_track_data_get_excluded_list (PARRILLADA_TRACK_DATA (current));
		for (; unreadable; unreadable = next) {
			gchar *new_uri;

			next = unreadable->next;
			new_uri = parrillada_local_track_translate_uri (self, unreadable->data);
			g_free (unreadable->data);

			if (new_uri)
				unreadable->data = new_uri;
			else
				unreadable = g_slist_remove (unreadable, unreadable->data);
		}
	}
	else if (PARRILLADA_IS_TRACK_STREAM (current)) {
		gchar *uri;
		gchar *newuri;

		uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (current), TRUE);
		newuri = parrillada_local_track_translate_uri (self, uri);

		track = PARRILLADA_TRACK (parrillada_track_stream_new ());
		parrillada_track_tag_copy_missing (PARRILLADA_TRACK (track), current);
		parrillada_track_stream_set_source (PARRILLADA_TRACK_STREAM (track), newuri);
		parrillada_track_stream_set_format (PARRILLADA_TRACK_STREAM (track), parrillada_track_stream_get_format (PARRILLADA_TRACK_STREAM (current)));
		parrillada_track_stream_set_boundaries (PARRILLADA_TRACK_STREAM (track),
						     parrillada_track_stream_get_start (PARRILLADA_TRACK_STREAM (current)),
						     parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (current)),
						     parrillada_track_stream_get_gap (PARRILLADA_TRACK_STREAM (current)));
		g_free (uri);
	}
	else if (PARRILLADA_IS_TRACK_IMAGE (current)) {
		gchar *uri;
		gchar *newtoc;
		gchar *newimage;
		goffset blocks = 0;

		uri = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (current), TRUE);
		newimage = parrillada_local_track_translate_uri (self, uri);
		g_free (uri);

		uri = parrillada_track_image_get_toc_source (PARRILLADA_TRACK_IMAGE (current), TRUE);
		newtoc = parrillada_local_track_translate_uri (self, uri);
		g_free (uri);

		parrillada_track_get_size (current, &blocks, NULL);

		track = PARRILLADA_TRACK (parrillada_track_image_new ());
		parrillada_track_tag_copy_missing (PARRILLADA_TRACK (track), current);
		parrillada_track_image_set_source (PARRILLADA_TRACK_IMAGE (track),
						newimage,
						newtoc,
						parrillada_track_image_get_format (PARRILLADA_TRACK_IMAGE (current)));
		parrillada_track_image_set_block_num (PARRILLADA_TRACK_IMAGE (track), blocks);
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (self);

	if (priv->download_checksum)
		parrillada_track_set_checksum (track,
					    priv->checksum_type,
					    priv->checksum);

	parrillada_job_add_track (PARRILLADA_JOB (self), track);

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. ParrilladaTaskCtx refs it. */
	g_object_unref (track);

	return PARRILLADA_BURN_OK;
}

static gboolean
parrillada_local_track_thread_finished (ParrilladaLocalTrack *self)
{
	ParrilladaLocalTrackPrivate *priv;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	priv->thread_id = 0;

	if (priv->xfer_ctx) {
		parrillada_xfer_free (priv->xfer_ctx);
		priv->xfer_ctx = NULL;
	}

	if (priv->cancel) {
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
		if (g_cancellable_is_cancelled (priv->cancel))
			return FALSE;
	}

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		parrillada_job_error (PARRILLADA_JOB (self), error);
		return FALSE;
	}

	parrillada_local_track_update_track (self);

	parrillada_job_finished_track (PARRILLADA_JOB (self));
	return FALSE;
}

static gpointer
parrillada_local_track_thread (gpointer data)
{
	ParrilladaLocalTrack *self = PARRILLADA_LOCAL_TRACK (data);
	ParrilladaLocalTrackPrivate *priv;
	GSList *src, *dest;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);
	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_FILE_COPY,
					_("Copying files locally"),
					TRUE);

	for (src = priv->src_list, dest = priv->dest_list;
	     src && dest;
	     src = src->next, dest = dest->next) {
		gchar *name;
		gchar *string;
		GFile *src_file;
		GFile *dest_file;
		ParrilladaBurnResult result;

		src_file = src->data;
		dest_file = dest->data;

		name = g_file_get_basename (src_file);
		PARRILLADA_JOB_LOG (self, "Downloading %s", name);
		string = g_strdup_printf (_("Copying `%s` locally"), name);
		g_free (name);

		result = parrillada_xfer_start (priv->xfer_ctx,
					     src_file,
					     dest_file,
					     priv->cancel,
					     &priv->error);

		if (g_cancellable_is_cancelled (priv->cancel))
			goto end;

		if (result != PARRILLADA_BURN_OK)
			goto end;
	}

	/* successfully downloaded files, get a checksum if we can. */
	if (priv->download_checksum
	&& !priv->checksum_path
	&&  parrillada_local_track_download_checksum (self) == PARRILLADA_BURN_OK)
		parrillada_local_track_read_checksum (self);

end:

	if (!g_cancellable_is_cancelled (priv->cancel))
		priv->thread_id = g_idle_add ((GSourceFunc) parrillada_local_track_thread_finished, self);

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static ParrilladaBurnResult
parrillada_local_track_start_thread (ParrilladaLocalTrack *self,
				  GError **error)
{
	ParrilladaLocalTrackPrivate *priv;
	GError *thread_error = NULL;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	if (priv->thread)
		return PARRILLADA_BURN_RUNNING;

	priv->cancel = g_cancellable_new ();
	priv->xfer_ctx = parrillada_xfer_new ();

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (parrillada_local_track_thread,
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
_foreach_non_local_cb (const gchar *uri,
		       const gchar *localuri,
		       gpointer *data)
{
	ParrilladaLocalTrack *self = PARRILLADA_LOCAL_TRACK (data);
	ParrilladaLocalTrackPrivate *priv;
	GFile *file, *tmpfile;
	gchar *parent;
	gchar *tmp;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (data);

	/* check that is hasn't any parent in the hash */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		gchar *uri_local;

		uri_local = g_hash_table_lookup (priv->nonlocals, parent);
		if (uri_local) {
			PARRILLADA_JOB_LOG (self, "Parent for %s was found %s", uri, parent);
			g_free (parent);
			return TRUE;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}
	g_free (parent);

	file = g_file_new_for_uri (uri);
	priv->src_list = g_slist_append (priv->src_list, file);

	tmpfile = g_file_new_for_uri (localuri);
	priv->dest_list = g_slist_append (priv->dest_list, tmpfile);

	PARRILLADA_JOB_LOG (self, "%s set to be downloaded to %s", uri, localuri);
	return FALSE;
}

static ParrilladaBurnResult
parrillada_local_track_add_if_non_local (ParrilladaLocalTrack *self,
				      const gchar *uri,
				      GError **error)
{
	ParrilladaLocalTrackPrivate *priv;
	ParrilladaBurnResult result;
	gchar *localuri = NULL;

	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	if (!uri
	||   uri [0] == '\0'
	||   uri [0] == '/'
	||   g_str_has_prefix (uri, "file://")
	||   g_str_has_prefix (uri, "burn://"))
		return PARRILLADA_BURN_OK;

	/* add it to the list or uris to download */
	if (!priv->nonlocals)
		priv->nonlocals = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 NULL,
							 g_free);

	/* generate a unique name */
	result = parrillada_job_get_tmp_file (PARRILLADA_JOB (self),
					   NULL,
					   &localuri,
					   error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	if (!g_str_has_prefix (localuri, "file://")) {
		gchar *tmp;

		tmp = localuri;
		localuri = g_strconcat ("file://", tmp, NULL);
		g_free (tmp);
	}

	/* we don't want to replace it if it has already been downloaded */
	if (!g_hash_table_lookup (priv->nonlocals, uri))
		g_hash_table_insert (priv->nonlocals, g_strdup (uri), localuri);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_local_track_start (ParrilladaJob *job,
			   GError **error)
{
	ParrilladaLocalTrackPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaJobAction action;
	ParrilladaLocalTrack *self;
	ParrilladaTrack *track;
	GSList *grafts;
	gchar *uri;

	self = PARRILLADA_LOCAL_TRACK (job);
	priv = PARRILLADA_LOCAL_TRACK_PRIVATE (self);

	/* skip that part */
	parrillada_job_get_action (job, &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		/* say we won't write to disc */
		parrillada_job_set_output_size_for_current_track (job, 0, 0);
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	/* can't be piped so parrillada_job_get_current_track will work */
	parrillada_job_get_current_track (job, &track);

	result = PARRILLADA_BURN_OK;

	/* make a list of all non local uris to be downloaded and put them in a
	 * list to avoid to download the same file twice. */
	if (PARRILLADA_IS_TRACK_DATA (track)) {
		/* we put all the non local graft point uris in the hash */
		grafts = parrillada_track_data_get_grafts (PARRILLADA_TRACK_DATA (track));
		for (; grafts; grafts = grafts->next) {
			ParrilladaGraftPt *graft;

			graft = grafts->data;
			result = parrillada_local_track_add_if_non_local (self, graft->uri, error);
			if (result != PARRILLADA_BURN_OK)
				break;
		}
	}
	else if (PARRILLADA_IS_TRACK_STREAM (track)) {
		/* NOTE: don't delete URI as they will be inserted in hash */
		uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), TRUE);
		result = parrillada_local_track_add_if_non_local (self, uri, error);
		g_free (uri);
	}
	else if (PARRILLADA_IS_TRACK_IMAGE (track)) {
		/* NOTE: don't delete URI as they will be inserted in hash */
		uri = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), TRUE);
		result = parrillada_local_track_add_if_non_local (self, uri, error);
		g_free (uri);

		if (result == PARRILLADA_BURN_OK) {
			priv->download_checksum = TRUE;

			uri = parrillada_track_image_get_toc_source (PARRILLADA_TRACK_IMAGE (track), TRUE);
			result = parrillada_local_track_add_if_non_local (self, uri, error);
			g_free (uri);
		}
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (self);

	if (result != PARRILLADA_BURN_OK)
		return result;

	/* see if there is anything to download */
	if (!priv->nonlocals) {
		PARRILLADA_JOB_LOG (self, "no remote URIs");
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	/* first we create a list of all the non local files that need to be
	 * downloaded. To be elligible a file must not have one of his parent
	 * in the hash. */
	g_hash_table_foreach_remove (priv->nonlocals,
				     (GHRFunc) _foreach_non_local_cb,
				     job);

	return parrillada_local_track_start_thread (self, error);
}

static ParrilladaBurnResult
parrillada_local_track_stop (ParrilladaJob *job,
			  GError **error)
{
	ParrilladaLocalTrackPrivate *priv = PARRILLADA_LOCAL_TRACK_PRIVATE (job);

	if (priv->cancel) {
		/* signal that we've been cancelled */
		g_cancellable_cancel (priv->cancel);
	}

	g_mutex_lock (priv->mutex);
	if (priv->thread)
		g_cond_wait (priv->cond, priv->mutex);
	g_mutex_unlock (priv->mutex);

	if (priv->xfer_ctx) {
		parrillada_xfer_free (priv->xfer_ctx);
		priv->xfer_ctx = NULL;
	}

	if (priv->cancel) {
		/* unref it after the thread has stopped */
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	if (priv->src_list) {
		g_slist_foreach (priv->src_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->src_list);
		priv->src_list = NULL;
	}

	if (priv->dest_list) {
		g_slist_foreach (priv->dest_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->dest_list);
		priv->dest_list = NULL;
	}

	if (priv->nonlocals) {
		g_hash_table_destroy (priv->nonlocals);
		priv->nonlocals = NULL;
	}

	if (priv->checksum_path) {
		g_free (priv->checksum_path);
		priv->checksum_path = NULL;
	}

	if (priv->checksum) {
		g_free (priv->checksum);
		priv->checksum = NULL;
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_local_track_finalize (GObject *object)
{
	ParrilladaLocalTrackPrivate *priv = PARRILLADA_LOCAL_TRACK_PRIVATE (object);

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_local_track_class_init (ParrilladaLocalTrackClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaLocalTrackPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_local_track_finalize;

	job_class->start = parrillada_local_track_start;
	job_class->stop = parrillada_local_track_stop;
	job_class->clock_tick = parrillada_local_track_clock_tick;
}

static void
parrillada_local_track_init (ParrilladaLocalTrack *obj)
{
	ParrilladaLocalTrackPrivate *priv = PARRILLADA_LOCAL_TRACK_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
parrillada_local_track_export_caps (ParrilladaPlugin *plugin)
{
	GSList *caps;

	parrillada_plugin_define (plugin,
	                       "file-downloader",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("File Downloader"),
			       _("Allows files not stored locally to be burned"),
			       "Philippe Rouquier",
			       10);

	caps = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
				       PARRILLADA_IMAGE_FORMAT_ANY);
	parrillada_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
				       PARRILLADA_AUDIO_FORMAT_UNDEFINED|
	                               PARRILLADA_AUDIO_FORMAT_DTS|
				       PARRILLADA_AUDIO_FORMAT_RAW|
				       PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
				       PARRILLADA_VIDEO_FORMAT_UNDEFINED|
				       PARRILLADA_VIDEO_FORMAT_VCD|
				       PARRILLADA_VIDEO_FORMAT_VIDEO_DVD|
				       PARRILLADA_AUDIO_FORMAT_AC3|
				       PARRILLADA_AUDIO_FORMAT_MP2|
				       PARRILLADA_METADATA_INFO);
	parrillada_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
				       PARRILLADA_AUDIO_FORMAT_UNDEFINED|
	                               PARRILLADA_AUDIO_FORMAT_DTS|
				       PARRILLADA_AUDIO_FORMAT_RAW|
				       PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
				       PARRILLADA_VIDEO_FORMAT_UNDEFINED|
				       PARRILLADA_VIDEO_FORMAT_VCD|
				       PARRILLADA_VIDEO_FORMAT_VIDEO_DVD|
				       PARRILLADA_AUDIO_FORMAT_AC3|
				       PARRILLADA_AUDIO_FORMAT_MP2);
	parrillada_plugin_process_caps (plugin, caps);
	g_slist_free (caps);
	caps = parrillada_caps_data_new (PARRILLADA_IMAGE_FS_ANY);
	parrillada_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	parrillada_plugin_set_process_flags (plugin, PARRILLADA_PLUGIN_RUN_PREPROCESSING);

	parrillada_plugin_set_compulsory (plugin, FALSE);
}

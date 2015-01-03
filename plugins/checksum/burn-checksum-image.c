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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include "parrillada-plugin-registration.h"
#include "burn-job.h"
#include "burn-volume.h"
#include "parrillada-drive.h"
#include "parrillada-track-disc.h"
#include "parrillada-track-image.h"
#include "parrillada-tags.h"


#define PARRILLADA_TYPE_CHECKSUM_IMAGE		(parrillada_checksum_image_get_type ())
#define PARRILLADA_CHECKSUM_IMAGE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_CHECKSUM_IMAGE, ParrilladaChecksumImage))
#define PARRILLADA_CHECKSUM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_CHECKSUM_IMAGE, ParrilladaChecksumImageClass))
#define PARRILLADA_IS_CHECKSUM_IMAGE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_CHECKSUM_IMAGE))
#define PARRILLADA_IS_CHECKSUM_IMAGE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_CHECKSUM_IMAGE))
#define PARRILLADA_CHECKSUM_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_CHECKSUM_IMAGE, ParrilladaChecksumImageClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaChecksumImage, parrillada_checksum_image, PARRILLADA_TYPE_JOB, ParrilladaJob);

struct _ParrilladaChecksumImagePrivate {
	GChecksum *checksum;
	ParrilladaChecksumType checksum_type;

	/* That's for progress reporting */
	goffset total;
	goffset bytes;

	/* this is for the thread and the end of it */
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	gint end_id;

	guint cancel;
};
typedef struct _ParrilladaChecksumImagePrivate ParrilladaChecksumImagePrivate;

#define PARRILLADA_CHECKSUM_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_CHECKSUM_IMAGE, ParrilladaChecksumImagePrivate))

#define PARRILLADA_SCHEMA_CONFIG		"org.mate.parrillada.config"
#define PARRILLADA_PROPS_CHECKSUM_IMAGE	"checksum-image"

static ParrilladaJobClass *parent_class = NULL;

static gint
parrillada_checksum_image_read (ParrilladaChecksumImage *self,
			     int fd,
			     guchar *buffer,
			     gint bytes,
			     GError **error)
{
	gint total = 0;
	gint read_bytes;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	while (1) {
		read_bytes = read (fd, buffer + total, (bytes - total));

		/* maybe that's the end of the stream ... */
		if (!read_bytes)
			return total;

		if (priv->cancel)
			return -2;

		/* ... or an error =( */
		if (read_bytes == -1) {
			if (errno != EAGAIN && errno != EINTR) {
                                int errsv = errno;

				g_set_error (error,
					     PARRILLADA_BURN_ERROR,
					     PARRILLADA_BURN_ERROR_GENERAL,
					     _("Data could not be read (%s)"),
					     g_strerror (errsv));
				return -1;
			}
		}
		else {
			total += read_bytes;

			if (total == bytes)
				return total;
		}

		g_usleep (500);
	}

	return total;
}

static ParrilladaBurnResult
parrillada_checksum_image_write (ParrilladaChecksumImage *self,
			      int fd,
			      guchar *buffer,
			      gint bytes,
			      GError **error)
{
	gint bytes_remaining;
	gint bytes_written = 0;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	bytes_remaining = bytes;
	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 buffer + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			return PARRILLADA_BURN_CANCEL;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
                                int errsv = errno;

				/* unrecoverable error */
				g_set_error (error,
					     PARRILLADA_BURN_ERROR,
					     PARRILLADA_BURN_ERROR_GENERAL,
					     _("Data could not be written (%s)"),
					     g_strerror (errsv));
				return PARRILLADA_BURN_ERR;
			}
		}

		g_usleep (500);

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_checksum_image_checksum (ParrilladaChecksumImage *self,
				 GChecksumType checksum_type,
				 int fd_in,
				 int fd_out,
				 GError **error)
{
	gint read_bytes;
	guchar buffer [2048];
	ParrilladaBurnResult result;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	priv->checksum = g_checksum_new (checksum_type);
	result = PARRILLADA_BURN_OK;
	while (1) {
		read_bytes = parrillada_checksum_image_read (self,
							  fd_in,
							  buffer,
							  sizeof (buffer),
							  error);
		if (read_bytes == -2)
			return PARRILLADA_BURN_CANCEL;

		if (read_bytes == -1)
			return PARRILLADA_BURN_ERR;

		if (!read_bytes)
			break;

		/* it can happen when we're just asked to generate a checksum
		 * that we don't need to output the received data */
		if (fd_out > 0) {
			result = parrillada_checksum_image_write (self,
							       fd_out,
							       buffer,
							       read_bytes, error);
			if (result != PARRILLADA_BURN_OK)
				break;
		}

		g_checksum_update (priv->checksum,
				   buffer,
				   read_bytes);

		priv->bytes += read_bytes;
	}

	return result;
}

static ParrilladaBurnResult
parrillada_checksum_image_checksum_fd_input (ParrilladaChecksumImage *self,
					  GChecksumType checksum_type,
					  GError **error)
{
	int fd_in = -1;
	int fd_out = -1;
	ParrilladaBurnResult result;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Starting checksum generation live (size = %lli)", priv->total);
	result = parrillada_job_set_nonblocking (PARRILLADA_JOB (self), error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	parrillada_job_get_fd_in (PARRILLADA_JOB (self), &fd_in);
	parrillada_job_get_fd_out (PARRILLADA_JOB (self), &fd_out);

	return parrillada_checksum_image_checksum (self, checksum_type, fd_in, fd_out, error);
}

static ParrilladaBurnResult
parrillada_checksum_image_checksum_file_input (ParrilladaChecksumImage *self,
					    GChecksumType checksum_type,
					    GError **error)
{
	ParrilladaChecksumImagePrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaTrack *track;
	int fd_out = -1;
	int fd_in = -1;
	gchar *path;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	/* get all information */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	path = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), FALSE);
	if (!path) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_FILE_NOT_LOCAL,
			     _("The file is not stored locally"));
		return PARRILLADA_BURN_ERR;
	}

	PARRILLADA_JOB_LOG (self,
			 "Starting checksuming file %s (size = %"G_GOFFSET_FORMAT")",
			 path,
			 priv->total);

	fd_in = open (path, O_RDONLY);
	if (!fd_in) {
                int errsv;
		gchar *name = NULL;

		if (errno == ENOENT)
			return PARRILLADA_BURN_RETRY;

		name = g_path_get_basename (path);

                errsv = errno;

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: first %s is the filename, second %s
			      * is the error generated from errno */
			     _("\"%s\" could not be opened (%s)"),
			     name,
			     g_strerror (errsv));
		g_free (name);
		g_free (path);

		return PARRILLADA_BURN_ERR;
	}

	/* and here we go */
	parrillada_job_get_fd_out (PARRILLADA_JOB (self), &fd_out);
	result = parrillada_checksum_image_checksum (self, checksum_type, fd_in, fd_out, error);
	g_free (path);
	close (fd_in);

	return result;
}

static ParrilladaBurnResult
parrillada_checksum_image_create_checksum (ParrilladaChecksumImage *self,
					GError **error)
{
	ParrilladaBurnResult result;
	ParrilladaTrack *track = NULL;
	GChecksumType checksum_type;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	/* get the checksum type */
	switch (priv->checksum_type) {
		case PARRILLADA_CHECKSUM_MD5:
			checksum_type = G_CHECKSUM_MD5;
			break;
		case PARRILLADA_CHECKSUM_SHA1:
			checksum_type = G_CHECKSUM_SHA1;
			break;
		case PARRILLADA_CHECKSUM_SHA256:
			checksum_type = G_CHECKSUM_SHA256;
			break;
		default:
			return PARRILLADA_BURN_ERR;
	}

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);
	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);

	/* see if another plugin is sending us data to checksum
	 * or if we do it ourself (and then that must be from an
	 * image file only). */
	if (parrillada_job_get_fd_in (PARRILLADA_JOB (self), NULL) == PARRILLADA_BURN_OK) {
		ParrilladaMedium *medium;
		GValue *value = NULL;
		ParrilladaDrive *drive;
		guint64 start, end;
		goffset sectors;
		goffset bytes;

		parrillada_track_tag_lookup (track,
					  PARRILLADA_TRACK_MEDIUM_ADDRESS_START_TAG,
					  &value);

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		parrillada_track_tag_lookup (track,
					  PARRILLADA_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);

		priv->total = end - start;

		/* we're only able to checksum ISO format at the moment so that
		 * means we can only handle last session */
		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
		medium = parrillada_drive_get_medium (drive);
		parrillada_medium_get_last_data_track_space (medium,
							  &bytes,
							  &sectors);

		/* That's the only way to get the sector size */
		priv->total *= bytes / sectors;

		return parrillada_checksum_image_checksum_fd_input (self, checksum_type, error);
	}
	else {
		result = parrillada_track_get_size (track,
						 NULL,
						 &priv->total);
		if (result != PARRILLADA_BURN_OK)
			return result;

		return parrillada_checksum_image_checksum_file_input (self, checksum_type, error);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaChecksumType
parrillada_checksum_get_checksum_type (void)
{
	GSettings *settings;
	GChecksumType checksum_type;

	settings = g_settings_new (PARRILLADA_SCHEMA_CONFIG);
	checksum_type = g_settings_get_int (settings, PARRILLADA_PROPS_CHECKSUM_IMAGE);
	g_object_unref (settings);

	return checksum_type;
}

static ParrilladaBurnResult
parrillada_checksum_image_image_and_checksum (ParrilladaChecksumImage *self,
					   GError **error)
{
	ParrilladaBurnResult result;
	GChecksumType checksum_type;
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	priv->checksum_type = parrillada_checksum_get_checksum_type ();

	if (priv->checksum_type & PARRILLADA_CHECKSUM_MD5)
		checksum_type = G_CHECKSUM_MD5;
	else if (priv->checksum_type & PARRILLADA_CHECKSUM_SHA1)
		checksum_type = G_CHECKSUM_SHA1;
	else if (priv->checksum_type & PARRILLADA_CHECKSUM_SHA256)
		checksum_type = G_CHECKSUM_SHA256;
	else {
		checksum_type = G_CHECKSUM_MD5;
		priv->checksum_type = PARRILLADA_CHECKSUM_MD5;
	}

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);
	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);

	if (parrillada_job_get_fd_in (PARRILLADA_JOB (self), NULL) != PARRILLADA_BURN_OK) {
		ParrilladaTrack *track;

		parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
		result = parrillada_track_get_size (track,
						 NULL,
						 &priv->total);
		if (result != PARRILLADA_BURN_OK)
			return result;

		result = parrillada_checksum_image_checksum_file_input (self,
								     checksum_type,
								     error);
	}
	else
		result = parrillada_checksum_image_checksum_fd_input (self,
								   checksum_type,
								   error);

	return result;
}

struct _ParrilladaChecksumImageThreadCtx {
	ParrilladaChecksumImage *sum;
	ParrilladaBurnResult result;
	GError *error;
};
typedef struct _ParrilladaChecksumImageThreadCtx ParrilladaChecksumImageThreadCtx;

static gboolean
parrillada_checksum_image_end (gpointer data)
{
	ParrilladaChecksumImage *self;
	ParrilladaTrack *track;
	const gchar *checksum;
	ParrilladaBurnResult result;
	ParrilladaChecksumImagePrivate *priv;
	ParrilladaChecksumImageThreadCtx *ctx;

	ctx = data;
	self = ctx->sum;
	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	/* NOTE ctx/data is destroyed in its own callback */
	priv->end_id = 0;

	if (ctx->result != PARRILLADA_BURN_OK) {
		GError *error;

		error = ctx->error;
		ctx->error = NULL;

		g_checksum_free (priv->checksum);
		priv->checksum = NULL;

		parrillada_job_error (PARRILLADA_JOB (self), error);
		return FALSE;
	}

	/* we were asked to check the sum of the track so get the type
	 * of the checksum first to see what to do */
	track = NULL;
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);

	/* Set the checksum for the track and at the same time compare it to a
	 * potential previous one. */
	checksum = g_checksum_get_string (priv->checksum);
	PARRILLADA_JOB_LOG (self,
			 "Setting new checksum (type = %i) %s (%s before)",
			 priv->checksum_type,
			 checksum,
			 parrillada_track_get_checksum (track));
	result = parrillada_track_set_checksum (track,
					     priv->checksum_type,
					     checksum);
	g_checksum_free (priv->checksum);
	priv->checksum = NULL;

	if (result != PARRILLADA_BURN_OK)
		goto error;

	parrillada_job_finished_track (PARRILLADA_JOB (self));
	return FALSE;

error:
{
	GError *error = NULL;

	error = g_error_new (PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_BAD_CHECKSUM,
			     _("Some files may be corrupted on the disc"));
	parrillada_job_error (PARRILLADA_JOB (self), error);
	return FALSE;
}
}

static void
parrillada_checksum_image_destroy (gpointer data)
{
	ParrilladaChecksumImageThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
parrillada_checksum_image_thread (gpointer data)
{
	GError *error = NULL;
	ParrilladaJobAction action;
	ParrilladaTrack *track = NULL;
	ParrilladaChecksumImage *self;
	ParrilladaChecksumImagePrivate *priv;
	ParrilladaChecksumImageThreadCtx *ctx;
	ParrilladaBurnResult result = PARRILLADA_BURN_NOT_SUPPORTED;

	self = PARRILLADA_CHECKSUM_IMAGE (data);
	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	parrillada_job_get_action (PARRILLADA_JOB (self), &action);
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);

	if (action == PARRILLADA_JOB_ACTION_CHECKSUM) {
		priv->checksum_type = parrillada_track_get_checksum_type (track);
		if (priv->checksum_type & (PARRILLADA_CHECKSUM_MD5|PARRILLADA_CHECKSUM_SHA1|PARRILLADA_CHECKSUM_SHA256))
			result = parrillada_checksum_image_create_checksum (self, &error);
		else
			result = PARRILLADA_BURN_ERR;
	}
	else if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		ParrilladaTrackType *input;

		input = parrillada_track_type_new ();
		parrillada_job_get_input_type (PARRILLADA_JOB (self), input);

		if (parrillada_track_type_get_has_image (input))
			result = parrillada_checksum_image_image_and_checksum (self, &error);
		else
			result = PARRILLADA_BURN_ERR;

		parrillada_track_type_free (input);
	}

	if (result != PARRILLADA_BURN_CANCEL) {
		ctx = g_new0 (ParrilladaChecksumImageThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						parrillada_checksum_image_end,
						ctx,
						parrillada_checksum_image_destroy);
	}

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);
	return NULL;
}

static ParrilladaBurnResult
parrillada_checksum_image_start (ParrilladaJob *job,
			      GError **error)
{
	ParrilladaChecksumImagePrivate *priv;
	GError *thread_error = NULL;
	ParrilladaJobAction action;

	parrillada_job_get_action (job, &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		/* say we won't write to disc if we're just checksuming "live" */
		if (parrillada_job_get_fd_in (job, NULL) == PARRILLADA_BURN_OK)
			return PARRILLADA_BURN_NOT_SUPPORTED;

		/* otherwise return an output of 0 since we're not actually 
		 * writing anything to the disc. That will prevent a disc space
		 * failure. */
		parrillada_job_set_output_size_for_current_track (job, 0, 0);
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	/* we start a thread for the exploration of the graft points */
	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (job);
	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (parrillada_checksum_image_thread,
					PARRILLADA_CHECKSUM_IMAGE (job),
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

static ParrilladaBurnResult
parrillada_checksum_image_activate (ParrilladaJob *job,
				 GError **error)
{
	ParrilladaBurnFlag flags = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaTrack *track = NULL;
	ParrilladaJobAction action;

	parrillada_job_get_current_track (job, &track);
	parrillada_job_get_action (job, &action);

	if (action == PARRILLADA_JOB_ACTION_IMAGE
	&&  parrillada_track_get_checksum_type (track) != PARRILLADA_CHECKSUM_NONE
	&&  parrillada_track_get_checksum_type (track) == parrillada_checksum_get_checksum_type ()) {
		PARRILLADA_JOB_LOG (job,
				 "There is a checksum already %d",
				 parrillada_track_get_checksum_type (track));
		/* if there is a checksum already, if so no need to redo one */
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	flags = PARRILLADA_BURN_FLAG_NONE;
	parrillada_job_get_flags (job, &flags);
	if (flags & PARRILLADA_BURN_FLAG_DUMMY) {
		PARRILLADA_JOB_LOG (job, "Dummy operation, skipping");
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_checksum_image_clock_tick (ParrilladaJob *job)
{
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (job);

	if (!priv->checksum)
		return PARRILLADA_BURN_OK;

	if (!priv->total)
		return PARRILLADA_BURN_OK;

	parrillada_job_start_progress (job, FALSE);
	parrillada_job_set_progress (job,
				  (gdouble) priv->bytes /
				  (gdouble) priv->total);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_checksum_image_stop (ParrilladaJob *job,
			     GError **error)
{
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (job);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_checksum_image_init (ParrilladaChecksumImage *obj)
{
	ParrilladaChecksumImagePrivate *priv;

	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
parrillada_checksum_image_finalize (GObject *object)
{
	ParrilladaChecksumImagePrivate *priv;
	
	priv = PARRILLADA_CHECKSUM_IMAGE_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
	}

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
parrillada_checksum_image_class_init (ParrilladaChecksumImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaChecksumImagePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_checksum_image_finalize;

	job_class->activate = parrillada_checksum_image_activate;
	job_class->start = parrillada_checksum_image_start;
	job_class->stop = parrillada_checksum_image_stop;
	job_class->clock_tick = parrillada_checksum_image_clock_tick;
}

static void
parrillada_checksum_image_export_caps (ParrilladaPlugin *plugin)
{
	GSList *input;
	ParrilladaPluginConfOption *checksum_type;

	parrillada_plugin_define (plugin,
	                       "image-checksum",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("Image Checksum"),
			       _("Checks disc integrity after it is burnt"),
			       "Philippe Rouquier",
			       0);

	/* For images we can process (thus generating a sum on the fly or simply
	 * test images). */
	input = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					PARRILLADA_IMAGE_FORMAT_BIN);
	parrillada_plugin_process_caps (plugin, input);

	parrillada_plugin_set_process_flags (plugin,
					  PARRILLADA_PLUGIN_RUN_PREPROCESSING|
					  PARRILLADA_PLUGIN_RUN_BEFORE_TARGET);

	parrillada_plugin_check_caps (plugin,
				   PARRILLADA_CHECKSUM_MD5|
				   PARRILLADA_CHECKSUM_SHA1|
				   PARRILLADA_CHECKSUM_SHA256,
				   input);
	g_slist_free (input);

	/* add some configure options */
	checksum_type = parrillada_plugin_conf_option_new (PARRILLADA_PROPS_CHECKSUM_IMAGE,
							_("Hashing algorithm to be used:"),
							PARRILLADA_PLUGIN_OPTION_CHOICE);
	parrillada_plugin_conf_option_choice_add (checksum_type,
					       _("MD5"), PARRILLADA_CHECKSUM_MD5);
	parrillada_plugin_conf_option_choice_add (checksum_type,
					       _("SHA1"), PARRILLADA_CHECKSUM_SHA1);
	parrillada_plugin_conf_option_choice_add (checksum_type,
					       _("SHA256"), PARRILLADA_CHECKSUM_SHA256);

	parrillada_plugin_add_conf_option (plugin, checksum_type);

	parrillada_plugin_set_compulsory (plugin, FALSE);
}

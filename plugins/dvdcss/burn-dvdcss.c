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

#include "parrillada-units.h"

#include "burn-job.h"
#include "parrillada-plugin-registration.h"
#include "burn-dvdcss-private.h"
#include "burn-volume.h"
#include "parrillada-medium.h"
#include "parrillada-track-image.h"
#include "parrillada-track-disc.h"


#define PARRILLADA_TYPE_DVDCSS         (parrillada_dvdcss_get_type ())
#define PARRILLADA_DVDCSS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_DVDCSS, ParrilladaDvdcss))
#define PARRILLADA_DVDCSS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_DVDCSS, ParrilladaDvdcssClass))
#define PARRILLADA_IS_DVDCSS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_DVDCSS))
#define PARRILLADA_IS_DVDCSS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_DVDCSS))
#define PARRILLADA_DVDCSS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_DVDCSS, ParrilladaDvdcssClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaDvdcss, parrillada_dvdcss, PARRILLADA_TYPE_JOB, ParrilladaJob);

struct _ParrilladaDvdcssPrivate {
	GError *error;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	guint thread_id;

	guint cancel:1;
};
typedef struct _ParrilladaDvdcssPrivate ParrilladaDvdcssPrivate;

#define PARRILLADA_DVDCSS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DVDCSS, ParrilladaDvdcssPrivate))

#define PARRILLADA_DVDCSS_I_BLOCKS	16ULL

static GObjectClass *parent_class = NULL;

static gboolean
parrillada_dvdcss_library_init (ParrilladaPlugin *plugin)
{
	gpointer address;
	GModule *module;
	gchar *dvdcss_interface_2 = NULL;

	if (css_ready)
		return TRUE;

	/* load libdvdcss library and see the version (mine is 1.2.0) */
	module = g_module_open ("libdvdcss.so.2", G_MODULE_BIND_LOCAL);
	if (!module)
		goto error_doesnt_exist;

	if (!g_module_symbol (module, "dvdcss_interface_2", &address))
		goto error_version;
	dvdcss_interface_2 = address;

	if (!g_module_symbol (module, "dvdcss_open", &address))
		goto error_version;
	dvdcss_open = address;

	if (!g_module_symbol (module, "dvdcss_close", &address))
		goto error_version;
	dvdcss_close = address;

	if (!g_module_symbol (module, "dvdcss_read", &address))
		goto error_version;
	dvdcss_read = address;

	if (!g_module_symbol (module, "dvdcss_seek", &address))
		goto error_version;
	dvdcss_seek = address;

	if (!g_module_symbol (module, "dvdcss_error", &address))
		goto error_version;
	dvdcss_error = address;

	if (plugin) {
		g_module_close (module);
		return TRUE;
	}

	css_ready = TRUE;
	return TRUE;

error_doesnt_exist:
	parrillada_plugin_add_error (plugin,
	                          PARRILLADA_PLUGIN_ERROR_MISSING_LIBRARY,
	                          "libdvdcss.so.2");
	return FALSE;

error_version:
	parrillada_plugin_add_error (plugin,
	                          PARRILLADA_PLUGIN_ERROR_LIBRARY_VERSION,
	                          "libdvdcss.so.2");
	g_module_close (module);
	return FALSE;
}

static gboolean
parrillada_dvdcss_thread_finished (gpointer data)
{
	goffset blocks = 0;
	gchar *image = NULL;
	ParrilladaDvdcss *self = data;
	ParrilladaDvdcssPrivate *priv;
	ParrilladaTrackImage *track = NULL;

	priv = PARRILLADA_DVDCSS_PRIVATE (self);
	priv->thread_id = 0;

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		parrillada_job_error (PARRILLADA_JOB (self), error);
		return FALSE;
	}

	track = parrillada_track_image_new ();
	parrillada_job_get_image_output (PARRILLADA_JOB (self),
				      &image,
				      NULL);
	parrillada_track_image_set_source (track,
					image,
					NULL,
					PARRILLADA_IMAGE_FORMAT_BIN);

	parrillada_job_get_session_output_size (PARRILLADA_JOB (self), &blocks, NULL);
	parrillada_track_image_set_block_num (track, blocks);

	parrillada_job_add_track (PARRILLADA_JOB (self), PARRILLADA_TRACK (track));

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. ParrilladaTaskCtx refs it. */
	g_object_unref (track);

	parrillada_job_finished_track (PARRILLADA_JOB (self));

	return FALSE;
}

static ParrilladaBurnResult
parrillada_dvdcss_write_sector_to_fd (ParrilladaDvdcss *self,
				   gpointer buffer,
				   gint bytes_remaining)
{
	int fd;
	gint bytes_written = 0;
	ParrilladaDvdcssPrivate *priv;

	priv = PARRILLADA_DVDCSS_PRIVATE (self);

	parrillada_job_get_fd_out (PARRILLADA_JOB (self), &fd);
	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 ((gchar *) buffer)  + bytes_written,
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

struct _ParrilladaScrambledSectorRange {
	gint start;
	gint end;
};
typedef struct _ParrilladaScrambledSectorRange ParrilladaScrambledSectorRange;

static gboolean
parrillada_dvdcss_create_scrambled_sectors_map (ParrilladaDvdcss *self,
                                             GQueue *map,
					     dvdcss_handle *handle,
					     ParrilladaVolFile *parent,
					     GError **error)
{
	GList *iter;

	/* this allows to cache keys for encrypted files */
	for (iter = parent->specific.dir.children; iter; iter = iter->next) {
		ParrilladaVolFile *file;

		file = iter->data;
		if (!file->isdir) {
			if (!strncmp (file->name + strlen (file->name) - 6, ".VOB", 4)) {
				ParrilladaScrambledSectorRange *range;
				gsize current_extent;
				GSList *extents;

				PARRILLADA_JOB_LOG (self, "Retrieving keys for %s", file->name);

				/* take the first address for each extent of the file */
				if (!file->specific.file.extents) {
					PARRILLADA_JOB_LOG (self, "Problem: file has no extents");
					return FALSE;
				}

				range = g_new0 (ParrilladaScrambledSectorRange, 1);
				for (extents = file->specific.file.extents; extents; extents = extents->next) {
					ParrilladaVolFileExtent *extent;

					extent = extents->data;

					range->start = extent->block;
					range->end = extent->block + PARRILLADA_BYTES_TO_SECTORS (extent->size, DVDCSS_BLOCK_SIZE);

					PARRILLADA_JOB_LOG (self, "From 0x%llx to 0x%llx", range->start, range->end);
					g_queue_push_head (map, range);

					if (extent->size == 0) {
						PARRILLADA_JOB_LOG (self, "0 size extent");
						continue;
					}

					current_extent = dvdcss_seek (handle, range->start, DVDCSS_SEEK_KEY);
					if (current_extent != range->start) {
						PARRILLADA_JOB_LOG (self, "Problem: could not retrieve key");
						g_set_error (error,
							     PARRILLADA_BURN_ERROR,
							     PARRILLADA_BURN_ERROR_GENERAL,
							     _("Error while reading video DVD (%s)"),
							     dvdcss_error (handle));
						return FALSE;
					}
				}
			}
		}
		else if (!parrillada_dvdcss_create_scrambled_sectors_map (self, map, handle, file, error))
			return FALSE;
	}

	return TRUE;
}

static gint
parrillada_dvdcss_sort_ranges (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const ParrilladaScrambledSectorRange *range_a = a;
	const ParrilladaScrambledSectorRange *range_b = b;

	return range_a->start - range_b->start;
}

static gpointer
parrillada_dvdcss_write_image_thread (gpointer data)
{
	guchar buf [DVDCSS_BLOCK_SIZE * PARRILLADA_DVDCSS_I_BLOCKS];
	ParrilladaScrambledSectorRange *range = NULL;
	ParrilladaMedium *medium = NULL;
	ParrilladaVolFile *files = NULL;
	dvdcss_handle *handle = NULL;
	ParrilladaDrive *drive = NULL;
	ParrilladaDvdcssPrivate *priv;
	gint64 written_sectors = 0;
	ParrilladaDvdcss *self = data;
	ParrilladaTrack *track = NULL;
	guint64 remaining_sectors;
	FILE *output_fd = NULL;
	ParrilladaVolSrc *vol;
	gint64 volume_size;
	GQueue *map = NULL;

	parrillada_job_set_use_average_rate (PARRILLADA_JOB (self), TRUE);
	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_ANALYSING,
					_("Retrieving DVD keys"),
					FALSE);
	parrillada_job_start_progress (PARRILLADA_JOB (self), FALSE);

	priv = PARRILLADA_DVDCSS_PRIVATE (self);

	/* get the contents of the DVD */
	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));

	vol = parrillada_volume_source_open_file (parrillada_drive_get_device (drive), &priv->error);
	files = parrillada_volume_get_files (vol,
					  0,
					  NULL,
					  NULL,
					  NULL,
					  &priv->error);
	parrillada_volume_source_close (vol);
	if (!files)
		goto end;

	medium = parrillada_drive_get_medium (drive);
	parrillada_medium_get_data_size (medium, NULL, &volume_size);
	if (volume_size == -1) {
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("The size of the volume could not be retrieved"));
		goto end;
	}

	/* create a handle/open DVD */
	handle = dvdcss_open (parrillada_drive_get_device (drive));
	if (!handle) {
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("Video DVD could not be opened"));
		goto end;
	}

	/* look through the files to get the ranges of encrypted sectors
	 * and cache the CSS keys while at it. */
	map = g_queue_new ();
	if (!parrillada_dvdcss_create_scrambled_sectors_map (self, map, handle, files, &priv->error))
		goto end;

	PARRILLADA_JOB_LOG (self, "DVD map created (keys retrieved)");

	g_queue_sort (map, parrillada_dvdcss_sort_ranges, NULL);

	parrillada_volume_file_free (files);
	files = NULL;

	if (dvdcss_seek (handle, 0, DVDCSS_NOFLAGS) < 0) {
		PARRILLADA_JOB_LOG (self, "Error initial seeking");
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   _("Error while reading video DVD (%s)"),
					   dvdcss_error (handle));
		goto end;
	}

	parrillada_job_set_current_action (PARRILLADA_JOB (self),
					PARRILLADA_BURN_ACTION_DRIVE_COPY,
					_("Copying video DVD"),
					FALSE);

	parrillada_job_start_progress (PARRILLADA_JOB (self), TRUE);

	remaining_sectors = volume_size;
	range = g_queue_pop_head (map);

	if (parrillada_job_get_fd_out (PARRILLADA_JOB (self), NULL) != PARRILLADA_BURN_OK) {
		gchar *output = NULL;

		parrillada_job_get_image_output (PARRILLADA_JOB (self), &output, NULL);
		output_fd = fopen (output, "w");
		if (!output_fd) {
			priv->error = g_error_new_literal (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_GENERAL,
							   g_strerror (errno));
			g_free (output);
			goto end;
		}
		g_free (output);
	}

	while (remaining_sectors) {
		gint flag;
		guint64 num_blocks, data_size;

		if (priv->cancel)
			break;

		num_blocks = PARRILLADA_DVDCSS_I_BLOCKS;

		/* see if we are approaching the end of the dvd */
		if (num_blocks > remaining_sectors)
			num_blocks = remaining_sectors;

		/* see if we need to update the key */
		if (!range || written_sectors < range->start) {
			/* this is in a non scrambled sectors range */
			flag = DVDCSS_NOFLAGS;
	
			/* we don't want to mix scrambled and non scrambled sectors */
			if (range && written_sectors + num_blocks > range->start)
				num_blocks = range->start - written_sectors;
		}
		else {
			/* this is in a scrambled sectors range */
			flag = DVDCSS_READ_DECRYPT;

			/* see if we need to update the key */
			if (written_sectors == range->start) {
				int pos;

				pos = dvdcss_seek (handle, written_sectors, DVDCSS_SEEK_KEY);
				if (pos < 0) {
					PARRILLADA_JOB_LOG (self, "Error seeking");
					priv->error = g_error_new (PARRILLADA_BURN_ERROR,
								   PARRILLADA_BURN_ERROR_GENERAL,
								   _("Error while reading video DVD (%s)"),
								   dvdcss_error (handle));
					break;
				}
			}

			/* we don't want to mix scrambled and non scrambled sectors
			 * NOTE: range->end address is the next non scrambled sector */
			if (written_sectors + num_blocks > range->end)
				num_blocks = range->end - written_sectors;

			if (written_sectors + num_blocks == range->end) {
				/* update to get the next range of scrambled sectors */
				g_free (range);
				range = g_queue_pop_head (map);
			}
		}

		num_blocks = dvdcss_read (handle, buf, num_blocks, flag);
		if (num_blocks < 0) {
			PARRILLADA_JOB_LOG (self, "Error reading");
			priv->error = g_error_new (PARRILLADA_BURN_ERROR,
						   PARRILLADA_BURN_ERROR_GENERAL,
						   _("Error while reading video DVD (%s)"),
						   dvdcss_error (handle));
			break;
		}

		data_size = num_blocks * DVDCSS_BLOCK_SIZE;
		if (output_fd) {
			if (fwrite (buf, 1, data_size, output_fd) != data_size) {
                                int errsv = errno;

				priv->error = g_error_new (PARRILLADA_BURN_ERROR,
							   PARRILLADA_BURN_ERROR_GENERAL,
							   _("Data could not be written (%s)"),
							   g_strerror (errsv));
				break;
			}
		}
		else {
			ParrilladaBurnResult result;

			result = parrillada_dvdcss_write_sector_to_fd (self,
								    buf,
								    data_size);
			if (result != PARRILLADA_BURN_OK)
				break;
		}

		written_sectors += num_blocks;
		remaining_sectors -= num_blocks;
		parrillada_job_set_written_track (PARRILLADA_JOB (self), written_sectors * DVDCSS_BLOCK_SIZE);
	}

end:

	if (range)
		g_free (range);

	if (handle)
		dvdcss_close (handle);

	if (files)
		parrillada_volume_file_free (files);

	if (output_fd)
		fclose (output_fd);

	if (map) {
		g_queue_foreach (map, (GFunc) g_free, NULL);
		g_queue_free (map);
	}

	if (!priv->cancel)
		priv->thread_id = g_idle_add (parrillada_dvdcss_thread_finished, self);

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static ParrilladaBurnResult
parrillada_dvdcss_start (ParrilladaJob *job,
		      GError **error)
{
	ParrilladaDvdcss *self;
	ParrilladaJobAction action;
	ParrilladaDvdcssPrivate *priv;
	GError *thread_error = NULL;

	self = PARRILLADA_DVDCSS (job);
	priv = PARRILLADA_DVDCSS_PRIVATE (self);

	parrillada_job_get_action (job, &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		goffset blocks = 0;
		ParrilladaTrack *track;

		parrillada_job_get_current_track (job, &track);
		parrillada_track_get_size (track, &blocks, NULL);
		parrillada_job_set_output_size_for_current_track (job,
							       blocks,
							       blocks * DVDCSS_BLOCK_SIZE);
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	if (priv->thread)
		return PARRILLADA_BURN_RUNNING;

	if (!parrillada_dvdcss_library_init (NULL))
		return PARRILLADA_BURN_ERR;

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (parrillada_dvdcss_write_image_thread,
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
parrillada_dvdcss_stop_real (ParrilladaDvdcss *self)
{
	ParrilladaDvdcssPrivate *priv;

	priv = PARRILLADA_DVDCSS_PRIVATE (self);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}

static ParrilladaBurnResult
parrillada_dvdcss_stop (ParrilladaJob *job,
		     GError **error)
{
	ParrilladaDvdcss *self;

	self = PARRILLADA_DVDCSS (job);

	parrillada_dvdcss_stop_real (self);
	return PARRILLADA_BURN_OK;
}

static void
parrillada_dvdcss_class_init (ParrilladaDvdcssClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDvdcssPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_dvdcss_finalize;

	job_class->start = parrillada_dvdcss_start;
	job_class->stop = parrillada_dvdcss_stop;
}

static void
parrillada_dvdcss_init (ParrilladaDvdcss *obj)
{
	ParrilladaDvdcssPrivate *priv;

	priv = PARRILLADA_DVDCSS_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
parrillada_dvdcss_finalize (GObject *object)
{
	ParrilladaDvdcssPrivate *priv;

	priv = PARRILLADA_DVDCSS_PRIVATE (object);

	parrillada_dvdcss_stop_real (PARRILLADA_DVDCSS (object));

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
parrillada_dvdcss_export_caps (ParrilladaPlugin *plugin)
{
	GSList *output;
	GSList *input;

	parrillada_plugin_define (plugin,
			       "dvdcss",
	                       NULL,
			       _("Copies CSS encrypted video DVDs to a disc image"),
			       "Philippe Rouquier",
			       0);

	/* to my knowledge, css can only be applied to pressed discs so no need
	 * to specify anything else but ROM */
	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					 PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					 PARRILLADA_IMAGE_FORMAT_BIN);
	input = parrillada_caps_disc_new (PARRILLADA_MEDIUM_DVD|
				       PARRILLADA_MEDIUM_DUAL_L|
				       PARRILLADA_MEDIUM_ROM|
				       PARRILLADA_MEDIUM_CLOSED|
				       PARRILLADA_MEDIUM_HAS_DATA|
				       PARRILLADA_MEDIUM_PROTECTED);

	parrillada_plugin_link_caps (plugin, output, input);

	g_slist_free (input);
	g_slist_free (output);
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	parrillada_dvdcss_library_init (plugin);
}

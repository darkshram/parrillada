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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "burn-cdrkit.h"
#include "burn-process.h"
#include "burn-job.h"
#include "parrillada-plugin-registration.h"
#include "parrillada-tags.h"
#include "parrillada-track-disc.h"

#include "burn-volume.h"
#include "parrillada-drive.h"


#define PARRILLADA_TYPE_READOM         (parrillada_readom_get_type ())
#define PARRILLADA_READOM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_READOM, ParrilladaReadom))
#define PARRILLADA_READOM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_READOM, ParrilladaReadomClass))
#define PARRILLADA_IS_READOM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_READOM))
#define PARRILLADA_IS_READOM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_READOM))
#define PARRILLADA_READOM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_READOM, ParrilladaReadomClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaReadom, parrillada_readom, PARRILLADA_TYPE_PROCESS, ParrilladaProcess);

static GObjectClass *parent_class = NULL;

static ParrilladaBurnResult
parrillada_readom_read_stderr (ParrilladaProcess *process, const gchar *line)
{
	ParrilladaReadom *readom;
	gint dummy1;
	gint dummy2;
	gchar *pos;

	readom = PARRILLADA_READOM (process);

	if ((pos = strstr (line, "addr:"))) {
		gint sector;
		gint64 written;
		ParrilladaTrackType *output = NULL;

		pos += strlen ("addr:");
		sector = strtoll (pos, NULL, 10);

		output = parrillada_track_type_new ();
		parrillada_job_get_output_type (PARRILLADA_JOB (readom), output);

		if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_BIN)
			written = (gint64) ((gint64) sector * 2048ULL);
		else if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_CLONE)
			written = (gint64) ((gint64) sector * 2448ULL);
		else
			written = (gint64) ((gint64) sector * 2048ULL);

		parrillada_job_set_written_track (PARRILLADA_JOB (readom), written);

		if (sector > 10)
			parrillada_job_start_progress (PARRILLADA_JOB (readom), FALSE);

		parrillada_track_type_free (output);
	}
	else if ((pos = strstr (line, "Capacity:"))) {
		parrillada_job_set_current_action (PARRILLADA_JOB (readom),
						PARRILLADA_BURN_ACTION_DRIVE_COPY,
						NULL,
						FALSE);
	}
	else if (strstr (line, "Device not ready.")) {
		parrillada_job_error (PARRILLADA_JOB (readom),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_DRIVE_BUSY,
						_("The drive is busy")));
	}
	else if (strstr (line, "Cannot open SCSI driver.")) {
		parrillada_job_error (PARRILLADA_JOB (readom),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));		
	}
	else if (strstr (line, "Cannot send SCSI cmd via ioctl")) {
		parrillada_job_error (PARRILLADA_JOB (readom),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));
	}
	/* we scan for this error as in this case readcd returns success */
	else if (sscanf (line, "Input/output error. Error on sector %d not corrected. Total of %d error", &dummy1, &dummy2) == 2) {
		parrillada_job_error (PARRILLADA_JOB (process),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_GENERAL,
						_("An internal error occurred")));
	}
	else if (strstr (line, "No space left on device")) {
		/* This is necessary as readcd won't return an error code on exit */
		parrillada_job_error (PARRILLADA_JOB (readom),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_DISK_SPACE,
						_("The location you chose to store the image on does not have enough free space for the disc image")));
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_readom_argv_set_iso_boundary (ParrilladaReadom *readom,
				      GPtrArray *argv,
				      GError **error)
{
	goffset nb_blocks;
	ParrilladaTrack *track;
	GValue *value = NULL;
	ParrilladaTrackType *output = NULL;

	parrillada_job_get_current_track (PARRILLADA_JOB (readom), &track);

	output = parrillada_track_type_new ();
	parrillada_job_get_output_type (PARRILLADA_JOB (readom), output);

	parrillada_track_tag_lookup (track,
				  PARRILLADA_TRACK_MEDIUM_ADDRESS_START_TAG,
				  &value);
	if (value) {
		guint64 start, end;

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		parrillada_track_tag_lookup (track,
					  PARRILLADA_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);

		PARRILLADA_JOB_LOG (readom,
				 "reading from sector %lli to %lli",
				 start,
				 end);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							end));
	}
	/* 0 means all disc, -1 problem */
	else if (parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)) > 0) {
		goffset start;
		ParrilladaDrive *drive;
		ParrilladaMedium *medium;

		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
		medium = parrillada_drive_get_medium (drive);
		parrillada_medium_get_track_space (medium,
						parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)),
						NULL,
						&nb_blocks);
		parrillada_medium_get_track_address (medium,
						  parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)),
						  NULL,
						  &start);

		PARRILLADA_JOB_LOG (readom,
				 "reading %i from sector %lli to %lli",
				 parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)),
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							start + nb_blocks));
	}
	/* if it's BIN output just read the last track */
	else if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_BIN) {
		goffset start;
		ParrilladaDrive *drive;
		ParrilladaMedium *medium;

		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
		medium = parrillada_drive_get_medium (drive);
		parrillada_medium_get_last_data_track_space (medium,
							  NULL,
							  &nb_blocks);
		parrillada_medium_get_last_data_track_address (medium,
							    NULL,
							    &start);
		PARRILLADA_JOB_LOG (readom,
				 "reading last track from sector %"G_GINT64_FORMAT" to %"G_GINT64_FORMAT,
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							start + nb_blocks));
	}
	else {
		parrillada_track_get_size (track, &nb_blocks, NULL);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=0-%"G_GINT64_FORMAT, nb_blocks));
	}

	parrillada_track_type_free (output);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_readom_get_size (ParrilladaReadom *self,
			 GError **error)
{
	goffset blocks;
	GValue *value = NULL;
	ParrilladaTrack *track = NULL;
	ParrilladaTrackType *output = NULL;

	output = parrillada_track_type_new ();
	parrillada_job_get_output_type (PARRILLADA_JOB (self), output);

	if (!parrillada_track_type_get_has_image (output)) {
		parrillada_track_type_free (output);
		return PARRILLADA_BURN_ERR;
	}

	parrillada_job_get_current_track (PARRILLADA_JOB (self), &track);
	parrillada_track_tag_lookup (track,
				  PARRILLADA_TRACK_MEDIUM_ADDRESS_START_TAG,
				  &value);
	if (value) {
		guint64 start, end;

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		parrillada_track_tag_lookup (track,
					  PARRILLADA_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);
		blocks = end - start;
	}
	else if (parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)) > 0) {
		ParrilladaDrive *drive;
		ParrilladaMedium *medium;

		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
		medium = parrillada_drive_get_medium (drive);
		parrillada_medium_get_track_space (medium,
						parrillada_track_disc_get_track_num (PARRILLADA_TRACK_DISC (track)),
						NULL,
						&blocks);
	}
	else if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_BIN) {
		ParrilladaDrive *drive;
		ParrilladaMedium *medium;

		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
		medium = parrillada_drive_get_medium (drive);
		parrillada_medium_get_last_data_track_space (medium,
							  NULL,
							  &blocks);
	}
	else
		parrillada_track_get_size (track, &blocks, NULL);

	if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_BIN) {
		parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (self),
							       blocks,
							       blocks * 2048ULL);
	}
	else if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_CLONE) {
		parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (self),
							       blocks,
							       blocks * 2448ULL);
	}
	else {
		parrillada_track_type_free (output);
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	parrillada_track_type_free (output);

	/* no need to go any further */
	return PARRILLADA_BURN_NOT_RUNNING;
}

static ParrilladaBurnResult
parrillada_readom_set_argv (ParrilladaProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	ParrilladaBurnResult result = FALSE;
	ParrilladaTrackType *output = NULL;
	ParrilladaImageFormat format;
	ParrilladaJobAction action;
	ParrilladaReadom *readom;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;
	ParrilladaTrack *track;
	ParrilladaMedia media;
	gchar *outfile_arg;
	gchar *dev_str;

	readom = PARRILLADA_READOM (process);

	/* This is a kind of shortcut */
	parrillada_job_get_action (PARRILLADA_JOB (process), &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE)
		return parrillada_readom_get_size (readom, error);

	g_ptr_array_add (argv, g_strdup ("readom"));

	parrillada_job_get_current_track (PARRILLADA_JOB (readom), &track);
	drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
	if (!parrillada_drive_get_device (drive))
		return PARRILLADA_BURN_ERR;

	dev_str = g_strdup_printf ("dev=%s", parrillada_drive_get_device (drive));
	g_ptr_array_add (argv, dev_str);

	g_ptr_array_add (argv, g_strdup ("-nocorr"));

	medium = parrillada_drive_get_medium (drive);
	media = parrillada_medium_get_status (medium);

	output = parrillada_track_type_new ();
	parrillada_job_get_output_type (PARRILLADA_JOB (readom), output);
	format = parrillada_track_type_get_image_format (output);
	parrillada_track_type_free (output);

	if ((media & PARRILLADA_MEDIUM_DVD)
	&&   format != PARRILLADA_IMAGE_FORMAT_BIN) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("An internal error occurred"));
		return PARRILLADA_BURN_ERR;
	}

	if (format == PARRILLADA_IMAGE_FORMAT_CLONE) {
		/* NOTE: with this option the sector size is 2448 
		 * because it is raw96 (2352+96) otherwise it is 2048  */
		g_ptr_array_add (argv, g_strdup ("-clone"));
	}
	else if (format == PARRILLADA_IMAGE_FORMAT_BIN) {
		g_ptr_array_add (argv, g_strdup ("-noerror"));

		/* don't do it for clone since we need the entire disc */
		result = parrillada_readom_argv_set_iso_boundary (readom, argv, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (readom);

	if (parrillada_job_get_fd_out (PARRILLADA_JOB (readom), NULL) != PARRILLADA_BURN_OK) {
		gchar *image;

		if (format != PARRILLADA_IMAGE_FORMAT_CLONE
		&&  format != PARRILLADA_IMAGE_FORMAT_BIN)
			PARRILLADA_JOB_NOT_SUPPORTED (readom);

		result = parrillada_job_get_image_output (PARRILLADA_JOB (readom),
						       &image,
						       NULL);
		if (result != PARRILLADA_BURN_OK)
			return result;

		outfile_arg = g_strdup_printf ("-f=%s", image);
		g_ptr_array_add (argv, outfile_arg);
		g_free (image);
	}
	else if (format == PARRILLADA_IMAGE_FORMAT_BIN) {
		outfile_arg = g_strdup ("-f=-");
		g_ptr_array_add (argv, outfile_arg);
	}
	else 	/* unfortunately raw images can't be piped out */
		PARRILLADA_JOB_NOT_SUPPORTED (readom);

	parrillada_job_set_use_average_rate (PARRILLADA_JOB (process), TRUE);
	return PARRILLADA_BURN_OK;
}

static void
parrillada_readom_class_init (ParrilladaReadomClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	ParrilladaProcessClass *process_class = PARRILLADA_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_readom_finalize;

	process_class->stderr_func = parrillada_readom_read_stderr;
	process_class->set_argv = parrillada_readom_set_argv;
}

static void
parrillada_readom_init (ParrilladaReadom *obj)
{ }

static void
parrillada_readom_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_readom_export_caps (ParrilladaPlugin *plugin)
{
	GSList *output;
	GSList *input;

	parrillada_plugin_define (plugin,
			       "readom",
	                       NULL,
			       _("Copies any disc to a disc image"),
			       "Philippe Rouquier",
			       1);

	/* that's for clone mode only The only one to copy audio */
	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					 PARRILLADA_IMAGE_FORMAT_CLONE);

	input = parrillada_caps_disc_new (PARRILLADA_MEDIUM_CD|
				       PARRILLADA_MEDIUM_ROM|
				       PARRILLADA_MEDIUM_WRITABLE|
				       PARRILLADA_MEDIUM_REWRITABLE|
				       PARRILLADA_MEDIUM_APPENDABLE|
				       PARRILLADA_MEDIUM_CLOSED|
				       PARRILLADA_MEDIUM_HAS_AUDIO|
				       PARRILLADA_MEDIUM_HAS_DATA);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* that's for regular mode: it accepts the previous type of discs 
	 * plus the DVDs types as well */
	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					 PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					 PARRILLADA_IMAGE_FORMAT_BIN);

	input = parrillada_caps_disc_new (PARRILLADA_MEDIUM_CD|
				       PARRILLADA_MEDIUM_DVD|
				       PARRILLADA_MEDIUM_DUAL_L|
				       PARRILLADA_MEDIUM_PLUS|
				       PARRILLADA_MEDIUM_SEQUENTIAL|
				       PARRILLADA_MEDIUM_RESTRICTED|
				       PARRILLADA_MEDIUM_ROM|
				       PARRILLADA_MEDIUM_WRITABLE|
				       PARRILLADA_MEDIUM_REWRITABLE|
				       PARRILLADA_MEDIUM_CLOSED|
				       PARRILLADA_MEDIUM_APPENDABLE|
				       PARRILLADA_MEDIUM_HAS_DATA);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	parrillada_plugin_register_group (plugin, _(CDRKIT_DESCRIPTION));
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	gint version [3] = { 1, 1, 0};
	parrillada_plugin_test_app (plugin,
	                         "readom",
	                         "--version",
	                         "readcd %*s is not what you see here. This line is only a fake for too clever\nGUIs and other frontend applications. In fact, this program is:\nreadom %d.%d.%d",
	                         version);
}

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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "parrillada-error.h"
#include "parrillada-plugin-registration.h"
#include "burn-job.h"
#include "burn-process.h"
#include "parrillada-track-disc.h"
#include "parrillada-track-image.h"
#include "parrillada-drive.h"
#include "parrillada-medium.h"

#define CDRDAO_DESCRIPTION		N_("cdrdao burning suite")

#define PARRILLADA_TYPE_CDRDAO         (parrillada_cdrdao_get_type ())
#define PARRILLADA_CDRDAO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_CDRDAO, ParrilladaCdrdao))
#define PARRILLADA_CDRDAO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_CDRDAO, ParrilladaCdrdaoClass))
#define PARRILLADA_IS_CDRDAO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_CDRDAO))
#define PARRILLADA_IS_CDRDAO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_CDRDAO))
#define PARRILLADA_CDRDAO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_CDRDAO, ParrilladaCdrdaoClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaCdrdao, parrillada_cdrdao, PARRILLADA_TYPE_PROCESS, ParrilladaProcess);

struct _ParrilladaCdrdaoPrivate {
 	gchar *tmp_toc_path;
	guint use_raw:1;
};
typedef struct _ParrilladaCdrdaoPrivate ParrilladaCdrdaoPrivate;
#define PARRILLADA_CDRDAO_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_CDRDAO, ParrilladaCdrdaoPrivate)) 

static GObjectClass *parent_class = NULL;

#define PARRILLADA_SCHEMA_CONFIG		"org.mate.parrillada.config"
#define PARRILLADA_KEY_RAW_FLAG		"raw-flag"

static gboolean
parrillada_cdrdao_read_stderr_image (ParrilladaCdrdao *cdrdao, const gchar *line)
{
	int min, sec, sub, s1;

	if (sscanf (line, "%d:%d:%d", &min, &sec, &sub) == 3) {
		guint64 secs = min * 60 + sec;

		parrillada_job_set_written_track (PARRILLADA_JOB (cdrdao), secs * 75 * 2352);
		if (secs > 2)
			parrillada_job_start_progress (PARRILLADA_JOB (cdrdao), FALSE);
	}
	else if (sscanf (line, "Leadout %*s %*d %d:%d:%*d(%i)", &min, &sec, &s1) == 3) {
		ParrilladaJobAction action;

		parrillada_job_get_action (PARRILLADA_JOB (cdrdao), &action);
		if (action == PARRILLADA_JOB_ACTION_SIZE) {
			/* get the number of sectors. As we added -raw sector = 2352 bytes */
			parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (cdrdao), s1, (gint64) s1 * 2352ULL);
			parrillada_job_finished_session (PARRILLADA_JOB (cdrdao));
		}
	}
	else if (strstr (line, "Copying audio tracks")) {
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_DRIVE_COPY,
						_("Copying audio track"),
						FALSE);
	}
	else if (strstr (line, "Copying data track")) {
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_DRIVE_COPY,
						_("Copying data track"),
						FALSE);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
parrillada_cdrdao_read_stderr_record (ParrilladaCdrdao *cdrdao, const gchar *line)
{
	int fifo, track, min, sec;
	guint written, total;

	if (sscanf (line, "Wrote %u of %u (Buffers %d%%  %*s", &written, &total, &fifo) >= 2) {
		parrillada_job_set_dangerous (PARRILLADA_JOB (cdrdao), TRUE);

		parrillada_job_set_written_session (PARRILLADA_JOB (cdrdao), written * 1048576);
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_RECORDING,
						NULL,
						FALSE);

		parrillada_job_start_progress (PARRILLADA_JOB (cdrdao), FALSE);
	}
	else if (sscanf (line, "Wrote %*s blocks. Buffer fill min") == 1) {
		/* this is for fixating phase */
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_FIXATING,
						NULL,
						FALSE);
	}
	else if (sscanf (line, "Analyzing track %d %*s start %d:%d:%*d, length %*d:%*d:%*d", &track, &min, &sec) == 3) {
		gchar *string;

		string = g_strdup_printf (_("Analysing track %02i"), track);
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_ANALYSING,
						string,
						TRUE);
		g_free (string);
	}
	else if (sscanf (line, "%d:%d:%*d", &min, &sec) == 2) {
		gint64 written;
		guint64 secs = min * 60 + sec;

		if (secs > 2)
			parrillada_job_start_progress (PARRILLADA_JOB (cdrdao), FALSE);

		written = secs * 75 * 2352;
		parrillada_job_set_written_session (PARRILLADA_JOB (cdrdao), written);
	}
	else if (strstr (line, "Writing track")) {
		parrillada_job_set_dangerous (PARRILLADA_JOB (cdrdao), TRUE);
	}
	else if (strstr (line, "Writing finished successfully")
	     ||  strstr (line, "On-the-fly CD copying finished successfully")) {
		parrillada_job_set_dangerous (PARRILLADA_JOB (cdrdao), FALSE);
	}
	else if (strstr (line, "Blanking disk...")) {
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_BLANKING,
						NULL,
						FALSE);
		parrillada_job_start_progress (PARRILLADA_JOB (cdrdao), FALSE);
		parrillada_job_set_dangerous (PARRILLADA_JOB (cdrdao), TRUE);
	}
	else {
		gchar *name = NULL;
		gchar *cuepath = NULL;
		ParrilladaTrack *track = NULL;
		ParrilladaJobAction action;

		/* Try to catch error could not find cue file */

		/* Track could be NULL here if we're simply blanking a medium */
		parrillada_job_get_action (PARRILLADA_JOB (cdrdao), &action);
		if (action == PARRILLADA_JOB_ACTION_ERASE)
			return TRUE;

		parrillada_job_get_current_track (PARRILLADA_JOB (cdrdao), &track);
		if (!track)
			return FALSE;

		cuepath = parrillada_track_image_get_toc_source (PARRILLADA_TRACK_IMAGE (track), FALSE);

		if (!cuepath)
			return FALSE;

		if (!strstr (line, "ERROR: Could not find input file")) {
			g_free (cuepath);
			return FALSE;
		}

		name = g_path_get_basename (cuepath);
		g_free (cuepath);

		parrillada_job_error (PARRILLADA_JOB (cdrdao),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_FILE_NOT_FOUND,
						/* Translators: %s is a filename */
						_("\"%s\" could not be found"),
						name));
		g_free (name);
	}

	return FALSE;
}

static ParrilladaBurnResult
parrillada_cdrdao_read_stderr (ParrilladaProcess *process, const gchar *line)
{
	ParrilladaCdrdao *cdrdao;
	gboolean result = FALSE;
	ParrilladaJobAction action;

	cdrdao = PARRILLADA_CDRDAO (process);

	parrillada_job_get_action (PARRILLADA_JOB (cdrdao), &action);
	if (action == PARRILLADA_JOB_ACTION_RECORD
	||  action == PARRILLADA_JOB_ACTION_ERASE)
		result = parrillada_cdrdao_read_stderr_record (cdrdao, line);
	else if (action == PARRILLADA_JOB_ACTION_IMAGE
	     ||  action == PARRILLADA_JOB_ACTION_SIZE)
		result = parrillada_cdrdao_read_stderr_image (cdrdao, line);

	if (result)
		return PARRILLADA_BURN_OK;

	if (strstr (line, "Cannot setup device")) {
		parrillada_job_error (PARRILLADA_JOB (cdrdao),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_DRIVE_BUSY,
						_("The drive is busy")));
	}
	else if (strstr (line, "Operation not permitted. Cannot send SCSI")) {
		parrillada_job_error (PARRILLADA_JOB (cdrdao),
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_cdrdao_set_argv_device (ParrilladaCdrdao *cdrdao,
				GPtrArray *argv)
{
	gchar *device = NULL;

	g_ptr_array_add (argv, g_strdup ("--device"));

	/* NOTE: that function returns either bus_target_lun or the device path
	 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
	 * which is the only OS in need for that. For all others it returns the device
	 * path. */
	parrillada_job_get_bus_target_lun (PARRILLADA_JOB (cdrdao), &device);
	g_ptr_array_add (argv, device);
}

static void
parrillada_cdrdao_set_argv_common_rec (ParrilladaCdrdao *cdrdao,
				    GPtrArray *argv)
{
	ParrilladaBurnFlag flags;
	gchar *speed_str;
	guint speed;

	parrillada_job_get_flags (PARRILLADA_JOB (cdrdao), &flags);
	if (flags & PARRILLADA_BURN_FLAG_DUMMY)
		g_ptr_array_add (argv, g_strdup ("--simulate"));

	g_ptr_array_add (argv, g_strdup ("--speed"));

	parrillada_job_get_speed (PARRILLADA_JOB (cdrdao), &speed);
	speed_str = g_strdup_printf ("%d", speed);
	g_ptr_array_add (argv, speed_str);

	if (flags & PARRILLADA_BURN_FLAG_OVERBURN)
		g_ptr_array_add (argv, g_strdup ("--overburn"));
	if (flags & PARRILLADA_BURN_FLAG_MULTI)
		g_ptr_array_add (argv, g_strdup ("--multi"));
}

static void
parrillada_cdrdao_set_argv_common (ParrilladaCdrdao *cdrdao,
				GPtrArray *argv)
{
	ParrilladaBurnFlag flags;

	parrillada_job_get_flags (PARRILLADA_JOB (cdrdao), &flags);

	/* cdrdao manual says it is a similar option to gracetime */
	if (flags & PARRILLADA_BURN_FLAG_NOGRACE)
		g_ptr_array_add (argv, g_strdup ("-n"));

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));
}

static ParrilladaBurnResult
parrillada_cdrdao_set_argv_record (ParrilladaCdrdao *cdrdao,
				GPtrArray *argv)
{
	ParrilladaTrackType *type = NULL;
	ParrilladaCdrdaoPrivate *priv;

	priv = PARRILLADA_CDRDAO_PRIVATE (cdrdao); 

	g_ptr_array_add (argv, g_strdup ("cdrdao"));

	type = parrillada_track_type_new ();
	parrillada_job_get_input_type (PARRILLADA_JOB (cdrdao), type);

        if (parrillada_track_type_get_has_medium (type)) {
		ParrilladaDrive *drive;
		ParrilladaTrack *track;
		ParrilladaBurnFlag flags;

		g_ptr_array_add (argv, g_strdup ("copy"));
		parrillada_cdrdao_set_argv_device (cdrdao, argv);
		parrillada_cdrdao_set_argv_common (cdrdao, argv);
		parrillada_cdrdao_set_argv_common_rec (cdrdao, argv);

		parrillada_job_get_flags (PARRILLADA_JOB (cdrdao), &flags);
		if (flags & PARRILLADA_BURN_FLAG_NO_TMP_FILES)
			g_ptr_array_add (argv, g_strdup ("--on-the-fly"));

		if (priv->use_raw)
		  	g_ptr_array_add (argv, g_strdup ("--driver generic-mmc-raw")); 

		g_ptr_array_add (argv, g_strdup ("--source-device"));

		parrillada_job_get_current_track (PARRILLADA_JOB (cdrdao), &track);
		drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));

		/* NOTE: that function returns either bus_target_lun or the device path
		 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
		 * which is the only OS in need for that. For all others it returns the device
		 * path. */
		g_ptr_array_add (argv, parrillada_drive_get_bus_target_lun_string (drive));
	}
	else if (parrillada_track_type_get_has_image (type)) {
		gchar *cuepath;
		ParrilladaTrack *track;

		g_ptr_array_add (argv, g_strdup ("write"));
		
		parrillada_job_get_current_track (PARRILLADA_JOB (cdrdao), &track);

		if (parrillada_track_type_get_image_format (type) == PARRILLADA_IMAGE_FORMAT_CUE) {
			gchar *parent;

			cuepath = parrillada_track_image_get_toc_source (PARRILLADA_TRACK_IMAGE (track), FALSE);
			parent = g_path_get_dirname (cuepath);
			parrillada_process_set_working_directory (PARRILLADA_PROCESS (cdrdao), parent);
			g_free (parent);

			/* This does not work as toc2cue will use BINARY even if
			 * if endianness is big endian */
			/* we need to check endianness */
			/* if (parrillada_track_image_need_byte_swap (PARRILLADA_TRACK_IMAGE (track)))
				g_ptr_array_add (argv, g_strdup ("--swap")); */
		}
		else if (parrillada_track_type_get_image_format (type) == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
			/* CDRDAO files are always BIG ENDIAN */
			cuepath = parrillada_track_image_get_toc_source (PARRILLADA_TRACK_IMAGE (track), FALSE);
		}
		else {
			parrillada_track_type_free (type);
			PARRILLADA_JOB_NOT_SUPPORTED (cdrdao);
		}

		if (!cuepath) {
			parrillada_track_type_free (type);
			PARRILLADA_JOB_NOT_READY (cdrdao);
		}

		parrillada_cdrdao_set_argv_device (cdrdao, argv);
		parrillada_cdrdao_set_argv_common (cdrdao, argv);
		parrillada_cdrdao_set_argv_common_rec (cdrdao, argv);

		g_ptr_array_add (argv, cuepath);
	}
	else {
		parrillada_track_type_free (type);
		PARRILLADA_JOB_NOT_SUPPORTED (cdrdao);
	}

	parrillada_track_type_free (type);
	parrillada_job_set_use_average_rate (PARRILLADA_JOB (cdrdao), TRUE);
	parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
					PARRILLADA_BURN_ACTION_START_RECORDING,
					NULL,
					FALSE);
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_cdrdao_set_argv_blank (ParrilladaCdrdao *cdrdao,
			       GPtrArray *argv)
{
	ParrilladaBurnFlag flags;

	g_ptr_array_add (argv, g_strdup ("cdrdao"));
	g_ptr_array_add (argv, g_strdup ("blank"));

	parrillada_cdrdao_set_argv_device (cdrdao, argv);
	parrillada_cdrdao_set_argv_common (cdrdao, argv);

	g_ptr_array_add (argv, g_strdup ("--blank-mode"));
	parrillada_job_get_flags (PARRILLADA_JOB (cdrdao), &flags);
	if (!(flags & PARRILLADA_BURN_FLAG_FAST_BLANK))
		g_ptr_array_add (argv, g_strdup ("full"));
	else
		g_ptr_array_add (argv, g_strdup ("minimal"));

	parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
					PARRILLADA_BURN_ACTION_BLANKING,
					NULL,
					FALSE);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_cdrdao_post (ParrilladaJob *job)
{
	ParrilladaCdrdaoPrivate *priv;

	priv = PARRILLADA_CDRDAO_PRIVATE (job);
	if (!priv->tmp_toc_path) {
		parrillada_job_finished_session (job);
		return PARRILLADA_BURN_OK;
	}

	/* we have to run toc2cue now to convert the toc file into a cue file */
	return PARRILLADA_BURN_RETRY;
}

static ParrilladaBurnResult
parrillada_cdrdao_start_toc2cue (ParrilladaCdrdao *cdrdao,
			      GPtrArray *argv,
			      GError **error)
{
	gchar *cue_output;
	ParrilladaBurnResult result;
	ParrilladaCdrdaoPrivate *priv;

	priv = PARRILLADA_CDRDAO_PRIVATE (cdrdao);

	g_ptr_array_add (argv, g_strdup ("toc2cue"));

	g_ptr_array_add (argv, priv->tmp_toc_path);
	priv->tmp_toc_path = NULL;

	result = parrillada_job_get_image_output (PARRILLADA_JOB (cdrdao),
					       NULL,
					       &cue_output);
	if (result != PARRILLADA_BURN_OK)
		return result;

	g_ptr_array_add (argv, cue_output);

	/* if there is a file toc2cue will fail */
	g_remove (cue_output);

	parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
					PARRILLADA_BURN_ACTION_CREATING_IMAGE,
					_("Converting toc file"),
					TRUE);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_cdrdao_set_argv_image (ParrilladaCdrdao *cdrdao,
			       GPtrArray *argv,
			       GError **error)
{
	gchar *image = NULL, *toc = NULL;
	ParrilladaTrackType *output = NULL;
	ParrilladaCdrdaoPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaJobAction action;
	ParrilladaDrive *drive;
	ParrilladaTrack *track;

	priv = PARRILLADA_CDRDAO_PRIVATE (cdrdao);
	if (priv->tmp_toc_path)
		return parrillada_cdrdao_start_toc2cue (cdrdao, argv, error);

	g_ptr_array_add (argv, g_strdup ("cdrdao"));
	g_ptr_array_add (argv, g_strdup ("read-cd"));
	g_ptr_array_add (argv, g_strdup ("--device"));

	parrillada_job_get_current_track (PARRILLADA_JOB (cdrdao), &track);
	drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));

	/* NOTE: that function returns either bus_target_lun or the device path
	 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
	 * which is the only OS in need for that. For all others it returns the device
	 * path. */
	g_ptr_array_add (argv, parrillada_drive_get_bus_target_lun_string (drive));
	g_ptr_array_add (argv, g_strdup ("--read-raw"));

	/* This is done so that if a cue file is required we first generate
	 * a temporary toc file that will be later converted to a cue file.
	 * The datafile is written where it should be from the start. */
	output = parrillada_track_type_new ();
	parrillada_job_get_output_type (PARRILLADA_JOB (cdrdao), output);

	if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
		result = parrillada_job_get_image_output (PARRILLADA_JOB (cdrdao),
						       &image,
						       &toc);
		if (result != PARRILLADA_BURN_OK) {
			parrillada_track_type_free (output);
			return result;
		}
	}
	else if (parrillada_track_type_get_image_format (output) == PARRILLADA_IMAGE_FORMAT_CUE) {
		/* NOTE: we don't generate the .cue file right away; we'll call
		 * toc2cue right after we finish */
		result = parrillada_job_get_image_output (PARRILLADA_JOB (cdrdao),
						       &image,
						       NULL);
		if (result != PARRILLADA_BURN_OK) {
			parrillada_track_type_free (output);
			return result;
		}

		result = parrillada_job_get_tmp_file (PARRILLADA_JOB (cdrdao),
						   NULL,
						   &toc,
						   error);
		if (result != PARRILLADA_BURN_OK) {
			g_free (image);
			parrillada_track_type_free (output);
			return result;
		}

		/* save the temporary toc path to resuse it later. */
		priv->tmp_toc_path = g_strdup (toc);
	}

	parrillada_track_type_free (output);

	/* it's safe to remove them: session/task make sure they don't exist 
	 * when there is the proper flag whether it be tmp or real output. */ 
	if (toc)
		g_remove (toc);
	if (image)
		g_remove (image);

	parrillada_job_get_action (PARRILLADA_JOB (cdrdao), &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		parrillada_job_set_current_action (PARRILLADA_JOB (cdrdao),
						PARRILLADA_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		parrillada_job_start_progress (PARRILLADA_JOB (cdrdao), FALSE);
	}

	g_ptr_array_add (argv, g_strdup ("--datafile"));
	g_ptr_array_add (argv, image);

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));

	g_ptr_array_add (argv, toc);

	parrillada_job_set_use_average_rate (PARRILLADA_JOB (cdrdao), TRUE);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_cdrdao_set_argv (ParrilladaProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	ParrilladaCdrdao *cdrdao;
	ParrilladaJobAction action;

	cdrdao = PARRILLADA_CDRDAO (process);

	/* sets the first argv */
	parrillada_job_get_action (PARRILLADA_JOB (cdrdao), &action);
	if (action == PARRILLADA_JOB_ACTION_RECORD)
		return parrillada_cdrdao_set_argv_record (cdrdao, argv);
	else if (action == PARRILLADA_JOB_ACTION_ERASE)
		return parrillada_cdrdao_set_argv_blank (cdrdao, argv);
	else if (action == PARRILLADA_JOB_ACTION_IMAGE)
		return parrillada_cdrdao_set_argv_image (cdrdao, argv, error);
	else if (action == PARRILLADA_JOB_ACTION_SIZE) {
		ParrilladaTrack *track;

		parrillada_job_get_current_track (PARRILLADA_JOB (cdrdao), &track);
		if (PARRILLADA_IS_TRACK_DISC (track)) {
			goffset sectors = 0;

			parrillada_track_get_size (track, &sectors, NULL);

			/* cdrdao won't get a track size under 300 sectors */
			if (sectors < 300)
				sectors = 300;

			parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (cdrdao),
								       sectors,
								       sectors * 2352ULL);
		}
		else
			return PARRILLADA_BURN_NOT_SUPPORTED;

		return PARRILLADA_BURN_NOT_RUNNING;
	}

	PARRILLADA_JOB_NOT_SUPPORTED (cdrdao);
}

static void
parrillada_cdrdao_class_init (ParrilladaCdrdaoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaProcessClass *process_class = PARRILLADA_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaCdrdaoPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = parrillada_cdrdao_finalize;

	process_class->stderr_func = parrillada_cdrdao_read_stderr;
	process_class->set_argv = parrillada_cdrdao_set_argv;
	process_class->post = parrillada_cdrdao_post;
}

static void
parrillada_cdrdao_init (ParrilladaCdrdao *obj)
{  
	GSettings *settings;
 	ParrilladaCdrdaoPrivate *priv;
 	
	/* load our "configuration" */
 	priv = PARRILLADA_CDRDAO_PRIVATE (obj);

	settings = g_settings_new (PARRILLADA_SCHEMA_CONFIG);
	priv->use_raw = g_settings_get_boolean (settings, PARRILLADA_KEY_RAW_FLAG);
	g_object_unref (settings);
}

static void
parrillada_cdrdao_finalize (GObject *object)
{
	ParrilladaCdrdaoPrivate *priv;

	priv = PARRILLADA_CDRDAO_PRIVATE (object);
	if (priv->tmp_toc_path) {
		g_free (priv->tmp_toc_path);
		priv->tmp_toc_path = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_cdrdao_export_caps (ParrilladaPlugin *plugin)
{
	GSList *input;
	GSList *output;
	ParrilladaPluginConfOption *use_raw; 
	const ParrilladaMedia media_w = PARRILLADA_MEDIUM_CD|
				     PARRILLADA_MEDIUM_WRITABLE|
				     PARRILLADA_MEDIUM_REWRITABLE|
				     PARRILLADA_MEDIUM_BLANK;
	const ParrilladaMedia media_rw = PARRILLADA_MEDIUM_CD|
				      PARRILLADA_MEDIUM_REWRITABLE|
				      PARRILLADA_MEDIUM_APPENDABLE|
				      PARRILLADA_MEDIUM_CLOSED|
				      PARRILLADA_MEDIUM_HAS_DATA|
				      PARRILLADA_MEDIUM_HAS_AUDIO|
				      PARRILLADA_MEDIUM_BLANK;

	parrillada_plugin_define (plugin,
			       "cdrdao",
	                       NULL,
			       _("Copies, burns and blanks CDs"),
			       "Philippe Rouquier",
			       0);

	/* that's for cdrdao images: CDs only as input */
	input = parrillada_caps_disc_new (PARRILLADA_MEDIUM_CD|
				       PARRILLADA_MEDIUM_ROM|
				       PARRILLADA_MEDIUM_WRITABLE|
				       PARRILLADA_MEDIUM_REWRITABLE|
				       PARRILLADA_MEDIUM_APPENDABLE|
				       PARRILLADA_MEDIUM_CLOSED|
				       PARRILLADA_MEDIUM_HAS_AUDIO|
				       PARRILLADA_MEDIUM_HAS_DATA);

	/* an image can be created ... */
	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					 PARRILLADA_IMAGE_FORMAT_CDRDAO);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					 PARRILLADA_IMAGE_FORMAT_CUE);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... or a disc */
	output = parrillada_caps_disc_new (media_w);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	/* cdrdao can also record these types of images to a disc */
	input = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_IMAGE_FORMAT_CDRDAO|
					PARRILLADA_IMAGE_FORMAT_CUE);
	
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* cdrdao is used to burn images so it can't APPEND and the disc must
	 * have been blanked before (it can't overwrite)
	 * NOTE: PARRILLADA_MEDIUM_FILE is needed here because of restriction API
	 * when we output an image. */
	parrillada_plugin_set_flags (plugin,
				  media_w|
				  PARRILLADA_MEDIUM_FILE,
				  PARRILLADA_BURN_FLAG_DAO|
				  PARRILLADA_BURN_FLAG_BURNPROOF|
				  PARRILLADA_BURN_FLAG_OVERBURN|
				  PARRILLADA_BURN_FLAG_DUMMY|
				  PARRILLADA_BURN_FLAG_NOGRACE,
				  PARRILLADA_BURN_FLAG_NONE);

	/* cdrdao can also blank */
	output = parrillada_caps_disc_new (media_rw);
	parrillada_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	parrillada_plugin_set_blank_flags (plugin,
					media_rw,
					PARRILLADA_BURN_FLAG_NOGRACE|
					PARRILLADA_BURN_FLAG_FAST_BLANK,
					PARRILLADA_BURN_FLAG_NONE);

	use_raw = parrillada_plugin_conf_option_new (PARRILLADA_KEY_RAW_FLAG,
						  _("Enable the \"--driver generic-mmc-raw\" flag (see cdrdao manual)"),
						  PARRILLADA_PLUGIN_OPTION_BOOL);

	parrillada_plugin_add_conf_option (plugin, use_raw);

	parrillada_plugin_register_group (plugin, _(CDRDAO_DESCRIPTION));
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	gint version [3] = { 1, 2, 0};
	parrillada_plugin_test_app (plugin,
	                         "cdrdao",
	                         "version",
	                         "Cdrdao version %d.%d.%d - (C) Andreas Mueller <andreas@daneb.de>",
	                         version);

	parrillada_plugin_test_app (plugin,
	                         "toc2cue",
	                         "-V",
	                         "%d.%d.%d",
	                         version);
}

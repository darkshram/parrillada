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

#ifndef JOB_H
#define JOB_H

#include <glib.h>
#include <glib-object.h>

#include "parrillada-track.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_JOB         (parrillada_job_get_type ())
#define PARRILLADA_JOB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_JOB, ParrilladaJob))
#define PARRILLADA_JOB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_JOB, ParrilladaJobClass))
#define PARRILLADA_IS_JOB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_JOB))
#define PARRILLADA_IS_JOB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_JOB))
#define PARRILLADA_JOB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_JOB, ParrilladaJobClass))

typedef enum {
	PARRILLADA_JOB_ACTION_NONE		= 0,
	PARRILLADA_JOB_ACTION_SIZE,
	PARRILLADA_JOB_ACTION_IMAGE,
	PARRILLADA_JOB_ACTION_RECORD,
	PARRILLADA_JOB_ACTION_ERASE,
	PARRILLADA_JOB_ACTION_CHECKSUM
} ParrilladaJobAction;

typedef struct {
	GObject parent;
} ParrilladaJob;

typedef struct {
	GObjectClass parent_class;

	/**
	 * Virtual functions to implement in each object deriving from
	 * ParrilladaJob.
	 */

	/**
	 * This function is not compulsory. If not implemented then the library
	 * will act as if OK had been returned.
	 * returns 	PARRILLADA_BURN_OK on successful initialization
	 *		The job can return PARRILLADA_BURN_NOT_RUNNING if it should
	 *		not be started.
	 * 		PARRILLADA_BURN_ERR otherwise
	 */
	ParrilladaBurnResult	(*activate)		(ParrilladaJob *job,
							 GError **error);

	/**
	 * This function is compulsory.
	 * returns 	PARRILLADA_BURN_OK if a loop should be run afterward
	 *		The job can return PARRILLADA_BURN_NOT_RUNNING if it already
	 *		completed successfully the task or don't need to be run. In this
	 *		case, it's the whole task that will be skipped.
	 *		NOT_SUPPORTED if it can't do the action required. When running
	 *		in fake mode (to get size mostly), the job will be "forgiven" and
	 *		deactivated.
	 *		RETRY if the job is not able to start at the moment but should
	 *		be given another chance later.
	 * 		ERR otherwise
	 */
	ParrilladaBurnResult	(*start)		(ParrilladaJob *job,
							 GError **error);

	ParrilladaBurnResult	(*clock_tick)		(ParrilladaJob *job);

	ParrilladaBurnResult	(*stop)			(ParrilladaJob *job,
							 GError **error);

	/**
	 * you should not connect to this signal. It's used internally to 
	 * "autoconfigure" the backend when an error occurs
	 */
	ParrilladaBurnResult	(*error)		(ParrilladaJob *job,
							 ParrilladaBurnError error);
} ParrilladaJobClass;

GType parrillada_job_get_type (void);

/**
 * These functions are to be used to get information for running jobs.
 * They are only available when a job is running.
 */

ParrilladaBurnResult
parrillada_job_set_nonblocking (ParrilladaJob *self,
			     GError **error);

ParrilladaBurnResult
parrillada_job_get_action (ParrilladaJob *job, ParrilladaJobAction *action);

ParrilladaBurnResult
parrillada_job_get_flags (ParrilladaJob *job, ParrilladaBurnFlag *flags);

ParrilladaBurnResult
parrillada_job_get_fd_in (ParrilladaJob *job, int *fd_in);

ParrilladaBurnResult
parrillada_job_get_tracks (ParrilladaJob *job, GSList **tracks);

ParrilladaBurnResult
parrillada_job_get_done_tracks (ParrilladaJob *job, GSList **tracks);

ParrilladaBurnResult
parrillada_job_get_current_track (ParrilladaJob *job, ParrilladaTrack **track);

ParrilladaBurnResult
parrillada_job_get_input_type (ParrilladaJob *job, ParrilladaTrackType *type);

ParrilladaBurnResult
parrillada_job_get_audio_title (ParrilladaJob *job, gchar **album);

ParrilladaBurnResult
parrillada_job_get_data_label (ParrilladaJob *job, gchar **label);

ParrilladaBurnResult
parrillada_job_get_session_output_size (ParrilladaJob *job,
				     goffset *blocks,
				     goffset *bytes);

/**
 * Used to get information of the destination media
 */

ParrilladaBurnResult
parrillada_job_get_medium (ParrilladaJob *job, ParrilladaMedium **medium);

ParrilladaBurnResult
parrillada_job_get_bus_target_lun (ParrilladaJob *job, gchar **BTL);

ParrilladaBurnResult
parrillada_job_get_device (ParrilladaJob *job, gchar **device);

ParrilladaBurnResult
parrillada_job_get_media (ParrilladaJob *job, ParrilladaMedia *media);

ParrilladaBurnResult
parrillada_job_get_last_session_address (ParrilladaJob *job, goffset *address);

ParrilladaBurnResult
parrillada_job_get_next_writable_address (ParrilladaJob *job, goffset *address);

ParrilladaBurnResult
parrillada_job_get_rate (ParrilladaJob *job, guint64 *rate);

ParrilladaBurnResult
parrillada_job_get_speed (ParrilladaJob *self, guint *speed);

ParrilladaBurnResult
parrillada_job_get_max_rate (ParrilladaJob *job, guint64 *rate);

ParrilladaBurnResult
parrillada_job_get_max_speed (ParrilladaJob *job, guint *speed);

/**
 * necessary for objects imaging either to another or to a file
 */

ParrilladaBurnResult
parrillada_job_get_output_type (ParrilladaJob *job, ParrilladaTrackType *type);

ParrilladaBurnResult
parrillada_job_get_fd_out (ParrilladaJob *job, int *fd_out);

ParrilladaBurnResult
parrillada_job_get_image_output (ParrilladaJob *job,
			      gchar **image,
			      gchar **toc);
ParrilladaBurnResult
parrillada_job_get_audio_output (ParrilladaJob *job,
			      gchar **output);

/**
 * get a temporary file that will be deleted once the session is finished
 */
 
ParrilladaBurnResult
parrillada_job_get_tmp_file (ParrilladaJob *job,
			  const gchar *suffix,
			  gchar **output,
			  GError **error);

ParrilladaBurnResult
parrillada_job_get_tmp_dir (ParrilladaJob *job,
			 gchar **path,
			 GError **error);

/**
 * Each tag can be retrieved by any job
 */

ParrilladaBurnResult
parrillada_job_tag_lookup (ParrilladaJob *job,
			const gchar *tag,
			GValue **value);

ParrilladaBurnResult
parrillada_job_tag_add (ParrilladaJob *job,
		     const gchar *tag,
		     GValue *value);

/**
 * Used to give job results and tell when a job has finished
 */

ParrilladaBurnResult
parrillada_job_add_track (ParrilladaJob *job,
		       ParrilladaTrack *track);

ParrilladaBurnResult
parrillada_job_finished_track (ParrilladaJob *job);

ParrilladaBurnResult
parrillada_job_finished_session (ParrilladaJob *job);

ParrilladaBurnResult
parrillada_job_error (ParrilladaJob *job,
		   GError *error);

/**
 * Used to start progress reporting and starts an internal timer to keep track
 * of remaining time among other things
 */

ParrilladaBurnResult
parrillada_job_start_progress (ParrilladaJob *job,
			    gboolean force);

/**
 * task progress report: you can use only some of these functions
 */

ParrilladaBurnResult
parrillada_job_set_rate (ParrilladaJob *job,
		      gint64 rate);
ParrilladaBurnResult
parrillada_job_set_written_track (ParrilladaJob *job,
			       goffset written);
ParrilladaBurnResult
parrillada_job_set_written_session (ParrilladaJob *job,
				 goffset written);
ParrilladaBurnResult
parrillada_job_set_progress (ParrilladaJob *job,
			  gdouble progress);
ParrilladaBurnResult
parrillada_job_reset_progress (ParrilladaJob *job);
ParrilladaBurnResult
parrillada_job_set_current_action (ParrilladaJob *job,
				ParrilladaBurnAction action,
				const gchar *string,
				gboolean force);
ParrilladaBurnResult
parrillada_job_get_current_action (ParrilladaJob *job,
				ParrilladaBurnAction *action);
ParrilladaBurnResult
parrillada_job_set_output_size_for_current_track (ParrilladaJob *job,
					       goffset sectors,
					       goffset bytes);

/**
 * Used to tell it's (or not) dangerous to interrupt this job
 */

void
parrillada_job_set_dangerous (ParrilladaJob *job, gboolean value);

/**
 * This is for apps with a jerky current rate (like cdrdao)
 */

ParrilladaBurnResult
parrillada_job_set_use_average_rate (ParrilladaJob *job,
				  gboolean value);

/**
 * logging facilities
 */

void
parrillada_job_log_message (ParrilladaJob *job,
			 const gchar *location,
			 const gchar *format,
			 ...);

#define PARRILLADA_JOB_LOG(job, message, ...) 			\
{								\
	gchar *format;						\
	format = g_strdup_printf ("%s %s",			\
				  G_OBJECT_TYPE_NAME (job),	\
				  message);			\
	parrillada_job_log_message (PARRILLADA_JOB (job),		\
				 G_STRLOC,			\
				 format,		 	\
				 ##__VA_ARGS__);		\
	g_free (format);					\
}
#define PARRILLADA_JOB_LOG_ARG(job, message, ...)			\
{								\
	gchar *format;						\
	format = g_strdup_printf ("\t%s",			\
				  (gchar*) message);		\
	parrillada_job_log_message (PARRILLADA_JOB (job),		\
				 G_STRLOC,			\
				 format,			\
				 ##__VA_ARGS__);		\
	g_free (format);					\
}

#define PARRILLADA_JOB_NOT_SUPPORTED(job) 					\
	{								\
		PARRILLADA_JOB_LOG (job, "unsupported operation");		\
		return PARRILLADA_BURN_NOT_SUPPORTED;			\
	}

#define PARRILLADA_JOB_NOT_READY(job)						\
	{									\
		PARRILLADA_JOB_LOG (job, "not ready to operate");	\
		return PARRILLADA_BURN_NOT_READY;					\
	}


G_END_DECLS

#endif /* JOB_H */

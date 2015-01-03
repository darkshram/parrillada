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

#ifndef _BURN_TASK_CTX_H_
#define _BURN_TASK_CTX_H_

#include <glib-object.h>

#include "burn-basics.h"
#include "parrillada-session.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TASK_CTX             (parrillada_task_ctx_get_type ())
#define PARRILLADA_TASK_CTX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TASK_CTX, ParrilladaTaskCtx))
#define PARRILLADA_TASK_CTX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TASK_CTX, ParrilladaTaskCtxClass))
#define PARRILLADA_IS_TASK_CTX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TASK_CTX))
#define PARRILLADA_IS_TASK_CTX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TASK_CTX))
#define PARRILLADA_TASK_CTX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TASK_CTX, ParrilladaTaskCtxClass))

typedef enum {
	PARRILLADA_TASK_ACTION_NONE		= 0,
	PARRILLADA_TASK_ACTION_ERASE,
	PARRILLADA_TASK_ACTION_NORMAL,
	PARRILLADA_TASK_ACTION_CHECKSUM,
} ParrilladaTaskAction;

typedef struct _ParrilladaTaskCtxClass ParrilladaTaskCtxClass;
typedef struct _ParrilladaTaskCtx ParrilladaTaskCtx;

struct _ParrilladaTaskCtxClass
{
	GObjectClass parent_class;

	void			(* finished)		(ParrilladaTaskCtx *ctx,
							 ParrilladaBurnResult retval,
							 GError *error);

	/* signals */
	void			(*progress_changed)	(ParrilladaTaskCtx *task,
							 gdouble fraction,
							 glong remaining_time);
	void			(*action_changed)	(ParrilladaTaskCtx *task,
							 ParrilladaBurnAction action);
};

struct _ParrilladaTaskCtx
{
	GObject parent_instance;
};

GType parrillada_task_ctx_get_type (void) G_GNUC_CONST;

void
parrillada_task_ctx_reset (ParrilladaTaskCtx *ctx);

void
parrillada_task_ctx_set_fake (ParrilladaTaskCtx *ctx,
			   gboolean fake);

void
parrillada_task_ctx_set_dangerous (ParrilladaTaskCtx *ctx,
				gboolean value);

guint
parrillada_task_ctx_get_dangerous (ParrilladaTaskCtx *ctx);

/**
 * Used to get the session it is associated with
 */

ParrilladaBurnSession *
parrillada_task_ctx_get_session (ParrilladaTaskCtx *ctx);

ParrilladaTaskAction
parrillada_task_ctx_get_action (ParrilladaTaskCtx *ctx);

ParrilladaBurnResult
parrillada_task_ctx_get_stored_tracks (ParrilladaTaskCtx *ctx,
				    GSList **tracks);

ParrilladaBurnResult
parrillada_task_ctx_get_current_track (ParrilladaTaskCtx *ctx,
				    ParrilladaTrack **track);

/**
 * Used to give job results and tell when a job has finished
 */

ParrilladaBurnResult
parrillada_task_ctx_add_track (ParrilladaTaskCtx *ctx,
			    ParrilladaTrack *track);

ParrilladaBurnResult
parrillada_task_ctx_next_track (ParrilladaTaskCtx *ctx);

ParrilladaBurnResult
parrillada_task_ctx_finished (ParrilladaTaskCtx *ctx);

ParrilladaBurnResult
parrillada_task_ctx_error (ParrilladaTaskCtx *ctx,
			ParrilladaBurnResult retval,
			GError *error);

/**
 * Used to start progress reporting and starts an internal timer to keep track
 * of remaining time among other things
 */

ParrilladaBurnResult
parrillada_task_ctx_start_progress (ParrilladaTaskCtx *ctx,
				 gboolean force);

void
parrillada_task_ctx_report_progress (ParrilladaTaskCtx *ctx);

void
parrillada_task_ctx_stop_progress (ParrilladaTaskCtx *ctx);

/**
 * task progress report for jobs
 */

ParrilladaBurnResult
parrillada_task_ctx_set_rate (ParrilladaTaskCtx *ctx,
			   gint64 rate);

ParrilladaBurnResult
parrillada_task_ctx_set_written_session (ParrilladaTaskCtx *ctx,
				      gint64 written);
ParrilladaBurnResult
parrillada_task_ctx_set_written_track (ParrilladaTaskCtx *ctx,
				    gint64 written);
ParrilladaBurnResult
parrillada_task_ctx_reset_progress (ParrilladaTaskCtx *ctx);
ParrilladaBurnResult
parrillada_task_ctx_set_progress (ParrilladaTaskCtx *ctx,
			       gdouble progress);
ParrilladaBurnResult
parrillada_task_ctx_set_current_action (ParrilladaTaskCtx *ctx,
				     ParrilladaBurnAction action,
				     const gchar *string,
				     gboolean force);
ParrilladaBurnResult
parrillada_task_ctx_set_use_average (ParrilladaTaskCtx *ctx,
				  gboolean use_average);
ParrilladaBurnResult
parrillada_task_ctx_set_output_size_for_current_track (ParrilladaTaskCtx *ctx,
						    goffset sectors,
						    goffset bytes);

/**
 * task progress for library
 */

ParrilladaBurnResult
parrillada_task_ctx_get_rate (ParrilladaTaskCtx *ctx,
			   guint64 *rate);
ParrilladaBurnResult
parrillada_task_ctx_get_remaining_time (ParrilladaTaskCtx *ctx,
				     long *remaining);
ParrilladaBurnResult
parrillada_task_ctx_get_session_output_size (ParrilladaTaskCtx *ctx,
					  goffset *blocks,
					  goffset *bytes);
ParrilladaBurnResult
parrillada_task_ctx_get_written (ParrilladaTaskCtx *ctx,
			      goffset *written);
ParrilladaBurnResult
parrillada_task_ctx_get_current_action_string (ParrilladaTaskCtx *ctx,
					    ParrilladaBurnAction action,
					    gchar **string);
ParrilladaBurnResult
parrillada_task_ctx_get_progress (ParrilladaTaskCtx *ctx, 
			       gdouble *progress);
ParrilladaBurnResult
parrillada_task_ctx_get_current_action (ParrilladaTaskCtx *ctx,
				     ParrilladaBurnAction *action);

G_END_DECLS

#endif /* _BURN_TASK_CTX_H_ */

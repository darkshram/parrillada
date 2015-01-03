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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "parrillada-session.h"
#include "parrillada-session-helper.h"
#include "burn-debug.h"
#include "burn-task-ctx.h"

typedef struct _ParrilladaTaskCtxPrivate ParrilladaTaskCtxPrivate;
struct _ParrilladaTaskCtxPrivate
{
	/* these two are set at creation time and can't be changed */
	ParrilladaTaskAction action;
	ParrilladaBurnSession *session;

	GMutex *lock;

	ParrilladaTrack *current_track;
	GSList *tracks;

	/* used to poll for progress (every 0.5 sec) */
	gdouble progress;
	goffset track_bytes;
	goffset session_bytes;

	goffset size;
	goffset blocks;

	/* keep track of time */
	GTimer *timer;
	goffset first_written;
	gdouble first_progress;

	/* used for immediate rate */
	gdouble current_elapsed;
	gdouble last_elapsed;

	goffset last_written;
	gdouble last_progress;

	/* used for remaining time */
	GSList *times;
	gdouble total_time;

	/* used for rates that certain jobs are able to report */
	guint64 rate;

	/* the current action */
	ParrilladaBurnAction current_action;
	gchar *action_string;

	guint dangerous;

	guint fake:1;
	guint action_changed:1;
	guint update_action_string:1;

	guint written_changed:1;
	guint progress_changed:1;
	guint use_average_rate:1;
};

#define PARRILLADA_TASK_CTX_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TASK_CTX, ParrilladaTaskCtxPrivate))

G_DEFINE_TYPE (ParrilladaTaskCtx, parrillada_task_ctx, G_TYPE_OBJECT);

#define MAX_VALUE_AVERAGE	16

enum _ParrilladaTaskCtxSignalType {
	ACTION_CHANGED_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint parrillada_task_ctx_signals [LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_ACTION,
	PROP_SESSION
};

static GObjectClass* parent_class = NULL;

void
parrillada_task_ctx_set_dangerous (ParrilladaTaskCtx *self, gboolean value)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	if (value)
		priv->dangerous ++;
	else
		priv->dangerous --;
}

guint
parrillada_task_ctx_get_dangerous (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	return priv->dangerous;
}

void
parrillada_task_ctx_reset (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;
	GSList *tracks;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->tracks) {
		g_slist_foreach (priv->tracks, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	tracks = parrillada_burn_session_get_tracks (priv->session);
	PARRILLADA_BURN_LOG ("Setting current track (%i tracks)", g_slist_length (tracks));
	if (priv->current_track)
		g_object_unref (priv->current_track);

	if (tracks) {
		priv->current_track = tracks->data;
		g_object_ref (priv->current_track);
	}
	else
		PARRILLADA_BURN_LOG ("no tracks");

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	priv->dangerous = 0;
	priv->progress = -1.0;
	priv->track_bytes = -1;
	priv->session_bytes = -1;
	priv->written_changed = 0;

	priv->current_elapsed = 0;
	priv->last_written = 0;
	priv->last_elapsed = 0;
	priv->last_progress = 0;

	if (priv->times) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	g_signal_emit (self,
		       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
		       0);
}

void
parrillada_task_ctx_set_fake (ParrilladaTaskCtx *ctx,
			   gboolean fake)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (ctx);
	priv->fake = fake;
}

/**
 * Used to get config
 */

ParrilladaBurnSession *
parrillada_task_ctx_get_session (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), NULL);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	if (!priv->session)
		return NULL;

	return priv->session;
}

ParrilladaBurnResult
parrillada_task_ctx_get_stored_tracks (ParrilladaTaskCtx *self,
				    GSList **tracks)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	if (!priv->current_track)
		return PARRILLADA_BURN_ERR;

	if (tracks)
		*tracks = priv->tracks;

	/* If no track has been added let the caller
	 * know with PARRILLADA_BURN_NOT_READY */
	if (!priv->tracks)
		return PARRILLADA_BURN_NOT_READY;

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_current_track (ParrilladaTaskCtx *self,
				    ParrilladaTrack **track)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (track != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	if (!priv->current_track)
		return PARRILLADA_BURN_ERR;

	*track = priv->current_track;
	return PARRILLADA_BURN_OK;
}

ParrilladaTaskAction
parrillada_task_ctx_get_action (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->fake)
		return PARRILLADA_TASK_ACTION_NONE;

	return priv->action;
}

/**
 * Used to report task status
 */

ParrilladaBurnResult
parrillada_task_ctx_add_track (ParrilladaTaskCtx *self,
			    ParrilladaTrack *track)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	PARRILLADA_BURN_LOG ("Adding track %s",
//			  parrillada_track_get_track_type (track, NULL),
			  priv->tracks? "already some tracks":"");

	/* Ref the track and store it for later. */
	g_object_ref (track);
	priv->tracks = g_slist_prepend (priv->tracks, track);
	return PARRILLADA_BURN_OK;
}

static gboolean
parrillada_task_ctx_set_next_track (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;
	GSList *tracks;
	GSList *node;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	/* we need to set the next track if our action is NORMAL or CHECKSUM */
	if (priv->action != PARRILLADA_TASK_ACTION_NORMAL
	&&  priv->action != PARRILLADA_TASK_ACTION_CHECKSUM)
		return PARRILLADA_BURN_OK;

	/* see if there is another track left */
	tracks = parrillada_burn_session_get_tracks (priv->session);
	node = g_slist_find (tracks, priv->current_track);
	if (!node || !node->next)
		return PARRILLADA_BURN_OK;

	priv->session_bytes += priv->track_bytes;
	priv->track_bytes = 0;
	priv->last_written = 0;
	priv->progress = 0;

	if (priv->current_track)
		g_object_unref (priv->current_track);

	priv->current_track = node->next->data;
	g_object_ref (priv->current_track);

	return PARRILLADA_BURN_RETRY;
}

ParrilladaBurnResult
parrillada_task_ctx_next_track (ParrilladaTaskCtx *self)

{
	ParrilladaTaskCtxPrivate *priv;
	ParrilladaBurnResult retval;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	retval = parrillada_task_ctx_set_next_track (self);
	if (retval == PARRILLADA_BURN_RETRY) {
		ParrilladaTaskCtxClass *klass;

		PARRILLADA_BURN_LOG ("Set next track to be processed");

		klass = PARRILLADA_TASK_CTX_GET_CLASS (self);
		if (!klass->finished)
			return PARRILLADA_BURN_NOT_SUPPORTED;

		klass->finished (self,
				 PARRILLADA_BURN_RETRY,
				 NULL);
		return PARRILLADA_BURN_RETRY;
	}

	PARRILLADA_BURN_LOG ("No next track to process");
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_finished (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;
	ParrilladaTaskCtxClass *klass;
	GError *error = NULL;
	GSList *iter;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	klass = PARRILLADA_TASK_CTX_GET_CLASS (self);
	if (!klass->finished)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	klass->finished (self,
			 PARRILLADA_BURN_OK,
			 error);

	if (priv->tracks) {
		parrillada_burn_session_push_tracks (priv->session);
		priv->tracks = g_slist_reverse (priv->tracks);
		for (iter = priv->tracks; iter; iter = iter->next) {
			ParrilladaTrack *track;

			track = iter->data;
			parrillada_burn_session_add_track (priv->session, track, NULL);

			/* It's good practice to unref the track afterwards as
			 * we don't need it anymore. ParrilladaBurnSession refs it.
			 */
			g_object_unref (track);
		}
	
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_error (ParrilladaTaskCtx *self,
			ParrilladaBurnResult retval,
			GError *error)
{
	ParrilladaTaskCtxClass *klass;
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	klass = PARRILLADA_TASK_CTX_GET_CLASS (self);
	if (!klass->finished)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	klass->finished (self,
			 retval,
			 error);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_start_progress (ParrilladaTaskCtx *self,
				 gboolean force)

{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (!priv->timer) {
		priv->timer = g_timer_new ();
		priv->first_written = priv->session_bytes + priv->track_bytes;
		priv->first_progress = priv->progress;
	}
	else if (force) {
		g_timer_start (priv->timer);
		priv->first_written = priv->session_bytes + priv->track_bytes;
		priv->first_progress = priv->progress;
	}

	return PARRILLADA_BURN_OK;
}

static gdouble
parrillada_task_ctx_get_average (GSList **values, gdouble value)
{
	const unsigned int scale = 10000;
	unsigned int num = 0;
	gdouble average;
	gint32 int_value;
	GSList *l;

	if (value * scale < G_MAXINT)
		int_value = (gint32) ceil (scale * value);
	else if (value / scale < G_MAXINT)
		int_value = (gint32) ceil (-1.0 * value / scale);
	else
		return value;
		
	*values = g_slist_prepend (*values, GINT_TO_POINTER (int_value));

	average = 0;
	for (l = *values; l; l = l->next) {
		gdouble r = (gdouble) GPOINTER_TO_INT (l->data);

		if (r < 0)
			r *= scale * -1.0;
		else
			r /= scale;

		average += r;
		num++;
		if (num == MAX_VALUE_AVERAGE && l->next)
			l = g_slist_delete_link (l, l->next);
	}

	average /= num;
	return average;
}

void
parrillada_task_ctx_report_progress (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;
	gdouble progress, elapsed;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->action_changed) {
		/* Give a last progress-changed signal
		 * setting previous action as completely
		 * finished only if the plugin set any
		 * progress for it.
		 * This helps having the tray icon or the
		 * taskbar icon set to be full on quick
		 * burns. */

		if (priv->progress >= 0.0
		||  priv->track_bytes >= 0
		||  priv->session_bytes >= 0) {
			goffset total = 0;

			priv->progress = 1.0;
			priv->track_bytes = 0;
			parrillada_task_ctx_get_session_output_size (self, NULL, &total);
			priv->session_bytes = total;

			g_signal_emit (self,
				       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
				       0);
		}

		g_signal_emit (self,
			       parrillada_task_ctx_signals [ACTION_CHANGED_SIGNAL],
			       0,
			       priv->current_action);

		parrillada_task_ctx_reset_progress (self);
		g_signal_emit (self,
			       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
			       0);

		priv->action_changed = 0;
	}
	else if (priv->update_action_string) {
		g_signal_emit (self,
			       parrillada_task_ctx_signals [ACTION_CHANGED_SIGNAL],
			       0,
			       priv->current_action);

		priv->update_action_string = 0;
	}

	if (priv->timer) {
		elapsed = g_timer_elapsed (priv->timer, NULL);
		if (parrillada_task_ctx_get_progress (self, &progress) == PARRILLADA_BURN_OK) {
			gdouble total_time;

			total_time = (gdouble) elapsed / (gdouble) progress;

			g_mutex_lock (priv->lock);
			priv->total_time = parrillada_task_ctx_get_average (&priv->times,
									 total_time);
			g_mutex_unlock (priv->lock);
		}
	}

	if (priv->progress_changed) {
		priv->progress_changed = 0;
		g_signal_emit (self,
			       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
			       0);
	}
	else if (priv->written_changed) {
		priv->written_changed = 0;
		g_signal_emit (self,
			       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
			       0);
	}
}

ParrilladaBurnResult
parrillada_task_ctx_set_rate (ParrilladaTaskCtx *self,
			   gint64 rate)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	priv->rate = rate;
	return PARRILLADA_BURN_OK;
}

/**
 * This is used by jobs that are imaging to tell what's going to be the output 
 * size for a particular track
 */

ParrilladaBurnResult
parrillada_task_ctx_set_output_size_for_current_track (ParrilladaTaskCtx *self,
						    goffset sectors,
						    goffset bytes)
{
	ParrilladaTaskCtxPrivate *priv;

	/* NOTE: we don't need block size here as it's pretty easy to have it by
	 * dividing size by sectors or by guessing it with image or audio format
	 * of the output */

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	/* we only allow plugins to set these values during the init phase of a 
	 * task when it's fakely running. One exception is if size or blocks are
	 * 0 at the start of a task in normal mode */
	if (sectors >= 0)
		priv->blocks += sectors;

	if (bytes >= 0)
		priv->size += bytes;

	PARRILLADA_BURN_LOG ("Task output modified %lli blocks %lli bytes",
			  priv->blocks,
			  priv->size);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_set_written_track (ParrilladaTaskCtx *self,
				    gint64 written)
{
	ParrilladaTaskCtxPrivate *priv;
	gdouble elapsed = 0.0;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	priv->written_changed = 1;

	if (priv->use_average_rate) {
		priv->track_bytes = written;
		return PARRILLADA_BURN_OK;
	}

	if (priv->timer)
		elapsed = g_timer_elapsed (priv->timer, NULL);

	if ((elapsed - priv->last_elapsed) > 0.5) {
		priv->last_written = priv->track_bytes;
		priv->last_elapsed = priv->current_elapsed;
		priv->current_elapsed = elapsed;
	}

	priv->track_bytes = written;
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_set_written_session (ParrilladaTaskCtx *self,
				      gint64 written)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	priv->session_bytes = 0;
	return parrillada_task_ctx_set_written_track (self, written);
}

ParrilladaBurnResult
parrillada_task_ctx_set_progress (ParrilladaTaskCtx *self,
			       gdouble progress)
{
	ParrilladaTaskCtxPrivate *priv;
	gdouble elapsed;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	priv->progress_changed = 1;

	if (priv->use_average_rate) {
		if (priv->progress < progress)
			priv->progress = progress;

		return PARRILLADA_BURN_OK;
	}

	/* here we prefer to use track written bytes instead of progress.
	 * NOTE: usually plugins will return only one information. */
	if (priv->last_written) {
		if (priv->progress < progress)
			priv->progress = progress;
		return PARRILLADA_BURN_OK;
	}

	if (priv->timer) {
		elapsed = g_timer_elapsed (priv->timer, NULL);

		if ((elapsed - priv->last_elapsed) > 0.5) {
			priv->last_progress = priv->progress;
			priv->last_elapsed = priv->current_elapsed;
			priv->current_elapsed = elapsed;
		}
	}

	if (priv->progress < progress)
		priv->progress = progress;

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_reset_progress (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	priv->progress_changed = 1;

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	priv->dangerous = 0;
	priv->progress = -1.0;
	priv->track_bytes = -1;
	priv->session_bytes = -1;

	priv->current_elapsed = 0;
	priv->last_written = 0;
	priv->last_elapsed = 0;
	priv->last_progress = 0;

	if (priv->times) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_set_current_action (ParrilladaTaskCtx *self,
				     ParrilladaBurnAction action,
				     const gchar *string,
				     gboolean force)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->current_action == action) {
		if (!force)
			return PARRILLADA_BURN_OK;

		g_mutex_lock (priv->lock);

		priv->update_action_string = 1;
	}
	else {
		g_mutex_lock (priv->lock);

		priv->current_action = action;
		priv->action_changed = 1;
	}

	if (priv->action_string)
		g_free (priv->action_string);

	priv->action_string = string ? g_strdup (string): NULL;

	if (!force) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	g_mutex_unlock (priv->lock);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_set_use_average (ParrilladaTaskCtx *self,
				  gboolean use_average)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);
	priv->use_average_rate = use_average;
	return PARRILLADA_BURN_OK;
}

/**
 * Used to retrieve the values for a given task
 */

ParrilladaBurnResult
parrillada_task_ctx_get_rate (ParrilladaTaskCtx *self,
			   guint64 *rate)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (rate != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->current_action != PARRILLADA_BURN_ACTION_RECORDING
	&&  priv->current_action != PARRILLADA_BURN_ACTION_DRIVE_COPY) {
		*rate = -1;
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	if (priv->rate) {
		*rate = priv->rate;
		return PARRILLADA_BURN_OK;
	}

	if (priv->use_average_rate) {
		gdouble elapsed;

		if (!priv->timer)
			return PARRILLADA_BURN_NOT_READY;

		elapsed = g_timer_elapsed (priv->timer, NULL);

		if ((priv->session_bytes + priv->track_bytes) > 0)
			*rate = (gdouble) ((priv->session_bytes + priv->track_bytes) - priv->first_written) / elapsed;
		else if (priv->progress > 0.0)
			*rate = (gdouble) (priv->progress - priv->first_progress) * priv->size / elapsed;
		else
			return PARRILLADA_BURN_NOT_READY;
	}
	else {
		if (priv->last_written > 0) {			
			*rate = (gdouble) (priv->track_bytes - priv->last_written) /
				(gdouble) (priv->current_elapsed - priv->last_elapsed);
		}
		else if (priv->last_progress > 0.0) {
			*rate = (gdouble)  priv->size *
				(gdouble) (priv->progress - priv->last_progress) /
				(gdouble) (priv->current_elapsed - priv->last_elapsed);
		}
		else
			return PARRILLADA_BURN_NOT_READY;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_remaining_time (ParrilladaTaskCtx *self,
				     long *remaining)
{
	ParrilladaTaskCtxPrivate *priv;
	gdouble elapsed;
	gint len;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (remaining != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	g_mutex_lock (priv->lock);
	len = g_slist_length (priv->times);
	g_mutex_unlock (priv->lock);

	if (len < MAX_VALUE_AVERAGE)
		return PARRILLADA_BURN_NOT_READY;

	elapsed = g_timer_elapsed (priv->timer, NULL);
	*remaining = priv->total_time - elapsed;

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_session_output_size (ParrilladaTaskCtx *self,
					  goffset *blocks,
					  goffset *bytes)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (blocks != NULL || bytes != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->size <= 0 && priv->blocks <= 0)
		return PARRILLADA_BURN_NOT_READY;

	if (bytes)
		*bytes = priv->size;

	if (blocks)
		*blocks = priv->blocks;

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_written (ParrilladaTaskCtx *self,
			      gint64 *written)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TASK_CTX (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (written != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (priv->track_bytes + priv->session_bytes <= 0)
		return PARRILLADA_BURN_NOT_READY;

	if (!written)
		return PARRILLADA_BURN_OK;

	*written = priv->track_bytes + priv->session_bytes;
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_current_action_string (ParrilladaTaskCtx *self,
					    ParrilladaBurnAction action,
					    gchar **string)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (string != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	if (action != priv->current_action)
		return PARRILLADA_BURN_ERR;

	*string = priv->action_string ? g_strdup (priv->action_string):
					g_strdup (parrillada_burn_action_to_string (priv->current_action));

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_progress (ParrilladaTaskCtx *self, 
			       gdouble *progress)
{
	ParrilladaTaskCtxPrivate *priv;
	gdouble track_num = 0;
	gdouble track_nb = 0;
	goffset total = 0;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	/* The following can happen when we're blanking since there's no track */
	if (priv->action == PARRILLADA_TASK_ACTION_ERASE) {
		track_num = 1.0;
		track_nb = 0.0;
	}
	else {
		GSList *tracks;

		tracks = parrillada_burn_session_get_tracks (priv->session);
		track_num = g_slist_length (tracks);
		track_nb = g_slist_index (tracks, priv->current_track);
	}

	if (priv->progress >= 0.0) {
		if (progress)
			*progress = (gdouble) (track_nb + priv->progress) / (gdouble) track_num;

		return PARRILLADA_BURN_OK;
	}

	total = 0;
	parrillada_task_ctx_get_session_output_size (self, NULL, &total);

	if ((priv->session_bytes + priv->track_bytes) <= 0 || total <= 0) {
		/* if parrillada_task_ctx_start_progress () was called (and a timer
		 * created), assume that the task will report either a progress
		 * of the written bytes. Then it means we just started. */
		if (priv->timer) {
			if (progress)
				*progress = 0.0;

			return PARRILLADA_BURN_OK;
		}

		if (progress)
			*progress = -1.0;

		return PARRILLADA_BURN_NOT_READY;
	}

	if (!progress)
		return PARRILLADA_BURN_OK;

	*progress = (gdouble) ((gdouble) (priv->track_bytes + priv->session_bytes) / (gdouble)  total);
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_task_ctx_get_current_action (ParrilladaTaskCtx *self,
				     ParrilladaBurnAction *action)
{
	ParrilladaTaskCtxPrivate *priv;

	g_return_val_if_fail (action != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	g_mutex_lock (priv->lock);
	*action = priv->current_action;
	g_mutex_unlock (priv->lock);

	return PARRILLADA_BURN_OK;
}

void
parrillada_task_ctx_stop_progress (ParrilladaTaskCtx *self)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	/* one last report */
	g_signal_emit (self,
		       parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
		       0);

	priv->current_action = PARRILLADA_BURN_ACTION_NONE;
	priv->action_changed = 0;
	priv->update_action_string = 0;

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}
	priv->first_written = 0;
	priv->first_progress = 0.0;

	g_mutex_lock (priv->lock);

	if (priv->action_string) {
		g_free (priv->action_string);
		priv->action_string = NULL;
	}

	if (priv->times) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	g_mutex_unlock (priv->lock);
}

static void
parrillada_task_ctx_init (ParrilladaTaskCtx *object)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (object);
	priv->lock = g_mutex_new ();
}

static void
parrillada_task_ctx_finalize (GObject *object)
{
	ParrilladaTaskCtxPrivate *priv;

	priv = PARRILLADA_TASK_CTX_PRIVATE (object);

	if (priv->lock) {
		g_mutex_free (priv->lock);
		priv->lock = NULL;
	}

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	if (priv->current_track) {
		g_object_unref (priv->current_track);
		priv->current_track = NULL;
	}

	if (priv->tracks) {
		g_slist_foreach (priv->tracks, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_task_ctx_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ParrilladaTaskCtx *self;
	ParrilladaTaskCtxPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_TASK_CTX (object));

	self = PARRILLADA_TASK_CTX (object);
	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_ACTION:
		priv->action = g_value_get_int (value);
		break;
	case PROP_SESSION:
		priv->session = g_value_get_object (value);
		g_object_ref (priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_task_ctx_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ParrilladaTaskCtx *self;
	ParrilladaTaskCtxPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_TASK_CTX (object));

	self = PARRILLADA_TASK_CTX (object);
	priv = PARRILLADA_TASK_CTX_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_ACTION:
		g_value_set_int (value, priv->action);
		break;
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_task_ctx_class_init (ParrilladaTaskCtxClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (ParrilladaTaskCtxPrivate));

	object_class->finalize = parrillada_task_ctx_finalize;
	object_class->set_property = parrillada_task_ctx_set_property;
	object_class->get_property = parrillada_task_ctx_get_property;

	parrillada_task_ctx_signals [PROGRESS_CHANGED_SIGNAL] =
	    g_signal_new ("progress_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	parrillada_task_ctx_signals [ACTION_CHANGED_SIGNAL] =
	    g_signal_new ("action_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	g_object_class_install_property (object_class,
	                                 PROP_ACTION,
	                                 g_param_spec_int ("action",
							   "The action the task must perform",
							   "The action the task must perform",
							   PARRILLADA_TASK_ACTION_ERASE,
							   PARRILLADA_TASK_ACTION_CHECKSUM,
							   PARRILLADA_TASK_ACTION_NORMAL,
							   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_SESSION,
	                                 g_param_spec_object ("session",
	                                                      "The session this object is tied to",
	                                                      "The session this object is tied to",
	                                                      PARRILLADA_TYPE_BURN_SESSION,
	                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

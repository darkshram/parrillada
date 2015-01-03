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

#include "parrillada-drive.h"
#include "parrillada-medium.h"

#include "burn-debug.h"
#include "parrillada-session-helper.h"
#include "parrillada-track.h"
#include "parrillada-track-data.h"
#include "parrillada-track-data-cfg.h"
#include "parrillada-session-span.h"

typedef struct _ParrilladaSessionSpanPrivate ParrilladaSessionSpanPrivate;
struct _ParrilladaSessionSpanPrivate
{
	GSList * track_list;
	ParrilladaTrack * last_track;
};

#define PARRILLADA_SESSION_SPAN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_SESSION_SPAN, ParrilladaSessionSpanPrivate))

G_DEFINE_TYPE (ParrilladaSessionSpan, parrillada_session_span, PARRILLADA_TYPE_BURN_SESSION);

/**
 * parrillada_session_span_get_max_space:
 * @session: a #ParrilladaSessionSpan
 *
 * Returns the maximum required space (in sectors) 
 * among all the possible spanned batches.
 * This means that when burningto a media
 * it will also be the minimum required
 * space to burn all the contents in several
 * batches.
 *
 * Return value: a #goffset.
 **/

goffset
parrillada_session_span_get_max_space (ParrilladaSessionSpan *session)
{
	GSList *tracks;
	goffset max_sectors = 0;
	ParrilladaSessionSpanPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_SESSION_SPAN (session), 0);

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);

		if (!tracks->next)
			return 0;

		tracks = tracks->next;
	}
	else if (priv->track_list)
		tracks = priv->track_list;
	else
		tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (session));

	for (; tracks; tracks = tracks->next) {
		ParrilladaTrack *track;
		goffset track_blocks = 0;

		track = tracks->data;

		if (PARRILLADA_IS_TRACK_DATA_CFG (track))
			return parrillada_track_data_cfg_span_max_space (PARRILLADA_TRACK_DATA_CFG (track));

		/* This is the common case */
		parrillada_track_get_size (PARRILLADA_TRACK (track),
					&track_blocks,
					NULL);

		max_sectors = MAX (max_sectors, track_blocks);
	}

	return max_sectors;
}

/**
 * parrillada_session_span_again:
 * @session: a #ParrilladaSessionSpan
 *
 * Checks whether some data were not included during calls to parrillada_session_span_next ().
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is not anymore data.
 * PARRILLADA_BURN_RETRY if the operation was successful and a new #ParrilladaTrackDataCfg was created.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_session_span_again (ParrilladaSessionSpan *session)
{
	GSList *tracks;
	ParrilladaTrack *track;
	ParrilladaSessionSpanPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_SESSION_SPAN (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	/* This is not an error */
	if (!priv->track_list)
		return PARRILLADA_BURN_OK;

	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		if (!tracks->next) {
			priv->track_list = NULL;
			return PARRILLADA_BURN_OK;
		}

		return PARRILLADA_BURN_RETRY;
	}

	tracks = priv->track_list;
	track = tracks->data;

	if (PARRILLADA_IS_TRACK_DATA_CFG (track))
		return parrillada_track_data_cfg_span_again (PARRILLADA_TRACK_DATA_CFG (track));

	return (tracks != NULL)? PARRILLADA_BURN_RETRY:PARRILLADA_BURN_OK;
}

/**
 * parrillada_session_span_possible:
 * @session: a #ParrilladaSessionSpan
 *
 * Checks if a new #ParrilladaTrackData can be created from the files remaining in the tree 
 * after calls to parrillada_session_span_next (). The maximum size of the data will be the one
 * of the medium inserted in the #ParrilladaDrive set for @session (see parrillada_burn_session_set_burner ()).
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is not anymore data.
 * PARRILLADA_BURN_RETRY if the operation was successful and a new #ParrilladaTrackDataCfg was created.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_session_span_possible (ParrilladaSessionSpan *session)
{
	GSList *tracks;
	ParrilladaTrack *track;
	goffset max_sectors = 0;
	goffset track_blocks = 0;
	ParrilladaSessionSpanPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_SESSION_SPAN (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	max_sectors = parrillada_burn_session_get_available_medium_space (PARRILLADA_BURN_SESSION (session));
	if (max_sectors <= 0)
		return PARRILLADA_BURN_ERR;

	if (!priv->track_list)
		tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (session));
	else if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		if (!tracks->next) {
			priv->track_list = NULL;
			return PARRILLADA_BURN_OK;
		}
		tracks = tracks->next;
	}
	else
		tracks = priv->track_list;

	if (!tracks)
		return PARRILLADA_BURN_ERR;

	track = tracks->data;

	if (PARRILLADA_IS_TRACK_DATA_CFG (track))
		return parrillada_track_data_cfg_span_possible (PARRILLADA_TRACK_DATA_CFG (track), max_sectors);

	/* This is the common case */
	parrillada_track_get_size (PARRILLADA_TRACK (track),
				&track_blocks,
				NULL);

	if (track_blocks >= max_sectors)
		return PARRILLADA_BURN_ERR;

	return PARRILLADA_BURN_RETRY;
}

/**
 * parrillada_session_span_start:
 * @session: a #ParrilladaSessionSpan
 *
 * Get the object ready for spanning a #ParrilladaBurnSession object. This function
 * must be called before parrillada_session_span_next ().
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if successful.
 **/

ParrilladaBurnResult
parrillada_session_span_start (ParrilladaSessionSpan *session)
{
	ParrilladaSessionSpanPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_SESSION_SPAN (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	priv->track_list = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (session));
	if (priv->last_track) {
		g_object_unref (priv->last_track);
		priv->last_track = NULL;
	}

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_session_span_next:
 * @session: a #ParrilladaSessionSpan
 *
 * Sets the next batch of data to be burnt onto the medium inserted in the #ParrilladaDrive
 * set for @session (see parrillada_burn_session_set_burner ()). Its free space or it capacity
 * will be used as the maximum amount of data to be burnt.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if successful.
 **/

ParrilladaBurnResult
parrillada_session_span_next (ParrilladaSessionSpan *session)
{
	GSList *tracks;
	gboolean pushed = FALSE;
	goffset max_sectors = 0;
	goffset total_sectors = 0;
	ParrilladaSessionSpanPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_SESSION_SPAN (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	g_return_val_if_fail (priv->track_list != NULL, PARRILLADA_BURN_ERR);

	max_sectors = parrillada_burn_session_get_available_medium_space (PARRILLADA_BURN_SESSION (session));
	if (max_sectors <= 0)
		return PARRILLADA_BURN_ERR;

	/* NOTE: should we pop here? */
	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		g_object_unref (priv->last_track);
		priv->last_track = NULL;

		if (!tracks->next) {
			priv->track_list = NULL;
			return PARRILLADA_BURN_OK;
		}
		tracks = tracks->next;
	}
	else
		tracks = priv->track_list;

	for (; tracks; tracks = tracks->next) {
		ParrilladaTrack *track;
		goffset track_blocks = 0;

		track = tracks->data;

		if (PARRILLADA_IS_TRACK_DATA_CFG (track)) {
			ParrilladaTrackData *new_track;
			ParrilladaBurnResult result;

			/* NOTE: the case where track_blocks < max_blocks will
			 * be handled by parrillada_track_data_cfg_span () */

			/* This track type is the only one to be able to span itself */
			new_track = parrillada_track_data_new ();
			result = parrillada_track_data_cfg_span (PARRILLADA_TRACK_DATA_CFG (track),
							      max_sectors,
							      new_track);
			if (result != PARRILLADA_BURN_RETRY) {
				g_object_unref (new_track);
				return result;
			}

			pushed = TRUE;
			parrillada_burn_session_push_tracks (PARRILLADA_BURN_SESSION (session));
			parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session),
							PARRILLADA_TRACK (new_track),
							NULL);
			break;
		}

		/* This is the common case */
		parrillada_track_get_size (PARRILLADA_TRACK (track),
					&track_blocks,
					NULL);

		/* NOTE: keep the order of tracks */
		if (track_blocks + total_sectors >= max_sectors) {
			PARRILLADA_BURN_LOG ("Reached end of spanned size");
			break;
		}

		total_sectors += track_blocks;

		if (!pushed) {
			PARRILLADA_BURN_LOG ("Pushing tracks for media spanning");
			parrillada_burn_session_push_tracks (PARRILLADA_BURN_SESSION (session));
			pushed = TRUE;
		}

		PARRILLADA_BURN_LOG ("Adding tracks");
		parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session), track, NULL);

		if (priv->last_track)
			g_object_unref (priv->last_track);

		priv->last_track = g_object_ref (track);
	}

	/* If we pushed anything it means we succeeded */
	return (pushed? PARRILLADA_BURN_RETRY:PARRILLADA_BURN_ERR);
}

/**
 * parrillada_session_span_stop:
 * @session: a #ParrilladaSessionSpan
 *
 * Ends and cleans a spanning operation started with parrillada_session_span_start ().
 *
 **/

void
parrillada_session_span_stop (ParrilladaSessionSpan *session)
{
	ParrilladaSessionSpanPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_SESSION_SPAN (session));

	priv = PARRILLADA_SESSION_SPAN_PRIVATE (session);

	if (priv->last_track) {
		g_object_unref (priv->last_track);
		priv->last_track = NULL;
	}
	else if (priv->track_list) {
		ParrilladaTrack *track;

		track = priv->track_list->data;
		if (PARRILLADA_IS_TRACK_DATA_CFG (track))
			parrillada_track_data_cfg_span_stop (PARRILLADA_TRACK_DATA_CFG (track));
	}

	priv->track_list = NULL;
}

static void
parrillada_session_span_init (ParrilladaSessionSpan *object)
{ }

static void
parrillada_session_span_finalize (GObject *object)
{
	parrillada_session_span_stop (PARRILLADA_SESSION_SPAN (object));
	G_OBJECT_CLASS (parrillada_session_span_parent_class)->finalize (object);
}

static void
parrillada_session_span_class_init (ParrilladaSessionSpanClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaSessionSpanPrivate));

	object_class->finalize = parrillada_session_span_finalize;
}

/**
 * parrillada_session_span_new:
 *
 * Creates a new #ParrilladaSessionSpan object.
 *
 * Return value: a #ParrilladaSessionSpan object
 **/

ParrilladaSessionSpan *
parrillada_session_span_new (void)
{
	return g_object_new (PARRILLADA_TYPE_SESSION_SPAN, NULL);
}


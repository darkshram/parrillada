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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-debug.h"
#include "burn-basics.h"
#include "parrillada-track-stream.h"

typedef struct _ParrilladaTrackStreamPrivate ParrilladaTrackStreamPrivate;
struct _ParrilladaTrackStreamPrivate
{
        GFileMonitor *monitor;
	gchar *uri;

	ParrilladaStreamFormat format;

	guint64 gap;
	guint64 start;
	guint64 end;
};

#define PARRILLADA_TRACK_STREAM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_STREAM, ParrilladaTrackStreamPrivate))

G_DEFINE_TYPE (ParrilladaTrackStream, parrillada_track_stream, PARRILLADA_TYPE_TRACK);

static ParrilladaBurnResult
parrillada_track_stream_set_source_real (ParrilladaTrackStream *track,
				      const gchar *uri)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (uri);

	/* Since that's a new URI chances are, the end point is different */
	priv->end = 0;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_stream_set_source:
 * @track: a #ParrilladaTrackStream
 * @uri: a #gchar
 *
 * Sets the stream (song or video) uri.
 *
 * Note: it resets the end point of the track to 0 but keeps start point and gap
 * unchanged.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it is successful.
 **/

ParrilladaBurnResult
parrillada_track_stream_set_source (ParrilladaTrackStream *track,
				 const gchar *uri)
{
	ParrilladaTrackStreamClass *klass;
	ParrilladaBurnResult res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), PARRILLADA_BURN_ERR);

	klass = PARRILLADA_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_source)
		return PARRILLADA_BURN_ERR;

	res = klass->set_source (track, uri);
	if (res != PARRILLADA_BURN_OK)
		return res;

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_stream_get_format:
 * @track: a #ParrilladaTrackStream
 *
 * This function returns the format of the stream.
 *
 * Return value: a #ParrilladaStreamFormat.
 **/

ParrilladaStreamFormat
parrillada_track_stream_get_format (ParrilladaTrackStream *track)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), PARRILLADA_AUDIO_FORMAT_NONE);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	return priv->format;
}

static ParrilladaBurnResult
parrillada_track_stream_set_format_real (ParrilladaTrackStream *track,
				      ParrilladaStreamFormat format)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	if (format == PARRILLADA_AUDIO_FORMAT_NONE)
		PARRILLADA_BURN_LOG ("Setting a NONE audio format with a valid uri");

	priv->format = format;
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_stream_set_format:
 * @track: a #ParrilladaTrackStream
 * @format: a #ParrilladaStreamFormat
 *
 * Sets the format of the stream.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it is successful.
 **/

ParrilladaBurnResult
parrillada_track_stream_set_format (ParrilladaTrackStream *track,
				 ParrilladaStreamFormat format)
{
	ParrilladaTrackStreamClass *klass;
	ParrilladaBurnResult res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), PARRILLADA_BURN_ERR);

	klass = PARRILLADA_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_format)
		return PARRILLADA_BURN_ERR;

	res = klass->set_format (track, format);
	if (res != PARRILLADA_BURN_OK)
		return res;

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_stream_set_boundaries_real (ParrilladaTrackStream *track,
					  gint64 start,
					  gint64 end,
					  gint64 gap)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	if (gap >= 0)
		priv->gap = gap;

	if (end > 0)
		priv->end = end;

	if (start >= 0)
		priv->start = start;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_stream_set_boundaries:
 * @track: a #ParrilladaTrackStream
 * @start: a #gint64 or -1 to ignore
 * @end: a #gint64 or -1 to ignore
 * @gap: a #gint64 or -1 to ignore
 *
 * Sets the boundaries of the stream (where it starts, ends in the file;
 * how long is the gap with the next track) in nano seconds.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it is successful.
 **/

ParrilladaBurnResult
parrillada_track_stream_set_boundaries (ParrilladaTrackStream *track,
				     gint64 start,
				     gint64 end,
				     gint64 gap)
{
	ParrilladaTrackStreamClass *klass;
	ParrilladaBurnResult res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), PARRILLADA_BURN_ERR);

	klass = PARRILLADA_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_boundaries)
		return PARRILLADA_BURN_ERR;

	res = klass->set_boundaries (track, start, end, gap);
	if (res != PARRILLADA_BURN_OK)
		return res;

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_stream_get_source:
 * @track: a #ParrilladaTrackStream
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE)
 * of the stream (song or video file).
 *
 * Note: this function resets any length previously set to 0.
 * Return value: a #gchar.
 **/

gchar *
parrillada_track_stream_get_source (ParrilladaTrackStream *track,
				 gboolean uri)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), NULL);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);
	if (uri)
		return parrillada_string_get_uri (priv->uri);
	else
		return parrillada_string_get_localpath (priv->uri);
}

/**
 * parrillada_track_stream_get_gap:
 * @track: a #ParrilladaTrackStream
 *
 * This function returns length of the gap (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
parrillada_track_stream_get_gap (ParrilladaTrackStream *track)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), 0);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);
	return priv->gap;
}

/**
 * parrillada_track_stream_get_start:
 * @track: a #ParrilladaTrackStream
 *
 * This function returns start time in the stream (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
parrillada_track_stream_get_start (ParrilladaTrackStream *track)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), 0);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);
	return priv->start;
}

/**
 * parrillada_track_stream_get_end:
 * @track: a #ParrilladaTrackStream
 *
 * This function returns end time in the stream (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
parrillada_track_stream_get_end (ParrilladaTrackStream *track)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), 0);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);
	return priv->end;
}

/**
 * parrillada_track_stream_get_length:
 * @track: a #ParrilladaTrackStream
 * @length: a #guint64
 *
 * This function returns the length of the stream (in nano seconds)
 * taking into account the start and end time as well as the length
 * of the gap. It stores it in @length.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if @length was set.
 **/

ParrilladaBurnResult
parrillada_track_stream_get_length (ParrilladaTrackStream *track,
				 guint64 *length)
{
	ParrilladaTrackStreamPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_STREAM (track), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	if (priv->start < 0 || priv->end <= 0)
		return PARRILLADA_BURN_ERR;

	*length = PARRILLADA_STREAM_LENGTH (priv->start, priv->end + priv->gap);
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_stream_get_size (ParrilladaTrack *track,
			       goffset *blocks,
			       goffset *block_size)
{
	ParrilladaStreamFormat format;

	format = parrillada_track_stream_get_format (PARRILLADA_TRACK_STREAM (track));
	if (!PARRILLADA_STREAM_FORMAT_HAS_VIDEO (format)) {
		if (blocks) {
			guint64 length = 0;

			parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (track), &length);
			*blocks = length * 75LL / 1000000000LL;
		}

		if (block_size)
			*block_size = 2352;
	}
	else {
		if (blocks) {
			guint64 length = 0;

			/* This is based on a simple formula:
			 * 4700000000 bytes means 2 hours */
			parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (track), &length);
			*blocks = length * 47LL / 72000LL / 2048LL;
		}

		if (block_size)
			*block_size = 2048;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_stream_get_status (ParrilladaTrack *track,
				 ParrilladaStatus *status)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);
	if (!priv->uri) {
		if (status)
			parrillada_status_set_error (status,
						  g_error_new (PARRILLADA_BURN_ERROR,
							       PARRILLADA_BURN_ERROR_EMPTY,
							       _("There are no files to write to disc")));

		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_stream_get_track_type (ParrilladaTrack *track,
				     ParrilladaTrackType *type)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (track);

	parrillada_track_type_set_has_stream (type);
	parrillada_track_type_set_stream_format (type, priv->format);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_track_stream_init (ParrilladaTrackStream *object)
{ }

static void
parrillada_track_stream_finalize (GObject *object)
{
	ParrilladaTrackStreamPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_PRIVATE (object);
	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	G_OBJECT_CLASS (parrillada_track_stream_parent_class)->finalize (object);
}

static void
parrillada_track_stream_class_init (ParrilladaTrackStreamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass *track_class = PARRILLADA_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackStreamPrivate));

	object_class->finalize = parrillada_track_stream_finalize;

	track_class->get_size = parrillada_track_stream_get_size;
	track_class->get_status = parrillada_track_stream_get_status;
	track_class->get_type = parrillada_track_stream_get_track_type;

	klass->set_source = parrillada_track_stream_set_source_real;
	klass->set_format = parrillada_track_stream_set_format_real;
	klass->set_boundaries = parrillada_track_stream_set_boundaries_real;
}

/**
 * parrillada_track_stream_new:
 *
 *  Creates a new #ParrilladaTrackStream object.
 *
 * This type of tracks is used to burn audio or
 * video files.
 *
 * Return value: a #ParrilladaTrackStream object.
 **/

ParrilladaTrackStream *
parrillada_track_stream_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_STREAM, NULL);
}

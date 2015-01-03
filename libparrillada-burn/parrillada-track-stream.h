/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-media is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_TRACK_STREAM_H_
#define _PARRILLADA_TRACK_STREAM_H_

#include <glib-object.h>

#include <parrillada-enums.h>
#include <parrillada-track.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TRACK_STREAM             (parrillada_track_stream_get_type ())
#define PARRILLADA_TRACK_STREAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK_STREAM, ParrilladaTrackStream))
#define PARRILLADA_TRACK_STREAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK_STREAM, ParrilladaTrackStreamClass))
#define PARRILLADA_IS_TRACK_STREAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK_STREAM))
#define PARRILLADA_IS_TRACK_STREAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK_STREAM))
#define PARRILLADA_TRACK_STREAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK_STREAM, ParrilladaTrackStreamClass))

typedef struct _ParrilladaTrackStreamClass ParrilladaTrackStreamClass;
typedef struct _ParrilladaTrackStream ParrilladaTrackStream;

struct _ParrilladaTrackStreamClass
{
	ParrilladaTrackClass parent_class;

	/* Virtual functions */
	ParrilladaBurnResult       (*set_source)		(ParrilladaTrackStream *track,
							 const gchar *uri);

	ParrilladaBurnResult       (*set_format)		(ParrilladaTrackStream *track,
							 ParrilladaStreamFormat format);

	ParrilladaBurnResult       (*set_boundaries)       (ParrilladaTrackStream *track,
							 gint64 start,
							 gint64 end,
							 gint64 gap);
};

struct _ParrilladaTrackStream
{
	ParrilladaTrack parent_instance;
};

GType parrillada_track_stream_get_type (void) G_GNUC_CONST;

ParrilladaTrackStream *
parrillada_track_stream_new (void);

ParrilladaBurnResult
parrillada_track_stream_set_source (ParrilladaTrackStream *track,
				 const gchar *uri);

ParrilladaBurnResult
parrillada_track_stream_set_format (ParrilladaTrackStream *track,
				 ParrilladaStreamFormat format);

ParrilladaBurnResult
parrillada_track_stream_set_boundaries (ParrilladaTrackStream *track,
				     gint64 start,
				     gint64 end,
				     gint64 gap);

gchar *
parrillada_track_stream_get_source (ParrilladaTrackStream *track,
				 gboolean uri);

ParrilladaBurnResult
parrillada_track_stream_get_length (ParrilladaTrackStream *track,
				 guint64 *length);

guint64
parrillada_track_stream_get_start (ParrilladaTrackStream *track);

guint64
parrillada_track_stream_get_end (ParrilladaTrackStream *track);

guint64
parrillada_track_stream_get_gap (ParrilladaTrackStream *track);

ParrilladaStreamFormat
parrillada_track_stream_get_format (ParrilladaTrackStream *track);

G_END_DECLS

#endif /* _PARRILLADA_TRACK_STREAM_H_ */

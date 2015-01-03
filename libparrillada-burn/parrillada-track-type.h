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

#ifndef _BURN_TRACK_TYPE_H
#define _BURN_TRACK_TYPE_H

#include <glib.h>

#include <parrillada-enums.h>
#include <parrillada-media.h>

G_BEGIN_DECLS

typedef struct _ParrilladaTrackType ParrilladaTrackType;

ParrilladaTrackType *
parrillada_track_type_new (void);

void
parrillada_track_type_free (ParrilladaTrackType *type);

gboolean
parrillada_track_type_is_empty (const ParrilladaTrackType *type);
gboolean
parrillada_track_type_get_has_data (const ParrilladaTrackType *type);
gboolean
parrillada_track_type_get_has_image (const ParrilladaTrackType *type);
gboolean
parrillada_track_type_get_has_stream (const ParrilladaTrackType *type);
gboolean
parrillada_track_type_get_has_medium (const ParrilladaTrackType *type);

void
parrillada_track_type_set_has_data (ParrilladaTrackType *type);
void
parrillada_track_type_set_has_image (ParrilladaTrackType *type);
void
parrillada_track_type_set_has_stream (ParrilladaTrackType *type);
void
parrillada_track_type_set_has_medium (ParrilladaTrackType *type);

ParrilladaStreamFormat
parrillada_track_type_get_stream_format (const ParrilladaTrackType *type);
ParrilladaImageFormat
parrillada_track_type_get_image_format (const ParrilladaTrackType *type);
ParrilladaMedia
parrillada_track_type_get_medium_type (const ParrilladaTrackType *type);
ParrilladaImageFS
parrillada_track_type_get_data_fs (const ParrilladaTrackType *type);

void
parrillada_track_type_set_stream_format (ParrilladaTrackType *type,
				      ParrilladaStreamFormat format);
void
parrillada_track_type_set_image_format (ParrilladaTrackType *type,
				     ParrilladaImageFormat format);
void
parrillada_track_type_set_medium_type (ParrilladaTrackType *type,
				    ParrilladaMedia media);
void
parrillada_track_type_set_data_fs (ParrilladaTrackType *type,
				ParrilladaImageFS fs_type);

gboolean
parrillada_track_type_equal (const ParrilladaTrackType *type_A,
			  const ParrilladaTrackType *type_B);

G_END_DECLS

#endif

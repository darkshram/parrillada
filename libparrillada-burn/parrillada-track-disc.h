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

#ifndef _PARRILLADA_TRACK_DISC_H_
#define _PARRILLADA_TRACK_DISC_H_

#include <glib-object.h>

#include <parrillada-drive.h>

#include <parrillada-track.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TRACK_DISC             (parrillada_track_disc_get_type ())
#define PARRILLADA_TRACK_DISC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK_DISC, ParrilladaTrackDisc))
#define PARRILLADA_TRACK_DISC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK_DISC, ParrilladaTrackDiscClass))
#define PARRILLADA_IS_TRACK_DISC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK_DISC))
#define PARRILLADA_IS_TRACK_DISC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK_DISC))
#define PARRILLADA_TRACK_DISC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK_DISC, ParrilladaTrackDiscClass))

typedef struct _ParrilladaTrackDiscClass ParrilladaTrackDiscClass;
typedef struct _ParrilladaTrackDisc ParrilladaTrackDisc;

struct _ParrilladaTrackDiscClass
{
	ParrilladaTrackClass parent_class;
};

struct _ParrilladaTrackDisc
{
	ParrilladaTrack parent_instance;
};

GType parrillada_track_disc_get_type (void) G_GNUC_CONST;

ParrilladaTrackDisc *
parrillada_track_disc_new (void);

ParrilladaBurnResult
parrillada_track_disc_set_drive (ParrilladaTrackDisc *track,
			      ParrilladaDrive *drive);

ParrilladaDrive *
parrillada_track_disc_get_drive (ParrilladaTrackDisc *track);

ParrilladaBurnResult
parrillada_track_disc_set_track_num (ParrilladaTrackDisc *track,
				  guint num);

guint
parrillada_track_disc_get_track_num (ParrilladaTrackDisc *track);

ParrilladaMedia
parrillada_track_disc_get_medium_type (ParrilladaTrackDisc *track);

G_END_DECLS

#endif /* _PARRILLADA_TRACK_DISC_H_ */

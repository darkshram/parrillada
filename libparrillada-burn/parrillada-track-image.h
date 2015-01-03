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

#ifndef _BURN_TRACK_IMAGE_H_
#define _BURN_TRACK_IMAGE_H_

#include <glib-object.h>

#include <parrillada-enums.h>
#include <parrillada-track.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TRACK_IMAGE             (parrillada_track_image_get_type ())
#define PARRILLADA_TRACK_IMAGE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK_IMAGE, ParrilladaTrackImage))
#define PARRILLADA_TRACK_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK_IMAGE, ParrilladaTrackImageClass))
#define PARRILLADA_IS_TRACK_IMAGE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK_IMAGE))
#define PARRILLADA_IS_TRACK_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK_IMAGE))
#define PARRILLADA_TRACK_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK_IMAGE, ParrilladaTrackImageClass))

typedef struct _ParrilladaTrackImageClass ParrilladaTrackImageClass;
typedef struct _ParrilladaTrackImage ParrilladaTrackImage;

struct _ParrilladaTrackImageClass
{
	ParrilladaTrackClass parent_class;

	ParrilladaBurnResult	(*set_source)		(ParrilladaTrackImage *track,
							 const gchar *image,
							 const gchar *toc,
							 ParrilladaImageFormat format);

	ParrilladaBurnResult       (*set_block_num)	(ParrilladaTrackImage *track,
							 goffset blocks);
};

struct _ParrilladaTrackImage
{
	ParrilladaTrack parent_instance;
};

GType parrillada_track_image_get_type (void) G_GNUC_CONST;

ParrilladaTrackImage *
parrillada_track_image_new (void);

ParrilladaBurnResult
parrillada_track_image_set_source (ParrilladaTrackImage *track,
				const gchar *image,
				const gchar *toc,
				ParrilladaImageFormat format);

ParrilladaBurnResult
parrillada_track_image_set_block_num (ParrilladaTrackImage *track,
				   goffset blocks);

gchar *
parrillada_track_image_get_source (ParrilladaTrackImage *track,
				gboolean uri);

gchar *
parrillada_track_image_get_toc_source (ParrilladaTrackImage *track,
				    gboolean uri);

ParrilladaImageFormat
parrillada_track_image_get_format (ParrilladaTrackImage *track);

gboolean
parrillada_track_image_need_byte_swap (ParrilladaTrackImage *track);

G_END_DECLS

#endif /* _BURN_TRACK_IMAGE_H_ */

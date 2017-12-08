/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 * 
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with parrillada.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_VIDEO_DISC_H_
#define _PARRILLADA_VIDEO_DISC_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_VIDEO_DISC             (parrillada_video_disc_get_type ())
#define PARRILLADA_VIDEO_DISC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_VIDEO_DISC, ParrilladaVideoDisc))
#define PARRILLADA_VIDEO_DISC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_VIDEO_DISC, ParrilladaVideoDiscClass))
#define PARRILLADA_IS_VIDEO_DISC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_VIDEO_DISC))
#define PARRILLADA_IS_VIDEO_DISC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_VIDEO_DISC))
#define PARRILLADA_VIDEO_DISC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_VIDEO_DISC, ParrilladaVideoDiscClass))

typedef struct _ParrilladaVideoDiscClass ParrilladaVideoDiscClass;
typedef struct _ParrilladaVideoDisc ParrilladaVideoDisc;

struct _ParrilladaVideoDiscClass
{
	GtkBoxClass parent_class;
};

struct _ParrilladaVideoDisc
{
	GtkBox parent_instance;
};

GType parrillada_video_disc_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_video_disc_new (void);

G_END_DECLS

#endif /* _PARRILLADA_VIDEO_DISC_H_ */

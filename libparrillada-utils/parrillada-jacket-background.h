/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-misc is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_JACKET_BACKGROUND_H_
#define _PARRILLADA_JACKET_BACKGROUND_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_JACKET_IMAGE_CENTER	= 0,
	PARRILLADA_JACKET_IMAGE_TILE,
	PARRILLADA_JACKET_IMAGE_STRETCH
} ParrilladaJacketImageStyle;

typedef enum {
	PARRILLADA_JACKET_COLOR_SOLID	= 0,
	PARRILLADA_JACKET_COLOR_HGRADIENT	= 1,
	PARRILLADA_JACKET_COLOR_VGRADIENT 	= 2
} ParrilladaJacketColorStyle;

#define PARRILLADA_TYPE_JACKET_BACKGROUND             (parrillada_jacket_background_get_type ())
#define PARRILLADA_JACKET_BACKGROUND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_JACKET_BACKGROUND, ParrilladaJacketBackground))
#define PARRILLADA_JACKET_BACKGROUND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_JACKET_BACKGROUND, ParrilladaJacketBackgroundClass))
#define PARRILLADA_IS_JACKET_BACKGROUND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_JACKET_BACKGROUND))
#define PARRILLADA_IS_JACKET_BACKGROUND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_JACKET_BACKGROUND))
#define PARRILLADA_JACKET_BACKGROUND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_JACKET_BACKGROUND, ParrilladaJacketBackgroundClass))

typedef struct _ParrilladaJacketBackgroundClass ParrilladaJacketBackgroundClass;
typedef struct _ParrilladaJacketBackground ParrilladaJacketBackground;

struct _ParrilladaJacketBackgroundClass
{
	GtkDialogClass parent_class;
};

struct _ParrilladaJacketBackground
{
	GtkDialog parent_instance;
};

GType parrillada_jacket_background_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_jacket_background_new (void);

ParrilladaJacketColorStyle
parrillada_jacket_background_get_color_style (ParrilladaJacketBackground *back);

ParrilladaJacketImageStyle
parrillada_jacket_background_get_image_style (ParrilladaJacketBackground *back);

gchar *
parrillada_jacket_background_get_image_path (ParrilladaJacketBackground *back);

void
parrillada_jacket_background_get_color (ParrilladaJacketBackground *back,
				     GdkColor *color,
				     GdkColor *color2);

void
parrillada_jacket_background_set_color_style (ParrilladaJacketBackground *back,
					   ParrilladaJacketColorStyle style);

void
parrillada_jacket_background_set_image_style (ParrilladaJacketBackground *back,
					   ParrilladaJacketImageStyle style);

void
parrillada_jacket_background_set_image_path (ParrilladaJacketBackground *back,
					  const gchar *path);

void
parrillada_jacket_background_set_color (ParrilladaJacketBackground *back,
				     GdkColor *color,
				     GdkColor *color2);
G_END_DECLS

#endif /* _PARRILLADA_JACKET_BACKGROUND_H_ */

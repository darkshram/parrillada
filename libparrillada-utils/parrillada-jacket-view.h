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

#ifndef _PARRILLADA_JACKET_VIEW_H_
#define _PARRILLADA_JACKET_VIEW_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-jacket-background.h"

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_JACKET_FRONT		= 0,
	PARRILLADA_JACKET_BACK		= 1,
} ParrilladaJacketSide;

#define COVER_HEIGHT_FRONT_MM		120.0
#define COVER_WIDTH_FRONT_MM		120.0
#define COVER_WIDTH_FRONT_INCH		4.724
#define COVER_HEIGHT_FRONT_INCH		4.724

#define COVER_HEIGHT_BACK_MM		117.5
#define COVER_WIDTH_BACK_MM		152.0
#define COVER_HEIGHT_BACK_INCH		4.646
#define COVER_WIDTH_BACK_INCH		5.984

#define COVER_HEIGHT_SIDE_MM		COVER_HEIGHT_BACK_MM
#define COVER_WIDTH_SIDE_MM		6.0
#define COVER_HEIGHT_SIDE_INCH		COVER_HEIGHT_BACK_INCH
#define COVER_WIDTH_SIDE_INCH		0.235

#define COVER_TEXT_MARGIN		/*1.*/0.03 //0.079

#define PARRILLADA_TYPE_JACKET_VIEW             (parrillada_jacket_view_get_type ())
#define PARRILLADA_JACKET_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_JACKET_VIEW, ParrilladaJacketView))
#define PARRILLADA_JACKET_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_JACKET_VIEW, ParrilladaJacketViewClass))
#define PARRILLADA_IS_JACKET_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_JACKET_VIEW))
#define PARRILLADA_IS_JACKET_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_JACKET_VIEW))
#define PARRILLADA_JACKET_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_JACKET_VIEW, ParrilladaJacketViewClass))

typedef struct _ParrilladaJacketViewClass ParrilladaJacketViewClass;
typedef struct _ParrilladaJacketView ParrilladaJacketView;

struct _ParrilladaJacketViewClass
{
	GtkContainerClass parent_class;
};

struct _ParrilladaJacketView
{
	GtkContainer parent_instance;
};

GType parrillada_jacket_view_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_jacket_view_new (void);

void
parrillada_jacket_view_add_default_tag (ParrilladaJacketView *self,
				     GtkTextTag *tag);

void
parrillada_jacket_view_set_side (ParrilladaJacketView *view,
			      ParrilladaJacketSide side);

void
parrillada_jacket_view_set_image_style (ParrilladaJacketView *view,
				     ParrilladaJacketImageStyle style);

void
parrillada_jacket_view_set_color_background (ParrilladaJacketView *view,
					  GdkColor *color,
					  GdkColor *color2);
void
parrillada_jacket_view_set_color_style (ParrilladaJacketView *view,
				     ParrilladaJacketColorStyle style);

const gchar *
parrillada_jacket_view_get_image (ParrilladaJacketView *self);

const gchar *
parrillada_jacket_view_set_image (ParrilladaJacketView *view,
			       const gchar *path);

void
parrillada_jacket_view_configure_background (ParrilladaJacketView *view);

guint
parrillada_jacket_view_print (ParrilladaJacketView *view,
			   GtkPrintContext *context,
			   gdouble x,
			   gdouble y);

GtkTextBuffer *
parrillada_jacket_view_get_active_buffer (ParrilladaJacketView *view);

GtkTextBuffer *
parrillada_jacket_view_get_body_buffer (ParrilladaJacketView *view);

GtkTextBuffer *
parrillada_jacket_view_get_side_buffer (ParrilladaJacketView *view);

GtkTextAttributes *
parrillada_jacket_view_get_attributes (ParrilladaJacketView *view,
				    GtkTextIter *iter);

G_END_DECLS

#endif /* _PARRILLADA_JACKET_VIEW_H_ */

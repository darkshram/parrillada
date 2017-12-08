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

#ifndef PARRILLADA_IMAGE_TYPE_CHOOSER_H
#define PARRILLADA_IMAGE_TYPE_CHOOSER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER         (parrillada_image_type_chooser_get_type ())
#define PARRILLADA_IMAGE_TYPE_CHOOSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER, ParrilladaImageTypeChooser))
#define PARRILLADA_IMAGE_TYPE_CHOOSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER, ParrilladaImageTypeChooserClass))
#define PARRILLADA_IS_IMAGE_TYPE_CHOOSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER))
#define PARRILLADA_IS_IMAGE_TYPE_CHOOSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER))
#define PARRILLADA_IMAGE_TYPE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_IMAGE_TYPE_CHOOSER, ParrilladaImageTypeChooserClass))

typedef struct _ParrilladaImageTypeChooser ParrilladaImageTypeChooser;
typedef struct _ParrilladaImageTypeChooserPrivate ParrilladaImageTypeChooserPrivate;
typedef struct _ParrilladaImageTypeChooserClass ParrilladaImageTypeChooserClass;

struct _ParrilladaImageTypeChooser {
	GtkBox parent;
};

struct _ParrilladaImageTypeChooserClass {
	GtkBoxClass parent_class;
};

GType parrillada_image_type_chooser_get_type (void);
GtkWidget *parrillada_image_type_chooser_new (void);

guint
parrillada_image_type_chooser_set_formats (ParrilladaImageTypeChooser *self,
				        ParrilladaImageFormat formats,
                                        gboolean show_autodetect,
                                        gboolean is_video);
void
parrillada_image_type_chooser_set_format (ParrilladaImageTypeChooser *self,
				       ParrilladaImageFormat format);
void
parrillada_image_type_chooser_get_format (ParrilladaImageTypeChooser *self,
				       ParrilladaImageFormat *format);
gboolean
parrillada_image_type_chooser_get_VCD_type (ParrilladaImageTypeChooser *chooser);

void
parrillada_image_type_chooser_set_VCD_type (ParrilladaImageTypeChooser *chooser,
                                         gboolean is_svcd);

G_END_DECLS

#endif /* PARRILLADA_IMAGE_TYPE_CHOOSER_H */

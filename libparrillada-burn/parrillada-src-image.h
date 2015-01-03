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

#ifndef _PARRILLADA_SRC_IMAGE_H_
#define _PARRILLADA_SRC_IMAGE_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SRC_IMAGE             (parrillada_src_image_get_type ())
#define PARRILLADA_SRC_IMAGE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SRC_IMAGE, ParrilladaSrcImage))
#define PARRILLADA_SRC_IMAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SRC_IMAGE, ParrilladaSrcImageClass))
#define PARRILLADA_IS_SRC_IMAGE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SRC_IMAGE))
#define PARRILLADA_IS_SRC_IMAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SRC_IMAGE))
#define PARRILLADA_SRC_IMAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SRC_IMAGE, ParrilladaSrcImageClass))

typedef struct _ParrilladaSrcImageClass ParrilladaSrcImageClass;
typedef struct _ParrilladaSrcImage ParrilladaSrcImage;

struct _ParrilladaSrcImageClass
{
	GtkButtonClass parent_class;
};

struct _ParrilladaSrcImage
{
	GtkButton parent_instance;
};

GType parrillada_src_image_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_src_image_new (ParrilladaBurnSession *session);

G_END_DECLS

#endif /* _PARRILLADA_SRC_IMAGE_H_ */

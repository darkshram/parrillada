/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_PREVIEW

#ifndef _PARRILLADA_PREVIEW_H_
#define _PARRILLADA_PREVIEW_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-uri-container.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PREVIEW             (parrillada_preview_get_type ())
#define PARRILLADA_PREVIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_PREVIEW, ParrilladaPreview))
#define PARRILLADA_PREVIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_PREVIEW, ParrilladaPreviewClass))
#define PARRILLADA_IS_PREVIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_PREVIEW))
#define PARRILLADA_IS_PREVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_PREVIEW))
#define PARRILLADA_PREVIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_PREVIEW, ParrilladaPreviewClass))

typedef struct _ParrilladaPreviewClass ParrilladaPreviewClass;
typedef struct _ParrilladaPreview ParrilladaPreview;

struct _ParrilladaPreviewClass
{
	GtkAlignmentClass parent_class;
};

struct _ParrilladaPreview
{
	GtkAlignment parent_instance;
};

GType parrillada_preview_get_type (void) G_GNUC_CONST;
GtkWidget *parrillada_preview_new (void);

void
parrillada_preview_add_source (ParrilladaPreview *preview,
			    ParrilladaURIContainer *source);

void
parrillada_preview_hide (ParrilladaPreview *preview);

void
parrillada_preview_set_enabled (ParrilladaPreview *self,
			     gboolean preview);

G_END_DECLS

#endif				/* PLAYER_H */

#endif /* _PARRILLADA_PREVIEW_H_ */

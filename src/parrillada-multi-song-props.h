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

#ifndef _PARRILLADA_MULTI_SONG_PROPS_H_
#define _PARRILLADA_MULTI_SONG_PROPS_H_

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-rename.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_MULTI_SONG_PROPS             (parrillada_multi_song_props_get_type ())
#define PARRILLADA_MULTI_SONG_PROPS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_MULTI_SONG_PROPS, ParrilladaMultiSongProps))
#define PARRILLADA_MULTI_SONG_PROPS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_MULTI_SONG_PROPS, ParrilladaMultiSongPropsClass))
#define PARRILLADA_IS_MULTI_SONG_PROPS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_MULTI_SONG_PROPS))
#define PARRILLADA_IS_MULTI_SONG_PROPS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_MULTI_SONG_PROPS))
#define PARRILLADA_MULTI_SONG_PROPS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_MULTI_SONG_PROPS, ParrilladaMultiSongPropsClass))

typedef struct _ParrilladaMultiSongPropsClass ParrilladaMultiSongPropsClass;
typedef struct _ParrilladaMultiSongProps ParrilladaMultiSongProps;

struct _ParrilladaMultiSongPropsClass
{
	GtkDialogClass parent_class;
};

struct _ParrilladaMultiSongProps
{
	GtkDialog parent_instance;
};

GType parrillada_multi_song_props_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_multi_song_props_new (void);

void
parrillada_multi_song_props_set_show_gap (ParrilladaMultiSongProps *props,
				       gboolean show);

void
parrillada_multi_song_props_set_rename_callback (ParrilladaMultiSongProps *props,
					      GtkTreeSelection *selection,
					      gint column_num,
					      ParrilladaRenameCallback callback);
void
parrillada_multi_song_props_get_properties (ParrilladaMultiSongProps *props,
					 gchar **artist,
					 gchar **composer,
					 gchar **isrc,
					 gint64 *gap);

G_END_DECLS

#endif /* _PARRILLADA_MULTI_SONG_PROPS_H_ */

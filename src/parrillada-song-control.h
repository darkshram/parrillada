/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2010 <bonfire-app@wanadoo.fr>
 * 
 * parrillada is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PARRILLADA_SONG_CONTROL_H_
#define _PARRILLADA_SONG_CONTROL_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SONG_CONTROL             (parrillada_song_control_get_type ())
#define PARRILLADA_SONG_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SONG_CONTROL, ParrilladaSongControl))
#define PARRILLADA_SONG_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SONG_CONTROL, ParrilladaSongControlClass))
#define PARRILLADA_IS_SONG_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SONG_CONTROL))
#define PARRILLADA_IS_SONG_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SONG_CONTROL))
#define PARRILLADA_SONG_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SONG_CONTROL, ParrilladaSongControlClass))

typedef struct _ParrilladaSongControlClass ParrilladaSongControlClass;
typedef struct _ParrilladaSongControl ParrilladaSongControl;

struct _ParrilladaSongControlClass
{
	GtkAlignmentClass parent_class;
};

struct _ParrilladaSongControl
{
	GtkAlignment parent_instance;
};

GType parrillada_song_control_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_song_control_new (void);

void
parrillada_song_control_set_uri (ParrilladaSongControl *player,
                              const gchar *uri);

void
parrillada_song_control_set_info (ParrilladaSongControl *player,
                               const gchar *title,
                               const gchar *artist);

void
parrillada_song_control_set_boundaries (ParrilladaSongControl *player, 
                                     gsize start,
                                     gsize end);

gint64
parrillada_song_control_get_pos (ParrilladaSongControl *control);

gint64
parrillada_song_control_get_length (ParrilladaSongControl *control);

const gchar *
parrillada_song_control_get_uri (ParrilladaSongControl *control);

G_END_DECLS

#endif /* _PARRILLADA_SONG_CONTROL_H_ */

/***************************************************************************
 *            song-properties.h
 *
 *  lun avr 10 18:39:17 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Parrillada is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef SONG_PROPERTIES_H
#define SONG_PROPERTIES_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SONG_PROPS         (parrillada_song_props_get_type ())
#define PARRILLADA_SONG_PROPS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_SONG_PROPS, ParrilladaSongProps))
#define PARRILLADA_SONG_PROPS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_SONG_PROPS, ParrilladaSongPropsClass))
#define PARRILLADA_IS_SONG_PROPS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_SONG_PROPS))
#define PARRILLADA_IS_SONG_PROPS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_SONG_PROPS))
#define PARRILLADA_SONG_PROPS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_SONG_PROPS, ParrilladaSongPropsClass))

typedef struct ParrilladaSongPropsPrivate ParrilladaSongPropsPrivate;

typedef struct {
	GtkDialog parent;
	ParrilladaSongPropsPrivate *priv;
} ParrilladaSongProps;

typedef struct {
	GtkDialogClass parent_class;
} ParrilladaSongPropsClass;

GType parrillada_song_props_get_type (void);
GtkWidget *parrillada_song_props_new (void);

void
parrillada_song_props_get_properties (ParrilladaSongProps *self,
				   gchar **artist,
				   gchar **title,
				   gchar **composer,
				   gint *isrc,
				   gint64 *start,
				   gint64 *end,
				   gint64 *gap);
void
parrillada_song_props_set_properties (ParrilladaSongProps *self,
				   gint track_num,
				   const gchar *artist,
				   const gchar *title,
				   const gchar *composer,
				   gint isrc,
				   gint64 length,
				   gint64 start,
				   gint64 end,
				   gint64 gap);

G_END_DECLS

#endif /* SONG_PROPERTIES_H */

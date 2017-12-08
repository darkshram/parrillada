/***************************************************************************
 *            player-bacon.h
 *
 *  ven déc 30 11:29:33 2005
 *  Copyright  2005  Rouquier Philippe
 *  parrillada-app@wanadoo.fr
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

#ifndef PLAYER_BACON_H
#define PLAYER_BACON_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PLAYER_BACON         (parrillada_player_bacon_get_type ())
#define PARRILLADA_PLAYER_BACON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_PLAYER_BACON, ParrilladaPlayerBacon))
#define PARRILLADA_PLAYER_BACON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_PLAYER_BACON, ParrilladaPlayerBaconClass))
#define PARRILLADA_IS_PLAYER_BACON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_PLAYER_BACON))
#define PARRILLADA_IS_PLAYER_BACON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_PLAYER_BACON))
#define PARRILLADA_PLAYER_BACON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_PLAYER_BACON, ParrilladaPlayerBaconClass))

#define	PLAYER_BACON_WIDTH 120
#define	PLAYER_BACON_HEIGHT 90

typedef struct ParrilladaPlayerBaconPrivate ParrilladaPlayerBaconPrivate;

typedef enum {
	BACON_STATE_ERROR,
	BACON_STATE_READY,
	BACON_STATE_PAUSED,
	BACON_STATE_PLAYING
} ParrilladaPlayerBaconState;

typedef struct {
	GtkWidget parent;
	ParrilladaPlayerBaconPrivate *priv;
} ParrilladaPlayerBacon;

typedef struct {
	GtkWidgetClass parent_class;

	void	(*state_changed)	(ParrilladaPlayerBacon *bacon,
					 ParrilladaPlayerBaconState state);

	void	(*eof)			(ParrilladaPlayerBacon *bacon);

} ParrilladaPlayerBaconClass;

GType parrillada_player_bacon_get_type (void);
GtkWidget *parrillada_player_bacon_new (void);

void parrillada_player_bacon_set_uri (ParrilladaPlayerBacon *bacon, const gchar *uri);
void parrillada_player_bacon_set_volume (ParrilladaPlayerBacon *bacon, gdouble volume);
gboolean parrillada_player_bacon_set_boundaries (ParrilladaPlayerBacon *bacon, gint64 start, gint64 end);
gboolean parrillada_player_bacon_play (ParrilladaPlayerBacon *bacon);
gboolean parrillada_player_bacon_stop (ParrilladaPlayerBacon *bacon);
gboolean parrillada_player_bacon_set_pos (ParrilladaPlayerBacon *bacon, gdouble pos);
gboolean parrillada_player_bacon_get_pos (ParrilladaPlayerBacon *bacon, gint64 *pos);
gdouble  parrillada_player_bacon_get_volume (ParrilladaPlayerBacon *bacon);
gboolean parrillada_player_bacon_forward (ParrilladaPlayerBacon *bacon, gint64 value);
gboolean parrillada_player_bacon_backward (ParrilladaPlayerBacon *bacon, gint64 value);
G_END_DECLS

#endif /* PLAYER_BACON_H */

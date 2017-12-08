/***************************************************************************
*            player.h
*
*  lun mai 30 08:15:01 2005
*  Copyright  2005  Philippe Rouquier
*  parrillada-app@wanadoo.fr
****************************************************************************/

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef PLAYER_H
#define PLAYER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PLAYER         (parrillada_player_get_type ())
#define PARRILLADA_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_PLAYER, ParrilladaPlayer))
#define PARRILLADA_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_PLAYER, ParrilladaPlayerClass))
#define PARRILLADA_IS_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_PLAYER))
#define PARRILLADA_IS_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_PLAYER))
#define PARRILLADA_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_PLAYER, ParrilladaPlayerClass))

typedef struct ParrilladaPlayerPrivate ParrilladaPlayerPrivate;

typedef struct {
	GtkAlignment parent;
	ParrilladaPlayerPrivate *priv;
} ParrilladaPlayer;

typedef struct {
	GtkAlignmentClass parent_class;

	void		(*error)	(ParrilladaPlayer *player);
	void		(*ready)	(ParrilladaPlayer *player);
} ParrilladaPlayerClass;

GType parrillada_player_get_type (void);
GtkWidget *parrillada_player_new (void);

void
parrillada_player_set_uri (ParrilladaPlayer *player,
			const gchar *uri);
void
parrillada_player_set_boundaries (ParrilladaPlayer *player, 
			       gint64 start,
			       gint64 end);

G_END_DECLS

#endif

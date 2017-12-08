/***************************************************************************
 *            parrillada-disc.h
 *
 *  dim nov 27 14:58:13 2005
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

#ifndef DISC_H
#define DISC_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-project-parse.h"
#include "parrillada-session.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_DISC         (parrillada_disc_get_type ())
#define PARRILLADA_DISC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_DISC, ParrilladaDisc))
#define PARRILLADA_IS_DISC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_DISC))
#define PARRILLADA_DISC_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), PARRILLADA_TYPE_DISC, ParrilladaDiscIface))

#define PARRILLADA_DISC_ACTION "DiscAction"


typedef enum {
	PARRILLADA_DISC_OK = 0,
	PARRILLADA_DISC_NOT_IN_TREE,
	PARRILLADA_DISC_NOT_READY,
	PARRILLADA_DISC_LOADING,
	PARRILLADA_DISC_BROKEN_SYMLINK,
	PARRILLADA_DISC_CANCELLED,
	PARRILLADA_DISC_ERROR_SIZE,
	PARRILLADA_DISC_ERROR_EMPTY_SELECTION,
	PARRILLADA_DISC_ERROR_FILE_NOT_FOUND,
	PARRILLADA_DISC_ERROR_UNREADABLE,
	PARRILLADA_DISC_ERROR_ALREADY_IN_TREE,
	PARRILLADA_DISC_ERROR_JOLIET,
	PARRILLADA_DISC_ERROR_FILE_TYPE,
	PARRILLADA_DISC_ERROR_THREAD,
	PARRILLADA_DISC_ERROR_UNKNOWN
} ParrilladaDiscResult;

typedef struct _ParrilladaDisc        ParrilladaDisc;
typedef struct _ParrilladaDiscIface   ParrilladaDiscIface;

struct _ParrilladaDiscIface {
	GTypeInterface g_iface;

	/* signals */
	void	(*selection_changed)			(ParrilladaDisc *disc);

	/* virtual functions */
	ParrilladaDiscResult	(*set_session_contents)	(ParrilladaDisc *disc,
							 ParrilladaBurnSession *session);

	ParrilladaDiscResult	(*add_uri)		(ParrilladaDisc *disc,
							 const gchar *uri);

	gboolean		(*is_empty)		(ParrilladaDisc *disc);
	gboolean		(*get_selected_uri)	(ParrilladaDisc *disc,
							 gchar **uri);
	gboolean		(*get_boundaries)	(ParrilladaDisc *disc,
							 gint64 *start,
							 gint64 *end);

	void			(*delete_selected)	(ParrilladaDisc *disc);
	void			(*clear)		(ParrilladaDisc *disc);

	guint			(*add_ui)		(ParrilladaDisc *disc,
							 GtkUIManager *manager,
							 GtkWidget *message);
};

GType parrillada_disc_get_type (void);

guint
parrillada_disc_add_ui (ParrilladaDisc *disc,
		     GtkUIManager *manager,
		     GtkWidget *message);

ParrilladaDiscResult
parrillada_disc_add_uri (ParrilladaDisc *disc, const gchar *escaped_uri);

gboolean
parrillada_disc_get_selected_uri (ParrilladaDisc *disc, gchar **uri);

gboolean
parrillada_disc_get_boundaries (ParrilladaDisc *disc,
			     gint64 *start,
			     gint64 *end);

void
parrillada_disc_delete_selected (ParrilladaDisc *disc);

gboolean
parrillada_disc_clear (ParrilladaDisc *disc);

ParrilladaDiscResult
parrillada_disc_get_status (ParrilladaDisc *disc,
			 gint *remaining,
			 gchar **current_task);

ParrilladaDiscResult
parrillada_disc_set_session_contents (ParrilladaDisc *disc,
				   ParrilladaBurnSession *session);

gboolean
parrillada_disc_is_empty (ParrilladaDisc *disc);

void
parrillada_disc_selection_changed (ParrilladaDisc *disc);

G_END_DECLS

#endif /* DISC_H */

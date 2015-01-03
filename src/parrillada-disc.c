/***************************************************************************
 *            disc.c
 *
 *  dim nov 27 14:58:13 2005
 *  Copyright  2005  Rouquier Philippe
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "parrillada-disc.h"
#include "parrillada-session.h"
 
static void parrillada_disc_base_init (gpointer g_class);

typedef enum {
	SELECTION_CHANGED_SIGNAL,
	LAST_SIGNAL
} ParrilladaDiscSignalType;

static guint parrillada_disc_signals [LAST_SIGNAL] = { 0 };

GType
parrillada_disc_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (ParrilladaDiscIface),
			parrillada_disc_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "ParrilladaDisc",
					       &our_info,
					       0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
parrillada_disc_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	parrillada_disc_signals [SELECTION_CHANGED_SIGNAL] =
	    g_signal_new ("selection_changed",
			  PARRILLADA_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (ParrilladaDiscIface, selection_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	initialized = TRUE;
}

ParrilladaDiscResult
parrillada_disc_add_uri (ParrilladaDisc *disc,
		      const gchar *uri)
{
	ParrilladaDiscIface *iface;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), PARRILLADA_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (uri != NULL, PARRILLADA_DISC_ERROR_UNKNOWN);
	
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->add_uri)
		return (* iface->add_uri) (disc, uri);

	return PARRILLADA_DISC_ERROR_UNKNOWN;
}

void
parrillada_disc_delete_selected (ParrilladaDisc *disc)
{
	ParrilladaDiscIface *iface;

	g_return_if_fail (PARRILLADA_IS_DISC (disc));
	
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->delete_selected)
		(* iface->delete_selected) (disc);
}

gboolean
parrillada_disc_clear (ParrilladaDisc *disc)
{
	ParrilladaDiscIface *iface;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), FALSE);
	
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (!iface->clear)
		return FALSE;

	(* iface->clear) (disc);
	return TRUE;
}

ParrilladaDiscResult
parrillada_disc_set_session_contents (ParrilladaDisc *disc,
				   ParrilladaBurnSession *session)
{
	ParrilladaDiscIface *iface;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), PARRILLADA_DISC_ERROR_UNKNOWN);
	
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->set_session_contents)
		return (* iface->set_session_contents) (disc, session);

	return PARRILLADA_DISC_ERROR_UNKNOWN;
}

gboolean
parrillada_disc_get_selected_uri (ParrilladaDisc *disc, gchar **uri)
{
	ParrilladaDiscIface *iface;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), FALSE);
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->get_selected_uri)
		return (* iface->get_selected_uri) (disc, uri);

	return FALSE;
}

gboolean
parrillada_disc_get_boundaries (ParrilladaDisc *disc,
			     gint64 *start,
			     gint64 *end)
{
	ParrilladaDiscIface *iface;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), FALSE);
	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->get_boundaries)
		return (* iface->get_boundaries) (disc,
						  start,
						  end);

	return FALSE;
}

guint
parrillada_disc_add_ui (ParrilladaDisc *disc, GtkUIManager *manager, GtkWidget *message)
{
	ParrilladaDiscIface *iface;

	if (!disc)
		return 0;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), 0);
	g_return_val_if_fail (GTK_IS_UI_MANAGER (manager), 0);

	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->add_ui)
		return (* iface->add_ui) (disc, manager, message);

	return 0;
}

gboolean
parrillada_disc_is_empty (ParrilladaDisc *disc)
{
	ParrilladaDiscIface *iface;

	if (!disc)
		return 0;

	g_return_val_if_fail (PARRILLADA_IS_DISC (disc), 0);

	iface = PARRILLADA_DISC_GET_IFACE (disc);
	if (iface->is_empty)
		return (* iface->is_empty) (disc);

	return FALSE;

}

void
parrillada_disc_selection_changed (ParrilladaDisc *disc)
{
	g_return_if_fail (PARRILLADA_IS_DISC (disc));
	g_signal_emit (disc,
		       parrillada_disc_signals [SELECTION_CHANGED_SIGNAL],
		       0);
}

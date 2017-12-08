/***************************************************************************
 *            parrillada-uri-container.c
 *
 *  lun mai 22 08:54:18 2006
 *  Copyright  2006  Rouquier Philippe
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

#include <glib.h>
#include <gtk/gtk.h>

#include "parrillada-uri-container.h"
 
static void parrillada_uri_container_base_init (gpointer g_class);

typedef enum {
	URI_ACTIVATED_SIGNAL,
	URI_SELECTED_SIGNAL,
	LAST_SIGNAL
} ParrilladaURIContainerSignalType;

static guint parrillada_uri_container_signals[LAST_SIGNAL] = { 0 };

GType
parrillada_uri_container_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (ParrilladaURIContainerIFace),
			parrillada_uri_container_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "ParrilladaURIContainer",
					       &our_info,
					       0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
parrillada_uri_container_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	parrillada_uri_container_signals [URI_SELECTED_SIGNAL] =
	    g_signal_new ("uri_selected",
			  PARRILLADA_TYPE_URI_CONTAINER,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (ParrilladaURIContainerIFace, uri_selected),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	parrillada_uri_container_signals [URI_ACTIVATED_SIGNAL] =
	    g_signal_new ("uri_activated",
			  PARRILLADA_TYPE_URI_CONTAINER,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (ParrilladaURIContainerIFace, uri_activated),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	initialized = TRUE;
}

gboolean
parrillada_uri_container_get_boundaries (ParrilladaURIContainer *container,
				      gint64 *start,
				      gint64 *end)
{
	ParrilladaURIContainerIFace *iface;

	g_return_val_if_fail (PARRILLADA_IS_URI_CONTAINER (container), FALSE);

	if (!gtk_widget_get_mapped (GTK_WIDGET (container)))
		return FALSE;

	iface = PARRILLADA_URI_CONTAINER_GET_IFACE (container);
	if (iface->get_boundaries)
		return (* iface->get_boundaries) (container, start, end);

	return FALSE;
}

gchar *
parrillada_uri_container_get_selected_uri (ParrilladaURIContainer *container)
{
	ParrilladaURIContainerIFace *iface;

	g_return_val_if_fail (PARRILLADA_IS_URI_CONTAINER (container), NULL);

	if (!gtk_widget_get_mapped (GTK_WIDGET (container)))
		return NULL;

	iface = PARRILLADA_URI_CONTAINER_GET_IFACE (container);
	if (iface->get_selected_uri)
		return (* iface->get_selected_uri) (container);

	return NULL;
}

gchar **
parrillada_uri_container_get_selected_uris (ParrilladaURIContainer *container)
{
	ParrilladaURIContainerIFace *iface;

	g_return_val_if_fail (PARRILLADA_IS_URI_CONTAINER (container), NULL);

	if (!gtk_widget_get_mapped (GTK_WIDGET (container)))
		return NULL;

	iface = PARRILLADA_URI_CONTAINER_GET_IFACE (container);
	if (iface->get_selected_uris)
		return (* iface->get_selected_uris) (container);

	return NULL;
}

void
parrillada_uri_container_uri_selected (ParrilladaURIContainer *container)
{
	g_return_if_fail (PARRILLADA_IS_URI_CONTAINER (container));
	g_signal_emit (container,
		       parrillada_uri_container_signals [URI_SELECTED_SIGNAL],
		       0);
}

void
parrillada_uri_container_uri_activated (ParrilladaURIContainer *container)
{
	g_return_if_fail (PARRILLADA_IS_URI_CONTAINER (container));
	g_signal_emit (container,
		       parrillada_uri_container_signals [URI_ACTIVATED_SIGNAL],
		       0);
}

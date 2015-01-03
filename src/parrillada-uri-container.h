/***************************************************************************
 *            parrillada-uri-container.h
 *
 *  lun mai 22 08:54:18 2006
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

#ifndef PARRILLADA_URI_CONTAINER_H
#define PARRILLADA_URI_CONTAINER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_URI_CONTAINER         (parrillada_uri_container_get_type ())
#define PARRILLADA_URI_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_URI_CONTAINER, ParrilladaURIContainer))
#define PARRILLADA_IS_URI_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_URI_CONTAINER))
#define PARRILLADA_URI_CONTAINER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), PARRILLADA_TYPE_URI_CONTAINER, ParrilladaURIContainerIFace))


typedef struct _ParrilladaURIContainer ParrilladaURIContainer;

typedef struct {
	GTypeInterface g_iface;

	/* signals */
	void		(*uri_selected)		(ParrilladaURIContainer *container);
	void		(*uri_activated)	(ParrilladaURIContainer *container);

	/* virtual functions */
	gboolean	(*get_boundaries)	(ParrilladaURIContainer *container,
						 gint64 *start,
						 gint64 *end);
	gchar*		(*get_selected_uri)	(ParrilladaURIContainer *container);
	gchar**		(*get_selected_uris)	(ParrilladaURIContainer *container);

} ParrilladaURIContainerIFace;


GType parrillada_uri_container_get_type (void);

gboolean
parrillada_uri_container_get_boundaries (ParrilladaURIContainer *container,
				      gint64 *start,
				      gint64 *end);
gchar *
parrillada_uri_container_get_selected_uri (ParrilladaURIContainer *container);
gchar **
parrillada_uri_container_get_selected_uris (ParrilladaURIContainer *container);

void
parrillada_uri_container_uri_selected (ParrilladaURIContainer *container);
void
parrillada_uri_container_uri_activated (ParrilladaURIContainer *container);

G_END_DECLS

#endif /* PARRILLADA_URI_CONTAINER_H */

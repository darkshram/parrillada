/***************************************************************************
 *            parrillada-layout-object.h
 *
 *  dim oct 15 17:15:58 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
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

#ifndef PARRILLADA_LAYOUT_OBJECT_H
#define PARRILLADA_LAYOUT_OBJECT_H

#include <glib.h>
#include <glib-object.h>

#include "parrillada-layout.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_LAYOUT_OBJECT         (parrillada_layout_object_get_type ())
#define PARRILLADA_LAYOUT_OBJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_LAYOUT_OBJECT, ParrilladaLayoutObject))
#define PARRILLADA_IS_LAYOUT_OBJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_LAYOUT_OBJECT))
#define PARRILLADA_LAYOUT_OBJECT_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), PARRILLADA_TYPE_LAYOUT_OBJECT, ParrilladaLayoutObjectIFace))

typedef struct _ParrilladaLayoutObject ParrilladaLayoutObject;
typedef struct _ParrilladaLayoutIFace ParrilladaLayoutObjectIFace;

struct _ParrilladaLayoutIFace {
	GTypeInterface g_iface;

	void	(*get_proportion)	(ParrilladaLayoutObject *self,
					 gint *header,
					 gint *center,
					 gint *footer);
	void	(*set_context)		(ParrilladaLayoutObject *self,
					 ParrilladaLayoutType type);
};

GType parrillada_layout_object_get_type (void);

void parrillada_layout_object_get_proportion (ParrilladaLayoutObject *self,
					   gint *header,
					   gint *center,
					   gint *footer);

void parrillada_layout_object_set_context (ParrilladaLayoutObject *self,
					ParrilladaLayoutType type);

G_END_DECLS

#endif /* PARRILLADA_LAYOUT_OBJECT_H */

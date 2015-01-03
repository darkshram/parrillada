/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_DEST_SELECTION_H_
#define _PARRILLADA_DEST_SELECTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-medium-selection.h"
#include "parrillada-session.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_DEST_SELECTION             (parrillada_dest_selection_get_type ())
#define PARRILLADA_DEST_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DEST_SELECTION, ParrilladaDestSelection))
#define PARRILLADA_DEST_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DEST_SELECTION, ParrilladaDestSelectionClass))
#define PARRILLADA_IS_DEST_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DEST_SELECTION))
#define PARRILLADA_IS_DEST_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DEST_SELECTION))
#define PARRILLADA_DEST_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DEST_SELECTION, ParrilladaDestSelectionClass))

typedef struct _ParrilladaDestSelectionClass ParrilladaDestSelectionClass;
typedef struct _ParrilladaDestSelection ParrilladaDestSelection;

struct _ParrilladaDestSelectionClass
{
	ParrilladaMediumSelectionClass parent_class;
};

struct _ParrilladaDestSelection
{
	ParrilladaMediumSelection parent_instance;
};

GType parrillada_dest_selection_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_dest_selection_new (ParrilladaBurnSession *selection);

void
parrillada_dest_selection_set_session (ParrilladaDestSelection *selection,
				    ParrilladaBurnSession *session);

void
parrillada_dest_selection_choose_best (ParrilladaDestSelection *selection);

G_END_DECLS

#endif /* _PARRILLADA_DEST_SELECTION_H_ */

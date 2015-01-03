/***************************************************************************
*            search-entry.h
*
*  jeu mai 19 20:06:55 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
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

#ifndef SEARCH_ENTRY_H
#define SEARCH_ENTRY_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-layout.h"
#include "parrillada-search-engine.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SEARCH_ENTRY         (parrillada_search_entry_get_type ())
#define PARRILLADA_SEARCH_ENTRY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_SEARCH_ENTRY, ParrilladaSearchEntry))
#define PARRILLADA_SEARCH_ENTRY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_SEARCH_ENTRY, ParrilladaSearchEntryClass))
#define PARRILLADA_IS_SEARCH_ENTRY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_SEARCH_ENTRY))
#define PARRILLADA_IS_SEARCH_ENTRY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_SEARCH_ENTRY))
#define PARRILLADA_SEARCH_ENTRY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_SEARCH_ENTRY, ParrilladaSearchEntryClass))

typedef struct ParrilladaSearchEntryPrivate ParrilladaSearchEntryPrivate;

typedef struct {
	GtkTable parent;
	ParrilladaSearchEntryPrivate *priv;
} ParrilladaSearchEntry;

typedef struct {
	GtkTableClass parent_class;

	void	(*activated)	(ParrilladaSearchEntry *entry);

} ParrilladaSearchEntryClass;

GType parrillada_search_entry_get_type (void);
GtkWidget *parrillada_search_entry_new (void);

gboolean
parrillada_search_entry_set_query (ParrilladaSearchEntry *entry,
                                ParrilladaSearchEngine *search);

void
parrillada_search_entry_set_context (ParrilladaSearchEntry *entry,
				  ParrilladaLayoutType type);

G_END_DECLS

#endif				/* SEARCH_ENTRY_H */

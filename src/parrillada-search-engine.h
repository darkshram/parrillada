/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 * 
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with parrillada.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _SEARCH_ENGINE_H
#define _SEARCH_ENGINE_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
	PARRILLADA_SEARCH_TREE_HIT_COL		= 0,
	PARRILLADA_SEARCH_TREE_NB_COL
};

typedef enum {
	PARRILLADA_SEARCH_SCOPE_ANY		= 0,
	PARRILLADA_SEARCH_SCOPE_VIDEO		= 1,
	PARRILLADA_SEARCH_SCOPE_MUSIC		= 1 << 1,
	PARRILLADA_SEARCH_SCOPE_PICTURES	= 1 << 2,
	PARRILLADA_SEARCH_SCOPE_DOCUMENTS	= 1 << 3,
} ParrilladaSearchScope;

#define PARRILLADA_TYPE_SEARCH_ENGINE         (parrillada_search_engine_get_type ())
#define PARRILLADA_SEARCH_ENGINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_SEARCH_ENGINE, ParrilladaSearchEngine))
#define PARRILLADA_IS_SEARCH_ENGINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_SEARCH_ENGINE))
#define PARRILLADA_SEARCH_ENGINE_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), PARRILLADA_TYPE_SEARCH_ENGINE, ParrilladaSearchEngineIface))

typedef struct _ParrilladaSearchEngine        ParrilladaSearchEngine;
typedef struct _ParrilladaSearchEngineIface   ParrilladaSearchEngineIface;

struct _ParrilladaSearchEngineIface {
	GTypeInterface g_iface;

	/* <Signals> */
	void	(*search_error)				(ParrilladaSearchEngine *search,
							 GError *error);
	void	(*search_finished)			(ParrilladaSearchEngine *search);
	void	(*hit_removed)				(ParrilladaSearchEngine *search,
					                 gpointer hit);
	void	(*hit_added)				(ParrilladaSearchEngine *search,
						         gpointer hit);

	/* <Virtual functions> */
	gboolean	(*is_available)			(ParrilladaSearchEngine *search);
	gboolean	(*query_new)			(ParrilladaSearchEngine *search,
					                 const gchar *keywords);
	gboolean	(*query_set_scope)		(ParrilladaSearchEngine *search,
					                 ParrilladaSearchScope scope);
	gboolean	(*query_set_mime)		(ParrilladaSearchEngine *search,
					                 const gchar **mimes);
	gboolean	(*query_start)			(ParrilladaSearchEngine *search);

	gboolean	(*add_hits)			(ParrilladaSearchEngine *search,
					                 GtkTreeModel *model,
					                 gint range_start,
					                 gint range_end);

	gint		(*num_hits)			(ParrilladaSearchEngine *engine);

	const gchar*	(*uri_from_hit)			(ParrilladaSearchEngine *engine,
				                         gpointer hit);
	const gchar *	(*mime_from_hit)		(ParrilladaSearchEngine *engine,
				                	 gpointer hit);
	gint		(*score_from_hit)		(ParrilladaSearchEngine *engine,
							 gpointer hit);
};

GType parrillada_search_engine_get_type (void);

ParrilladaSearchEngine *
parrillada_search_engine_new_default (void);

gboolean
parrillada_search_engine_is_available (ParrilladaSearchEngine *search);

gint
parrillada_search_engine_num_hits (ParrilladaSearchEngine *search);

gboolean
parrillada_search_engine_new_query (ParrilladaSearchEngine *search,
                                 const gchar *keywords);

gboolean
parrillada_search_engine_set_query_scope (ParrilladaSearchEngine *search,
                                       ParrilladaSearchScope scope);

gboolean
parrillada_search_engine_set_query_mime (ParrilladaSearchEngine *search,
                                      const gchar **mimes);

gboolean
parrillada_search_engine_start_query (ParrilladaSearchEngine *search);

void
parrillada_search_engine_query_finished (ParrilladaSearchEngine *search);

void
parrillada_search_engine_query_error (ParrilladaSearchEngine *search,
                                   GError *error);

void
parrillada_search_engine_hit_removed (ParrilladaSearchEngine *search,
                                   gpointer hit);
void
parrillada_search_engine_hit_added (ParrilladaSearchEngine *search,
                                 gpointer hit);

const gchar *
parrillada_search_engine_uri_from_hit (ParrilladaSearchEngine *search,
                                     gpointer hit);
const gchar *
parrillada_search_engine_mime_from_hit (ParrilladaSearchEngine *search,
                                     gpointer hit);
gint
parrillada_search_engine_score_from_hit (ParrilladaSearchEngine *search,
                                      gpointer hit);

gint
parrillada_search_engine_add_hits (ParrilladaSearchEngine *search,
                                GtkTreeModel *model,
                                gint range_start,
                                gint range_end);

G_END_DECLS

#endif /* _SEARCH_ENGINE_H */

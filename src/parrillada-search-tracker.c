/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Rouquier Philippe 2009 <bonfire-app@wanadoo.fr>
 * 
 * parrillada is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <gio/gio.h>

#include "parrillada-search-tracker.h"
#include "parrillada-search-engine.h"

typedef struct _ParrilladaSearchTrackerPrivate ParrilladaSearchTrackerPrivate;
struct _ParrilladaSearchTrackerPrivate
{
	TrackerSparqlConnection *connection;
	GCancellable *cancellable;
	GPtrArray *results;

	ParrilladaSearchScope scope;

	gchar **mimes;
	gchar *keywords;

	guint current_call_id;
};

#define PARRILLADA_SEARCH_TRACKER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_SEARCH_TRACKER, ParrilladaSearchTrackerPrivate))

static void parrillada_search_tracker_init_engine (ParrilladaSearchEngineIface *iface);

G_DEFINE_TYPE_WITH_CODE (ParrilladaSearchTracker,
			 parrillada_search_tracker,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (PARRILLADA_TYPE_SEARCH_ENGINE,
					        parrillada_search_tracker_init_engine));

static gboolean
parrillada_search_tracker_is_available (ParrilladaSearchEngine *engine)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (engine);
	GError *error = NULL;
	if (priv->connection)
		return TRUE;
	
	priv->cancellable = g_cancellable_new ();
 	priv->connection = tracker_sparql_connection_get (priv->cancellable, &error);
	return (priv->connection != NULL);
}

static gint
parrillada_search_tracker_num_hits (ParrilladaSearchEngine *engine)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (engine);
	if (!priv->results)
		return 0;

	return priv->results->len;
}

static const gchar *
parrillada_search_tracker_uri_from_hit (ParrilladaSearchEngine *engine,
                                     gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return NULL;

	if (g_strv_length (tracker_hit) >= 2)
		return tracker_hit [1];

	return NULL;
}

static const gchar *
parrillada_search_tracker_mime_from_hit (ParrilladaSearchEngine *engine,
                                      gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return NULL;

	if (g_strv_length (tracker_hit) >= 3)
		return tracker_hit [2];

	return NULL;
}

static int
parrillada_search_tracker_score_from_hit (ParrilladaSearchEngine *engine,
                                       gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return 0;

	if (g_strv_length (tracker_hit) >= 4)
		return (int) strtof (tracker_hit [3], NULL);

	return 0;
}

static void parrillada_search_tracker_cursor_callback (GObject      *object,
						    GAsyncResult *result,
						    gpointer      user_data);

static void
parrillada_search_tracker_cursor_next (ParrilladaSearchEngine *search,
				    TrackerSparqlCursor    *cursor)
{
	ParrilladaSearchTrackerPrivate *priv;
	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);
	
	tracker_sparql_cursor_next_async (cursor,
					  priv->cancellable,
					  parrillada_search_tracker_cursor_callback,
					  search);
}

static void
parrillada_search_tracker_cursor_callback (GObject      *object,
					GAsyncResult *result,
					gpointer      user_data)
{
	ParrilladaSearchEngine *search;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error) {
		parrillada_search_engine_query_error (search, error);
		g_error_free (error);

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	if (!success) {
		parrillada_search_engine_query_finished (search);

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	/* We iterate result by result, not n at a time. */
	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	parrillada_search_engine_hit_added (search, hits);
	g_list_free (hits);

	/* Get next */
	parrillada_search_tracker_cursor_next (search, cursor);
}

static void
parrillada_search_tracker_reply (GObject      *object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	ParrilladaSearchEngine *search = PARRILLADA_SEARCH_ENGINE (user_data);
	GError *error = NULL;
	
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (cursor) {
		g_object_unref (cursor);
	}

	if (error) {
		parrillada_search_engine_query_error (search, error);
		return;
	}

	if (!success) {
		parrillada_search_engine_query_finished (search);
		
		if (cursor) {
			g_object_unref (cursor);
		}
		return;
    
	}

	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	parrillada_search_engine_hit_added (search, result);
	g_list_free (hits);

	parrillada_search_engine_query_finished (search);
}

static gboolean
parrillada_search_tracker_query_start_real (ParrilladaSearchEngine *search,
					 gint index)
{
	ParrilladaSearchTrackerPrivate *priv;
	gboolean res = FALSE;
	GString *query = NULL;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);

	query = g_string_new ("SELECT ?file ?url ?mime fts:rank(?file) "	/* Which variables should be returned */
			      "WHERE {"						/* Start defining the search and its scope */
			      "  ?file a nfo:FileDataObject . "			/* File must be a file (not a stream, ...) */
	                      "  ?file nie:url ?url . "				/* Get the url of the file */
	                      "  ?file nie:mimeType ?mime . ");			/* Get its mime */

	if (priv->mimes) {
		int i;

		g_string_append (query, " FILTER ( ");
		for (i = 0; priv->mimes [i]; i ++) {				/* Filter files according to their mime type */
			if (i > 0)
				g_string_append (query, " || ");

			g_string_append_printf (query,
						"?mime = \"%s\"",
						priv->mimes [i]);
		}
		g_string_append (query, " ) ");
	}

	if (priv->scope) {
		gboolean param_added = FALSE;

		g_string_append (query,
				 "  ?file a ?type . "
				 "  FILTER ( ");

		if (priv->scope & PARRILLADA_SEARCH_SCOPE_MUSIC) {
			query = g_string_append (query, "?type = nmm:MusicPiece");
			param_added = TRUE;
		}

		if (priv->scope & PARRILLADA_SEARCH_SCOPE_VIDEO) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Video");

			param_added = TRUE;
		}
	
		if (priv->scope & PARRILLADA_SEARCH_SCOPE_PICTURES) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Image");

			param_added = TRUE;
		}

		if (priv->scope & PARRILLADA_SEARCH_SCOPE_DOCUMENTS) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Document");
		}

		g_string_append (query,
				 " ) ");
	}

	if (priv->keywords)
		g_string_append_printf (query,
					"  ?file fts:match \"%s\" ",		/* File must match possible keywords */
					priv->keywords);

	g_string_append (query,
			 " } "
			 "ORDER BY ASC(fts:rank(?file)) "
			 "OFFSET 0 "
			 "LIMIT 10000");

	g_string_append (query, ")");

	g_string_append (query,
			 "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");

	tracker_sparql_connection_query_async (priv->connection,
					       query->str,
					       priv->cancellable,
					       parrillada_search_tracker_reply,
					       search);
	g_string_free (query, TRUE);

	return res;
}

static gboolean
parrillada_search_tracker_query_start (ParrilladaSearchEngine *search)
{
	return parrillada_search_tracker_query_start_real (search, 0);
}

static gboolean
parrillada_search_tracker_add_hit_to_tree (ParrilladaSearchEngine *search,
                                        GtkTreeModel *model,
                                        gint range_start,
                                        gint range_end)
{
	ParrilladaSearchTrackerPrivate *priv;
	gint i;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);

	if (!priv->results)
		return FALSE;

	for (i = range_start; g_ptr_array_index (priv->results, i) && i < range_end; i ++) {
		gchar **hit;
		GtkTreeIter row;

		hit = g_ptr_array_index (priv->results, i);
		gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &row, -1,
		                                   PARRILLADA_SEARCH_TREE_HIT_COL, hit,
		                                   -1);
	}

	return TRUE;
}

static gboolean
parrillada_search_tracker_query_set_scope (ParrilladaSearchEngine *search,
                                        ParrilladaSearchScope scope)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);
	priv->scope = scope;
	return TRUE;
}

static gboolean
parrillada_search_tracker_set_query_mime (ParrilladaSearchEngine *search,
				       const gchar **mimes)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);

	if (priv->mimes) {
		g_strfreev (priv->mimes);
		priv->mimes = NULL;
	}

	priv->mimes = g_strdupv ((gchar **) mimes);
	return TRUE;
}

static void
parrillada_search_tracker_clean (ParrilladaSearchTracker *search)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);

	if (priv->current_call_id)
		g_cancellable_cancel (priv->cancellable);

	if (priv->results) {
		g_ptr_array_foreach (priv->results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (priv->results, TRUE);
		priv->results = NULL;
	}

	if (priv->keywords) {
		g_free (priv->keywords);
		priv->keywords = NULL;
	}

	if (priv->mimes) {
		g_strfreev (priv->mimes);
		priv->mimes = NULL;
	}
}

static gboolean
parrillada_search_tracker_query_new (ParrilladaSearchEngine *search,
				  const gchar *keywords)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (search);

	parrillada_search_tracker_clean (PARRILLADA_SEARCH_TRACKER (search));
	priv->keywords = g_strdup (keywords);

	return TRUE;
}

static void
parrillada_search_tracker_init_engine (ParrilladaSearchEngineIface *iface)
{
	iface->is_available = parrillada_search_tracker_is_available;
	iface->query_new = parrillada_search_tracker_query_new;
	iface->query_set_mime = parrillada_search_tracker_set_query_mime;
	iface->query_set_scope = parrillada_search_tracker_query_set_scope;
	iface->query_start = parrillada_search_tracker_query_start;

	iface->uri_from_hit = parrillada_search_tracker_uri_from_hit;
	iface->mime_from_hit = parrillada_search_tracker_mime_from_hit;
	iface->score_from_hit = parrillada_search_tracker_score_from_hit;

	iface->add_hits = parrillada_search_tracker_add_hit_to_tree;
	iface->num_hits = parrillada_search_tracker_num_hits;
}

static void
parrillada_search_tracker_init (ParrilladaSearchTracker *object)
{
	ParrilladaSearchTrackerPrivate *priv;
	GError *error = NULL;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (object);
	priv->cancellable = g_cancellable_new ();
	priv->connection = tracker_sparql_connection_get (priv->cancellable, &error);

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		g_object_unref (priv->cancellable);
		
		return;
	} else if (!priv->connection) {
		g_warning ("Could not establish a connection to Tracker, no TrackerSparqlConnection was returned");
		g_object_unref (priv->cancellable);
		
		return;
	}
}

static void
parrillada_search_tracker_finalize (GObject *object)
{
	ParrilladaSearchTrackerPrivate *priv;

	priv = PARRILLADA_SEARCH_TRACKER_PRIVATE (object);

	parrillada_search_tracker_clean (PARRILLADA_SEARCH_TRACKER (object));

	if (priv->connection) {
		g_object_unref (priv->connection);
		priv->connection = NULL;
	}

	G_OBJECT_CLASS (parrillada_search_tracker_parent_class)->finalize (object);
}

static void
parrillada_search_tracker_class_init (ParrilladaSearchTrackerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);


	g_type_class_add_private (klass, sizeof (ParrilladaSearchTrackerPrivate));

	object_class->finalize = parrillada_search_tracker_finalize;
}


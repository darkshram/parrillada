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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <fcntl.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include "parrillada-units.h"
#include "parrillada-volume.h"

#include "parrillada-track-data-cfg.h"

#include "libparrillada-marshal.h"

#include "parrillada-filtered-uri.h"

#include "parrillada-misc.h"
#include "burn-basics.h"
#include "parrillada-data-project.h"
#include "parrillada-data-tree-model.h"

typedef struct _ParrilladaTrackDataCfgPrivate ParrilladaTrackDataCfgPrivate;
struct _ParrilladaTrackDataCfgPrivate
{
	ParrilladaImageFS forced_fs;
	ParrilladaImageFS banned_fs;

	ParrilladaFileNode *autorun;
	ParrilladaFileNode *icon;
	GFile *image_file;

	ParrilladaDataTreeModel *tree;
	guint stamp;

	/* allows some sort of caching */
	GSList *grafts;
	GSList *excluded;

	guint loading;
	guint loading_remaining;
	GSList *load_errors;

	GtkIconTheme *theme;

	GSList *shown;

	gint sort_column;
	GtkSortType sort_type;

	guint joliet_rename:1;

	guint deep_directory:1;
	guint G2_files:1;
};

#define PARRILLADA_TRACK_DATA_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_DATA_CFG, ParrilladaTrackDataCfgPrivate))


static void
parrillada_track_data_cfg_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
parrillada_track_data_cfg_drag_dest_iface_init (gpointer g_iface, gpointer data);
static void
parrillada_track_data_cfg_sortable_iface_init (gpointer g_iface, gpointer data);
static void
parrillada_track_data_cfg_iface_init (gpointer g_iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ParrilladaTrackDataCfg,
			 parrillada_track_data_cfg,
			 PARRILLADA_TYPE_TRACK_DATA,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
					        parrillada_track_data_cfg_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
					        parrillada_track_data_cfg_drag_dest_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
					        parrillada_track_data_cfg_drag_source_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						parrillada_track_data_cfg_sortable_iface_init));

typedef enum {
	PARRILLADA_ROW_REGULAR		= 0,
	PARRILLADA_ROW_BOGUS
} ParrilladaFileRowType;

enum {
	AVAILABLE,
	LOADED,
	IMAGE,
	UNREADABLE,
	RECURSIVE,
	UNKNOWN,
	G2_FILE,
	ICON_CHANGED,
	NAME_COLLISION,
	DEEP_DIRECTORY,
	SOURCE_LOADED, 
	SOURCE_LOADING,
	JOLIET_RENAME_SIGNAL,
	LAST_SIGNAL
};

static gulong parrillada_track_data_cfg_signals [LAST_SIGNAL] = { 0 };


/**
 * GtkTreeModel part
 */

static guint
parrillada_track_data_cfg_get_pos_as_child (ParrilladaFileNode *node)
{
	ParrilladaFileNode *parent;
	ParrilladaFileNode *peers;
	guint pos = 0;

	if (!node)
		return 0;

	parent = node->parent;
	for (peers = PARRILLADA_FILE_NODE_CHILDREN (parent); peers; peers = peers->next) {
		if (peers == node)
			break;

		/* Don't increment when is_hidden */
		if (peers->is_hidden)
			continue;

		pos ++;
	}

	return pos;
}

static GtkTreePath *
parrillada_track_data_cfg_node_to_path (ParrilladaTrackDataCfg *self,
				     ParrilladaFileNode *node)
{
	ParrilladaTrackDataCfgPrivate *priv;
	GtkTreePath *path;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	path = gtk_tree_path_new ();
	for (; node->parent && !node->is_root; node = node->parent) {
		guint nth;

		nth = parrillada_track_data_cfg_get_pos_as_child (node);
		gtk_tree_path_prepend_index (path, nth);
	}

	return path;
}

static gboolean
parrillada_track_data_cfg_iter_parent (GtkTreeModel *model,
				    GtkTreeIter *iter,
				    GtkTreeIter *child)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == child->stamp, FALSE);
	g_return_val_if_fail (child->user_data != NULL, FALSE);

	node = child->user_data;
	if (child->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		/* This is a bogus row intended for empty directories
		 * user_data has the parent empty directory. */
		iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
		iter->user_data = child->user_data;
		iter->stamp = priv->stamp;
		return TRUE;
	}

	if (!node->parent) {
		iter->user_data = NULL;
		return FALSE;
	}

	iter->stamp = priv->stamp;
	iter->user_data = node->parent;
	iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
	return TRUE;
}

static ParrilladaFileNode *
parrillada_track_data_cfg_nth_child (ParrilladaFileNode *parent,
				  guint nth)
{
	ParrilladaFileNode *peers;
	gint pos;

	if (!parent)
		return NULL;

	peers = PARRILLADA_FILE_NODE_CHILDREN (parent);
	while (peers && peers->is_hidden)
		peers = peers->next;
		
	for (pos = 0; pos < nth && peers; pos ++) {
		peers = peers->next;

		/* Skip hidden */
		while (peers && peers->is_hidden)
			peers = peers->next;
	}

	return peers;
}

static gboolean
parrillada_track_data_cfg_iter_nth_child (GtkTreeModel *model,
				       GtkTreeIter *iter,
				       GtkTreeIter *parent,
				       gint n)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	if (parent) {
		/* make sure that iter comes from us */
		g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
		g_return_val_if_fail (parent->user_data != NULL, FALSE);

		if (parent->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
			/* This is a bogus row intended for empty directories,
			 * it hasn't got children. */
			return FALSE;
		}

		node = parent->user_data;
	}
	else
		node = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));

	iter->user_data = parrillada_track_data_cfg_nth_child (node, n);
	if (!iter->user_data)
		return FALSE;

	iter->stamp = priv->stamp;
	iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
	return TRUE;
}

static guint
parrillada_track_data_cfg_get_n_children (const ParrilladaFileNode *node)
{
	ParrilladaFileNode *children;
	guint num = 0;

	if (!node)
		return 0;

	for (children = PARRILLADA_FILE_NODE_CHILDREN (node); children; children = children->next) {
		if (children->is_hidden)
			continue;

		num ++;
	}

	return num;
}

static gint
parrillada_track_data_cfg_iter_n_children (GtkTreeModel *model,
					 GtkTreeIter *iter)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	if (iter == NULL) {
		/* special case */
		node = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
		return parrillada_track_data_cfg_get_n_children (node);
	}

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, 0);
	g_return_val_if_fail (iter->user_data != NULL, 0);

	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS))
		return 0;

	node = iter->user_data;
	if (node->is_file)
		return 0;

	/* return at least one for the bogus row labelled "empty". */
	if (!parrillada_track_data_cfg_get_n_children (node))
		return 1;

	return parrillada_track_data_cfg_get_n_children (node);
}

static gboolean
parrillada_track_data_cfg_iter_has_child (GtkTreeModel *model,
					GtkTreeIter *iter)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		/* This is a bogus row intended for empty directories
		 * it hasn't got children */
		return FALSE;
	}

	node = iter->user_data;

	/* This is a workaround for a warning in gailtreeview.c line 2946 where
	 * gail uses the GtkTreePath and not a copy which if the node inserted
	 * declares to have children and is not expanded leads to the path being
	 * upped and therefore wrong. */
	if (node->is_inserting)
		return FALSE;

	if (node->is_loading)
		return FALSE;

	if (node->is_file)
		return FALSE;

	/* always return TRUE here when it's a directory since even if
	 * it's empty we'll add a row written empty underneath it
	 * anyway. */
	return TRUE;
}

static gboolean
parrillada_track_data_cfg_iter_children (GtkTreeModel *model,
				       GtkTreeIter *iter,
				       GtkTreeIter *parent)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	if (!parent) {
		ParrilladaFileNode *root;
		ParrilladaFileNode *node;

		/* This is for the top directory */
		root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
		if (!root)
			return FALSE;

		node = PARRILLADA_FILE_NODE_CHILDREN (root);
		while (node && node->is_hidden)
			node = node->next;

		if (!node || node->is_hidden)
			return FALSE;

		iter->user_data = node;
		iter->stamp = priv->stamp;
		iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
		return TRUE;
	}

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
	g_return_val_if_fail (parent->user_data != NULL, FALSE);

	if (parent->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		iter->user_data = NULL;
		return FALSE;
	}

	node = parent->user_data;
	if (node->is_file) {
		iter->user_data = NULL;
		return FALSE;
	}

	iter->stamp = priv->stamp;
	if (!parrillada_track_data_cfg_get_n_children (node)) {
		/* This is a directory but it hasn't got any child; yet
		 * we show a row written empty for that. Set bogus in
		 * user_data and put parent in user_data. */
		iter->user_data = parent->user_data;
		iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_BOGUS);
		return TRUE;
	}

	iter->user_data = PARRILLADA_FILE_NODE_CHILDREN (node);
	iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
	return TRUE;
}

static gboolean
parrillada_track_data_cfg_iter_next (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		/* This is a bogus row intended for empty directories
		 * user_data has the parent empty directory. It hasn't
		 * got any peer.*/
		iter->user_data = NULL;
		return FALSE;
	}

	node = iter->user_data;
	node = node->next;

	/* skip all hidden files */
	while (node && node->is_hidden)
		node = node->next;

	if (!node || node->is_hidden)
		return FALSE;

	iter->user_data = node;
	return TRUE;
}

static void
parrillada_track_data_cfg_node_shown (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	ParrilladaFileNode *node;
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);
	node = iter->user_data;

	/* Check if that's a BOGUS row. In this case that means the parent was
	 * expanded. Therefore ask vfs to increase its priority if it's loading
	 * its contents. */
	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		/* NOTE: this has to be a directory */
		/* NOTE: there is no need to check for is_loading case here
		 * since before showing its BOGUS row the tree will have shown
		 * its parent itself and therefore that's the cases that follow
		 */
		if (node->is_exploring) {
			/* the directory is being explored increase priority */
			parrillada_data_vfs_require_directory_contents (PARRILLADA_DATA_VFS (priv->tree), node);
		}

		/* Otherwise, that's simply a BOGUS row and its parent was
		 * loaded but it is empty. Nothing to do. */
		node->is_expanded = TRUE;
		return;
	}

	if (!node)
		return;

	node->is_visible ++;

	if (node->parent && !node->parent->is_root) {
		if (!node->parent->is_expanded && node->is_visible > 0) {
			GtkTreePath *treepath;

			node->parent->is_expanded = TRUE;
			treepath = gtk_tree_model_get_path (model, iter);
			gtk_tree_model_row_changed (model,
						    treepath,
						    iter);
			gtk_tree_path_free (treepath);
		}
	}

	if (node->is_imported) {
		if (node->is_fake && !node->is_file) {
			/* we don't load all nodes when importing a session do it now */
			parrillada_data_session_load_directory_contents (PARRILLADA_DATA_SESSION (priv->tree), node, NULL);
		}

		return;
	}

	if (node->is_visible > 1)
		return;

	/* NOTE: no need to see if that's a directory being explored here. If it
	 * is being explored then it has a BOGUS row and that's the above case 
	 * that is reached. */
	if (node->is_loading) {
		/* in this case have vfs to increase priority for this node */
		parrillada_data_vfs_require_node_load (PARRILLADA_DATA_VFS (priv->tree), node);
	}
	else if (!PARRILLADA_FILE_NODE_MIME (node)) {
		/* that means that file wasn't completly loaded. To save
		 * some time we delayed the detection of the mime type
		 * since that takes a lot of time. */
		parrillada_data_vfs_load_mime (PARRILLADA_DATA_VFS (priv->tree), node);
	}

	/* add the node to the visible list that is used to update the disc 
	 * share for the node (we don't want to update the whole tree).
	 * Moreover, we only want files since directories don't have space. */
	priv->shown = g_slist_prepend (priv->shown, node);
}

static void
parrillada_track_data_cfg_node_hidden (GtkTreeModel *model,
				    GtkTreeIter *iter)
{
	ParrilladaFileNode *node;
	ParrilladaTrackDataCfgPrivate *priv;

	/* if it's a BOGUS row stop here since they are not added to shown list.
	 * In the same way returns if it is a file. */
	node = iter->user_data;
	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		node->is_expanded = FALSE;
		return;
	}

	if (!node)
		return;

	node->is_visible --;
	if (node->parent && !node->parent->is_root) {
		if (node->parent->is_expanded && node->is_visible == 0) {
			GtkTreePath *treepath;
			GtkTreeIter parent_iter;

			node->parent->is_expanded = FALSE;
			treepath = parrillada_track_data_cfg_node_to_path (PARRILLADA_TRACK_DATA_CFG (model), node->parent);
			gtk_tree_model_get_iter (model, &parent_iter, treepath);
			gtk_tree_model_row_changed (model,
						    treepath,
						    &parent_iter);
			gtk_tree_path_free (treepath);
		}
	}

	if (node->is_imported)
		return;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* update shown list */
	if (!node->is_visible)
		priv->shown = g_slist_remove (priv->shown, node);
}

static void
parrillada_track_data_cfg_get_value (GtkTreeModel *model,
				   GtkTreeIter *iter,
				   gint column,
				   GValue *value)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaTrackDataCfg *self;
	ParrilladaFileNode *node;

	self = PARRILLADA_TRACK_DATA_CFG (model);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_if_fail (priv->stamp == iter->stamp);
	g_return_if_fail (iter->user_data != NULL);

	node = iter->user_data;

	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS)) {
		switch (column) {
		case PARRILLADA_DATA_TREE_MODEL_NAME:
			g_value_init (value, G_TYPE_STRING);
			if (node->is_exploring)
				g_value_set_string (value, _("(loading…)"));
			else
				g_value_set_string (value, _("Empty"));

			return;

		case PARRILLADA_DATA_TREE_MODEL_MIME_DESC:
		case PARRILLADA_DATA_TREE_MODEL_MIME_ICON:
		case PARRILLADA_DATA_TREE_MODEL_SIZE:
			g_value_init (value, G_TYPE_STRING);
			g_value_set_string (value, NULL);
			return;

		case PARRILLADA_DATA_TREE_MODEL_SHOW_PERCENT:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case PARRILLADA_DATA_TREE_MODEL_PERCENT:
			g_value_init (value, G_TYPE_INT);
			g_value_set_int (value, 0);
			return;

		case PARRILLADA_DATA_TREE_MODEL_STYLE:
			g_value_init (value, PANGO_TYPE_STYLE);
			g_value_set_enum (value, PANGO_STYLE_ITALIC);
			return;

		case PARRILLADA_DATA_TREE_MODEL_EDITABLE:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;
	
		case PARRILLADA_DATA_TREE_MODEL_COLOR:
			g_value_init (value, G_TYPE_STRING);
			if (node->is_imported)
				g_value_set_string (value, "grey50");
			else
				g_value_set_string (value, NULL);
			return;

		case PARRILLADA_DATA_TREE_MODEL_IS_FILE:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case PARRILLADA_DATA_TREE_MODEL_IS_LOADING:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case PARRILLADA_DATA_TREE_MODEL_IS_IMPORTED:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case PARRILLADA_DATA_TREE_MODEL_URI:
			g_value_init (value, G_TYPE_STRING);
			g_value_set_string (value, NULL);
			return;

		default:
			return;
		}

		return;
	}

	switch (column) {
	case PARRILLADA_DATA_TREE_MODEL_EDITABLE:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, (node->is_imported == FALSE)/* && node->is_selected*/);
		return;

	case PARRILLADA_DATA_TREE_MODEL_NAME: {
		gchar *filename;

		g_value_init (value, G_TYPE_STRING);
		filename = g_filename_to_utf8 (PARRILLADA_FILE_NODE_NAME (node),
					       -1,
					       NULL,
					       NULL,
					       NULL);
		if (!filename)
			filename = parrillada_utils_validate_utf8 (PARRILLADA_FILE_NODE_NAME (node));

		if (filename)
			g_value_set_string (value, filename);
		else	/* Glib section on g_convert advise to use a string like
			 * "Invalid Filename". */
			g_value_set_string (value, PARRILLADA_FILE_NODE_NAME (node));

		g_free (filename);
		return;
	}

	case PARRILLADA_DATA_TREE_MODEL_MIME_DESC:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, _("(loading…)"));
		else if (!node->is_file) {
			gchar *description;

			description = g_content_type_get_description ("inode/directory");
			g_value_set_string (value, description);
			g_free (description);
		}
		else if (node->is_imported)
			g_value_set_string (value, _("Disc file"));
		else if (!PARRILLADA_FILE_NODE_MIME (node))
			g_value_set_string (value, _("(loading…)"));
		else {
			gchar *description;

			description = g_content_type_get_description (PARRILLADA_FILE_NODE_MIME (node));
			g_value_set_string (value, description);
			g_free (description);
		}

		return;

	case PARRILLADA_DATA_TREE_MODEL_MIME_ICON:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, "image-loading");
		else if (!node->is_file) {
			/* Here we have two states collapsed and expanded */
			if (node->is_expanded)
				g_value_set_string (value, "folder-open");
			else if (node->is_imported)
				/* that's for all the imported folders */
				g_value_set_string (value, "folder-visiting");
			else
				g_value_set_string (value, "folder");
		}
		else if (node->is_imported) {
			g_value_set_string (value, "media-cdrom");
		}
		else if (PARRILLADA_FILE_NODE_MIME (node)) {
			const gchar *icon_string = "text-x-preview";
			GIcon *icon;

			/* NOTE: implemented in glib 2.15.6 (not for windows though) */
			icon = g_content_type_get_icon (PARRILLADA_FILE_NODE_MIME (node));
			if (G_IS_THEMED_ICON (icon)) {
				const gchar * const *names = NULL;

				names = g_themed_icon_get_names (G_THEMED_ICON (icon));
				if (names) {
					gint i;

					for (i = 0; names [i]; i++) {
						if (gtk_icon_theme_has_icon (priv->theme, names [i])) {
							icon_string = names [i];
							break;
						}
					}
				}
			}

			g_value_set_string (value, icon_string);
			g_object_unref (icon);
		}
		else
			g_value_set_string (value, "image-loading");

		return;

	case PARRILLADA_DATA_TREE_MODEL_SIZE:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, _("(loading…)"));
		else if (!node->is_file) {
			guint nb_items;

			if (node->is_exploring) {
				g_value_set_string (value, _("(loading…)"));
				return;
			}

			nb_items = parrillada_track_data_cfg_get_n_children (node);
			if (!nb_items)
				g_value_set_string (value, _("Empty"));
			else {
				gchar *text;

				text = g_strdup_printf (ngettext ("%d item", "%d items", nb_items), nb_items);
				g_value_set_string (value, text);
				g_free (text);
			}
		}
		else {
			gchar *text;

			text = g_format_size_for_display (PARRILLADA_FILE_NODE_SECTORS (node) * 2048);
			g_value_set_string (value, text);
			g_free (text);
		}

		return;

	case PARRILLADA_DATA_TREE_MODEL_SHOW_PERCENT:
		g_value_init (value, G_TYPE_BOOLEAN);
		if (node->is_imported || node->is_loading)
			g_value_set_boolean (value, FALSE);
		else
			g_value_set_boolean (value, TRUE);

		return;

	case PARRILLADA_DATA_TREE_MODEL_PERCENT:
		g_value_init (value, G_TYPE_INT);
		if (!node->is_imported && !parrillada_data_vfs_is_active (PARRILLADA_DATA_VFS (priv->tree))) {
			gint64 sectors;
			goffset node_sectors;

			sectors = parrillada_data_project_get_sectors (PARRILLADA_DATA_PROJECT (priv->tree));

			if (!node->is_file)
				node_sectors = parrillada_data_project_get_folder_sectors (PARRILLADA_DATA_PROJECT (priv->tree), node);
			else
				node_sectors = PARRILLADA_FILE_NODE_SECTORS (node);

			if (sectors)
				g_value_set_int (value, MAX (0, MIN (node_sectors * 100 / sectors, 100)));
			else
				g_value_set_int (value, 0);
		}
		else
			g_value_set_int (value, 0);

		return;

	case PARRILLADA_DATA_TREE_MODEL_STYLE:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node->is_imported)
			g_value_set_enum (value, PANGO_STYLE_ITALIC);

		return;

	case PARRILLADA_DATA_TREE_MODEL_COLOR:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_imported)
			g_value_set_string (value, "grey50");

		return;

	case PARRILLADA_DATA_TREE_MODEL_URI: {
		gchar *uri;

		g_value_init (value, G_TYPE_STRING);
		uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), node);
		g_value_set_string (value, uri);
		g_free (uri);
		return;
	}

	case PARRILLADA_DATA_TREE_MODEL_IS_FILE:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_file);
		return;

	case PARRILLADA_DATA_TREE_MODEL_IS_LOADING:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_loading);
		return;

	case PARRILLADA_DATA_TREE_MODEL_IS_IMPORTED:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_imported);
		return;

	default:
		return;
	}

	return;
}

static GtkTreePath *
parrillada_track_data_cfg_get_path (GtkTreeModel *model,
				 GtkTreeIter *iter)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;
	GtkTreePath *path;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	node = iter->user_data;

	/* NOTE: there is only one single node without a name: root */
	path = parrillada_track_data_cfg_node_to_path (PARRILLADA_TRACK_DATA_CFG (model), node);

	/* Add index 0 for empty bogus row */
	if (iter->user_data2 == GINT_TO_POINTER (PARRILLADA_ROW_BOGUS))
		gtk_tree_path_append_index (path, 0);

	return path;
}

static ParrilladaFileNode *
parrillada_track_data_cfg_path_to_node (ParrilladaTrackDataCfg *self,
				     GtkTreePath *path)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;
	gint *indices;
	guint depth;
	guint i;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	node = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	for (i = 0; i < depth; i ++) {
		ParrilladaFileNode *parent;

		parent = node;
		node = parrillada_track_data_cfg_nth_child (parent, indices [i]);
		if (!node)
			return NULL;
	}
	return node;
}

static gboolean
parrillada_track_data_cfg_get_iter (GtkTreeModel *model,
				 GtkTreeIter *iter,
				 GtkTreePath *path)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *root;
	ParrilladaFileNode *node;
	const gint *indices;
	guint depth;
	guint i;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (model);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	/* NOTE: if we're in reset, then root won't exist anymore */
	if (!root)
		return FALSE;
		
	node = parrillada_track_data_cfg_nth_child (root, indices [0]);
	if (!node)
		return FALSE;

	for (i = 1; i < depth; i ++) {
		ParrilladaFileNode *parent;

		parent = node;
		node = parrillada_track_data_cfg_nth_child (parent, indices [i]);
		if (!node) {
			/* There is one case where this can happen and
			 * is allowed: that's when the parent is an
			 * empty directory. Then index must be 0. */
			if (!parent->is_file
			&&  !parrillada_track_data_cfg_get_n_children (parent)
			&&   indices [i] == 0) {
				iter->stamp = priv->stamp;
				iter->user_data = parent;
				iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_BOGUS);
				return TRUE;
			}

			iter->user_data = NULL;
			return FALSE;
		}
	}

	iter->user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);
	iter->stamp = priv->stamp;
	iter->user_data = node;

	return TRUE;
}

static GType
parrillada_track_data_cfg_get_column_type (GtkTreeModel *model,
					 gint index)
{
	switch (index) {
	case PARRILLADA_DATA_TREE_MODEL_NAME:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_URI:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_MIME_DESC:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_MIME_ICON:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_SIZE:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_SHOW_PERCENT:
		return G_TYPE_BOOLEAN;

	case PARRILLADA_DATA_TREE_MODEL_PERCENT:
		return G_TYPE_INT;

	case PARRILLADA_DATA_TREE_MODEL_STYLE:
		return PANGO_TYPE_STYLE;

	case PARRILLADA_DATA_TREE_MODEL_COLOR:
		return G_TYPE_STRING;

	case PARRILLADA_DATA_TREE_MODEL_EDITABLE:
		return G_TYPE_BOOLEAN;

	case PARRILLADA_DATA_TREE_MODEL_IS_FILE:
		return G_TYPE_BOOLEAN;

	case PARRILLADA_DATA_TREE_MODEL_IS_LOADING:
		return G_TYPE_BOOLEAN;

	case PARRILLADA_DATA_TREE_MODEL_IS_IMPORTED:
		return G_TYPE_BOOLEAN;

	default:
		break;
	}

	return G_TYPE_INVALID;
}

static gint
parrillada_track_data_cfg_get_n_columns (GtkTreeModel *model)
{
	return PARRILLADA_DATA_TREE_MODEL_COL_NUM;
}

static GtkTreeModelFlags
parrillada_track_data_cfg_get_flags (GtkTreeModel *model)
{
	return 0;
}

static gboolean
parrillada_track_data_cfg_row_draggable (GtkTreeDragSource *drag_source,
				      GtkTreePath *treepath)
{
	ParrilladaFileNode *node;

	node = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_source), treepath);
	if (node && !node->is_imported)
		return TRUE;

	return FALSE;
}

static gboolean
parrillada_track_data_cfg_drag_data_get (GtkTreeDragSource *drag_source,
				      GtkTreePath *treepath,
				      GtkSelectionData *selection_data)
{
	if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern (PARRILLADA_DND_TARGET_DATA_TRACK_REFERENCE_LIST, TRUE)) {
		GtkTreeRowReference *reference;

		reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (drag_source), treepath);
		gtk_selection_data_set (selection_data,
					gdk_atom_intern_static_string (PARRILLADA_DND_TARGET_DATA_TRACK_REFERENCE_LIST),
					8,
					(void *) g_list_prepend (NULL, reference),
					sizeof (GList));
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
parrillada_track_data_cfg_drag_data_delete (GtkTreeDragSource *drag_source,
					 GtkTreePath *path)
{
	/* NOTE: it's not the data in the selection_data here that should be
	 * deleted but rather the rows selected when there is a move. FALSE
	 * here means that we didn't delete anything. */
	/* return TRUE to stop other handlers */
	return TRUE;
}

static gboolean
parrillada_track_data_cfg_drag_data_received (GtkTreeDragDest *drag_dest,
					   GtkTreePath *dest_path,
					   GtkSelectionData *selection_data)
{
	ParrilladaFileNode *node;
	ParrilladaFileNode *parent;
	GtkTreePath *dest_parent;
	GdkAtom target;
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (drag_dest);

	/* NOTE: dest_path is the path to insert before; so we may not have a 
	 * valid path if it's in an empty directory */

	dest_parent = gtk_tree_path_copy (dest_path);
	gtk_tree_path_up (dest_parent);
	parent = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_dest), dest_parent);
	if (!parent) {
		gtk_tree_path_up (dest_parent);
		parent = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_dest), dest_parent);
	}
	else if (parent->is_file)
		parent = parent->parent;

	gtk_tree_path_free (dest_parent);

	target = gtk_selection_data_get_target (selection_data);
	/* Received data: see where it comes from:
	 * - from us, then that's a simple move
	 * - from another widget then it's going to be URIS and we add
	 *   them to the DataProject */
	if (target == gdk_atom_intern (PARRILLADA_DND_TARGET_DATA_TRACK_REFERENCE_LIST, TRUE)) {
		GList *iter;

		iter = (GList *) gtk_selection_data_get_data (selection_data);

		/* That's us: move the row and its children. */
		for (; iter; iter = iter->next) {
			GtkTreeRowReference *reference;
			GtkTreeModel *src_model;
			GtkTreePath *treepath;

			reference = iter->data;
			src_model = gtk_tree_row_reference_get_model (reference);
			if (src_model != GTK_TREE_MODEL (drag_dest))
				continue;

			treepath = gtk_tree_row_reference_get_path (reference);

			node = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_dest), treepath);
			gtk_tree_path_free (treepath);

			parrillada_data_project_move_node (PARRILLADA_DATA_PROJECT (priv->tree), node, parent);
		}
	}
	else if (target == gdk_atom_intern ("text/uri-list", TRUE)) {
		gint i;
		gchar **uris;
		gboolean success = FALSE;

		/* NOTE: there can be many URIs at the same time. One
		 * success is enough to return TRUE. */
		success = FALSE;
		uris = gtk_selection_data_get_uris (selection_data);
		if (!uris) {
			const guchar *selection_data_raw;

			selection_data_raw = gtk_selection_data_get_data (selection_data);
			uris = g_uri_list_extract_uris ((gchar *) selection_data_raw);
		}

		if (!uris)
			return TRUE;

		for (i = 0; uris [i]; i ++) {
			ParrilladaFileNode *node;

			/* Add the URIs to the project */
			node = parrillada_data_project_add_loading_node (PARRILLADA_DATA_PROJECT (priv->tree),
								      uris [i],
								      parent);
			if (node)
				success = TRUE;
		}
		g_strfreev (uris);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
parrillada_track_data_cfg_row_drop_possible (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data)
{
	GdkAtom target;

	target = gtk_selection_data_get_target (selection_data);
	/* See if we are dropping to ourselves */
	if (target == gdk_atom_intern_static_string (PARRILLADA_DND_TARGET_DATA_TRACK_REFERENCE_LIST)) {
		GtkTreePath *dest_parent;
		ParrilladaFileNode *parent;
		GList *iter;

		iter = (GList *) gtk_selection_data_get_data (selection_data);

		/* make sure the parent is a directory.
		 * NOTE: in this case dest_path is the exact path where it
		 * should be inserted. */
		dest_parent = gtk_tree_path_copy (dest_path);
		gtk_tree_path_up (dest_parent);

		parent = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_dest), dest_parent);

		if (!parent) {
			/* See if that isn't a BOGUS row; if so, try with parent */
			gtk_tree_path_up (dest_parent);
			parent = parrillada_track_data_cfg_path_to_node (PARRILLADA_TRACK_DATA_CFG (drag_dest), dest_parent);

			if (!parent) {
				gtk_tree_path_free (dest_parent);
				return FALSE;
			}
		}
		else if (parent->is_file) {
			/* if that's a file try with parent */
			gtk_tree_path_up (dest_parent);
			parent = parent->parent;
		}

		if (parent->is_loading) {
			gtk_tree_path_free (dest_parent);
			return FALSE;
		}

		for (; iter; iter = iter->next) {
			GtkTreePath *src_path;
			GtkTreeModel *src_model;
			GtkTreeRowReference *reference;

			reference = iter->data;
			src_model = gtk_tree_row_reference_get_model (reference);
			if (src_model != GTK_TREE_MODEL (drag_dest))
				continue;

			src_path = gtk_tree_row_reference_get_path (reference);

			/* see if we are not moving a parent to one of its children */
			if (gtk_tree_path_is_ancestor (src_path, dest_path)) {
				gtk_tree_path_free (src_path);
				continue;
			}

			if (gtk_tree_path_up (src_path)) {
				/* check that node was moved to another directory */
				if (!parent->parent) {
					if (gtk_tree_path_get_depth (src_path)) {
						gtk_tree_path_free (src_path);
						gtk_tree_path_free (dest_parent);
						return TRUE;
					}
				}
				else if (!gtk_tree_path_get_depth (src_path)
				     ||   gtk_tree_path_compare (src_path, dest_parent)) {
					gtk_tree_path_free (src_path);
					gtk_tree_path_free (dest_parent);
					return TRUE;
				}
			}

			gtk_tree_path_free (src_path);
		}

		gtk_tree_path_free (dest_parent);
		return FALSE;
	}
	else if (target == gdk_atom_intern_static_string ("text/uri-list"))
		return TRUE;

	return FALSE;
}

/**
 * Sorting part
 */

static gboolean
parrillada_track_data_cfg_get_sort_column_id (GtkTreeSortable *sortable,
					   gint *column,
					   GtkSortType *type)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (sortable);

	if (column)
		*column = priv->sort_column;

	if (type)
		*type = priv->sort_type;

	return TRUE;
}

static void
parrillada_track_data_cfg_set_sort_column_id (GtkTreeSortable *sortable,
					   gint column,
					   GtkSortType type)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (sortable);
	priv->sort_column = column;
	priv->sort_type = type;

	switch (column) {
	case PARRILLADA_DATA_TREE_MODEL_NAME:
		parrillada_data_project_set_sort_function (PARRILLADA_DATA_PROJECT (priv->tree),
							type,
							parrillada_file_node_sort_name_cb);
		break;
	case PARRILLADA_DATA_TREE_MODEL_SIZE:
		parrillada_data_project_set_sort_function (PARRILLADA_DATA_PROJECT (priv->tree),
							type,
							parrillada_file_node_sort_size_cb);
		break;
	case PARRILLADA_DATA_TREE_MODEL_MIME_DESC:
		parrillada_data_project_set_sort_function (PARRILLADA_DATA_PROJECT (priv->tree),
							type,
							parrillada_file_node_sort_mime_cb);
		break;
	default:
		parrillada_data_project_set_sort_function (PARRILLADA_DATA_PROJECT (priv->tree),
							type,
							parrillada_file_node_sort_default_cb);
		break;
	}

	gtk_tree_sortable_sort_column_changed (sortable);
}

static gboolean
parrillada_track_data_cfg_has_default_sort_func (GtkTreeSortable *sortable)
{
	/* That's always true since we sort files and directories */
	return TRUE;
}


static ParrilladaFileNode *
parrillada_track_data_cfg_autorun_inf_parse (ParrilladaTrackDataCfg *track,
					  const gchar *uri)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *root;
	ParrilladaFileNode *node;
	GKeyFile *key_file;
	gchar *icon_path;
	gchar *path;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (!uri)
		return FALSE;

	path = g_filename_from_uri (uri, NULL, NULL);
	key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL)) {
		g_key_file_free (key_file);
		g_free (path);
		return NULL;
	}
	g_free (path);

	/* NOTE: icon_path is the ON DISC path of the icon */
	icon_path = g_key_file_get_value (key_file, "autorun", "icon", NULL);
	g_key_file_free (key_file);

	if (icon_path && icon_path [0] == '\0') {
		g_free (icon_path);
		return NULL;
	}

	/* Get the node (hope it already exists) */
	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	node = parrillada_file_node_get_from_path (root, icon_path);
	if (node) {
		g_free (icon_path);
		return node;
	}

	/* Add a virtual node to get warned when/if the icon is added to the tree */
	node = parrillada_data_project_watch_path (PARRILLADA_DATA_PROJECT (priv->tree), icon_path);
	g_free (icon_path);

	return node;
}

static void
parrillada_track_data_cfg_clean_cache (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->grafts) {
		g_slist_foreach (priv->grafts, (GFunc) parrillada_graft_point_free, NULL);
		g_slist_free (priv->grafts);
		priv->grafts = NULL;
	}

	if (priv->excluded) {
		g_slist_foreach (priv->excluded, (GFunc) g_free, NULL);
		g_slist_free (priv->excluded);
		priv->excluded = NULL;
	}
}

static void
parrillada_track_data_cfg_node_added (ParrilladaDataProject *project,
				   ParrilladaFileNode *node,
				   ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *parent;
	GtkTreePath *path;
	GtkTreeIter iter;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->icon == node) {
		/* Our icon node has showed up, signal that */
		g_signal_emit (self,
			       parrillada_track_data_cfg_signals [ICON_CHANGED],
			       0);
	}

	/* Check if the parent is root */
	if (node->parent->is_root) {
		if (!strcasecmp (PARRILLADA_FILE_NODE_NAME (node), "autorun.inf")) {
			gchar *uri;

			
			/* This has been added by the user or by a project so
			 * we do display it; also we signal the change in icon.
			 * NOTE: if we had our own autorun.inf it was wiped out
			 * in the callback for "name-collision". */
			uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), node);
			if (!uri) {
				/* URI can be NULL sometimes if the autorun.inf is from
				 * the session of the imported medium */
				priv->icon = parrillada_track_data_cfg_autorun_inf_parse (self, uri);
				g_free (uri);
			}

			g_signal_emit (self,
				       parrillada_track_data_cfg_signals [ICON_CHANGED],
				       0);
		}
	}

	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);

	path = parrillada_track_data_cfg_node_to_path (self, node);

	/* if the node is reloading (because of a file system change or because
	 * it was a node that was a tmp folder) then no need to signal an added
	 * signal but a changed one */
	if (node->is_reloading) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
		return;
	}

	/* Add the row itself */
	/* This is a workaround for a warning in gailtreeview.c line 2946 where
	 * gail uses the GtkTreePath and not a copy which if the node inserted
	 * declares to have children and is not expanded leads to the path being
	 * upped and therefore wrong. */
	node->is_inserting = 1;
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (self),
				     path,
				     &iter);
	node->is_inserting = 0;
	gtk_tree_path_free (path);

	parent = node->parent;
	if (!parent->is_root) {
		/* Tell the tree that the parent changed (since the number of children
		 * changed as well). */
		iter.user_data = parent;
		path = parrillada_track_data_cfg_node_to_path (self, parent);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), path, &iter);

		/* Check if the parent of this node is empty if so remove the BOGUS row.
		 * Do it afterwards to prevent the parent row to be collapsed if it was
		 * previously expanded. */
		if (parent && parrillada_track_data_cfg_get_n_children (parent) == 1) {
			gtk_tree_path_append_index (path, 1);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), path);
		}

		gtk_tree_path_free (path);
	}

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file && !node->is_loading) {
		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		path = parrillada_track_data_cfg_node_to_path (self, node);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
	}

	/* we also have to set the is_visible property as all nodes added to 
	 * root are always visible but ref_node is not necessarily called on
	 * these nodes. */
//	if (parent->is_root)
//		node->is_visible = TRUE;
}

static guint
parrillada_track_data_cfg_convert_former_position (ParrilladaFileNode *former_parent,
                                                guint former_position)
{
	ParrilladaFileNode *child;
	guint hidden_num = 0;
	guint current_pos;

	/* We need to convert former_position to the right position in the
	 * treeview as there could be some hidden node before this position. */
	child = PARRILLADA_FILE_NODE_CHILDREN (former_parent);

	for (current_pos = 0; child && current_pos != former_position; current_pos ++) {
		if (child->is_hidden)
			hidden_num ++;

		child = child->next;
	}

	return former_position - hidden_num;
}

static void
parrillada_track_data_cfg_node_removed (ParrilladaDataProject *project,
				     ParrilladaFileNode *former_parent,
				     guint former_position,
				     ParrilladaFileNode *node,
				     ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	GSList *iter, *next;
	GtkTreePath *path;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);
	/* NOTE: there is no special case of autorun.inf here when we created
	 * it as a temprary file since it's hidden and ParrilladaDataTreeModel
	 * won't emit a signal for removed file in this case.
	 * On the other hand we check for a node at root of the CD called
	 * "autorun.inf" just in case an autorun added by the user would be
	 * removed. Do it also for the icon node. */
	if (former_parent->is_root) {
		if (!strcasecmp (PARRILLADA_FILE_NODE_NAME (node), "autorun.inf")) {
			priv->icon = NULL;
			g_signal_emit (self,
				       parrillada_track_data_cfg_signals [ICON_CHANGED],
				       0);
		}
		else if (priv->icon == node
		     || (priv->icon && !priv->autorun && parrillada_file_node_is_ancestor (node, priv->icon))) {
			/* This icon had been added by the user. Do nothing but
			 * register that the icon is no more on the disc */
			priv->icon = NULL;
			g_signal_emit (self,
				       parrillada_track_data_cfg_signals [ICON_CHANGED],
				       0);
		}
	}
	
	/* remove it from the shown list and all its children as well */
	priv->shown = g_slist_remove (priv->shown, node);
	for (iter = priv->shown; iter; iter = next) {
		ParrilladaFileNode *tmp;

		tmp = iter->data;
		next = iter->next;
		if (parrillada_file_node_is_ancestor (node, tmp))
			priv->shown = g_slist_remove (priv->shown, tmp);
	}

	/* See if the parent of this node still has children. If not we need to
	 * add a bogus row. If it hasn't got children then it only remains our
	 * node in the list.
	 * NOTE: parent has to be a directory. */
	if (!former_parent->is_root && !parrillada_track_data_cfg_get_n_children (former_parent)) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = former_parent;
		iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_BOGUS);

		path = parrillada_track_data_cfg_node_to_path (self, former_parent);
		gtk_tree_path_append_index (path, 1);

		gtk_tree_model_row_inserted (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
	}

	/* remove the node. Do it after adding a possible BOGUS row.
	 * NOTE since BOGUS row has been added move row. */
	path = parrillada_track_data_cfg_node_to_path (self, former_parent);
	gtk_tree_path_append_index (path, parrillada_track_data_cfg_convert_former_position (former_parent, former_position));

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), path);
	gtk_tree_path_free (path);
}

static void
parrillada_track_data_cfg_node_changed (ParrilladaDataProject *project,
				     ParrilladaFileNode *node,
				     ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	/* Get the iter for the node */
	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);

	path = parrillada_track_data_cfg_node_to_path (self, node);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (self),
				    path,
				    &iter);

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file) {
		/* NOTE: No need to check for the number of children ... */

		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (self),
						      path,
						      &iter);

		/* The problem is that without that, the folder contents on disc
		 * won't be added to the tree if the node it replaced was
		 * already visible. */
		if (node->is_imported
		&&  node->is_visible
		&&  node->is_fake)
			parrillada_data_session_load_directory_contents (PARRILLADA_DATA_SESSION (project),
								      node,
								      NULL);

		/* add the row */
		if (!parrillada_track_data_cfg_get_n_children (node))  {
			iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_BOGUS);
			gtk_tree_path_append_index (path, 0);

			gtk_tree_model_row_inserted (GTK_TREE_MODEL (self),
						     path,
						     &iter);
		}
	}
	gtk_tree_path_free (path);
}

static void
parrillada_track_data_cfg_node_reordered (ParrilladaDataProject *project,
				       ParrilladaFileNode *parent,
				       gint *new_order,
				       ParrilladaTrackDataCfg *self)
{
	GtkTreePath *treepath;
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	treepath = parrillada_track_data_cfg_node_to_path (self, parent);
	if (parent != parrillada_data_project_get_root (project)) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = parent;
		iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);

		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (self),
					       treepath,
					       &iter,
					       new_order);
	}
	else
		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (self),
					       treepath,
					       NULL,
					       new_order);

	gtk_tree_path_free (treepath);
}

static void
parrillada_track_data_clean_autorun (ParrilladaTrackDataCfg *track)
{
	gchar *uri;
	gchar *path;
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->image_file) {
		g_object_unref (priv->image_file);
		priv->image_file = NULL;
	}

	if (priv->autorun) {
		/* ONLY remove icon if it's our own */
		if (priv->icon) {
			uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), priv->icon);
			path = g_filename_from_uri (uri, NULL, NULL);
			g_free (uri);
			g_remove (path);
			g_free (path);
			priv->icon = NULL;
		}

		uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), priv->autorun);
		path = g_filename_from_uri (uri, NULL, NULL);
		g_free (uri);
		g_remove (path);
		g_free (path);
		priv->autorun = NULL;
	}
	else
		priv->icon = NULL;
}

static void
parrillada_track_data_cfg_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeModelIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->ref_node = parrillada_track_data_cfg_node_shown;
	iface->unref_node = parrillada_track_data_cfg_node_hidden;

	iface->get_flags = parrillada_track_data_cfg_get_flags;
	iface->get_n_columns = parrillada_track_data_cfg_get_n_columns;
	iface->get_column_type = parrillada_track_data_cfg_get_column_type;
	iface->get_iter = parrillada_track_data_cfg_get_iter;
	iface->get_path = parrillada_track_data_cfg_get_path;
	iface->get_value = parrillada_track_data_cfg_get_value;
	iface->iter_next = parrillada_track_data_cfg_iter_next;
	iface->iter_children = parrillada_track_data_cfg_iter_children;
	iface->iter_has_child = parrillada_track_data_cfg_iter_has_child;
	iface->iter_n_children = parrillada_track_data_cfg_iter_n_children;
	iface->iter_nth_child = parrillada_track_data_cfg_iter_nth_child;
	iface->iter_parent = parrillada_track_data_cfg_iter_parent;
}

static void
parrillada_track_data_cfg_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->row_draggable = parrillada_track_data_cfg_row_draggable;
	iface->drag_data_get = parrillada_track_data_cfg_drag_data_get;
	iface->drag_data_delete = parrillada_track_data_cfg_drag_data_delete;
}

static void
parrillada_track_data_cfg_drag_dest_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragDestIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_received = parrillada_track_data_cfg_drag_data_received;
	iface->row_drop_possible = parrillada_track_data_cfg_row_drop_possible;
}

static void
parrillada_track_data_cfg_sortable_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeSortableIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->get_sort_column_id = parrillada_track_data_cfg_get_sort_column_id;
	iface->set_sort_column_id = parrillada_track_data_cfg_set_sort_column_id;
	iface->has_default_sort_func = parrillada_track_data_cfg_has_default_sort_func;
}

/**
 * Track part
 */

/**
 * parrillada_track_data_cfg_add:
 * @track: a #ParrilladaTrackDataCfg
 * @uri: a #gchar
 * @parent: a #GtkTreePath or NULL
 *
 * Add a new file (with @uri as URI) under a directory (@parent).
 * If @parent is NULL, the file is added to the root.
 * Also if @uri is the path of a directory, this directory will be explored
 * and all its children added to the tree.
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_add (ParrilladaTrackDataCfg *track,
			    const gchar *uri,
			    GtkTreePath *parent)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *parent_node;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->loading)
		return FALSE;

	if (parent) {
		parent_node = parrillada_track_data_cfg_path_to_node (track, parent);
		if (parent_node && (parent_node->is_file || parent_node->is_loading))
			parent_node = parent_node->parent;
	}
	else
		parent_node = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));

	return (parrillada_data_project_add_loading_node (PARRILLADA_DATA_PROJECT (PARRILLADA_DATA_PROJECT (priv->tree)), uri, parent_node) != NULL);
}

/**
 * parrillada_track_data_cfg_add_empty_directory:
 * @track: a #ParrilladaTrackDataCfg
 * @name: a #gchar
 * @parent: a #GtkTreePath or NULL
 *
 * Add a new empty directory (with @name as name) under another directory (@parent).
 * If @parent is NULL, the file is added to the root.
 *
 * Return value: a #GtkTreePath which should be destroyed when not needed; NULL if the operation was not successful.
 **/

GtkTreePath *
parrillada_track_data_cfg_add_empty_directory (ParrilladaTrackDataCfg *track,
					    const gchar *name,
					    GtkTreePath *parent)
{
	ParrilladaFileNode *parent_node = NULL;
	ParrilladaTrackDataCfgPrivate *priv;
	gchar *default_name = NULL;
	ParrilladaFileNode *node;
	GtkTreePath *path;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return NULL;

	if (parent) {
		parent_node = parrillada_track_data_cfg_path_to_node (track, parent);
		if (parent_node && (parent_node->is_file || parent_node->is_loading))
			parent_node = parent_node->parent;
	}

	if (!parent_node)
		parent_node = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));

	if (!name) {
		guint nb = 1;

		default_name = g_strdup_printf (_("New folder"));
		while (parrillada_file_node_check_name_existence (parent_node, default_name)) {
			g_free (default_name);
			default_name = g_strdup_printf (_("New folder %i"), nb);
			nb++;
		}

	}

	node = parrillada_data_project_add_empty_directory (PARRILLADA_DATA_PROJECT (priv->tree),
							 name? name:default_name,
							 parent_node);
	if (default_name)
		g_free (default_name);

	if (!node)
		return NULL;

	path = parrillada_track_data_cfg_node_to_path (track, node);
	if (path)
		parrillada_track_changed (PARRILLADA_TRACK (track));

	return path;
}

/**
 * parrillada_track_data_cfg_remove:
 * @track: a #ParrilladaTrackDataCfg
 * @treepath: a #GtkTreePath
 *
 * Removes a file or a directory (as well as its children) from the tree.
 * NOTE: some files cannot be removed like files from an imported session.
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_remove (ParrilladaTrackDataCfg *track,
			       GtkTreePath *treepath)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return FALSE;

	node = parrillada_track_data_cfg_path_to_node (track, treepath);
	if (!node)
		return FALSE;

	parrillada_data_project_remove_node (PARRILLADA_DATA_PROJECT (priv->tree), node);
	return TRUE;
}

/**
 * parrillada_track_data_cfg_rename:
 * @track: a #ParrilladaTrackDataCfg
 * @newname: a #gchar
 * @treepath: a #GtkTreePath
 *
 * Renames the file in the tree pointed by @treepath.
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_rename (ParrilladaTrackDataCfg *track,
			       const gchar *newname,
			       GtkTreePath *treepath)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *node;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	node = parrillada_track_data_cfg_path_to_node (track, treepath);
	return parrillada_data_project_rename_node (PARRILLADA_DATA_PROJECT (priv->tree),
						 node,
						 newname);
}

/**
 * parrillada_track_data_cfg_reset:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Completely empties @track and unloads any currently loaded session
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_reset (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *root;
	GtkTreePath *treepath;
	guint num;
	guint i;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return FALSE;

	/* Do it now */
	parrillada_track_data_clean_autorun (track);

	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	num = parrillada_track_data_cfg_get_n_children (root);

	parrillada_data_project_reset (PARRILLADA_DATA_PROJECT (priv->tree));

	treepath = gtk_tree_path_new_first ();
	for (i = 0; i < num; i++)
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (track), treepath);
	gtk_tree_path_free (treepath);

	g_slist_free (priv->shown);
	priv->shown = NULL;

	priv->G2_files = FALSE;
	priv->deep_directory = FALSE;
	priv->joliet_rename = FALSE;

	parrillada_track_data_cfg_clean_cache (track);

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return TRUE;
}

/**
 * parrillada_track_data_cfg_get_filtered_model:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Gets a GtkTreeModel which contains all the files that were
 * automatically filtered while added directories were explored.
 *
 * Return value: a #GtkTreeModel. Unref when not needed.
 **/

GtkTreeModel *
parrillada_track_data_cfg_get_filtered_model (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), NULL);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	model = GTK_TREE_MODEL (parrillada_data_vfs_get_filtered_model (PARRILLADA_DATA_VFS (priv->tree)));
	g_object_ref (model);
	return model;
}

/**
 * parrillada_track_data_cfg_restore:
 * @track: a #ParrilladaTrackDataCfg
 * @treepath: a #GtkTreePath
 *
 * Removes a file from the filtered file list (see parrillada_track_data_cfg_get_filtered_model ())
 * and re-adds it wherever it should be in the tree.
 * @treepath is a #GtkTreePath associated with the #GtkTreeModel which holds the
 * filtered files not the main tree.
 *
 **/

void
parrillada_track_data_cfg_restore (ParrilladaTrackDataCfg *track,
				GtkTreePath *treepath)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFilteredUri *filtered;
	gchar *uri;

	g_return_if_fail (PARRILLADA_TRACK_DATA_CFG (track));
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	filtered = parrillada_data_vfs_get_filtered_model (PARRILLADA_DATA_VFS (priv->tree));
	uri = parrillada_filtered_uri_restore (filtered, treepath);

	parrillada_data_project_restore_uri (PARRILLADA_DATA_PROJECT (priv->tree), uri);
	g_free (uri);
}

/**
 * parrillada_track_data_cfg_dont_filter_uri:
 * @track: a #ParrilladaTrackDataCfg
 * @uri: a #gchar
 *
 * Prevents @uri to be filtered while automatic exploration
 * of added directories is performed.
 *
 **/

void
parrillada_track_data_cfg_dont_filter_uri (ParrilladaTrackDataCfg *track,
					const gchar *uri)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFilteredUri *filtered;

	g_return_if_fail (PARRILLADA_TRACK_DATA_CFG (track));
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	filtered = parrillada_data_vfs_get_filtered_model (PARRILLADA_DATA_VFS (priv->tree));
	parrillada_filtered_uri_dont_filter (filtered, uri);
	parrillada_data_project_restore_uri (PARRILLADA_DATA_PROJECT (priv->tree), uri);
}

/**
 * parrillada_track_data_cfg_get_restored_list:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Gets a list of URIs (as #gchar *) that were restored with parrillada_track_data_cfg_restore ().
 *
 * Return value: a #GSList; free the list and its contents when not needed anymore.
 **/

GSList *
parrillada_track_data_cfg_get_restored_list (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFilteredUri *filtered;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), NULL);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	filtered = parrillada_data_vfs_get_filtered_model (PARRILLADA_DATA_VFS (priv->tree));
	return parrillada_filtered_uri_get_restored_list (filtered);
}

/**
 * parrillada_track_data_cfg_load_medium:
 * @track: a #ParrilladaTrackDataCfg
 * @medium: a #ParrilladaMedium
 * @error: a #GError
 *
 * Tries to load the contents of the last session of @medium so all its files will be included in the tree
 * to perform a merge between files from the session and new added files.
 * Errors are stored in @error.
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_load_medium (ParrilladaTrackDataCfg *track,
				    ParrilladaMedium *medium,
				    GError **error)
{
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), FALSE);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	return parrillada_data_session_add_last (PARRILLADA_DATA_SESSION (priv->tree),
					      medium,
					      error);
}

/**
 * parrillada_track_data_cfg_unload_current_medium:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Unload the contents of the last session of the currently loaded medium.
 * See parrillada_track_data_cfg_load_medium ().
 *
 **/

void
parrillada_track_data_cfg_unload_current_medium (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_if_fail (PARRILLADA_TRACK_DATA_CFG (track));
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	parrillada_data_session_remove_last (PARRILLADA_DATA_SESSION (priv->tree));
}

/**
 * parrillada_track_data_cfg_get_current_medium:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Gets the currently loaded medium.
 *
 * Return value: a #ParrilladaMedium. NULL if no medium are currently loaded.
 * Do not unref when the #ParrilladaMedium is not needed anymore.
 **/

ParrilladaMedium *
parrillada_track_data_cfg_get_current_medium (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), NULL);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	return parrillada_data_session_get_loaded_medium (PARRILLADA_DATA_SESSION (priv->tree));
}

/**
 * parrillada_track_data_cfg_get_available_media:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Gets a list of all the media that can be appended with new data and which have a session that can be loaded.
 *
 * Return value: a #GSList of #ParrilladaMedium. Free the list and unref its contents when the list is not needed anymore.
 **/

GSList *
parrillada_track_data_cfg_get_available_media (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_val_if_fail (PARRILLADA_TRACK_DATA_CFG (track), NULL);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	return parrillada_data_session_get_available_media (PARRILLADA_DATA_SESSION (priv->tree));
}

static ParrilladaBurnResult
parrillada_track_data_cfg_set_source (ParrilladaTrackData *track,
				   GSList *grafts,
				   GSList *excluded)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (!grafts)
		return PARRILLADA_BURN_ERR;

	priv->loading = parrillada_data_project_load_contents (PARRILLADA_DATA_PROJECT (priv->tree),
							    grafts,
							    excluded);

	/* Remember that we own the list grafts and excluded
	 * so we have to free them ourselves. */
	g_slist_foreach (grafts, (GFunc) parrillada_graft_point_free, NULL);
	g_slist_free (grafts);

	g_slist_foreach (excluded, (GFunc) g_free, NULL);
	g_slist_free (excluded);

	if (!priv->loading)
		return PARRILLADA_BURN_OK;

	return PARRILLADA_BURN_NOT_READY;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_add_fs (ParrilladaTrackData *track,
			       ParrilladaImageFS fstype)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	priv->forced_fs |= fstype;
	priv->banned_fs &= ~(fstype);
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_rm_fs (ParrilladaTrackData *track,
			      ParrilladaImageFS fstype)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	priv->banned_fs |= fstype;
	priv->forced_fs &= ~(fstype);
	return PARRILLADA_BURN_OK;
}

static ParrilladaImageFS
parrillada_track_data_cfg_get_fs (ParrilladaTrackData *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileTreeStats *stats;
	ParrilladaImageFS fs_type;
	ParrilladaFileNode *root;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	stats = PARRILLADA_FILE_NODE_STATS (root);

	fs_type = PARRILLADA_IMAGE_FS_ISO;
	fs_type |= priv->forced_fs;

	if (parrillada_data_project_has_symlinks (PARRILLADA_DATA_PROJECT (priv->tree)))
		fs_type |= PARRILLADA_IMAGE_FS_SYMLINK;
	else {
		/* These two are incompatible with symlinks */
		if (parrillada_data_project_is_joliet_compliant (PARRILLADA_DATA_PROJECT (priv->tree)))
			fs_type |= PARRILLADA_IMAGE_FS_JOLIET;

		if (parrillada_data_project_is_video_project (PARRILLADA_DATA_PROJECT (priv->tree)))
			fs_type |= PARRILLADA_IMAGE_FS_VIDEO;
	}

	if (stats->num_2GiB != 0) {
		fs_type |= PARRILLADA_IMAGE_ISO_FS_LEVEL_3;
		if (!(fs_type & PARRILLADA_IMAGE_FS_SYMLINK))
			fs_type |= PARRILLADA_IMAGE_FS_UDF;
	}

	if (stats->num_deep != 0)
		fs_type |= PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY;

	fs_type &= ~(priv->banned_fs);
	return fs_type;
}

static GSList *
parrillada_track_data_cfg_get_grafts (ParrilladaTrackData *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaImageFS fs_type;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->grafts)
		return priv->grafts;

	/* append a slash for mkisofs */
	fs_type = parrillada_track_data_cfg_get_fs (track);
	parrillada_data_project_get_contents (PARRILLADA_DATA_PROJECT (priv->tree),
					   &priv->grafts,
					   &priv->excluded,
					   TRUE, /* include hidden nodes */
					   (fs_type & PARRILLADA_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return priv->grafts;
}

static GSList *
parrillada_track_data_cfg_get_excluded (ParrilladaTrackData *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaImageFS fs_type;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->excluded)
		return priv->excluded;

	/* append a slash for mkisofs */
	fs_type = parrillada_track_data_cfg_get_fs (track);
	parrillada_data_project_get_contents (PARRILLADA_DATA_PROJECT (priv->tree),
					   &priv->grafts,
					   &priv->excluded,
					   TRUE, /* include hidden nodes */
					   (fs_type & PARRILLADA_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return priv->excluded;
}

static guint64
parrillada_track_data_cfg_get_file_num (ParrilladaTrackData *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileTreeStats *stats;
	ParrilladaFileNode *root;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	stats = PARRILLADA_FILE_NODE_STATS (root);

	return stats->children;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_get_track_type (ParrilladaTrack *track,
				       ParrilladaTrackType *type)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	parrillada_track_type_set_has_data (type);
	parrillada_track_type_set_data_fs (type, parrillada_track_data_cfg_get_fs (PARRILLADA_TRACK_DATA (track)));

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_get_status (ParrilladaTrack *track,
				   ParrilladaStatus *status)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->loading) {
		parrillada_status_set_not_ready (status,
					      (gdouble) (priv->loading - priv->loading_remaining) / (gdouble) priv->loading,
					      _("Analysing files"));
		return PARRILLADA_BURN_NOT_READY;
	}

	/* This one goes before the next since a node may be loading but not
	 * yet in the project and therefore project will look empty */
	if (parrillada_data_vfs_is_active (PARRILLADA_DATA_VFS (priv->tree))) {
		if (status)
			parrillada_status_set_running (status,
						    -1.0,
						    _("Analysing files"));
		return PARRILLADA_BURN_NOT_READY;
	}

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free, NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
		return PARRILLADA_BURN_ERR;
	}

	if (parrillada_data_project_is_empty (PARRILLADA_DATA_PROJECT (priv->tree))) {
		if (status)
			parrillada_status_set_error (status,
						  g_error_new (PARRILLADA_BURN_ERROR,
						               PARRILLADA_BURN_ERROR_EMPTY,
						               _("There are no files to write to disc")));
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_get_size (ParrilladaTrack *track,
				 goffset *blocks,
				 goffset *block_size)
{
	ParrilladaTrackDataCfgPrivate *priv;
	goffset sectors = 0;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	sectors = parrillada_data_project_get_sectors (PARRILLADA_DATA_PROJECT (priv->tree));
	if (blocks) {
		ParrilladaFileNode *root;
		ParrilladaImageFS fs_type;
		ParrilladaFileTreeStats *stats;

		if (!sectors)
			return sectors;

		fs_type = parrillada_track_data_cfg_get_fs (PARRILLADA_TRACK_DATA (track));
		root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
		stats = PARRILLADA_FILE_NODE_STATS (root);
		sectors = parrillada_data_project_improve_image_size_accuracy (sectors,
									    stats->num_dir,
									    fs_type);
		*blocks = sectors;
	}

	if (block_size)
		*block_size = 2048;

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_cfg_image_uri_cb (ParrilladaDataVFS *vfs,
				     const gchar *uri,
				     ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	GValue instance_and_params [2];
	GValue return_value;
	GValue *params;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);
	if (priv->loading)
		return PARRILLADA_BURN_OK;

	/* object which signalled */
	instance_and_params->g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	/* arguments of signal (name) */
	params = instance_and_params + 1;
	params->g_type = 0;
	g_value_init (params, G_TYPE_STRING);
	g_value_set_string (params, uri);

	/* default to OK (for addition) */
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_OK);

	g_signal_emitv (instance_and_params,
			parrillada_track_data_cfg_signals [IMAGE],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (params);

	return g_value_get_int (&return_value);
}

static void
parrillada_track_data_cfg_unreadable_uri_cb (ParrilladaDataVFS *vfs,
					  const GError *error,
					  const gchar *uri,
					  ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		priv->load_errors = g_slist_prepend (priv->load_errors,
						     g_strdup (error->message));

		return;
	}

	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [UNREADABLE],
		       0,
		       error,
		       uri);
}

static void
parrillada_track_data_cfg_recursive_uri_cb (ParrilladaDataVFS *vfs,
					 const gchar *uri,
					 ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		gchar *message;
		gchar *name;

		name = parrillada_utils_get_uri_name (uri);
		message = g_strdup_printf (_("\"%s\" is a recursive symbolic link."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}
	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [RECURSIVE],
		       0,
		       uri);
}

static void
parrillada_track_data_cfg_unknown_uri_cb (ParrilladaDataVFS *vfs,
				       const gchar *uri,
				       ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		gchar *message;
		gchar *name;

		name = parrillada_utils_get_uri_name (uri);
		message = g_strdup_printf (_("\"%s\" cannot be found."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}
	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [UNKNOWN],
		       0,
		       uri);
}

static gboolean
parrillada_track_data_cfg_autorun_inf_update (ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	gchar *icon_path = NULL;
	gsize data_size = 0;
	GKeyFile *key_file;
	gchar *data = NULL;
	gchar *path = NULL;
	gchar *uri;
	int fd;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), priv->autorun);
	path = g_filename_from_uri (uri, NULL, NULL);
	g_free (uri);

	fd = open (path, O_WRONLY|O_TRUNC);
	g_free (path);

	if (fd == -1)
		return FALSE;

	icon_path = parrillada_data_project_node_to_path (PARRILLADA_DATA_PROJECT (priv->tree), priv->icon);

	/* Write the autorun.inf if we don't have one yet */
	key_file = g_key_file_new ();
	g_key_file_set_value (key_file, "autorun", "icon", icon_path);
	g_free (icon_path);

	data = g_key_file_to_data (key_file, &data_size, NULL);
	g_key_file_free (key_file);

	if (write (fd, data, data_size) == -1) {
		g_free (data);
		close (fd);
		return FALSE;
	}

	g_free (data);
	close (fd);
	return TRUE;
}

static gchar *
parrillada_track_data_cfg_find_icon_name (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaFileNode *root;
	gchar *name = NULL;
	int i = 0;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
	do {
		g_free (name);
		name = g_strdup_printf ("Autorun%i.ico", i++);
	} while (parrillada_file_node_check_name_existence (root, name));

	return name;
}

static void
parrillada_track_data_cfg_joliet_rename_cb (ParrilladaDataProject *project,
					 ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	/* Signal this once */
	if (priv->joliet_rename)
		return;

	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [JOLIET_RENAME_SIGNAL],
		       0);

	priv->joliet_rename = 1;
}

static void
parrillada_track_data_cfg_virtual_sibling_cb (ParrilladaDataProject *project,
					   ParrilladaFileNode *node,
					   ParrilladaFileNode *sibling,
					   ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);
	if (sibling == priv->icon) {
		/* This is a warning that the icon has been added. Update our 
		 * icon node and wait for it to appear in the callback for the
		 * 'node-added' signal. Then we'll be able to fire the "icon-
		 * changed" signal. */
		priv->icon = node;
	}
}

static gboolean
parrillada_track_data_cfg_name_collision_cb (ParrilladaDataProject *project,
					  ParrilladaFileNode *node,
					  ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	gboolean result;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	/* some names are interesting for us */
	if (node == priv->autorun) {
		ParrilladaFileNode *icon;

		/* An autorun.inf has been added by the user. Whether or not we
		 * are loading a project, if there is an autorun.inf file, then
		 * wipe it, parse the new and signal */

		/* Save icon node as we'll need it afterwards to remove it */
		icon = priv->icon;

		/* Do it now as this is the hidden temporarily created
		 * graft point whose deletion won't be signalled by 
		 * ParrilladaDataTreeModel */
		parrillada_track_data_clean_autorun (self);
		parrillada_data_project_remove_node (PARRILLADA_DATA_PROJECT (priv->tree), icon);

		g_signal_emit (self,
			       parrillada_track_data_cfg_signals [ICON_CHANGED],
			       0);

		return FALSE;
	}

	if (node == priv->icon) {
		gchar *uri;
		gchar *name = NULL;
		ParrilladaFileNode *root;

		/* we need to recreate another one with a different name */
		uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), node);
		root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));
		name = parrillada_track_data_cfg_find_icon_name (self);

		priv->icon = parrillada_data_project_add_hidden_node (PARRILLADA_DATA_PROJECT (priv->tree),
								   uri,
								   name,
								   root);
		g_free (name);
		g_free (uri);

		/* Update our autorun.inf */
		parrillada_track_data_cfg_autorun_inf_update (self);
		return FALSE;
	}

	if (priv->loading) {
		/* don't do anything accept replacement */
		return FALSE;
	}

	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [NAME_COLLISION],
		       0,
		       PARRILLADA_FILE_NODE_NAME (node),
		       &result);
	return result;
}

static gboolean
parrillada_track_data_cfg_deep_directory (ParrilladaDataProject *project,
				       const gchar *string,
				       ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	gboolean result;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->deep_directory)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these directories from now on */
		priv->deep_directory = TRUE;
		return FALSE;
	}

	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [DEEP_DIRECTORY],
		       0,
		       string,
		       &result);

	priv->deep_directory = result;
	return result;
}

static gboolean
parrillada_track_data_cfg_2G_file (ParrilladaDataProject *project,
				const gchar *string,
				ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;
	gboolean result;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	/* This is to make sure we asked once */
	if (priv->G2_files)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these files from now on */
		priv->G2_files = TRUE;
		return FALSE;
	}

	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [G2_FILE],
		       0,
		       string,
		       &result);
	priv->G2_files = result;
	return result;
}

static void
parrillada_track_data_cfg_project_loaded (ParrilladaDataProject *project,
				       gint loading,
				       ParrilladaTrackDataCfg *self)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	priv->loading_remaining = loading;
	if (loading > 0) {
		gdouble progress;

		progress = (gdouble) (priv->loading - priv->loading_remaining) / (gdouble) priv->loading;
		g_signal_emit (self,
			       parrillada_track_data_cfg_signals [SOURCE_LOADING],
			       0,
			       progress);
		return;
	}

	priv->loading = 0;
	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [SOURCE_LOADED],
		       0,
		       priv->load_errors);
}

static void
parrillada_track_data_cfg_activity_changed (ParrilladaDataVFS *vfs,
					 gboolean active,
					 ParrilladaTrackDataCfg *self)
{
	GSList *nodes;
	GtkTreeIter iter;
	ParrilladaTrackDataCfgPrivate *priv;

	if (parrillada_data_vfs_is_active (vfs))
		goto emit_signal;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (self);

	/* This is used when we finished exploring so we can update PARRILLADA_DATA_TREE_MODEL_PERCENT */
	iter.stamp = priv->stamp;
	iter.user_data2 = GINT_TO_POINTER (PARRILLADA_ROW_REGULAR);

	/* NOTE: we shouldn't need to use reference here as unref_node is used */
	for (nodes = priv->shown; nodes; nodes = nodes->next) {
		GtkTreePath *treepath;

		iter.user_data = nodes->data;
		treepath = parrillada_track_data_cfg_node_to_path (self, nodes->data);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), treepath, &iter);
		gtk_tree_path_free (treepath);
	}

emit_signal:

	parrillada_track_data_cfg_clean_cache (self);

	parrillada_track_changed (PARRILLADA_TRACK (self));
}

static void
parrillada_track_data_cfg_size_changed_cb (ParrilladaDataProject *project,
					ParrilladaTrackDataCfg *self)
{
	parrillada_track_data_cfg_clean_cache (self);
	parrillada_track_changed (PARRILLADA_TRACK (self));
}

static void
parrillada_track_data_cfg_session_available_cb (ParrilladaDataSession *session,
					     ParrilladaMedium *medium,
					     gboolean available,
					     ParrilladaTrackDataCfg *self)
{
	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [AVAILABLE],
		       0,
		       medium,
		       available);
}

static void
parrillada_track_data_cfg_session_loaded_cb (ParrilladaDataSession *session,
					  ParrilladaMedium *medium,
					  gboolean loaded,
					  ParrilladaTrackDataCfg *self)
{
	g_signal_emit (self,
		       parrillada_track_data_cfg_signals [LOADED],
		       0,
		       medium,
		       loaded);

	parrillada_track_changed (PARRILLADA_TRACK (self));
}

/**
 * parrillada_track_data_cfg_span:
 * @track: a #ParrilladaTrackDataCfg
 * @sectors: a #goffset
 * @new_track: a #ParrilladaTrackData
 *
 * Creates a new #ParrilladaTrackData (stored in @new_track) from the files contained in @track. The sum of their sizes
 * does not exceed @sectors. This allows to burn a tree on multiple discs. This function can be
 * called repeatedly; in this case if some files were left out after the previous calls, the newly created ParrilladaTrackData
 * is created with all or part of the remaining files.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is not anymore data.
 * PARRILLADA_BURN_RETRY if the operation was successful and a new #ParrilladaTrackDataCfg was created.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_cfg_span (ParrilladaTrackDataCfg *track,
			     goffset sectors,
			     ParrilladaTrackData *new_track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaBurnResult result;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading
	||  parrillada_data_vfs_is_active (PARRILLADA_DATA_VFS (priv->tree))
	||  parrillada_data_session_get_loaded_medium (PARRILLADA_DATA_SESSION (priv->tree)) != NULL)
		return PARRILLADA_BURN_NOT_READY;

	result = parrillada_data_project_span (PARRILLADA_DATA_PROJECT (priv->tree),
					    sectors,
					    TRUE,
					    TRUE, /* FIXME */
					    new_track);
	if (result != PARRILLADA_BURN_RETRY)
		return result;

	parrillada_track_tag_copy_missing (PARRILLADA_TRACK (new_track), PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_RETRY;
}

/**
 * parrillada_track_data_cfg_span_again:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Checks whether some files were not included during calls to parrillada_track_data_cfg_span ().
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is not anymore data.
 * PARRILLADA_BURN_RETRY if the operation was successful and a new #ParrilladaTrackDataCfg was created.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_cfg_span_again (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	return parrillada_data_project_span_again (PARRILLADA_DATA_PROJECT (priv->tree));
}

/**
 * parrillada_track_data_cfg_span_possible:
 * @track: a #ParrilladaTrackDataCfg
 * @sectors: a #goffset
 *
 * Checks if a new #ParrilladaTrackData can be created from the files remaining in the tree 
 * after calls to parrillada_track_data_cfg_span ().
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is not anymore data.
 * PARRILLADA_BURN_RETRY if the operation was successful and a new #ParrilladaTrackDataCfg was created.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_cfg_span_possible (ParrilladaTrackDataCfg *track,
				      goffset sectors)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading
	||  parrillada_data_vfs_is_active (PARRILLADA_DATA_VFS (priv->tree))
	||  parrillada_data_session_get_loaded_medium (PARRILLADA_DATA_SESSION (priv->tree)) != NULL)
		return PARRILLADA_BURN_NOT_READY;

	return parrillada_data_project_span_possible (PARRILLADA_DATA_PROJECT (priv->tree),
						   sectors);
}

/**
 * parrillada_track_data_cfg_span_stop:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Resets the list of files that were included after calls to parrillada_track_data_cfg_span ().
 **/

void
parrillada_track_data_cfg_span_stop (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	parrillada_data_project_span_stop (PARRILLADA_DATA_PROJECT (priv->tree));
}

/**
 * parrillada_track_data_cfg_span_max_space:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Returns the maximum required space (in sectors) 
 * among all the possible spanned batches.
 * This means that when burningto a media
 * it will also be the minimum required
 * space to burn all the contents in several
 * batches.
 *
 * Return value: a #goffset.
 **/

goffset
parrillada_track_data_cfg_span_max_space (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	return parrillada_data_project_get_max_space (PARRILLADA_DATA_PROJECT (priv->tree));
}

/**
 * This is to handle the icon for the image
 */

/**
 * parrillada_track_data_cfg_get_icon_path:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Returns a path pointing to the currently selected icon file.
 *
 * Return value: a #gchar or NULL.
 **/

gchar *
parrillada_track_data_cfg_get_icon_path (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA_CFG (track), NULL);

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (!priv->image_file)
		return NULL;

	return g_file_get_path (priv->image_file);
}

/**
 * parrillada_track_data_cfg_get_icon:
 * @track: a #ParrilladaTrackDataCfg
 *
 * Returns the currently selected icon.
 *
 * Return value: a #GIcon or NULL.
 **/

GIcon *
parrillada_track_data_cfg_get_icon (ParrilladaTrackDataCfg *track)
{
	gchar *array [] = {"media-optical", NULL};
	ParrilladaTrackDataCfgPrivate *priv;
	ParrilladaMedium *medium;
	GIcon *icon;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA_CFG (track), NULL);

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->image_file)
		icon = g_file_icon_new (priv->image_file);
	else if ((medium = parrillada_data_session_get_loaded_medium (PARRILLADA_DATA_SESSION (priv->tree))))
		icon = parrillada_volume_get_icon (PARRILLADA_VOLUME (medium));
	else
		icon = g_themed_icon_new_from_names (array, -1);

	return icon;
}

static gchar *
parrillada_track_data_cfg_get_scaled_icon_path (ParrilladaTrackDataCfg *track)
{
	ParrilladaTrackDataCfgPrivate *priv;
	gchar *path;
	gchar *uri;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA_CFG (track), NULL);
	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);
	if (!priv->icon || PARRILLADA_FILE_NODE_VIRTUAL (priv->icon))
		return NULL;

	uri = parrillada_data_project_node_to_uri (PARRILLADA_DATA_PROJECT (priv->tree), priv->icon);
	path = g_filename_from_uri (uri, NULL, NULL);
	g_free (uri);

	return path;
}

/**
 * parrillada_track_data_cfg_set_icon:
 * @track: a #ParrilladaTrackDataCfg
 * @icon_path: a #gchar
 * @error: a #GError
 *
 * Sets the current icon.
 *
 * Return value: a #gboolean. TRUE if the operation was successful, FALSE otherwise
 **/

gboolean
parrillada_track_data_cfg_set_icon (ParrilladaTrackDataCfg *track,
				 const gchar *icon_path,
				 GError **error)
{
	gboolean result;
	GdkPixbuf *pixbuf;
	ParrilladaFileNode *root;
	ParrilladaTrackDataCfgPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA_CFG (track), FALSE);

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (track);

	/* Check whether we don't have an added (by the user) autorun.inf as it
	 * won't be possible to edit it. */
	root = parrillada_data_project_get_root (PARRILLADA_DATA_PROJECT (priv->tree));

	if (!priv->autorun) {
		ParrilladaFileNode *node;

		node = parrillada_file_node_check_name_existence_case (root, "autorun.inf");
		if (node && !node->is_imported) {
			/* There is a native autorun.inf file. That's why we can't edit
			 * it; even if we were to create a temporary file with just the
			 * icon changed then we could not save it as a project later.
			 * If I change my mind, I should remember that it the path is
			 * the value ON DISC.
			 * The only exception is if the autorun.inf is an autorun.inf
			 * that was imported from another session when we're 
			 * merging. */
			return FALSE;
		}
	}

	/* Load and convert (48x48) the image into a pixbuf */
	pixbuf = gdk_pixbuf_new_from_file_at_scale (icon_path,
						    48,
						    48,
						    FALSE,
						    error);
	if (!pixbuf)
		return FALSE;

	/* See if we already have an icon set. If we do, reuse the tmp file */
	if (!priv->icon) {
		gchar *buffer = NULL;
		gchar *path = NULL;
		gchar *name = NULL;
		gsize buffer_size;
		int icon_fd;
		gchar *uri;

		icon_fd = g_file_open_tmp (PARRILLADA_BURN_TMP_FILE_NAME,
					   &path,
					   error);
		if (icon_fd == -1) {
			g_object_unref (pixbuf);
			return FALSE;
		}

		/* Add it as a graft to the project */
		uri = g_filename_to_uri (path, NULL, NULL);
		g_free (path);

 		name = parrillada_track_data_cfg_find_icon_name (track);
		priv->icon = parrillada_data_project_add_hidden_node (PARRILLADA_DATA_PROJECT (priv->tree),
								   uri,
								   name,
								   root);
		g_free (name);
		g_free (uri);

		/* Write it as an "ico" file (or a png?) */
		result = gdk_pixbuf_save_to_buffer (pixbuf,
						    &buffer,
						    &buffer_size,
						    "ico",
						    error,
						    NULL);
		if (!result) {
			close (icon_fd);
			g_object_unref (pixbuf);
			return FALSE;
		}

		if (write (icon_fd, buffer, buffer_size) == -1) {
			g_object_unref (pixbuf);
			g_free (buffer);
			close (icon_fd);
			return FALSE;
		}

		g_free (buffer);
		close (icon_fd);
	}
	else {
		gchar *path;

		path = parrillada_track_data_cfg_get_scaled_icon_path (track);

		/* Write it as an "ico" file (or a png?) */
		result = gdk_pixbuf_save (pixbuf,
					  path,
					  "ico",
					  error,
					  NULL);
		g_free (path);

		if (!result) {
			g_object_unref (pixbuf);
			return FALSE;
		}
	}

	g_object_unref (pixbuf);

	if (!priv->autorun) {
		gchar *path = NULL;
		gchar *uri;
		int fd;

		/* Get a temporary file if we don't have one yet */
		fd = g_file_open_tmp (PARRILLADA_BURN_TMP_FILE_NAME,
				      &path,
				      error);
		close (fd);

		/* Add it as a graft to the project */
		uri = g_filename_to_uri (path, NULL, NULL);
		g_free (path);

		priv->autorun = parrillada_data_project_add_hidden_node (PARRILLADA_DATA_PROJECT (priv->tree),
								      uri,
								      "autorun.inf",
								      root);
		g_free (uri);

		/* write the autorun.inf */
		parrillada_track_data_cfg_autorun_inf_update (track);
	}

	if (priv->image_file) {
		g_object_unref (priv->image_file);
		priv->image_file = NULL;
	}

	priv->image_file = g_file_new_for_path (icon_path);
	g_signal_emit (track,
		       parrillada_track_data_cfg_signals [ICON_CHANGED],
		       0);
	return TRUE;
}

static void
parrillada_track_data_cfg_init (ParrilladaTrackDataCfg *object)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (object);

	priv->sort_column = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
	do {
		priv->stamp = g_random_int ();
	} while (!priv->stamp);

	priv->theme = gtk_icon_theme_get_default ();
	priv->tree = parrillada_data_tree_model_new ();

	g_signal_connect (priv->tree,
			  "row-added",
			  G_CALLBACK (parrillada_track_data_cfg_node_added),
			  object);
	g_signal_connect (priv->tree,
			  "row-changed",
			  G_CALLBACK (parrillada_track_data_cfg_node_changed),
			  object);
	g_signal_connect (priv->tree,
			  "row-removed",
			  G_CALLBACK (parrillada_track_data_cfg_node_removed),
			  object);
	g_signal_connect (priv->tree,
			  "rows-reordered",
			  G_CALLBACK (parrillada_track_data_cfg_node_reordered),
			  object);

	g_signal_connect (priv->tree,
			  "size-changed",
			  G_CALLBACK (parrillada_track_data_cfg_size_changed_cb),
			  object);

	g_signal_connect (priv->tree,
			  "session-available",
			  G_CALLBACK (parrillada_track_data_cfg_session_available_cb),
			  object);
	g_signal_connect (priv->tree,
			  "session-loaded",
			  G_CALLBACK (parrillada_track_data_cfg_session_loaded_cb),
			  object);
	
	g_signal_connect (priv->tree,
			  "project-loaded",
			  G_CALLBACK (parrillada_track_data_cfg_project_loaded),
			  object);
	g_signal_connect (priv->tree,
			  "vfs-activity",
			  G_CALLBACK (parrillada_track_data_cfg_activity_changed),
			  object);
	g_signal_connect (priv->tree,
			  "deep-directory",
			  G_CALLBACK (parrillada_track_data_cfg_deep_directory),
			  object);
	g_signal_connect (priv->tree,
			  "2G-file",
			  G_CALLBACK (parrillada_track_data_cfg_2G_file),
			  object);
	g_signal_connect (priv->tree,
			  "unreadable-uri",
			  G_CALLBACK (parrillada_track_data_cfg_unreadable_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "unknown-uri",
			  G_CALLBACK (parrillada_track_data_cfg_unknown_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "recursive-sym",
			  G_CALLBACK (parrillada_track_data_cfg_recursive_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "image-uri",
			  G_CALLBACK (parrillada_track_data_cfg_image_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "virtual-sibling",
			  G_CALLBACK (parrillada_track_data_cfg_virtual_sibling_cb),
			  object);
	g_signal_connect (priv->tree,
			  "name-collision",
			  G_CALLBACK (parrillada_track_data_cfg_name_collision_cb),
			  object);
	g_signal_connect (priv->tree,
			  "joliet-rename",
			  G_CALLBACK (parrillada_track_data_cfg_joliet_rename_cb),
			  object);
}

static void
parrillada_track_data_cfg_finalize (GObject *object)
{
	ParrilladaTrackDataCfgPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_CFG_PRIVATE (object);

	parrillada_track_data_clean_autorun (PARRILLADA_TRACK_DATA_CFG (object));
	parrillada_track_data_cfg_clean_cache (PARRILLADA_TRACK_DATA_CFG (object));

	if (priv->shown) {
		g_slist_free (priv->shown);
		priv->shown = NULL;
	}

	if (priv->tree) {
		/* This object could outlive us just for some time
		 * so we better remove all signals.
		 * When an image URI is detected it can happen
		 * that we'll be destroyed. */
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_node_added,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_node_changed,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_node_removed,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_node_reordered,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_size_changed_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_session_available_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_session_loaded_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_project_loaded,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_activity_changed,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_deep_directory,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_2G_file,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_unreadable_uri_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_unknown_uri_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_recursive_uri_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_image_uri_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_virtual_sibling_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_name_collision_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->tree,
		                                      parrillada_track_data_cfg_joliet_rename_cb,
		                                      object);

		g_object_unref (priv->tree);
		priv->tree = NULL;
	}

	G_OBJECT_CLASS (parrillada_track_data_cfg_parent_class)->finalize (object);
}

static void
parrillada_track_data_cfg_class_init (ParrilladaTrackDataCfgClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass *track_class = PARRILLADA_TRACK_CLASS (klass);
	ParrilladaTrackDataClass *parent_class = PARRILLADA_TRACK_DATA_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackDataCfgPrivate));

	object_class->finalize = parrillada_track_data_cfg_finalize;

	track_class->get_size = parrillada_track_data_cfg_get_size;
	track_class->get_type = parrillada_track_data_cfg_get_track_type;
	track_class->get_status = parrillada_track_data_cfg_get_status;

	parent_class->set_source = parrillada_track_data_cfg_set_source;
	parent_class->add_fs = parrillada_track_data_cfg_add_fs;
	parent_class->rm_fs = parrillada_track_data_cfg_rm_fs;

	parent_class->get_fs = parrillada_track_data_cfg_get_fs;
	parent_class->get_grafts = parrillada_track_data_cfg_get_grafts;
	parent_class->get_excluded = parrillada_track_data_cfg_get_excluded;
	parent_class->get_file_num = parrillada_track_data_cfg_get_file_num;

	parrillada_track_data_cfg_signals [AVAILABLE] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	parrillada_track_data_cfg_signals [LOADED] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);

	parrillada_track_data_cfg_signals [IMAGE] = 
	    g_signal_new ("image_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_INT__STRING,
			  G_TYPE_INT,
			  1,
			  G_TYPE_STRING);

	parrillada_track_data_cfg_signals [UNREADABLE] = 
	    g_signal_new ("unreadable_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__POINTER_STRING,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_STRING);

	parrillada_track_data_cfg_signals [RECURSIVE] = 
	    g_signal_new ("recursive_sym",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	parrillada_track_data_cfg_signals [UNKNOWN] = 
	    g_signal_new ("unknown_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);
	parrillada_track_data_cfg_signals [G2_FILE] = 
	    g_signal_new ("2G_file",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	parrillada_track_data_cfg_signals [NAME_COLLISION] = 
	    g_signal_new ("name_collision",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	parrillada_track_data_cfg_signals [JOLIET_RENAME_SIGNAL] = 
	    g_signal_new ("joliet_rename",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);
	parrillada_track_data_cfg_signals [DEEP_DIRECTORY] = 
	    g_signal_new ("deep_directory",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);

	parrillada_track_data_cfg_signals [SOURCE_LOADING] = 
	    g_signal_new ("source_loading",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__DOUBLE,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_DOUBLE);
	parrillada_track_data_cfg_signals [SOURCE_LOADED] = 
	    g_signal_new ("source_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);

	parrillada_track_data_cfg_signals [ICON_CHANGED] = 
	    g_signal_new ("icon_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);
}

/**
 * parrillada_track_data_cfg_new:
 *
 * Creates a new #ParrilladaTrackDataCfg.
 *
 * Return value: a new #ParrilladaTrackDataCfg.
 **/

ParrilladaTrackDataCfg *
parrillada_track_data_cfg_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_DATA_CFG, NULL);
}

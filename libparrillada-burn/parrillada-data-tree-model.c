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

#include <glib.h>

#include "libparrillada-marshal.h"
#include "parrillada-data-tree-model.h"
#include "parrillada-data-project.h"
#include "parrillada-data-vfs.h"
#include "parrillada-file-node.h"

typedef struct _ParrilladaDataTreeModelPrivate ParrilladaDataTreeModelPrivate;
struct _ParrilladaDataTreeModelPrivate
{
	guint stamp;
};

#define PARRILLADA_DATA_TREE_MODEL_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DATA_TREE_MODEL, ParrilladaDataTreeModelPrivate))

G_DEFINE_TYPE (ParrilladaDataTreeModel, parrillada_data_tree_model, PARRILLADA_TYPE_DATA_VFS);

enum {
	ROW_ADDED,
	ROW_REMOVED,
	ROW_CHANGED,
	ROWS_REORDERED,
	LAST_SIGNAL
};

static guint parrillada_data_tree_model_signals [LAST_SIGNAL] = {0};

static gboolean
parrillada_data_tree_model_node_added (ParrilladaDataProject *project,
				    ParrilladaFileNode *node,
				    const gchar *uri)
{
	/* see if we really need to tell the treeview we changed */
	if (node->is_hidden)
		goto end;

	if (node->parent
	&& !node->parent->is_root
	&& !node->parent->is_visible)
		goto end;

	g_signal_emit (project,
		       parrillada_data_tree_model_signals [ROW_ADDED],
		       0,
		       node);

end:
	/* chain up this function */
	if (PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_added)
		return PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_added (project, node, uri);

	return TRUE;
}

static void
parrillada_data_tree_model_node_removed (ParrilladaDataProject *project,
				      ParrilladaFileNode *former_parent,
				      guint former_position,
				      ParrilladaFileNode *node)
{
	/* see if we really need to tell the treeview we changed */
	if (node->is_hidden)
		goto end;

	if (!node->is_visible
	&&   former_parent
	&&  !former_parent->is_root
	&&  !former_parent->is_visible)
		goto end;

	g_signal_emit (project,
		       parrillada_data_tree_model_signals [ROW_REMOVED],
		       0,
		       former_parent,
		       former_position,
		       node);

end:
	/* chain up this function */
	if (PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_removed)
		PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_removed (project,
												 former_parent,
												 former_position,
												 node);
}

static void
parrillada_data_tree_model_node_changed (ParrilladaDataProject *project,
				      ParrilladaFileNode *node)
{
	/* see if we really need to tell the treeview we changed */
	if (node->is_hidden)
		goto end;

	if (node->parent
	&& !node->parent->is_root
	&& !node->parent->is_visible)
		goto end;

	g_signal_emit (project,
		       parrillada_data_tree_model_signals [ROW_CHANGED],
		       0,
		       node);

end:
	/* chain up this function */
	if (PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_changed)
		PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_changed (project, node);
}

static void
parrillada_data_tree_model_node_reordered (ParrilladaDataProject *project,
					ParrilladaFileNode *parent,
					gint *new_order)
{
	/* see if we really need to tell the treeview we changed */
	if (!parent->is_root
	&&  !parent->is_visible)
		goto end;

	g_signal_emit (project,
		       parrillada_data_tree_model_signals [ROWS_REORDERED],
		       0,
		       parent,
		       new_order);

end:
	/* chain up this function */
	if (PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_reordered)
		PARRILLADA_DATA_PROJECT_CLASS (parrillada_data_tree_model_parent_class)->node_reordered (project, parent, new_order);
}

static void
parrillada_data_tree_model_init (ParrilladaDataTreeModel *object)
{ }

static void
parrillada_data_tree_model_finalize (GObject *object)
{
	G_OBJECT_CLASS (parrillada_data_tree_model_parent_class)->finalize (object);
}

static void
parrillada_data_tree_model_class_init (ParrilladaDataTreeModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaDataProjectClass *data_project_class = PARRILLADA_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDataTreeModelPrivate));

	object_class->finalize = parrillada_data_tree_model_finalize;

	data_project_class->node_added = parrillada_data_tree_model_node_added;
	data_project_class->node_removed = parrillada_data_tree_model_node_removed;
	data_project_class->node_changed = parrillada_data_tree_model_node_changed;
	data_project_class->node_reordered = parrillada_data_tree_model_node_reordered;

	parrillada_data_tree_model_signals [ROW_ADDED] = 
	    g_signal_new ("row_added",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);
	parrillada_data_tree_model_signals [ROW_REMOVED] = 
	    g_signal_new ("row_removed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__POINTER_UINT_POINTER,
			  G_TYPE_NONE,
			  3,
			  G_TYPE_POINTER,
			  G_TYPE_UINT,
			  G_TYPE_POINTER);
	parrillada_data_tree_model_signals [ROW_CHANGED] = 
	    g_signal_new ("row_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);
	parrillada_data_tree_model_signals [ROWS_REORDERED] = 
	    g_signal_new ("rows_reordered",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  parrillada_marshal_VOID__POINTER_POINTER,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_POINTER);
}

ParrilladaDataTreeModel *
parrillada_data_tree_model_new (void)
{
	return g_object_new (PARRILLADA_TYPE_DATA_TREE_MODEL, NULL);
}

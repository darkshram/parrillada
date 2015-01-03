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
 
#ifndef _PARRILLADA_FILE_NODE_H
#define _PARRILLADA_FILE_NODE_H

#include <glib.h>

#include <gio/gio.h>

#include "burn-volume.h"

G_BEGIN_DECLS

typedef struct _ParrilladaFileNode ParrilladaFileNode;

struct _ParrilladaURINode {
	/* List of all nodes that share the same URI */
	GSList *nodes;

	/* NOTE: uris are always escaped */
	gchar *uri;
};
typedef struct _ParrilladaURINode ParrilladaURINode;

struct _ParrilladaGraft {
	/* The name on CD (which could be different) from the original */
	gchar *name;
	ParrilladaURINode *node;
};
typedef struct _ParrilladaGraft ParrilladaGraft;

struct _ParrilladaImport {
	gchar *name;
	ParrilladaFileNode *replaced;
};
typedef struct _ParrilladaImport ParrilladaImport;

/**
 * NOTE: The root object keeps some statistics about its tree like
 * - number of children (files+directories)
 * - number of deep directories
 * - number of files over 2 GiB
 */

struct _ParrilladaFileTreeStats {
	guint children;
	guint num_dir;
	guint num_deep;
	guint num_2GiB;
	guint num_sym;
};
typedef struct _ParrilladaFileTreeStats ParrilladaFileTreeStats;

struct _ParrilladaFileNode {
	ParrilladaFileNode *parent;
	ParrilladaFileNode *next;

	/**
	 * This union can be:
	 * - the name of the file on file system and on disc
	 * - a pointer to the graft node for all grafted nodes
	 * - a pointer to saved imported session files
	 * Check is_grafted to determine.
	 */

	/* NOTE: names are unescaped (for display). */

	union {
		ParrilladaImport *import;
		ParrilladaGraft *graft;
		gchar *name;
	} union1;

	union {
		gchar *mime;
		ParrilladaFileNode *children;
	} union2;

	/* NOTE: overflow for sectors will probably not be hit
	 * before a few years given that we store the size in
	 * sectors of 2048. For the time being DVD are usually
	 * 4.3 GiB. To overflow the following member (provided
	 * we are on a 32 architecture) they would have to be
	 * over 8192 GiB. I think it's reasonable to think we
	 * have time. And even then, by this time most of the 
	 * computers will have switched to 64 architecture (in
	 * 2099) and I'll be dead anyway as well as optical
	 * discs. */
	union {
		guint sectors;

		/* stores the address of the children records in image */
		guint imported_address;
		ParrilladaFileTreeStats *stats;
	} union3;

	/* type of node */
	guint is_root:1;
	guint is_fake:1;
	guint is_file:1;
	guint is_symlink:1;
	guint is_imported:1;
	guint is_monitored:1;

	/* status of union1 */
	guint is_grafted:1;
	guint has_import:1;

	/* VFS status of node */
	guint is_loading:1;
	guint is_reloading:1;
	guint is_exploring:1;

	/* that's for some special nodes (usually counted in statistics) */
	guint is_2GiB:1;
	guint is_deep:1;

	/* This is for nodes created at project load time. This means
	 * that they can be replaced if a real directory is under the
	 * parent with the same name*/
	guint is_tmp_parent:1;

	/* Used to determine if is should be shown */
	guint is_hidden:1;

	/* Used by the model */
	/* This is a workaround for a warning in gailtreeview.c line 2946 where
	 * gail uses the GtkTreePath and not a copy which if the node inserted
	 * declares to have children and is not expanded leads to the path being
	 * upped and therefore wrong. */
	guint is_inserting:1;

	guint is_expanded:1; /* Used to choose the icon for folders */

	/* this is a ref count a max of 255 should be enough */
	guint is_visible:7;
};

/** Returns a const gchar* (it shouldn't be freed). */
#define PARRILLADA_FILE_NODE_NAME(MACRO_node)					\
	((MACRO_node)->is_grafted?(MACRO_node)->union1.graft->name:		\
	 (MACRO_node)->has_import?(MACRO_node)->union1.import->name:		\
	 (MACRO_node)->union1.name)

#define PARRILLADA_FILE_NODE_GRAFT(MACRO_node)					\
	((MACRO_node)->is_grafted?(MACRO_node)->union1.graft:NULL)

#define PARRILLADA_FILE_NODE_IMPORT(MACRO_node)					\
	((MACRO_node)->has_import?(MACRO_node)->union1.import:NULL)

#define PARRILLADA_FILE_NODE_CHILDREN(MACRO_node)					\
	((MACRO_node)->is_file?NULL:(MACRO_node)->union2.children)

#define PARRILLADA_FILE_NODE_MIME(MACRO_node)					\
	((MACRO_node)->is_file?(MACRO_node)->union2.mime:"x-directory/normal")

#define PARRILLADA_FILE_NODE_SECTORS(MACRO_node)					\
	((guint64) ((MACRO_node)->is_root?0:(MACRO_node)->union3.sectors))

#define PARRILLADA_FILE_NODE_STATS(MACRO_root)					\
	((MACRO_root)->is_root?(MACRO_root)->union3.stats:NULL)

#define PARRILLADA_FILE_NODE_VIRTUAL(MACRO_node)					\
	((MACRO_node)->is_hidden && (MACRO_node)->is_fake)

#define PARRILLADA_FILE_NODE_IMPORTED_ADDRESS(MACRO_node)				\
	((MACRO_node) && (MACRO_node)->is_imported && (MACRO_node)->is_fake?(MACRO_node)->union3.imported_address:-1)

#define PARRILLADA_FILE_2G_LIMIT		1048576

ParrilladaFileNode *
parrillada_file_node_root_new (void);

ParrilladaFileNode *
parrillada_file_node_get_root (ParrilladaFileNode *node,
			    guint *depth);

ParrilladaFileTreeStats *
parrillada_file_node_get_tree_stats (ParrilladaFileNode *node,
				  guint *depth);

ParrilladaFileNode *
parrillada_file_node_nth_child (ParrilladaFileNode *parent,
			     guint nth);

guint
parrillada_file_node_get_depth (ParrilladaFileNode *node);

guint
parrillada_file_node_get_pos_as_child (ParrilladaFileNode *node);

guint
parrillada_file_node_get_n_children (const ParrilladaFileNode *node);

guint
parrillada_file_node_get_pos_as_child (ParrilladaFileNode *node);

gboolean
parrillada_file_node_is_ancestor (ParrilladaFileNode *parent,
			       ParrilladaFileNode *node);
ParrilladaFileNode *
parrillada_file_node_get_from_path (ParrilladaFileNode *root,
				 const gchar *path);
ParrilladaFileNode *
parrillada_file_node_check_name_existence (ParrilladaFileNode *parent,
				        const gchar *name);
ParrilladaFileNode *
parrillada_file_node_check_name_existence_case (ParrilladaFileNode *parent,
					     const gchar *name);
ParrilladaFileNode *
parrillada_file_node_check_imported_sibling (ParrilladaFileNode *node);

/**
 * Nodes are strictly organised so there to be sort function all the time
 */

void
parrillada_file_node_add (ParrilladaFileNode *parent,
		       ParrilladaFileNode *child,
		       GCompareFunc sort_func);

ParrilladaFileNode *
parrillada_file_node_new (const gchar *name);

ParrilladaFileNode *
parrillada_file_node_new_virtual (const gchar *name);

ParrilladaFileNode *
parrillada_file_node_new_loading (const gchar *name);

ParrilladaFileNode *
parrillada_file_node_new_empty_folder (const gchar *name);

ParrilladaFileNode *
parrillada_file_node_new_imported_session_file (GFileInfo *info);

/**
 * If there are any change in the order it cannot be handled in these functions
 * A call to resort function must be made.
 */
void
parrillada_file_node_rename (ParrilladaFileNode *node,
			  const gchar *name);
void
parrillada_file_node_set_from_info (ParrilladaFileNode *node,
				 ParrilladaFileTreeStats *stats,
				 GFileInfo *info);

void
parrillada_file_node_graft (ParrilladaFileNode *file_node,
			 ParrilladaURINode *uri_node);
void
parrillada_file_node_ungraft (ParrilladaFileNode *node);

void
parrillada_file_node_move_from (ParrilladaFileNode *node,
			     ParrilladaFileTreeStats *stats);
void
parrillada_file_node_move_to (ParrilladaFileNode *node,
			   ParrilladaFileNode *parent,
			   GCompareFunc sort_func);

void
parrillada_file_node_unlink (ParrilladaFileNode *node);

void
parrillada_file_node_destroy (ParrilladaFileNode *node,
			   ParrilladaFileTreeStats *stats);

void
parrillada_file_node_save_imported (ParrilladaFileNode *node,
				 ParrilladaFileTreeStats *stats,
				 ParrilladaFileNode *parent,
				 GCompareFunc sort_func);

gint
parrillada_file_node_sort_name_cb (gconstpointer obj_a, gconstpointer obj_b);
gint
parrillada_file_node_sort_size_cb (gconstpointer obj_a, gconstpointer obj_b);
gint 
parrillada_file_node_sort_mime_cb (gconstpointer obj_a, gconstpointer obj_b);
gint
parrillada_file_node_sort_default_cb (gconstpointer obj_a, gconstpointer obj_b);
gint *
parrillada_file_node_sort_children (ParrilladaFileNode *parent,
				 GCompareFunc sort_func);
gint *
parrillada_file_node_need_resort (ParrilladaFileNode *node,
			       GCompareFunc sort_func);
gint *
parrillada_file_node_reverse_children (ParrilladaFileNode *parent);


G_END_DECLS

#endif /* _PARRILLADA_FILE_NODE_H */

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

#ifndef _PARRILLADA_DATA_PROJECT_H_
#define _PARRILLADA_DATA_PROJECT_H_

#include <glib-object.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include "parrillada-file-node.h"
#include "parrillada-session.h"

#include "parrillada-track-data.h"

#ifdef BUILD_INOTIFY
#include "parrillada-file-monitor.h"
#endif

G_BEGIN_DECLS


#define PARRILLADA_TYPE_DATA_PROJECT             (parrillada_data_project_get_type ())
#define PARRILLADA_DATA_PROJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DATA_PROJECT, ParrilladaDataProject))
#define PARRILLADA_DATA_PROJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DATA_PROJECT, ParrilladaDataProjectClass))
#define PARRILLADA_IS_DATA_PROJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DATA_PROJECT))
#define PARRILLADA_IS_DATA_PROJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DATA_PROJECT))
#define PARRILLADA_DATA_PROJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DATA_PROJECT, ParrilladaDataProjectClass))

typedef struct _ParrilladaDataProjectClass ParrilladaDataProjectClass;
typedef struct _ParrilladaDataProject ParrilladaDataProject;

struct _ParrilladaDataProjectClass
{
#ifdef BUILD_INOTIFY
	ParrilladaFileMonitorClass parent_class;
#else
	GObjectClass parent_class;
#endif

	/* virtual functions */

	/**
	 * num_nodes is the number of nodes that were at the root of the 
	 * project.
	 */
	void		(*reset)		(ParrilladaDataProject *project,
						 guint num_nodes);

	/* NOTE: node_added is also called when there is a moved node;
	 * in this case a node_removed is first called and then the
	 * following function is called (mostly to match GtkTreeModel
	 * API). To detect such a case look at uri which will then be
	 * set to NULL.
	 * NULL uri can also happen when it's a created directory.
	 * if return value is FALSE, node was invalidated during call */
	gboolean	(*node_added)		(ParrilladaDataProject *project,
						 ParrilladaFileNode *node,
						 const gchar *uri);

	/* This is more an unparent signal. It shouldn't be assumed that the
	 * node was destroyed or not destroyed. Like the above function, it is
	 * also called when a node is moved. */
	void		(*node_removed)		(ParrilladaDataProject *project,
						 ParrilladaFileNode *former_parent,
						 guint former_position,
						 ParrilladaFileNode *node);

	void		(*node_changed)		(ParrilladaDataProject *project,
						 ParrilladaFileNode *node);
	void		(*node_reordered)	(ParrilladaDataProject *project,
						 ParrilladaFileNode *parent,
						 gint *new_order);

	void		(*uri_removed)		(ParrilladaDataProject *project,
						 const gchar *uri);
};

struct _ParrilladaDataProject
{
#ifdef BUILD_INOTIFY
	ParrilladaFileMonitor parent_instance;
#else
	GObject parent_instance;
#endif
};

GType parrillada_data_project_get_type (void) G_GNUC_CONST;

void
parrillada_data_project_reset (ParrilladaDataProject *project);

goffset
parrillada_data_project_get_sectors (ParrilladaDataProject *project);

goffset
parrillada_data_project_improve_image_size_accuracy (goffset blocks,
						  guint64 dir_num,
						  ParrilladaImageFS fs_type);
goffset
parrillada_data_project_get_folder_sectors (ParrilladaDataProject *project,
					 ParrilladaFileNode *node);

gboolean
parrillada_data_project_get_contents (ParrilladaDataProject *project,
				   GSList **grafts,
				   GSList **unreadable,
				   gboolean hidden_nodes,
				   gboolean joliet_compat,
				   gboolean append_slash);

gboolean
parrillada_data_project_is_empty (ParrilladaDataProject *project);

gboolean
parrillada_data_project_has_symlinks (ParrilladaDataProject *project);

gboolean
parrillada_data_project_is_video_project (ParrilladaDataProject *project);

gboolean
parrillada_data_project_is_joliet_compliant (ParrilladaDataProject *project);

guint
parrillada_data_project_load_contents (ParrilladaDataProject *project,
				    GSList *grafts,
				    GSList *excluded);

ParrilladaFileNode *
parrillada_data_project_add_hidden_node (ParrilladaDataProject *project,
				      const gchar *uri,
				      const gchar *name,
				      ParrilladaFileNode *parent);

ParrilladaFileNode *
parrillada_data_project_add_loading_node (ParrilladaDataProject *project,
				       const gchar *uri,
				       ParrilladaFileNode *parent);
ParrilladaFileNode *
parrillada_data_project_add_node_from_info (ParrilladaDataProject *project,
					 const gchar *uri,
					 GFileInfo *info,
					 ParrilladaFileNode *parent);
ParrilladaFileNode *
parrillada_data_project_add_empty_directory (ParrilladaDataProject *project,
					  const gchar *name,
					  ParrilladaFileNode *parent);
ParrilladaFileNode *
parrillada_data_project_add_imported_session_file (ParrilladaDataProject *project,
						GFileInfo *info,
						ParrilladaFileNode *parent);

void
parrillada_data_project_remove_node (ParrilladaDataProject *project,
				  ParrilladaFileNode *node);
void
parrillada_data_project_destroy_node (ParrilladaDataProject *self,
				   ParrilladaFileNode *node);

gboolean
parrillada_data_project_node_loaded (ParrilladaDataProject *project,
				  ParrilladaFileNode *node,
				  const gchar *uri,
				  GFileInfo *info);
void
parrillada_data_project_node_reloaded (ParrilladaDataProject *project,
				    ParrilladaFileNode *node,
				    const gchar *uri,
				    GFileInfo *info);
void
parrillada_data_project_directory_node_loaded (ParrilladaDataProject *project,
					    ParrilladaFileNode *parent);

gboolean
parrillada_data_project_rename_node (ParrilladaDataProject *project,
				  ParrilladaFileNode *node,
				  const gchar *name);

gboolean
parrillada_data_project_move_node (ParrilladaDataProject *project,
				ParrilladaFileNode *node,
				ParrilladaFileNode *parent);

void
parrillada_data_project_restore_uri (ParrilladaDataProject *project,
				  const gchar *uri);
void
parrillada_data_project_exclude_uri (ParrilladaDataProject *project,
				  const gchar *uri);

guint
parrillada_data_project_reference_new (ParrilladaDataProject *project,
				    ParrilladaFileNode *node);
void
parrillada_data_project_reference_free (ParrilladaDataProject *project,
				     guint reference);
ParrilladaFileNode *
parrillada_data_project_reference_get (ParrilladaDataProject *project,
				    guint reference);

ParrilladaFileNode *
parrillada_data_project_get_root (ParrilladaDataProject *project);

gchar *
parrillada_data_project_node_to_uri (ParrilladaDataProject *project,
				  ParrilladaFileNode *node);
gchar *
parrillada_data_project_node_to_path (ParrilladaDataProject *self,
				   ParrilladaFileNode *node);

void
parrillada_data_project_set_sort_function (ParrilladaDataProject *project,
					GtkSortType sort_type,
					GCompareFunc sort_func);

ParrilladaFileNode *
parrillada_data_project_watch_path (ParrilladaDataProject *project,
				 const gchar *path);

ParrilladaBurnResult
parrillada_data_project_span (ParrilladaDataProject *project,
			   goffset max_sectors,
			   gboolean append_slash,
			   gboolean joliet,
			   ParrilladaTrackData *track);
ParrilladaBurnResult
parrillada_data_project_span_again (ParrilladaDataProject *project);

ParrilladaBurnResult
parrillada_data_project_span_possible (ParrilladaDataProject *project,
				    goffset max_sectors);
goffset
parrillada_data_project_get_max_space (ParrilladaDataProject *self);

void
parrillada_data_project_span_stop (ParrilladaDataProject *project);

G_END_DECLS

#endif /* _PARRILLADA_DATA_PROJECT_H_ */

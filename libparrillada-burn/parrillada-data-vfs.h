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

#ifndef _PARRILLADA_DATA_VFS_H_
#define _PARRILLADA_DATA_VFS_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-data-session.h"
#include "parrillada-filtered-uri.h"

G_BEGIN_DECLS

#define PARRILLADA_SCHEMA_FILTER			"org.mate.parrillada.filter"
#define PARRILLADA_PROPS_FILTER_HIDDEN	        "hidden"
#define PARRILLADA_PROPS_FILTER_BROKEN	        "broken-sym"
#define PARRILLADA_PROPS_FILTER_REPLACE_SYMLINK    "replace-sym"

#define PARRILLADA_TYPE_DATA_VFS             (parrillada_data_vfs_get_type ())
#define PARRILLADA_DATA_VFS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DATA_VFS, ParrilladaDataVFS))
#define PARRILLADA_DATA_VFS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DATA_VFS, ParrilladaDataVFSClass))
#define PARRILLADA_IS_DATA_VFS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DATA_VFS))
#define PARRILLADA_IS_DATA_VFS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DATA_VFS))
#define PARRILLADA_DATA_VFS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DATA_VFS, ParrilladaDataVFSClass))

typedef struct _ParrilladaDataVFSClass ParrilladaDataVFSClass;
typedef struct _ParrilladaDataVFS ParrilladaDataVFS;

struct _ParrilladaDataVFSClass
{
	ParrilladaDataSessionClass parent_class;

	void	(*activity_changed)	(ParrilladaDataVFS *vfs,
					 gboolean active);
};

struct _ParrilladaDataVFS
{
	ParrilladaDataSession parent_instance;
};

GType parrillada_data_vfs_get_type (void) G_GNUC_CONST;

gboolean
parrillada_data_vfs_is_active (ParrilladaDataVFS *vfs);

gboolean
parrillada_data_vfs_is_loading_uri (ParrilladaDataVFS *vfs);

gboolean
parrillada_data_vfs_load_mime (ParrilladaDataVFS *vfs,
			    ParrilladaFileNode *node);

gboolean
parrillada_data_vfs_require_node_load (ParrilladaDataVFS *vfs,
				    ParrilladaFileNode *node);

gboolean
parrillada_data_vfs_require_directory_contents (ParrilladaDataVFS *vfs,
					     ParrilladaFileNode *node);

ParrilladaFilteredUri *
parrillada_data_vfs_get_filtered_model (ParrilladaDataVFS *vfs);

G_END_DECLS

#endif /* _PARRILLADA_DATA_VFS_H_ */

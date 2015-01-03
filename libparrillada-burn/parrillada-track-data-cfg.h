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

#ifndef _PARRILLADA_TRACK_DATA_CFG_H_
#define _PARRILLADA_TRACK_DATA_CFG_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include <parrillada-track-data.h>

G_BEGIN_DECLS

/**
 * GtkTreeModel Part
 */

/* This DND target when moving nodes inside ourselves */
#define PARRILLADA_DND_TARGET_DATA_TRACK_REFERENCE_LIST	"GTK_TREE_MODEL_ROW"

typedef enum {
	PARRILLADA_DATA_TREE_MODEL_NAME		= 0,
	PARRILLADA_DATA_TREE_MODEL_URI,
	PARRILLADA_DATA_TREE_MODEL_MIME_DESC,
	PARRILLADA_DATA_TREE_MODEL_MIME_ICON,
	PARRILLADA_DATA_TREE_MODEL_SIZE,
	PARRILLADA_DATA_TREE_MODEL_SHOW_PERCENT,
	PARRILLADA_DATA_TREE_MODEL_PERCENT,
	PARRILLADA_DATA_TREE_MODEL_STYLE,
	PARRILLADA_DATA_TREE_MODEL_COLOR,
	PARRILLADA_DATA_TREE_MODEL_EDITABLE,
	PARRILLADA_DATA_TREE_MODEL_IS_FILE,
	PARRILLADA_DATA_TREE_MODEL_IS_LOADING,
	PARRILLADA_DATA_TREE_MODEL_IS_IMPORTED,
	PARRILLADA_DATA_TREE_MODEL_COL_NUM
} ParrilladaTrackDataCfgColumn;


#define PARRILLADA_TYPE_TRACK_DATA_CFG             (parrillada_track_data_cfg_get_type ())
#define PARRILLADA_TRACK_DATA_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK_DATA_CFG, ParrilladaTrackDataCfg))
#define PARRILLADA_TRACK_DATA_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK_DATA_CFG, ParrilladaTrackDataCfgClass))
#define PARRILLADA_IS_TRACK_DATA_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK_DATA_CFG))
#define PARRILLADA_IS_TRACK_DATA_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK_DATA_CFG))
#define PARRILLADA_TRACK_DATA_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK_DATA_CFG, ParrilladaTrackDataCfgClass))

typedef struct _ParrilladaTrackDataCfgClass ParrilladaTrackDataCfgClass;
typedef struct _ParrilladaTrackDataCfg ParrilladaTrackDataCfg;

struct _ParrilladaTrackDataCfgClass
{
	ParrilladaTrackDataClass parent_class;
};

struct _ParrilladaTrackDataCfg
{
	ParrilladaTrackData parent_instance;
};

GType parrillada_track_data_cfg_get_type (void) G_GNUC_CONST;

ParrilladaTrackDataCfg *
parrillada_track_data_cfg_new (void);

gboolean
parrillada_track_data_cfg_add (ParrilladaTrackDataCfg *track,
			    const gchar *uri,
			    GtkTreePath *parent);
GtkTreePath *
parrillada_track_data_cfg_add_empty_directory (ParrilladaTrackDataCfg *track,
					    const gchar *name,
					    GtkTreePath *parent);

gboolean
parrillada_track_data_cfg_remove (ParrilladaTrackDataCfg *track,
			       GtkTreePath *treepath);
gboolean
parrillada_track_data_cfg_rename (ParrilladaTrackDataCfg *track,
			       const gchar *newname,
			       GtkTreePath *treepath);

gboolean
parrillada_track_data_cfg_reset (ParrilladaTrackDataCfg *track);

gboolean
parrillada_track_data_cfg_load_medium (ParrilladaTrackDataCfg *track,
				    ParrilladaMedium *medium,
				    GError **error);
void
parrillada_track_data_cfg_unload_current_medium (ParrilladaTrackDataCfg *track);

ParrilladaMedium *
parrillada_track_data_cfg_get_current_medium (ParrilladaTrackDataCfg *track);

GSList *
parrillada_track_data_cfg_get_available_media (ParrilladaTrackDataCfg *track);

/**
 * For filtered URIs tree model
 */

void
parrillada_track_data_cfg_dont_filter_uri (ParrilladaTrackDataCfg *track,
					const gchar *uri);

GSList *
parrillada_track_data_cfg_get_restored_list (ParrilladaTrackDataCfg *track);

enum  {
	PARRILLADA_FILTERED_STOCK_ID_COL,
	PARRILLADA_FILTERED_URI_COL,
	PARRILLADA_FILTERED_STATUS_COL,
	PARRILLADA_FILTERED_FATAL_ERROR_COL,
	PARRILLADA_FILTERED_NB_COL,
};


void
parrillada_track_data_cfg_restore (ParrilladaTrackDataCfg *track,
				GtkTreePath *treepath);

GtkTreeModel *
parrillada_track_data_cfg_get_filtered_model (ParrilladaTrackDataCfg *track);


/**
 * Track Spanning
 */

ParrilladaBurnResult
parrillada_track_data_cfg_span (ParrilladaTrackDataCfg *track,
			     goffset sectors,
			     ParrilladaTrackData *new_track);
ParrilladaBurnResult
parrillada_track_data_cfg_span_again (ParrilladaTrackDataCfg *track);

ParrilladaBurnResult
parrillada_track_data_cfg_span_possible (ParrilladaTrackDataCfg *track,
				      goffset sectors);

goffset
parrillada_track_data_cfg_span_max_space (ParrilladaTrackDataCfg *track);

void
parrillada_track_data_cfg_span_stop (ParrilladaTrackDataCfg *track);

/**
 * Icon
 */

GIcon *
parrillada_track_data_cfg_get_icon (ParrilladaTrackDataCfg *track);

gchar *
parrillada_track_data_cfg_get_icon_path (ParrilladaTrackDataCfg *track);

gboolean
parrillada_track_data_cfg_set_icon (ParrilladaTrackDataCfg *track,
				 const gchar *icon_path,
				 GError **error);

G_END_DECLS

#endif /* _PARRILLADA_TRACK_DATA_CFG_H_ */

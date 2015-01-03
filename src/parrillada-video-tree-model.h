/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_VIDEO_TREE_MODEL_H_
#define _PARRILLADA_VIDEO_TREE_MODEL_H_

#include <glib-object.h>

#include "parrillada-track.h"
#include "parrillada-session-cfg.h"

G_BEGIN_DECLS

/* This DND target when moving nodes inside ourselves */
#define PARRILLADA_DND_TARGET_SELF_FILE_NODES	"GTK_TREE_MODEL_ROW"

struct _ParrilladaDNDVideoContext {
	GtkTreeModel *model;
	GList *references;
};
typedef struct _ParrilladaDNDVideoContext ParrilladaDNDVideoContext;

typedef enum {
	PARRILLADA_VIDEO_TREE_MODEL_NAME		= 0,		/* Markup */
	PARRILLADA_VIDEO_TREE_MODEL_ARTIST		= 1,
	PARRILLADA_VIDEO_TREE_MODEL_THUMBNAIL,
	PARRILLADA_VIDEO_TREE_MODEL_ICON_NAME,
	PARRILLADA_VIDEO_TREE_MODEL_SIZE,
	PARRILLADA_VIDEO_TREE_MODEL_EDITABLE,
	PARRILLADA_VIDEO_TREE_MODEL_SELECTABLE,
	PARRILLADA_VIDEO_TREE_MODEL_INDEX,
	PARRILLADA_VIDEO_TREE_MODEL_INDEX_NUM,
	PARRILLADA_VIDEO_TREE_MODEL_IS_GAP,
	PARRILLADA_VIDEO_TREE_MODEL_WEIGHT,
	PARRILLADA_VIDEO_TREE_MODEL_STYLE,
	PARRILLADA_VIDEO_TREE_MODEL_COL_NUM
} ParrilladaVideoProjectColumn;

#define PARRILLADA_TYPE_VIDEO_TREE_MODEL             (parrillada_video_tree_model_get_type ())
#define PARRILLADA_VIDEO_TREE_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_VIDEO_TREE_MODEL, ParrilladaVideoTreeModel))
#define PARRILLADA_VIDEO_TREE_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_VIDEO_TREE_MODEL, ParrilladaVideoTreeModelClass))
#define PARRILLADA_IS_VIDEO_TREE_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_VIDEO_TREE_MODEL))
#define PARRILLADA_IS_VIDEO_TREE_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_VIDEO_TREE_MODEL))
#define PARRILLADA_VIDEO_TREE_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_VIDEO_TREE_MODEL, ParrilladaVideoTreeModelClass))

typedef struct _ParrilladaVideoTreeModelClass ParrilladaVideoTreeModelClass;
typedef struct _ParrilladaVideoTreeModel ParrilladaVideoTreeModel;

struct _ParrilladaVideoTreeModelClass
{
	GObjectClass parent_class;
};

struct _ParrilladaVideoTreeModel
{
	GObject parent_instance;
};

GType parrillada_video_tree_model_get_type (void) G_GNUC_CONST;

ParrilladaVideoTreeModel *
parrillada_video_tree_model_new (void);

void
parrillada_video_tree_model_set_session (ParrilladaVideoTreeModel *model,
				      ParrilladaSessionCfg *session);
ParrilladaSessionCfg *
parrillada_video_tree_model_get_session (ParrilladaVideoTreeModel *model);

ParrilladaTrack *
parrillada_video_tree_model_path_to_track (ParrilladaVideoTreeModel *self,
					GtkTreePath *path);

GtkTreePath *
parrillada_video_tree_model_track_to_path (ParrilladaVideoTreeModel *self,
				        ParrilladaTrack *track);

void
parrillada_video_tree_model_move_before (ParrilladaVideoTreeModel *self,
				      GtkTreeIter *iter,
				      GtkTreePath *dest_before);

G_END_DECLS

#endif /* _PARRILLADA_VIDEO_TREE_MODEL_H_ */

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

#ifndef _PARRILLADA_TASK_ITEM_H
#define _PARRILLADA_TASK_ITEM_H

#include <glib-object.h>

#include "burn-basics.h"
#include "burn-task-ctx.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TASK_ITEM			(parrillada_task_item_get_type ())
#define PARRILLADA_TASK_ITEM(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TASK_ITEM, ParrilladaTaskItem))
#define PARRILLADA_TASK_ITEM_CLASS(vtable)		(G_TYPE_CHECK_CLASS_CAST ((vtable), PARRILLADA_TYPE_TASK_ITEM, ParrilladaTaskItemIFace))
#define PARRILLADA_IS_TASK_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TASK_ITEM))
#define PARRILLADA_IS_TASK_ITEM_CLASS(vtable)	(G_TYPE_CHECK_CLASS_TYPE ((vtable), PARRILLADA_TYPE_TASK_ITEM))
#define PARRILLADA_TASK_ITEM_GET_CLASS(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), PARRILLADA_TYPE_TASK_ITEM, ParrilladaTaskItemIFace))

typedef struct _ParrilladaTaskItem ParrilladaTaskItem;
typedef struct _ParrilladaTaskItemIFace ParrilladaTaskItemIFace;

struct _ParrilladaTaskItemIFace {
	GTypeInterface parent;

	ParrilladaBurnResult	(*link)		(ParrilladaTaskItem *input,
						 ParrilladaTaskItem *output);
	ParrilladaTaskItem *	(*previous)	(ParrilladaTaskItem *item);
	ParrilladaTaskItem *	(*next)		(ParrilladaTaskItem *item);

	gboolean		(*is_active)	(ParrilladaTaskItem *item);
	ParrilladaBurnResult	(*activate)	(ParrilladaTaskItem *item,
						 ParrilladaTaskCtx *ctx,
						 GError **error);
	ParrilladaBurnResult	(*start)	(ParrilladaTaskItem *item,
						 GError **error);
	ParrilladaBurnResult	(*clock_tick)	(ParrilladaTaskItem *item,
						 ParrilladaTaskCtx *ctx,
						 GError **error);
	ParrilladaBurnResult	(*stop)		(ParrilladaTaskItem *item,
						 ParrilladaTaskCtx *ctx,
						 GError **error);
};

GType
parrillada_task_item_get_type (void);

ParrilladaBurnResult
parrillada_task_item_link (ParrilladaTaskItem *input,
			ParrilladaTaskItem *output);

ParrilladaTaskItem *
parrillada_task_item_previous (ParrilladaTaskItem *item);

ParrilladaTaskItem *
parrillada_task_item_next (ParrilladaTaskItem *item);

gboolean
parrillada_task_item_is_active (ParrilladaTaskItem *item);

ParrilladaBurnResult
parrillada_task_item_activate (ParrilladaTaskItem *item,
			    ParrilladaTaskCtx *ctx,
			    GError **error);

ParrilladaBurnResult
parrillada_task_item_start (ParrilladaTaskItem *item,
			 GError **error);

ParrilladaBurnResult
parrillada_task_item_clock_tick (ParrilladaTaskItem *item,
			      ParrilladaTaskCtx *ctx,
			      GError **error);

ParrilladaBurnResult
parrillada_task_item_stop (ParrilladaTaskItem *item,
			ParrilladaTaskCtx *ctx,
			GError **error);

G_END_DECLS

#endif

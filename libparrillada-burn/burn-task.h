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

#ifndef BURN_TASK_H
#define BURN_TASK_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-task-ctx.h"
#include "burn-task-item.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TASK         (parrillada_task_get_type ())
#define PARRILLADA_TASK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_TASK, ParrilladaTask))
#define PARRILLADA_TASK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_TASK, ParrilladaTaskClass))
#define PARRILLADA_IS_TASK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_TASK))
#define PARRILLADA_IS_TASK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_TASK))
#define PARRILLADA_TASK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_TASK, ParrilladaTaskClass))

typedef struct _ParrilladaTask ParrilladaTask;
typedef struct _ParrilladaTaskClass ParrilladaTaskClass;

struct _ParrilladaTask {
	ParrilladaTaskCtx parent;
};

struct _ParrilladaTaskClass {
	ParrilladaTaskCtxClass parent_class;
};

GType parrillada_task_get_type (void);

ParrilladaTask *parrillada_task_new (void);

void
parrillada_task_add_item (ParrilladaTask *task, ParrilladaTaskItem *item);

void
parrillada_task_reset (ParrilladaTask *task);

ParrilladaBurnResult
parrillada_task_run (ParrilladaTask *task,
		  GError **error);

ParrilladaBurnResult
parrillada_task_check (ParrilladaTask *task,
		    GError **error);

ParrilladaBurnResult
parrillada_task_cancel (ParrilladaTask *task,
		     gboolean protect);

gboolean
parrillada_task_is_running (ParrilladaTask *task);

ParrilladaBurnResult
parrillada_task_get_output_type (ParrilladaTask *task, ParrilladaTrackType *output);

G_END_DECLS

#endif /* BURN_TASK_H */

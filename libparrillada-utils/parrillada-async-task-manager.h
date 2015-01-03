/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-misc is distributed in the hope that it will be useful,
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

#ifndef ASYNC_TASK_MANAGER_H
#define ASYNC_TASK_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_ASYNC_TASK_MANAGER         (parrillada_async_task_manager_get_type ())
#define PARRILLADA_ASYNC_TASK_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_ASYNC_TASK_MANAGER, ParrilladaAsyncTaskManager))
#define PARRILLADA_ASYNC_TASK_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_ASYNC_TASK_MANAGER, ParrilladaAsyncTaskManagerClass))
#define PARRILLADA_IS_ASYNC_TASK_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_ASYNC_TASK_MANAGER))
#define PARRILLADA_IS_ASYNC_TASK_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_ASYNC_TASK_MANAGER))
#define PARRILLADA_ASYNC_TASK_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_ASYNC_TASK_MANAGER, ParrilladaAsyncTaskManagerClass))

typedef struct ParrilladaAsyncTaskManagerPrivate ParrilladaAsyncTaskManagerPrivate;
typedef struct _ParrilladaAsyncTaskManagerClass ParrilladaAsyncTaskManagerClass;
typedef struct _ParrilladaAsyncTaskManager ParrilladaAsyncTaskManager;

struct _ParrilladaAsyncTaskManager {
	GObject parent;
	ParrilladaAsyncTaskManagerPrivate *priv;
};

struct _ParrilladaAsyncTaskManagerClass {
	GObjectClass parent_class;
};

GType parrillada_async_task_manager_get_type (void);

typedef enum {
	PARRILLADA_ASYNC_TASK_FINISHED		= 0,
	PARRILLADA_ASYNC_TASK_RESCHEDULE		= 1
} ParrilladaAsyncTaskResult;

typedef ParrilladaAsyncTaskResult	(*ParrilladaAsyncThread)		(ParrilladaAsyncTaskManager *manager,
								 GCancellable *cancel,
								 gpointer user_data);
typedef void			(*ParrilladaAsyncDestroy)		(ParrilladaAsyncTaskManager *manager,
								 gboolean cancelled,
								 gpointer user_data);
typedef gboolean		(*ParrilladaAsyncFindTask)		(ParrilladaAsyncTaskManager *manager,
								 gpointer task,
								 gpointer user_data);

struct _ParrilladaAsyncTaskType {
	ParrilladaAsyncThread thread;
	ParrilladaAsyncDestroy destroy;
};
typedef struct _ParrilladaAsyncTaskType ParrilladaAsyncTaskType;

typedef enum {
	/* used internally when reschedule */
	PARRILLADA_ASYNC_RESCHEDULE	= 1,

	PARRILLADA_ASYNC_IDLE		= 1 << 1,
	PARRILLADA_ASYNC_NORMAL		= 1 << 2,
	PARRILLADA_ASYNC_URGENT		= 1 << 3
} ParrilladaAsyncPriority;

gboolean
parrillada_async_task_manager_queue (ParrilladaAsyncTaskManager *manager,
				  ParrilladaAsyncPriority priority,
				  const ParrilladaAsyncTaskType *type,
				  gpointer data);

gboolean
parrillada_async_task_manager_foreach_active (ParrilladaAsyncTaskManager *manager,
					   ParrilladaAsyncFindTask func,
					   gpointer user_data);
gboolean
parrillada_async_task_manager_foreach_active_remove (ParrilladaAsyncTaskManager *manager,
						  ParrilladaAsyncFindTask func,
						  gpointer user_data);
gboolean
parrillada_async_task_manager_foreach_unprocessed_remove (ParrilladaAsyncTaskManager *self,
						       ParrilladaAsyncFindTask func,
						       gpointer user_data);

gboolean
parrillada_async_task_manager_find_urgent_task (ParrilladaAsyncTaskManager *manager,
					     ParrilladaAsyncFindTask func,
					     gpointer user_data);

G_END_DECLS

#endif /* ASYNC_JOB_MANAGER_H */

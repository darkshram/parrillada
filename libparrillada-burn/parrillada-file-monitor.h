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

#ifndef _PARRILLADA_FILE_MONITOR_H_
#define _PARRILLADA_FILE_MONITOR_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_FILE_MONITOR_FILE,
	PARRILLADA_FILE_MONITOR_FOLDER
} ParrilladaFileMonitorType;

typedef	gboolean	(*ParrilladaMonitorFindFunc)	(gpointer data, gpointer callback_data);

#define PARRILLADA_TYPE_FILE_MONITOR             (parrillada_file_monitor_get_type ())
#define PARRILLADA_FILE_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_FILE_MONITOR, ParrilladaFileMonitor))
#define PARRILLADA_FILE_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_FILE_MONITOR, ParrilladaFileMonitorClass))
#define PARRILLADA_IS_FILE_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_FILE_MONITOR))
#define PARRILLADA_IS_FILE_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_FILE_MONITOR))
#define PARRILLADA_FILE_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_FILE_MONITOR, ParrilladaFileMonitorClass))

typedef struct _ParrilladaFileMonitorClass ParrilladaFileMonitorClass;
typedef struct _ParrilladaFileMonitor ParrilladaFileMonitor;

struct _ParrilladaFileMonitorClass
{
	GObjectClass parent_class;

	/* Virtual functions */

	/* if name is NULL then it's happening on the callback_data */
	void		(*file_added)		(ParrilladaFileMonitor *monitor,
						 gpointer callback_data,
						 const gchar *name);

	/* NOTE: there is no dest_type here as it must be a FOLDER type */
	void		(*file_moved)		(ParrilladaFileMonitor *self,
						 ParrilladaFileMonitorType src_type,
						 gpointer callback_src,
						 const gchar *name_src,
						 gpointer callback_dest,
						 const gchar *name_dest);
	void		(*file_renamed)		(ParrilladaFileMonitor *monitor,
						 ParrilladaFileMonitorType type,
						 gpointer callback_data,
						 const gchar *old_name,
						 const gchar *new_name);
	void		(*file_removed)		(ParrilladaFileMonitor *monitor,
						 ParrilladaFileMonitorType type,
						 gpointer callback_data,
						 const gchar *name);
	void		(*file_modified)	(ParrilladaFileMonitor *monitor,
						 gpointer callback_data,
						 const gchar *name);
};

struct _ParrilladaFileMonitor
{
	GObject parent_instance;
};

GType parrillada_file_monitor_get_type (void) G_GNUC_CONST;

gboolean
parrillada_file_monitor_single_file (ParrilladaFileMonitor *monitor,
				  const gchar *uri,
				  gpointer callback_data);
gboolean
parrillada_file_monitor_directory_contents (ParrilladaFileMonitor *monitor,
				  	 const gchar *uri,
				       	 gpointer callback_data);
void
parrillada_file_monitor_reset (ParrilladaFileMonitor *monitor);

void
parrillada_file_monitor_foreach_cancel (ParrilladaFileMonitor *self,
				     ParrilladaMonitorFindFunc func,
				     gpointer callback_data);

G_END_DECLS

#endif /* _PARRILLADA_FILE_MONITOR_H_ */

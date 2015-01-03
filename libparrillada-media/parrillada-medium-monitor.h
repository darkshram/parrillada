/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-media is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_MEDIUM_MONITOR_H_
#define _PARRILLADA_MEDIUM_MONITOR_H_

#include <glib-object.h>

#include <parrillada-medium.h>
#include <parrillada-drive.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_MEDIUM_MONITOR             (parrillada_medium_monitor_get_type ())
#define PARRILLADA_MEDIUM_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_MEDIUM_MONITOR, ParrilladaMediumMonitor))
#define PARRILLADA_MEDIUM_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_MEDIUM_MONITOR, ParrilladaMediumMonitorClass))
#define PARRILLADA_IS_MEDIUM_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_MEDIUM_MONITOR))
#define PARRILLADA_IS_MEDIUM_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_MEDIUM_MONITOR))
#define PARRILLADA_MEDIUM_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_MEDIUM_MONITOR, ParrilladaMediumMonitorClass))

typedef struct _ParrilladaMediumMonitorClass ParrilladaMediumMonitorClass;
typedef struct _ParrilladaMediumMonitor ParrilladaMediumMonitor;


struct _ParrilladaMediumMonitorClass
{
	GObjectClass parent_class;

	/* Signals */
	void		(* drive_added)		(ParrilladaMediumMonitor *monitor,
						 ParrilladaDrive *drive);

	void		(* drive_removed)	(ParrilladaMediumMonitor *monitor,
						 ParrilladaDrive *drive);

	void		(* medium_added)	(ParrilladaMediumMonitor *monitor,
						 ParrilladaMedium *medium);

	void		(* medium_removed)	(ParrilladaMediumMonitor *monitor,
						 ParrilladaMedium *medium);
};

struct _ParrilladaMediumMonitor
{
	GObject parent_instance;
};

GType parrillada_medium_monitor_get_type (void) G_GNUC_CONST;

ParrilladaMediumMonitor *
parrillada_medium_monitor_get_default (void);

typedef enum {
	PARRILLADA_MEDIA_TYPE_NONE				= 0,
	PARRILLADA_MEDIA_TYPE_FILE				= 1,
	PARRILLADA_MEDIA_TYPE_DATA				= 1 << 1,
	PARRILLADA_MEDIA_TYPE_AUDIO			= 1 << 2,
	PARRILLADA_MEDIA_TYPE_WRITABLE			= 1 << 3,
	PARRILLADA_MEDIA_TYPE_REWRITABLE			= 1 << 4,
	PARRILLADA_MEDIA_TYPE_ANY_IN_BURNER		= 1 << 5,

	/* If combined with other flags it will filter.
	 * if alone all CDs are returned.
	 * It can't be combined with FILE type. */
	PARRILLADA_MEDIA_TYPE_CD					= 1 << 6,

	PARRILLADA_MEDIA_TYPE_ALL_BUT_FILE			= 0xFE,
	PARRILLADA_MEDIA_TYPE_ALL				= 0xFF
} ParrilladaMediaType;

typedef enum {
	PARRILLADA_DRIVE_TYPE_NONE				= 0,
	PARRILLADA_DRIVE_TYPE_FILE				= 1,
	PARRILLADA_DRIVE_TYPE_WRITER			= 1 << 1,
	PARRILLADA_DRIVE_TYPE_READER			= 1 << 2,
	PARRILLADA_DRIVE_TYPE_ALL_BUT_FILE			= 0xFE,
	PARRILLADA_DRIVE_TYPE_ALL				= 0xFF
} ParrilladaDriveType;

GSList *
parrillada_medium_monitor_get_media (ParrilladaMediumMonitor *monitor,
				  ParrilladaMediaType type);

GSList *
parrillada_medium_monitor_get_drives (ParrilladaMediumMonitor *monitor,
				   ParrilladaDriveType type);

ParrilladaDrive *
parrillada_medium_monitor_get_drive (ParrilladaMediumMonitor *monitor,
				  const gchar *device);

gboolean
parrillada_medium_monitor_is_probing (ParrilladaMediumMonitor *monitor);

G_END_DECLS

#endif /* _PARRILLADA_MEDIUM_MONITOR_H_ */

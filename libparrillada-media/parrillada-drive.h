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

#include <glib-object.h>
#include <gio/gio.h>

#ifndef _BURN_DRIVE_H_
#define _BURN_DRIVE_H_

#include <parrillada-medium.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_DRIVE_CAPS_NONE			= 0,
	PARRILLADA_DRIVE_CAPS_CDR			= 1,
	PARRILLADA_DRIVE_CAPS_CDRW			= 1 << 1,
	PARRILLADA_DRIVE_CAPS_DVDR			= 1 << 2,
	PARRILLADA_DRIVE_CAPS_DVDRW		= 1 << 3,
	PARRILLADA_DRIVE_CAPS_DVDR_PLUS		= 1 << 4,
	PARRILLADA_DRIVE_CAPS_DVDRW_PLUS		= 1 << 5,
	PARRILLADA_DRIVE_CAPS_DVDR_PLUS_DL		= 1 << 6,
	PARRILLADA_DRIVE_CAPS_DVDRW_PLUS_DL	= 1 << 7,
	PARRILLADA_DRIVE_CAPS_DVDRAM		= 1 << 10,
	PARRILLADA_DRIVE_CAPS_BDR			= 1 << 8,
	PARRILLADA_DRIVE_CAPS_BDRW			= 1 << 9
} ParrilladaDriveCaps;

#define PARRILLADA_TYPE_DRIVE             (parrillada_drive_get_type ())
#define PARRILLADA_DRIVE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DRIVE, ParrilladaDrive))
#define PARRILLADA_DRIVE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DRIVE, ParrilladaDriveClass))
#define PARRILLADA_IS_DRIVE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DRIVE))
#define PARRILLADA_IS_DRIVE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DRIVE))
#define PARRILLADA_DRIVE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DRIVE, ParrilladaDriveClass))

typedef struct _ParrilladaDriveClass ParrilladaDriveClass;

struct _ParrilladaDriveClass
{
	GObjectClass parent_class;

	/* Signals */
	void		(* medium_added)	(ParrilladaDrive *drive,
						 ParrilladaMedium *medium);

	void		(* medium_removed)	(ParrilladaDrive *drive,
						 ParrilladaMedium *medium);
};

struct _ParrilladaDrive
{
	GObject parent_instance;
};

GType parrillada_drive_get_type (void) G_GNUC_CONST;

void
parrillada_drive_reprobe (ParrilladaDrive *drive);

ParrilladaMedium *
parrillada_drive_get_medium (ParrilladaDrive *drive);

GDrive *
parrillada_drive_get_gdrive (ParrilladaDrive *drive);

const gchar *
parrillada_drive_get_udi (ParrilladaDrive *drive);

gboolean
parrillada_drive_is_fake (ParrilladaDrive *drive);

gchar *
parrillada_drive_get_display_name (ParrilladaDrive *drive);

const gchar *
parrillada_drive_get_device (ParrilladaDrive *drive);

const gchar *
parrillada_drive_get_block_device (ParrilladaDrive *drive);

gchar *
parrillada_drive_get_bus_target_lun_string (ParrilladaDrive *drive);

ParrilladaDriveCaps
parrillada_drive_get_caps (ParrilladaDrive *drive);

gboolean
parrillada_drive_can_write_media (ParrilladaDrive *drive,
                               ParrilladaMedia media);

gboolean
parrillada_drive_can_write (ParrilladaDrive *drive);

gboolean
parrillada_drive_can_eject (ParrilladaDrive *drive);

gboolean
parrillada_drive_eject (ParrilladaDrive *drive,
		     gboolean wait,
		     GError **error);

void
parrillada_drive_cancel_current_operation (ParrilladaDrive *drive);

gboolean
parrillada_drive_is_door_open (ParrilladaDrive *drive);

gboolean
parrillada_drive_can_use_exclusively (ParrilladaDrive *drive);

gboolean
parrillada_drive_lock (ParrilladaDrive *drive,
		    const gchar *reason,
		    gchar **reason_for_failure);
gboolean
parrillada_drive_unlock (ParrilladaDrive *drive);

gboolean
parrillada_drive_is_locked (ParrilladaDrive *drive,
                         gchar **reason);

G_END_DECLS

#endif /* _BURN_DRIVE_H_ */

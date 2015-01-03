/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
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

#ifndef _PARRILLADA_VOLUME_H_
#define _PARRILLADA_VOLUME_H_

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <parrillada-drive.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_VOLUME             (parrillada_volume_get_type ())
#define PARRILLADA_VOLUME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_VOLUME, ParrilladaVolume))
#define PARRILLADA_VOLUME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_VOLUME, ParrilladaVolumeClass))
#define PARRILLADA_IS_VOLUME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_VOLUME))
#define PARRILLADA_IS_VOLUME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_VOLUME))
#define PARRILLADA_VOLUME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_VOLUME, ParrilladaVolumeClass))

typedef struct _ParrilladaVolumeClass ParrilladaVolumeClass;
typedef struct _ParrilladaVolume ParrilladaVolume;

struct _ParrilladaVolumeClass
{
	ParrilladaMediumClass parent_class;
};

struct _ParrilladaVolume
{
	ParrilladaMedium parent_instance;
};

GType parrillada_volume_get_type (void) G_GNUC_CONST;

gchar *
parrillada_volume_get_name (ParrilladaVolume *volume);

GIcon *
parrillada_volume_get_icon (ParrilladaVolume *volume);

GVolume *
parrillada_volume_get_gvolume (ParrilladaVolume *volume);

gboolean
parrillada_volume_is_mounted (ParrilladaVolume *volume);

gchar *
parrillada_volume_get_mount_point (ParrilladaVolume *volume,
				GError **error);

gboolean
parrillada_volume_umount (ParrilladaVolume *volume,
		       gboolean wait,
		       GError **error);

gboolean
parrillada_volume_mount (ParrilladaVolume *volume,
		      GtkWindow *parent_window,
		      gboolean wait,
		      GError **error);

void
parrillada_volume_cancel_current_operation (ParrilladaVolume *volume);

G_END_DECLS

#endif /* _PARRILLADA_VOLUME_H_ */

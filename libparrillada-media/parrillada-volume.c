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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "parrillada-media-private.h"
#include "parrillada-volume.h"
#include "parrillada-gio-operation.h"

typedef struct _ParrilladaVolumePrivate ParrilladaVolumePrivate;
struct _ParrilladaVolumePrivate
{
	GCancellable *cancel;
};

#define PARRILLADA_VOLUME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_VOLUME, ParrilladaVolumePrivate))

G_DEFINE_TYPE (ParrilladaVolume, parrillada_volume, PARRILLADA_TYPE_MEDIUM);

/**
 * parrillada_volume_get_gvolume:
 * @volume: #ParrilladaVolume
 *
 * Gets the corresponding #GVolume for @volume.
 *
 * Return value: a #GVolume *.
 *
 **/
GVolume *
parrillada_volume_get_gvolume (ParrilladaVolume *volume)
{
	const gchar *volume_path = NULL;
	ParrilladaVolumePrivate *priv;
	GVolumeMonitor *monitor;
	GVolume *gvolume = NULL;
	ParrilladaDrive *drive;
	GList *volumes;
	GList *iter;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), NULL);

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	drive = parrillada_medium_get_drive (PARRILLADA_MEDIUM (volume));

	/* This returns the block device which is the
	 * same as the device for all OSes except
	 * Solaris where the device is the raw device. */
	volume_path = parrillada_drive_get_block_device (drive);

	/* NOTE: medium-monitor already holds a reference for GVolumeMonitor */
	monitor = g_volume_monitor_get ();
	volumes = g_volume_monitor_get_volumes (monitor);
	g_object_unref (monitor);

	for (iter = volumes; iter; iter = iter->next) {
		gchar *device_path;
		GVolume *tmp;

		tmp = iter->data;
		device_path = g_volume_get_identifier (tmp, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (!device_path)
			continue;

		PARRILLADA_MEDIA_LOG ("Found volume %s", device_path);
		if (!strcmp (device_path, volume_path)) {
			gvolume = tmp;
			g_free (device_path);
			g_object_ref (gvolume);
			break;
		}

		g_free (device_path);
	}
	g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
	g_list_free (volumes);

	if (!gvolume)
		PARRILLADA_MEDIA_LOG ("No volume found for medium");

	return gvolume;
}

/**
 * parrillada_volume_is_mounted:
 * @volume: #ParrilladaVolume
 *
 * Returns whether the volume is currently mounted.
 *
 * Return value: a #gboolean. TRUE if it is mounted.
 *
 **/
gboolean
parrillada_volume_is_mounted (ParrilladaVolume *volume)
{
	gchar *path;

	g_return_val_if_fail (volume != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), FALSE);

	/* NOTE: that's the surest way to know if a drive is really mounted. For
	 * GIO a blank medium can be mounted to burn:/// which is not really 
	 * what we're interested in. So the mount path must be also local. */
	path = parrillada_volume_get_mount_point (volume, NULL);
	if (path) {
		g_free (path);
		return TRUE;
	}

	return FALSE;
}

/**
 * parrillada_volume_get_mount_point:
 * @volume: #ParrilladaVolume
 * @error: #GError **
 *
 * Returns the path for mount point for @volume.
 *
 * Return value: a #gchar *
 *
 **/
gchar *
parrillada_volume_get_mount_point (ParrilladaVolume *volume,
				GError **error)
{
	ParrilladaVolumePrivate *priv;
	gchar *local_path = NULL;
	GVolume *gvolume;
	GMount *mount;
	GFile *root;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), NULL);

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	gvolume = parrillada_volume_get_gvolume (volume);
	if (!gvolume)
		return NULL;

	/* get the uri for the mount point */
	mount = g_volume_get_mount (gvolume);
	g_object_unref (gvolume);
	if (!mount)
		return NULL;

	root = g_mount_get_root (mount);
	g_object_unref (mount);

	if (!root) {
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     _("The disc mount point could not be retrieved"));
	}
	else {
		local_path = g_file_get_path (root);
		g_object_unref (root);
		PARRILLADA_MEDIA_LOG ("Mount point is %s", local_path);
	}

	return local_path;
}

/**
 * parrillada_volume_umount:
 * @volume: #ParrilladaVolume
 * @wait: #gboolean
 * @error: #GError **
 *
 * Unmount @volume. If wait is set to TRUE, then block (in a GMainLoop) until
 * the operation finishes.
 *
 * Return value: a #gboolean. TRUE if the operation succeeded.
 *
 **/
gboolean
parrillada_volume_umount (ParrilladaVolume *volume,
		       gboolean wait,
		       GError **error)
{
	gboolean result;
	GVolume *gvolume;
	ParrilladaVolumePrivate *priv;

	if (!volume)
		return TRUE;

	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), FALSE);

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	gvolume = parrillada_volume_get_gvolume (volume);
	if (!gvolume)
		return TRUE;

	if (g_cancellable_is_cancelled (priv->cancel)) {
		PARRILLADA_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	result = parrillada_gio_operation_umount (gvolume,
					       priv->cancel,
					       wait,
					       error);
	g_object_unref (gvolume);

	return result;
}

/**
 * parrillada_volume_mount:
 * @volume: #ParrilladaVolume *
 * @parent_window: #GtkWindow *
 * @wait: #gboolean
 * @error: #GError **
 *
 * Mount @volume. If wait is set to TRUE, then block (in a GMainLoop) until
 * the operation finishes.
 * @parent_window is used if an authentification is needed. Then the authentification
 * dialog will be set modal.
 *
 * Return value: a #gboolean. TRUE if the operation succeeded.
 *
 **/
gboolean
parrillada_volume_mount (ParrilladaVolume *volume,
		      GtkWindow *parent_window,
		      gboolean wait,
		      GError **error)
{
	gboolean result;
	GVolume *gvolume;
	ParrilladaVolumePrivate *priv;

	if (!volume)
		return TRUE;

	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), FALSE);

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	gvolume = parrillada_volume_get_gvolume (volume);
	if (!gvolume)
		return TRUE;

	if (g_cancellable_is_cancelled (priv->cancel)) {
		PARRILLADA_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	result = parrillada_gio_operation_mount (gvolume,
					      parent_window,
					      priv->cancel,
					      wait,
					      error);
	g_object_unref (gvolume);

	return result;
}

/**
 * parrillada_volume_cancel_current_operation:
 * @volume: #ParrilladaVolume *
 *
 * Cancels all operations currently running for @volume
 *
 **/
void
parrillada_volume_cancel_current_operation (ParrilladaVolume *volume)
{
	ParrilladaVolumePrivate *priv;

	g_return_if_fail (volume != NULL);
	g_return_if_fail (PARRILLADA_IS_VOLUME (volume));

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	PARRILLADA_MEDIA_LOG ("Cancelling volume operation");

	g_cancellable_cancel (priv->cancel);
}

/**
 * parrillada_volume_get_icon:
 * @volume: #ParrilladaVolume *
 *
 * Returns a GIcon pointer for the volume.
 *
 * Return value: a #GIcon*
 *
 **/
GIcon *
parrillada_volume_get_icon (ParrilladaVolume *volume)
{
	GVolume *gvolume;
	GMount *mount;
	GIcon *icon;

	if (!volume)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), NULL);

	if (parrillada_medium_get_status (PARRILLADA_MEDIUM (volume)) == PARRILLADA_MEDIUM_FILE)
		return g_themed_icon_new_with_default_fallbacks ("iso-image-new");

	gvolume = parrillada_volume_get_gvolume (volume);
	if (!gvolume)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	mount = g_volume_get_mount (gvolume);
	if (mount) {
		icon = g_mount_get_icon (mount);
		g_object_unref (mount);
	}
	else
		icon = g_volume_get_icon (gvolume);

	g_object_unref (gvolume);

	return icon;
}

/**
 * parrillada_volume_get_name:
 * @volume: #ParrilladaVolume *
 *
 * Returns a string that can be displayed to represent the volume²
 *
 * Return value: a #gchar *. Free when not needed anymore.
 *
 **/
gchar *
parrillada_volume_get_name (ParrilladaVolume *volume)
{
	ParrilladaVolumePrivate *priv;
	ParrilladaMedia media;
	const gchar *type;
	GVolume *gvolume;
	gchar *name;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_VOLUME (volume), NULL);

	priv = PARRILLADA_VOLUME_PRIVATE (volume);

	media = parrillada_medium_get_status (PARRILLADA_MEDIUM (volume));
	if (media & PARRILLADA_MEDIUM_FILE) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	if (media & PARRILLADA_MEDIUM_HAS_AUDIO) {
		const gchar *audio_name;

		audio_name = parrillada_medium_get_CD_TEXT_title (PARRILLADA_MEDIUM (volume));
		if (audio_name)
			return g_strdup (audio_name);
	}

	gvolume = parrillada_volume_get_gvolume (volume);
	if (!gvolume)
		goto last_chance;

	name = g_volume_get_name (gvolume);
	g_object_unref (gvolume);

	if (name)
		return name;

last_chance:

	type = parrillada_medium_get_type_string (PARRILLADA_MEDIUM (volume));
	name = NULL;
	if (media & PARRILLADA_MEDIUM_BLANK) {
		/* NOTE for translators: the first %s is the disc type and Blank is an adjective. */
		name = g_strdup_printf (_("Blank disc (%s)"), type);
	}
	else if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_HAS_AUDIO|PARRILLADA_MEDIUM_HAS_DATA)) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio and data disc (%s)"), type);
	}
	else if (media & PARRILLADA_MEDIUM_HAS_AUDIO) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio disc (%s)"), type);
	}
	else if (media & PARRILLADA_MEDIUM_HAS_DATA) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Data disc (%s)"), type);
	}
	else {
		name = g_strdup (type);
	}

	return name;
}

static void
parrillada_volume_init (ParrilladaVolume *object)
{
	ParrilladaVolumePrivate *priv;

	priv = PARRILLADA_VOLUME_PRIVATE (object);
	priv->cancel = g_cancellable_new ();
}

static void
parrillada_volume_finalize (GObject *object)
{
	ParrilladaVolumePrivate *priv;

	priv = PARRILLADA_VOLUME_PRIVATE (object);

	PARRILLADA_MEDIA_LOG ("Finalizing Volume object");
	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	G_OBJECT_CLASS (parrillada_volume_parent_class)->finalize (object);
}

static void
parrillada_volume_class_init (ParrilladaVolumeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaVolumePrivate));

	object_class->finalize = parrillada_volume_finalize;
}

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <string.h>

#ifdef HAVE_CAM_LIB_H
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <camlib.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "parrillada-media-private.h"
#include "parrillada-gio-operation.h"

#include "parrillada-medium.h"
#include "parrillada-volume.h"
#include "parrillada-drive.h"

#include "parrillada-drive-priv.h"
#include "scsi-device.h"
#include "scsi-utils.h"
#include "scsi-spc1.h"
#include "scsi-mmc1.h"
#include "scsi-mmc2.h"
#include "scsi-status-page.h"
#include "scsi-mode-pages.h"
#include "scsi-sbc.h"

typedef struct _ParrilladaDrivePrivate ParrilladaDrivePrivate;
struct _ParrilladaDrivePrivate
{
	GDrive *gdrive;

	GThread *probe;
	GMutex *mutex;
	GCond *cond;
	GCond *cond_probe;
	gint probe_id;

	ParrilladaMedium *medium;
	ParrilladaDriveCaps caps;

	gchar *udi;

	gchar *name;

	gchar *device;
	gchar *block_device;

	GCancellable *cancel;

	guint initial_probe:1;
	guint initial_probe_cancelled:1;

	guint has_medium:1;
	guint probe_cancelled:1;

	guint locked:1;
	guint ejecting:1;
	guint probe_waiting:1;
};

#define PARRILLADA_DRIVE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DRIVE, ParrilladaDrivePrivate))

enum {
	MEDIUM_REMOVED,
	MEDIUM_INSERTED,
	LAST_SIGNAL
};
static gulong drive_signals [LAST_SIGNAL] = {0, };

enum {
	PROP_NONE	= 0,
	PROP_DEVICE,
	PROP_GDRIVE,
	PROP_UDI
};

G_DEFINE_TYPE (ParrilladaDrive, parrillada_drive, G_TYPE_OBJECT);

#define PARRILLADA_DRIVE_OPEN_ATTEMPTS			5

static void
parrillada_drive_probe_inside (ParrilladaDrive *drive);

/**
 * parrillada_drive_get_gdrive:
 * @drive: a #ParrilladaDrive
 *
 * Returns the #GDrive corresponding to this #ParrilladaDrive
 *
 * Return value: a #GDrive or NULL. Unref after use.
 **/
GDrive *
parrillada_drive_get_gdrive (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	if (parrillada_drive_is_fake (drive))
		return NULL;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	if (!priv->gdrive)
		return NULL;

	return g_object_ref (priv->gdrive);
}

/**
 * parrillada_drive_can_eject:
 * @drive: #ParrilladaDrive
 *
 * Returns whether the drive can eject media.
 *
 * Return value: a #gboolean. TRUE if the media can be ejected, FALSE otherwise.
 *
 **/
gboolean
parrillada_drive_can_eject (ParrilladaDrive *drive)
{
	GVolume *volume;
	gboolean result;
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	if (!priv->gdrive) {
		PARRILLADA_MEDIA_LOG ("No GDrive");
		goto last_resort;
	}

	if (!g_drive_can_eject (priv->gdrive)) {
		PARRILLADA_MEDIA_LOG ("GDrive can't eject");
		goto last_resort;
	}

	return TRUE;

last_resort:

	if (!priv->medium)
		return FALSE;

	/* last resort */
	volume = parrillada_volume_get_gvolume (PARRILLADA_VOLUME (priv->medium));
	if (!volume)
		return FALSE;

	result = g_volume_can_eject (volume);
	g_object_unref (volume);

	return result;
}

static void
parrillada_drive_cancel_probing (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	priv->probe_waiting = FALSE;

	g_mutex_lock (priv->mutex);
	if (priv->probe) {
		/* This to signal that we are cancelling */
		priv->probe_cancelled = TRUE;
		priv->initial_probe_cancelled = TRUE;

		/* This is to wake up the thread if it
		 * was asleep waiting to retry to get
		 * hold of a handle to probe the drive */
		g_cond_signal (priv->cond_probe);

		g_cond_wait (priv->cond, priv->mutex);
	}
	g_mutex_unlock (priv->mutex);

	if (priv->probe_id) {
		g_source_remove (priv->probe_id);
		priv->probe_id = 0;
	}
}

static void
parrillada_drive_wait_probing_thread (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	g_mutex_lock (priv->mutex);
	if (priv->probe) {
		/* This is to wake up the thread if it
		 * was asleep waiting to retry to get
		 * hold of a handle to probe the drive */
		g_cond_signal (priv->cond_probe);
		g_cond_wait (priv->cond, priv->mutex);
	}
	g_mutex_unlock (priv->mutex);
}

/**
 * parrillada_drive_eject:
 * @drive: #ParrilladaDrive
 * @wait: #gboolean whether to wait for the completion of the operation (with a GMainLoop)
 * @error: #GError
 *
 * Open the drive tray or ejects the media if there is any inside.
 *
 * Return value: a #gboolean. TRUE on success, FALSE otherwise.
 *
 **/
gboolean
parrillada_drive_eject (ParrilladaDrive *drive,
		     gboolean wait,
		     GError **error)
{
	ParrilladaDrivePrivate *priv;
	GVolume *gvolume;
	gboolean res;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	/* reset if needed */
	if (g_cancellable_is_cancelled (priv->cancel)) {
		PARRILLADA_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	PARRILLADA_MEDIA_LOG ("Trying to eject drive");
	if (priv->gdrive) {
		/* Wait for any ongoing probing as it
		 * would prevent the door from being
		 * opened. */
		parrillada_drive_wait_probing_thread (drive);

		priv->ejecting = TRUE;
		res = parrillada_gio_operation_eject_drive (priv->gdrive,
							 priv->cancel,
							 wait,
							 error);
		priv->ejecting = FALSE;
		if (priv->probe_waiting)
			parrillada_drive_probe_inside (drive);

		if (res)
			return TRUE;

		if (g_cancellable_is_cancelled (priv->cancel))
			return FALSE;
	}
	else
		PARRILLADA_MEDIA_LOG ("No GDrive");

	if (!priv->medium)
		return FALSE;

	/* reset if needed */
	if (g_cancellable_is_cancelled (priv->cancel)) {
		PARRILLADA_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	gvolume = parrillada_volume_get_gvolume (PARRILLADA_VOLUME (priv->medium));
	if (gvolume) {
		PARRILLADA_MEDIA_LOG ("Trying to eject volume");

		/* Cancel any ongoing probing as it
		 * would prevent the door from being
		 * opened. */
		parrillada_drive_wait_probing_thread (drive);

		priv->ejecting = TRUE;
		res = parrillada_gio_operation_eject_volume (gvolume,
							  priv->cancel,
							  wait,
							  error);

		priv->ejecting = FALSE;
		if (priv->probe_waiting)
			parrillada_drive_probe_inside (drive);

		g_object_unref (gvolume);
	}

	return res;
}

/**
 * parrillada_drive_cancel_current_operation:
 * @drive: #ParrilladaDrive *
 *
 * Cancels all operations currently running for @drive
 *
 **/
void
parrillada_drive_cancel_current_operation (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_if_fail (drive != NULL);
	g_return_if_fail (PARRILLADA_IS_DRIVE (drive));

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	PARRILLADA_MEDIA_LOG ("Cancelling GIO operation");
	g_cancellable_cancel (priv->cancel);
}

/**
 * parrillada_drive_get_bus_target_lun_string:
 * @drive: a #ParrilladaDrive
 *
 * Returns the bus, target, lun ("{bus},{target},{lun}") as a string which is
 * sometimes needed by some backends like cdrecord.
 *
 * NOTE: that function returns either bus/target/lun or the device path
 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
 * which is the only OS in need for that. For all others it returns the device
 * path. 
 *
 * Return value: a string or NULL. The string must be freed when not needed
 *
 **/
gchar *
parrillada_drive_get_bus_target_lun_string (ParrilladaDrive *drive)
{
	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	return parrillada_device_get_bus_target_lun (parrillada_drive_get_device (drive));
}

/**
 * parrillada_drive_is_fake:
 * @drive: a #ParrilladaDrive
 *
 * Returns whether or not the drive is a fake one. There is only one and
 * corresponds to a file which is used when the user wants to burn to a file.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
parrillada_drive_is_fake (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return (priv->device == NULL);
}

/**
 * parrillada_drive_is_door_open:
 * @drive: a #ParrilladaDrive
 *
 * Returns whether or not the drive door is open.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
parrillada_drive_is_door_open (ParrilladaDrive *drive)
{
	const gchar *device;
	ParrilladaDrivePrivate *priv;
	ParrilladaDeviceHandle *handle;
	ParrilladaScsiMechStatusHdr hdr;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = parrillada_drive_get_device (drive);
	handle = parrillada_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	parrillada_mmc1_mech_status (handle,
				  &hdr,
				  NULL);

	parrillada_device_handle_close (handle);

	return hdr.door_open;
}

/**
 * parrillada_drive_can_use_exclusively:
 * @drive: a #ParrilladaDrive
 *
 * Returns whether or not the drive can be used exclusively, that is whether or
 * not it is currently used by another application.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
parrillada_drive_can_use_exclusively (ParrilladaDrive *drive)
{
	ParrilladaDeviceHandle *handle;
	const gchar *device;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	device = parrillada_drive_get_device (drive);
	handle = parrillada_device_handle_open (device, TRUE, NULL);
	if (!handle)
		return FALSE;

	parrillada_device_handle_close (handle);
	return TRUE;
}

/**
 * parrillada_drive_is_locked:
 * @drive: a #ParrilladaDrive
 * @reason: a #gchar or NULL. A string to indicate what the drive was locked for if return value is %TRUE
 *
 * Checks whether a #ParrilladaDrive is currently locked. Manual ejection shouldn't be possible any more.
 *
 * Since 2.29.0
 *
 * Return value: %TRUE if the drive is locked or %FALSE.
 **/
gboolean
parrillada_drive_is_locked (ParrilladaDrive *drive,
                         gchar **reason)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return priv->locked;
}

/**
 * parrillada_drive_lock:
 * @drive: a #ParrilladaDrive
 * @reason: a string to indicate what the drive was locked for
 * @reason_for_failure: a string (or NULL) to hold the reason why the locking failed
 *
 * Locks a #ParrilladaDrive. Manual ejection shouldn't be possible any more.
 *
 * Return value: %TRUE if the drive was successfully locked or %FALSE.
 **/
gboolean
parrillada_drive_lock (ParrilladaDrive *drive,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	ParrilladaDeviceHandle *handle;
	ParrilladaDrivePrivate *priv;
	const gchar *device;
	gboolean result;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = parrillada_drive_get_device (drive);
	handle = parrillada_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (parrillada_sbc_medium_removal (handle, 1, NULL) == PARRILLADA_SCSI_OK);
	if (result) {
		PARRILLADA_MEDIA_LOG ("Device locked");
		priv->locked = TRUE;
	}
	else
		PARRILLADA_MEDIA_LOG ("Device failed to lock");

	parrillada_device_handle_close (handle);
	return result;
}

/**
 * parrillada_drive_unlock:
 * @drive: a #ParrilladaDrive
 *
 * Unlocks a #ParrilladaDrive.
 *
 * Return value: %TRUE if the drive was successfully unlocked or %FALSE.
 **/
gboolean
parrillada_drive_unlock (ParrilladaDrive *drive)
{
	ParrilladaDeviceHandle *handle;
	ParrilladaDrivePrivate *priv;
	const gchar *device;
	gboolean result;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = parrillada_drive_get_device (drive);
	handle = parrillada_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (parrillada_sbc_medium_removal (handle, 0, NULL) == PARRILLADA_SCSI_OK);
	if (result) {
		PARRILLADA_MEDIA_LOG ("Device unlocked");
		priv->locked = FALSE;

		if (priv->probe_waiting) {
			PARRILLADA_MEDIA_LOG ("Probe on hold");

			/* A probe was waiting */
			parrillada_drive_probe_inside (drive);
		}
	}
	else
		PARRILLADA_MEDIA_LOG ("Device failed to unlock");

	parrillada_device_handle_close (handle);

	return result;
}

/**
 * parrillada_drive_get_display_name:
 * @drive: a #ParrilladaDrive
 *
 * Gets a string holding the name for the drive. That string can be then
 * displayed in a user interface.
 *
 * Return value: a string holding the name
 **/
gchar *
parrillada_drive_get_display_name (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (!priv->device) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	return g_strdup (priv->name);
}

/**
 * parrillada_drive_get_device:
 * @drive: a #ParrilladaDrive
 *
 * Gets a string holding the device path for the drive.
 *
 * Return value: a string holding the device path.
 * On Solaris returns raw device.
 **/
const gchar *
parrillada_drive_get_device (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return priv->device;
}

/**
 * parrillada_drive_get_block_device:
 * @drive: a #ParrilladaDrive
 *
 * Gets a string holding the block device path for the drive. This can be used on
 * some other OSes, like Solaris, for GIO operations instead of the device
 * path.
 *
 * Solaris uses block device for GIO operations and
 * uses raw device for system calls and backends
 * like cdrtool.
 *
 * If such a path is not available, it returns the device path.
 *
 * Return value: a string holding the block device path
 **/
const gchar *
parrillada_drive_get_block_device (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return priv->block_device? priv->block_device:priv->device;
}

/**
 * parrillada_drive_get_udi:
 * @drive: a #ParrilladaDrive
 *
 * Gets a string holding the HAL udi corresponding to this device. It can be used
 * to uniquely identify the drive.
 *
 * Return value: a string holding the HAL udi or NULL. Not to be freed
 **/
const gchar *
parrillada_drive_get_udi (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (!priv->device || !priv->gdrive)
		return NULL;

	if (priv->udi)
		return priv->udi;

	priv->udi = g_drive_get_identifier (priv->gdrive, G_VOLUME_IDENTIFIER_KIND_HAL_UDI);
	return priv->udi;
}

/**
 * parrillada_drive_get_medium:
 * @drive: a #ParrilladaDrive
 *
 * Gets the medium currently inserted in the drive. If there is no medium or if
 * the medium is not probed yet then it returns NULL.
 *
 * Return value: (transfer none): a #ParrilladaMedium object or NULL. No need to unref after use.
 **/
ParrilladaMedium *
parrillada_drive_get_medium (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), NULL);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (parrillada_drive_probing (drive))
		return NULL;

	return priv->medium;
}

/**
 * parrillada_drive_get_caps:
 * @drive: a #ParrilladaDrive
 *
 * Returns what type(s) of disc the drive can write to.
 *
 * Return value: a #ParrilladaDriveCaps.
 **/
ParrilladaDriveCaps
parrillada_drive_get_caps (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, PARRILLADA_DRIVE_CAPS_NONE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), PARRILLADA_DRIVE_CAPS_NONE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return priv->caps;
}

/**
 * parrillada_drive_can_write_media:
 * @drive: a #ParrilladaDrive
 * @media: a #ParrilladaMedia
 *
 * Returns whether the disc can burn a specific media type.
 *
 * Since 2.29.0
 *
 * Return value: a #gboolean. TRUE if the drive can write this type of media and FALSE otherwise
 **/
gboolean
parrillada_drive_can_write_media (ParrilladaDrive *drive,
                               ParrilladaMedia media)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	if (!(media & PARRILLADA_MEDIUM_REWRITABLE)
	&&   (media & PARRILLADA_MEDIUM_CLOSED))
		return FALSE;

	if (media & PARRILLADA_MEDIUM_FILE)
		return FALSE;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_CDR))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_CDR) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDR))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDR) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDR_PLUS))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDR_PLUS) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_CDRW))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_CDRW) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDRW) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_RESTRICTED))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDRW) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDRW_PLUS) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDR_PLUS_DL))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDR_PLUS_DL) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS_DL))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDRW_PLUS_DL) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVD_RAM))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_DVDRAM) != 0;

	/* All types of BD-R */
	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_BD|PARRILLADA_MEDIUM_WRITABLE))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_BDR) != 0;

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_BDRE))
		return (priv->caps & PARRILLADA_DRIVE_CAPS_BDRW) != 0;

	return FALSE;
}

/**
 * parrillada_drive_can_write:
 * @drive: a #ParrilladaDrive
 *
 * Returns whether the disc can burn any disc at all.
 *
 * Return value: a #gboolean. TRUE if the drive can write a disc and FALSE otherwise
 **/
gboolean
parrillada_drive_can_write (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	return (priv->caps & (PARRILLADA_DRIVE_CAPS_CDR|
			      PARRILLADA_DRIVE_CAPS_DVDR|
			      PARRILLADA_DRIVE_CAPS_DVDR_PLUS|
			      PARRILLADA_DRIVE_CAPS_CDRW|
			      PARRILLADA_DRIVE_CAPS_DVDRW|
			      PARRILLADA_DRIVE_CAPS_DVDRW_PLUS|
			      PARRILLADA_DRIVE_CAPS_DVDR_PLUS_DL|
			      PARRILLADA_DRIVE_CAPS_DVDRW_PLUS_DL));
}

static void
parrillada_drive_medium_probed (ParrilladaMedium *medium,
			     ParrilladaDrive *self)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (self);

	/* only when it is probed */
	/* NOTE: ParrilladaMedium calls GDK_THREADS_ENTER/LEAVE() around g_signal_emit () */
	if (parrillada_medium_get_status (priv->medium) == PARRILLADA_MEDIUM_NONE) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
		return;
	}

	g_signal_emit (self,
		       drive_signals [MEDIUM_INSERTED],
		       0,
		       priv->medium);
}

/**
 * This is not public API. Defined in parrillada-drive-priv.h.
 */
gboolean
parrillada_drive_probing (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE (drive), FALSE);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (priv->probe != NULL)
		return TRUE;

	if (priv->medium)
		return parrillada_medium_probing (priv->medium);

	return FALSE;
}

static void
parrillada_drive_update_medium (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	if (priv->has_medium) {
		if (priv->medium) {
			PARRILLADA_MEDIA_LOG ("Already a medium. Skipping");
			return;
		}

		PARRILLADA_MEDIA_LOG ("Probing new medium");
		priv->medium = g_object_new (PARRILLADA_TYPE_VOLUME,
					     "drive", drive,
					     NULL);

		g_signal_connect (priv->medium,
				  "probed",
				  G_CALLBACK (parrillada_drive_medium_probed),
				  drive);
	}
	else if (priv->medium) {
		ParrilladaMedium *medium;

		PARRILLADA_MEDIA_LOG ("Medium removed");

		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (drive,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);

		g_object_unref (medium);
	}
}

static gboolean
parrillada_drive_probed_inside (gpointer data)
{
	ParrilladaDrive *self;
	ParrilladaDrivePrivate *priv;

	self = PARRILLADA_DRIVE (data);
	priv = PARRILLADA_DRIVE_PRIVATE (self);

	if (!g_mutex_trylock (priv->mutex))
		return TRUE;

	priv->probe_id = 0;
	g_mutex_unlock (priv->mutex);

	parrillada_drive_update_medium (self);
	return FALSE;
}

static gpointer
parrillada_drive_probe_inside_thread (gpointer data)
{
	gint counter = 0;
	GTimeVal wait_time;
	const gchar *device;
	ParrilladaScsiErrCode code;
	ParrilladaDrivePrivate *priv;
	ParrilladaDeviceHandle *handle = NULL;
	ParrilladaDrive *drive = PARRILLADA_DRIVE (data);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	device = parrillada_drive_get_device (drive);
	PARRILLADA_MEDIA_LOG ("Trying to open device %s", device);

	priv->has_medium = FALSE;

	handle = parrillada_device_handle_open (device, FALSE, &code);
	while (!handle && counter <= PARRILLADA_DRIVE_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->probe_cancelled) {
			PARRILLADA_MEDIA_LOG ("Open () cancelled");
			goto end;
		}

		counter ++;
		handle = parrillada_device_handle_open (device, FALSE, &code);
	}

	if (!handle) {
		PARRILLADA_MEDIA_LOG ("Open () failed: medium busy");
		goto end;
	}

	if (priv->probe_cancelled) {
		PARRILLADA_MEDIA_LOG ("Open () cancelled");

		parrillada_device_handle_close (handle);
		goto end;
	}

	while (parrillada_spc1_test_unit_ready (handle, &code) != PARRILLADA_SCSI_OK) {
		if (code == PARRILLADA_SCSI_NO_MEDIUM) {
			PARRILLADA_MEDIA_LOG ("No medium inserted");

			parrillada_device_handle_close (handle);
			goto end;
		}

		if (code != PARRILLADA_SCSI_NOT_READY) {
			PARRILLADA_MEDIA_LOG ("Device does not respond");

			parrillada_device_handle_close (handle);
			goto end;
		}

		g_get_current_time (&wait_time);
		g_time_val_add (&wait_time, 2000000);

		g_mutex_lock (priv->mutex);
		g_cond_timed_wait (priv->cond_probe,
		                   priv->mutex,
		                   &wait_time);
		g_mutex_unlock (priv->mutex);

		if (priv->probe_cancelled) {
			PARRILLADA_MEDIA_LOG ("Device probing cancelled");

			parrillada_device_handle_close (handle);
			goto end;
		}
	}

	PARRILLADA_MEDIA_LOG ("Medium inserted");
	parrillada_device_handle_close (handle);

	priv->has_medium = TRUE;

end:

	g_mutex_lock (priv->mutex);

	if (!priv->probe_cancelled)
		priv->probe_id = g_idle_add (parrillada_drive_probed_inside, drive);

	priv->probe = NULL;
	g_cond_broadcast (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (0);

	return NULL;
}

static void
parrillada_drive_probe_inside (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	if (priv->initial_probe) {
		PARRILLADA_MEDIA_LOG ("Still initializing the drive properties");
		return;
	}

	/* Check that a probe is not already being performed */
	if (priv->probe) {
		PARRILLADA_MEDIA_LOG ("Ongoing probe");
		parrillada_drive_cancel_probing (drive);
	}

	PARRILLADA_MEDIA_LOG ("Setting new probe");

	g_mutex_lock (priv->mutex);

	priv->probe_waiting = FALSE;
	priv->probe_cancelled = FALSE;

	priv->probe = g_thread_create (parrillada_drive_probe_inside_thread,
	                               drive,
				       FALSE,
				       NULL);

	g_mutex_unlock (priv->mutex);
}

static void
parrillada_drive_medium_gdrive_changed_cb (ParrilladaDrive *gdrive,
					ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (priv->locked || priv->ejecting) {
		PARRILLADA_MEDIA_LOG ("Waiting for next unlocking of the drive to probe");

		/* Since the drive was locked, it should
		 * not be possible that the medium
		 * actually changed.
		 * This allows to avoid probing while
		 * we are burning something.
		 * Delay the probe until parrillada_drive_unlock ()
		 * is called.  */
		priv->probe_waiting = TRUE;
		return;
	}

	PARRILLADA_MEDIA_LOG ("GDrive changed");
	parrillada_drive_probe_inside (drive);
}

static void
parrillada_drive_update_gdrive (ParrilladaDrive *drive,
                             GDrive *gdrive)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	if (priv->gdrive) {
		g_signal_handlers_disconnect_by_func (priv->gdrive,
						      parrillada_drive_medium_gdrive_changed_cb,
						      drive);

		/* Stop any ongoing GIO operation */
		g_cancellable_cancel (priv->cancel);
	
		g_object_unref (priv->gdrive);
		priv->gdrive = NULL;
	}

	PARRILLADA_MEDIA_LOG ("Setting GDrive %p", gdrive);

	if (gdrive) {
		priv->gdrive = g_object_ref (gdrive);

		/* If it's not a fake drive then connect to signal for any
		 * change and check medium inside */
		g_signal_connect (priv->gdrive,
				  "changed",
				  G_CALLBACK (parrillada_drive_medium_gdrive_changed_cb),
				  drive);
	}

	if (priv->locked || priv->ejecting) {
		PARRILLADA_MEDIA_LOG ("Waiting for next unlocking of the drive to probe");

		/* Since the drive was locked, it should
		 * not be possible that the medium
		 * actually changed.
		 * This allows to avoid probing while
		 * we are burning something.
		 * Delay the probe until parrillada_drive_unlock ()
		 * is called.  */
		priv->probe_waiting = TRUE;
		return;
	}

	parrillada_drive_probe_inside (drive);
}

/**
 * parrillada_drive_reprobe:
 * @drive: a #ParrilladaDrive
 *
 * Reprobes the drive contents. Useful when an operation has just been performed
 * (blanking, burning, ...) and medium status should be updated.
 *
 * NOTE: This operation does not block.
 *
 **/

void
parrillada_drive_reprobe (ParrilladaDrive *drive)
{
	ParrilladaDrivePrivate *priv;
	ParrilladaMedium *medium;

	g_return_if_fail (drive != NULL);
	g_return_if_fail (PARRILLADA_IS_DRIVE (drive));

	priv = PARRILLADA_DRIVE_PRIVATE (drive);
	
	if (priv->gdrive) {
		/* reprobe the contents of the drive system wide */
		g_drive_poll_for_media (priv->gdrive, NULL, NULL, NULL);
	}

	priv->probe_waiting = FALSE;

	PARRILLADA_MEDIA_LOG ("Reprobing inserted medium");
	if (priv->medium) {
		/* remove current medium */
		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (drive,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);
		g_object_unref (medium);
	}

	parrillada_drive_probe_inside (drive);
}

static gboolean
parrillada_drive_get_caps_profiles (ParrilladaDrive *self,
                                 ParrilladaDeviceHandle *handle,
                                 ParrilladaScsiErrCode *code)
{
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaScsiProfileDesc *profiles;
	ParrilladaScsiFeatureDesc *desc;
	ParrilladaDrivePrivate *priv;
	ParrilladaScsiResult result;
	int profiles_num;
	int size;

	priv = PARRILLADA_DRIVE_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Checking supported profiles");
	result = parrillada_mmc2_get_configuration_feature (handle,
	                                                 PARRILLADA_SCSI_FEAT_PROFILES,
	                                                 &hdr,
	                                                 &size,
	                                                 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	PARRILLADA_MEDIA_LOG ("Dectected medium is 0x%x", PARRILLADA_GET_16 (hdr->current_profile));

	/* Go through all features available */
	desc = hdr->desc;
	profiles = (ParrilladaScsiProfileDesc *) desc->data;
	profiles_num = desc->add_len / sizeof (ParrilladaScsiProfileDesc);

	while (profiles_num) {
		switch (PARRILLADA_GET_16 (profiles->number)) {
			case PARRILLADA_SCSI_PROF_CDR:
				priv->caps |= PARRILLADA_DRIVE_CAPS_CDR;
				break;
			case PARRILLADA_SCSI_PROF_CDRW:
				priv->caps |= PARRILLADA_DRIVE_CAPS_CDRW;
				break;
			case PARRILLADA_SCSI_PROF_DVD_R: 
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDR;
				break;
			case PARRILLADA_SCSI_PROF_DVD_RW_SEQUENTIAL: 
			case PARRILLADA_SCSI_PROF_DVD_RW_RESTRICTED: 
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDRW;
				break;
			case PARRILLADA_SCSI_PROF_DVD_RAM: 
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDRAM;
				break;
			case PARRILLADA_SCSI_PROF_DVD_R_PLUS_DL:
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDR_PLUS_DL;
				break;
			case PARRILLADA_SCSI_PROF_DVD_RW_PLUS_DL:
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDRW_PLUS_DL;
				break;
			case PARRILLADA_SCSI_PROF_DVD_R_PLUS:
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDR_PLUS;
				break;
			case PARRILLADA_SCSI_PROF_DVD_RW_PLUS:
				priv->caps |= PARRILLADA_DRIVE_CAPS_DVDRW_PLUS;
				break;
			case PARRILLADA_SCSI_PROF_BR_R_SEQUENTIAL:
			case PARRILLADA_SCSI_PROF_BR_R_RANDOM:
				priv->caps |= PARRILLADA_DRIVE_CAPS_BDR;
				break;
			case PARRILLADA_SCSI_PROF_BD_RW:
				priv->caps |= PARRILLADA_DRIVE_CAPS_BDRW;
				break;
			default:
				break;
		}

		if (priv->initial_probe_cancelled)
			break;

		/* Move the pointer to the next features */
		profiles ++;
		profiles_num --;
	}

	g_free (hdr);
	return TRUE;
}

static void
parrillada_drive_get_caps_2A (ParrilladaDrive *self,
                           ParrilladaDeviceHandle *handle,
                           ParrilladaScsiErrCode *code)
{
	ParrilladaScsiStatusPage *page_2A = NULL;
	ParrilladaScsiModeData *data = NULL;
	ParrilladaDrivePrivate *priv;
	ParrilladaScsiResult result;
	int size = 0;

	priv = PARRILLADA_DRIVE_PRIVATE (self);

	result = parrillada_spc1_mode_sense_get_page (handle,
						   PARRILLADA_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("MODE SENSE failed");
		return;
	}

	page_2A = (ParrilladaScsiStatusPage *) &data->page;

	if (page_2A->wr_CDR != 0)
		priv->caps |= PARRILLADA_DRIVE_CAPS_CDR;
	if (page_2A->wr_CDRW != 0)
		priv->caps |= PARRILLADA_DRIVE_CAPS_CDRW;
	if (page_2A->wr_DVDR != 0)
		priv->caps |= PARRILLADA_DRIVE_CAPS_DVDR;
	if (page_2A->wr_DVDRAM != 0)
		priv->caps |= PARRILLADA_DRIVE_CAPS_DVDRAM;

	g_free (data);
}

static gpointer
parrillada_drive_probe_thread (gpointer data)
{
	gint counter = 0;
	GTimeVal wait_time;
	const gchar *device;
	ParrilladaScsiResult res;
	ParrilladaScsiInquiry hdr;
	ParrilladaScsiErrCode code;
	ParrilladaDrivePrivate *priv;
	ParrilladaDeviceHandle *handle;
	ParrilladaDrive *drive = PARRILLADA_DRIVE (data);

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	device = parrillada_drive_get_device (drive);
	PARRILLADA_MEDIA_LOG ("Trying to open device %s", device);

	handle = parrillada_device_handle_open (device, FALSE, &code);
	while (!handle && counter <= PARRILLADA_DRIVE_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->initial_probe_cancelled) {
			PARRILLADA_MEDIA_LOG ("Open () cancelled");
			goto end;
		}

		counter ++;
		handle = parrillada_device_handle_open (device, FALSE, &code);
	}

	if (priv->initial_probe_cancelled) {
		PARRILLADA_MEDIA_LOG ("Open () cancelled");
		goto end;
	}

	if (!handle) {
		PARRILLADA_MEDIA_LOG ("Open () failed: medium busy");
		goto end;
	}

	while (parrillada_spc1_test_unit_ready (handle, &code) != PARRILLADA_SCSI_OK) {
		if (code == PARRILLADA_SCSI_NO_MEDIUM) {
			PARRILLADA_MEDIA_LOG ("No medium inserted");
			goto capabilities;
		}

		if (code != PARRILLADA_SCSI_NOT_READY) {
			parrillada_device_handle_close (handle);
			PARRILLADA_MEDIA_LOG ("Device does not respond");
			goto end;
		}

		g_get_current_time (&wait_time);
		g_time_val_add (&wait_time, 2000000);

		g_mutex_lock (priv->mutex);
		g_cond_timed_wait (priv->cond_probe,
		                   priv->mutex,
		                   &wait_time);
		g_mutex_unlock (priv->mutex);

		if (priv->initial_probe_cancelled) {
			parrillada_device_handle_close (handle);
			PARRILLADA_MEDIA_LOG ("Device probing cancelled");
			goto end;
		}
	}

	PARRILLADA_MEDIA_LOG ("Device ready");
	priv->has_medium = TRUE;

capabilities:

	/* get additional information like the name */
	res = parrillada_spc1_inquiry (handle, &hdr, NULL);
	if (res == PARRILLADA_SCSI_OK) {
		gchar *name_utf8;
		gchar *vendor;
		gchar *model;
		gchar *name;

		vendor = g_strndup ((gchar *) hdr.vendor, sizeof (hdr.vendor));
		model = g_strndup ((gchar *) hdr.name, sizeof (hdr.name));
		name = g_strdup_printf ("%s %s", g_strstrip (vendor), g_strstrip (model));
		g_free (vendor);
		g_free (model);

		/* make sure that's proper UTF-8 */
		name_utf8 = g_convert_with_fallback (name,
		                                     -1,
		                                     "ASCII",
		                                     "UTF-8",
		                                     "_",
		                                     NULL,
		                                     NULL,
		                                     NULL);
		g_free (name);

		priv->name = name_utf8;
	}

	/* Get supported medium types */
	if (!parrillada_drive_get_caps_profiles (drive, handle, &code))
		parrillada_drive_get_caps_2A (drive, handle, &code);

	parrillada_device_handle_close (handle);

	PARRILLADA_MEDIA_LOG ("Drive caps are %d", priv->caps);

end:

	g_mutex_lock (priv->mutex);

	parrillada_drive_update_medium (drive);

	priv->probe = NULL;
	priv->initial_probe = FALSE;

	g_cond_broadcast (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (0);

	return NULL;
}

static void
parrillada_drive_init_real_device (ParrilladaDrive *drive,
                                const gchar *device)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (drive);

#if defined(HAVE_STRUCT_USCSI_CMD)
	/* On Solaris path points to raw device, block_path points to the block device. */
	g_assert(g_str_has_prefix(device, "/dev/dsk/"));
	priv->device = g_strdup_printf ("/dev/rdsk/%s", device + 9);
	priv->block_device = g_strdup (device);
	PARRILLADA_MEDIA_LOG ("Initializing block drive %s", priv->block_device);
#else
	priv->device = g_strdup (device);
#endif

	PARRILLADA_MEDIA_LOG ("Initializing drive %s from device", priv->device);

	/* NOTE: why a thread? Because in case of a damaged medium, parrillada can
	 * block on some functions until timeout and if we do this in the main
	 * thread then our whole UI blocks. This medium won't be exported by the
	 * ParrilladaDrive that exported until it returns PROBED signal.
	 * One (good) side effect is that it also improves start time. */
	g_mutex_lock (priv->mutex);

	priv->initial_probe = TRUE;
	priv->probe = g_thread_create (parrillada_drive_probe_thread,
				       drive,
				       FALSE,
				       NULL);

	g_mutex_unlock (priv->mutex);
}

static void
parrillada_drive_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	ParrilladaDrivePrivate *priv;
	GDrive *gdrive = NULL;

	g_return_if_fail (PARRILLADA_IS_DRIVE (object));

	priv = PARRILLADA_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		break;
	case PROP_GDRIVE:
		if (!priv->device)
			break;

		gdrive = g_value_get_object (value);
		parrillada_drive_update_gdrive (PARRILLADA_DRIVE (object), gdrive);
		break;
	case PROP_DEVICE:
		/* The first case is only a fake drive/medium */
		if (!g_value_get_string (value))
			priv->medium = g_object_new (PARRILLADA_TYPE_VOLUME,
						     "drive", object,
						     NULL);
		else
			parrillada_drive_init_real_device (PARRILLADA_DRIVE (object), g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_drive_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	ParrilladaDrivePrivate *priv;

	g_return_if_fail (PARRILLADA_IS_DRIVE (object));

	priv = PARRILLADA_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		break;
	case PROP_GDRIVE:
		g_value_set_object (value, priv->gdrive);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_drive_init (ParrilladaDrive *object)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (object);
	priv->cancel = g_cancellable_new ();

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
	priv->cond_probe = g_cond_new ();
}

static void
parrillada_drive_finalize (GObject *object)
{
	ParrilladaDrivePrivate *priv;

	priv = PARRILLADA_DRIVE_PRIVATE (object);

	PARRILLADA_MEDIA_LOG ("Finalizing ParrilladaDrive");

	parrillada_drive_cancel_probing (PARRILLADA_DRIVE (object));

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	if (priv->cond_probe) {
		g_cond_free (priv->cond_probe);
		priv->cond_probe = NULL;
	}

	if (priv->medium) {
		g_signal_emit (object,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       priv->medium);
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	if (priv->name) {
		g_free (priv->name);
		priv->name = NULL;
	}

	if (priv->device) {
		g_free (priv->device);
		priv->device = NULL;
	}

	if (priv->block_device) {
		g_free (priv->block_device);
		priv->block_device = NULL;
	}

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	if (priv->gdrive) {
		g_signal_handlers_disconnect_by_func (priv->gdrive,
						      parrillada_drive_medium_gdrive_changed_cb,
						      object);
		g_object_unref (priv->gdrive);
		priv->gdrive = NULL;
	}

	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	G_OBJECT_CLASS (parrillada_drive_parent_class)->finalize (object);
}

static void
parrillada_drive_class_init (ParrilladaDriveClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDrivePrivate));

	object_class->finalize = parrillada_drive_finalize;
	object_class->set_property = parrillada_drive_set_property;
	object_class->get_property = parrillada_drive_get_property;

	/**
 	* ParrilladaDrive::medium-added:
 	* @drive: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new medium was detected
 	*
 	*/
	drive_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (ParrilladaDriveClass, medium_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              PARRILLADA_TYPE_MEDIUM);

	/**
 	* ParrilladaDrive::medium-removed:
 	* @drive: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a medium is not longer available
 	*
 	*/
	drive_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (ParrilladaDriveClass, medium_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              PARRILLADA_TYPE_MEDIUM);

	g_object_class_install_property (object_class,
	                                 PROP_UDI,
	                                 g_param_spec_string("udi",
	                                                     "udi",
	                                                     "HAL udi as a string (Deprecated)",
	                                                     NULL,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_GDRIVE,
	                                 g_param_spec_object ("gdrive",
	                                                      "GDrive",
	                                                      "A GDrive object for the drive",
	                                                      G_TYPE_DRIVE,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_DEVICE,
	                                 g_param_spec_string ("device",
	                                                      "Device",
	                                                      "Device path for the drive",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}


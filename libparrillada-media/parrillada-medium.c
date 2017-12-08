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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdk.h>

#include "parrillada-media-private.h"
#include "parrillada-drive-priv.h"

#include "parrillada-medium.h"
#include "parrillada-drive.h"

#include "scsi-device.h"
#include "scsi-mmc1.h"
#include "scsi-mmc2.h"
#include "scsi-mmc3.h"
#include "scsi-spc1.h"
#include "scsi-utils.h"
#include "scsi-mode-pages.h"
#include "scsi-status-page.h"
#include "scsi-write-page.h"
#include "scsi-q-subchannel.h"
#include "scsi-dvd-structures.h"
#include "burn-volume.h"


const gchar *types [] = {	N_("File"),
				N_("CD-ROM"),
				N_("CD-R"),
				N_("CD-RW"),
				N_("DVD-ROM"),
				N_("DVD-R"),
				N_("DVD-RW"),
				N_("DVD+R"),
				N_("DVD+RW"),
				N_("DVD+R dual layer"),
				N_("DVD+RW dual layer"),
				N_("DVD-R dual layer"),
				N_("DVD-RAM"),
				N_("Blu-ray disc"),
				N_("Writable Blu-ray disc"),
				N_("Rewritable Blu-ray disc"),
				NULL };



typedef struct _ParrilladaMediumPrivate ParrilladaMediumPrivate;
struct _ParrilladaMediumPrivate
{
	GThread *probe;
	GMutex *mutex;
	GCond *cond;
	GCond *cond_probe;

	gint probe_id;

	GSList *tracks;

	const gchar *type;

	gchar *id;

	guint max_rd;
	guint max_wrt;

	guint *rd_speeds;
	guint *wr_speeds;

	goffset block_num;
	goffset block_size;

	guint first_open_track;
	goffset next_wr_add;

	ParrilladaMedia info;
	ParrilladaDrive *drive;

	gchar *CD_TEXT_title;

	/* Do we really need both? */
	guint dummy_sao:1;
	guint dummy_tao:1;
	guint burnfree:1;
	guint sao:1;
	guint tao:1;

	guint blank_command:1;
	guint write_command:1;

	guint probe_cancelled:1;
};

#define PARRILLADA_MEDIUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_MEDIUM, ParrilladaMediumPrivate))

/**
 * Try to open the drive exclusively but don't block; if drive can't be opened
 * exclusively then retry every second until we're shut or the drive state
 * changes to not busy.
 * No exclusive at the moment since when the medium is mounted we can't use excl
 */

#define BUSY_RETRY_TIME			1000

typedef enum {
	PARRILLADA_MEDIUM_TRACK_NONE		= 0,
	PARRILLADA_MEDIUM_TRACK_DATA		= 1,
	PARRILLADA_MEDIUM_TRACK_AUDIO		= 1 << 1,
	PARRILLADA_MEDIUM_TRACK_COPY		= 1 << 2,
	PARRILLADA_MEDIUM_TRACK_PREEMP		= 1 << 3,
	PARRILLADA_MEDIUM_TRACK_4_CHANNELS		= 1 << 4,
	PARRILLADA_MEDIUM_TRACK_INCREMENTAL	= 1 << 5,
	PARRILLADA_MEDIUM_TRACK_LEADOUT		= 1 << 6
} ParrilladaMediumTrackType;

typedef struct _ParrilladaMediumTrack ParrilladaMediumTrack;

struct _ParrilladaMediumTrack {
	guint session;
	ParrilladaMediumTrackType type;
	goffset start;
	goffset blocks_num;
};

enum
{
	PROP_0,
	PROP_DRIVE,
};

enum {
	PROBED,
	LAST_SIGNAL
};
static gulong medium_signals [LAST_SIGNAL] = {0, };

#define PARRILLADA_MEDIUM_OPEN_ATTEMPTS			5

static GObjectClass* parent_class = NULL;


/**
 * parrillada_medium_get_tooltip:
 * @medium: #ParrilladaMedium
 *
 * Returns a tooltip to be displayed in the UI.
 * It is of the form {content type} {disc type} in {drive name}.
 *
 * Return value: a #gchar *.
 *
 **/
gchar *
parrillada_medium_get_tooltip (ParrilladaMedium *medium)
{
	ParrilladaDrive *drive;
	ParrilladaMedia media;
	const gchar *type;
	gchar *label;
	gchar *name;

	g_return_val_if_fail (medium != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), NULL);

	media = parrillada_medium_get_status (PARRILLADA_MEDIUM (medium));
	if (media & PARRILLADA_MEDIUM_FILE) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	type = parrillada_medium_get_type_string (PARRILLADA_MEDIUM (medium));
	drive = parrillada_medium_get_drive (PARRILLADA_MEDIUM (medium));
	name = parrillada_drive_get_display_name (drive);

	if (media & PARRILLADA_MEDIUM_BLANK) {
		/* NOTE for translators: the first %s is the disc type and the
		 * second %s the name of the drive this disc is in. */
		label = g_strdup_printf (_("Blank %s in %s"),
					 type,
					 name);
	}
	else if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_HAS_AUDIO|PARRILLADA_MEDIUM_HAS_DATA)) {
		/* NOTE for translators: the first %s is the disc type and the
		 * second %s the name of the drive this disc is in. */
		label = g_strdup_printf (_("Audio and data %s in %s"),
					 type,
					 name);
	}
	else if (media & PARRILLADA_MEDIUM_HAS_AUDIO) {
		/* NOTE for translators: the first %s is the disc type and the
		 * second %s the name of the drive this disc is in. */
		label = g_strdup_printf (_("Audio %s in %s"),
					 type,
					 name);
	}
	else if (media & PARRILLADA_MEDIUM_HAS_DATA) {
		/* NOTE for translators: the first %s is the disc type and the
	 	* second %s the name of the drive this disc is in. */
		label = g_strdup_printf (_("Data %s in %s"),
					 type,
					 name);
	}
	else {
		/* NOTE for translators: the first %s is the disc type and the
	 	* second %s the name of the drive this disc is in. */
		label = g_strdup_printf (_("%s in %s"),
					 type,
					 name);
	}

	g_free (name);
	return label;
}

/**
 * parrillada_medium_get_type_string:
 * @medium: #ParrilladaMedium
 *
 * Returns the medium type as a string to be displayed in a UI.
 *
 * Return value: a #gchar *.
 *
 **/
const gchar *
parrillada_medium_get_type_string (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), NULL);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->type;
}

/**
 * parrillada_medium_get_status:
 * @medium: #ParrilladaMedium
 *
 * Gets the medium type and state.
 *
 * Return value: a #ParrilladaMedia.
 *
 **/
ParrilladaMedia
parrillada_medium_get_status (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	if (!medium)
		return PARRILLADA_MEDIUM_NONE;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), PARRILLADA_MEDIUM_NONE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->info;
}

/**
 * parrillada_medium_get_last_data_track_address:
 * @medium: #ParrilladaMedium
 * @bytes: a #goffset * or NULL
 * @sectors: a #goffset * or NULL
 *
 * Stores in either @bytes (in bytes) or in @sectors (in blocks) the address where
 * the last session starts. This is useful when creating a multisession image or
 * when reading the contents of this last track.
 *
 * Return value: a #gboolean. Returns TRUE if information could be retrieved.
 *
 **/
gboolean
parrillada_medium_get_last_data_track_address (ParrilladaMedium *medium,
					    goffset *bytes,
					    goffset *sectors)
{
	GSList *iter;
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track = NULL;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *current;

		current = iter->data;
		if (current->type & PARRILLADA_MEDIUM_TRACK_DATA)
			track = current;
	}

	if (!track)
		return FALSE;

	if (bytes)
		*bytes = track->start * priv->block_size;

	if (sectors)
		*sectors = track->start;

	return TRUE;
}

/**
 * parrillada_medium_get_last_data_track_space:
 * @medium: #ParrilladaMedium
 * @bytes: a #goffset * or NULL
 * @sectors: a #goffset * or NULL
 *
 * Stores in either @bytes (in bytes) or in @sectors (in blocks) the space used by
 * the last track on the medium.
 *
 * Return value: a #gboolean. Returns TRUE if information could be retrieved.
 *
 **/
gboolean
parrillada_medium_get_last_data_track_space (ParrilladaMedium *medium,
					  goffset *bytes,
					  goffset *sectors)
{
	GSList *iter;
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track = NULL;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *current;

		current = iter->data;
		if (current->type & PARRILLADA_MEDIUM_TRACK_DATA)
			track = current;
	}

	if (!track) {
		if (bytes)
			*bytes = 0;
		if (sectors)
			*sectors = 0;
		return FALSE;
	}

	if (bytes)
		*bytes = track->blocks_num * priv->block_size;
	if (sectors)
		*sectors = track->blocks_num;

	return TRUE;
}

/**
 * parrillada_medium_get_track_num:
 * @medium: #ParrilladaMedium
 *
 * Gets the number of tracks on the medium.
 *
 * Return value: a #guint.
 *
 **/
guint
parrillada_medium_get_track_num (ParrilladaMedium *medium)
{
	GSList *iter;
	guint retval = 0;
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, 0);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), 0);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *current;

		current = iter->data;
		if (current->type & PARRILLADA_MEDIUM_TRACK_LEADOUT)
			break;

		retval ++;
	}

	return retval;
}

static ParrilladaMediumTrack *
parrillada_medium_get_track (ParrilladaMedium *medium,
			  guint num)
{
	guint i = 1;
	GSList *iter;
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *current;

		current = iter->data;
		if (current->type == PARRILLADA_MEDIUM_TRACK_LEADOUT)
			break;

		if (i == num)
			return current;

		i++;
	}

	return NULL;
}

/**
 * parrillada_medium_get_track_space:
 * @medium: a #ParrilladaMedium
 * @num: a #guint
 * @bytes: a #goffset * or NULL
 * @sectors: a #goffset * or NULL
 *
 * Stores in either @bytes (in bytes) or in @sectors (in blocks) the space used
 * by session @num on the disc.
 *
 * Return value: a #gboolean. Returns TRUE if information could be retrieved;
 * FALSE otherwise (usually when track @num doesn't exist).
 *
 **/
gboolean
parrillada_medium_get_track_space (ParrilladaMedium *medium,
				guint num,
				goffset *bytes,
				goffset *sectors)
{
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	track = parrillada_medium_get_track (medium, num);
	if (!track) {
		if (bytes)
			*bytes = 0;
		if (sectors)
			*sectors = 0;
		return FALSE;
	}

	if (bytes)
		*bytes = track->blocks_num * priv->block_size;
	if (sectors)
		*sectors = track->blocks_num;

	return TRUE;
}

/**
 * parrillada_medium_get_track_address:
 * @medium: a #ParrilladaMedium
 * @num: a #guint
 * @bytes: a #goffset * or NULL
 * @sectors: a #goffset * or NULL
 *
 * Stores in either @bytes (in bytes) or in @sectors (in blocks) the address at
 * which the session identified by @num starts.
 *
 * Return value: a #gboolean. Returns TRUE if information could be retrieved;
 * FALSE otherwise (usually when track @num doesn't exist).
 *
 **/
gboolean
parrillada_medium_get_track_address (ParrilladaMedium *medium,
				  guint num,
				  goffset *bytes,
				  goffset *sectors)
{
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	track = parrillada_medium_get_track (medium, num);
	if (!track) {
		if (bytes)
			*bytes = 0;
		if (sectors)
			*sectors = 0;
		return FALSE;
	}

	if (bytes)
		*bytes = track->start * priv->block_size;
	if (sectors)
		*sectors = track->start;

	return TRUE;	
}

/**
 * parrillada_medium_get_next_writable_address:
 * @medium: #ParrilladaMedium
 *
 * Gets the address (block number) that can be used to write a new session on @medium
 *
 * Return value: a #gint64.
 *
 **/
gint64
parrillada_medium_get_next_writable_address (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, 0);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), 0);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	/* There is one exception to this with closed DVD+RW/DVD-RW restricted */
	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_PLUS)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_RESTRICTED)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_PLUS_DL)) {
		ParrilladaMediumTrack *first;

		/* These are always writable so give the next address after the 
		 * last volume. */
		if (!priv->tracks)
			return 0;

		first = priv->tracks->data;

		/* round to the nearest 16th block */
		return (((first->start + first->blocks_num) + 15) / 16) * 16;
	}

	return priv->next_wr_add;
}

/**
 * parrillada_medium_get_max_write_speed:
 * @medium: #ParrilladaMedium
 *
 * Gets the maximum speed that can be used to write to @medium.
 * Note: the speed are in B/sec.
 *
 * Return value: a #guint64.
 *
 **/
guint64
parrillada_medium_get_max_write_speed (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, 0);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), 0);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->max_wrt * 1000;
}

/**
 * parrillada_medium_get_write_speeds:
 * @medium: #ParrilladaMedium
 *
 * Gets an array holding all possible speeds to write to @medium.
 * Note: the speed are in B/sec.
 *
 * Return value: a #guint64 *.
 *
 **/
guint64 *
parrillada_medium_get_write_speeds (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;
	guint64 *speeds;
	guint max = 0;
	guint i;

	g_return_val_if_fail (medium != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), NULL);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	if (!priv->wr_speeds)
		return NULL;

	while (priv->wr_speeds [max] != 0) max ++;

	speeds = g_new0 (guint64, max + 1);

	/* NOTE: about the following, it's not KiB here but KB */
	for (i = 0; i < max; i ++)
		speeds [i] = priv->wr_speeds [i] * 1000;

	return speeds;
}

/**
 * NOTEs about the following functions:
 * for all closed media (including ROM types) capacity == size of data and 
 * should be the size of all data on the disc, free space is 0
 * for all blank -R types capacity == free space and size of data == 0
 * for all multisession -R types capacity == free space since having the real
 * capacity of the media would be useless as we can only use this type of media
 * to append more data
 * for all -RW types capacity = free space + size of data. Here they can be 
 * appended (use free space) or rewritten (whole capacity).
 *
 * Usually:
 * the free space is the size of the leadout track
 * the size of data is the sum of track sizes (excluding leadout)
 * the capacity depends on the media:
 * for closed discs == sum of track sizes
 * for multisession discs == free space (leadout size)
 * for blank discs == (free space) leadout size
 * for rewritable/blank == use SCSI functions to get capacity (see below)
 *
 * In fact we should really need the size of data in DVD+/-RW cases since the
 * session is always equal to the size of the disc. 
 */

/**
 * parrillada_medium_get_data_size:
 * @medium: #ParrilladaMedium
 * @bytes: a #gint64 * or NULL
 * @blocks: a #gint64 * or NULL
 *
 * Stores in either @size (in bytes) or @blocks (the number of blocks) the size
 * used to store data (including audio on CDs) on the disc.
 *
 **/
void
parrillada_medium_get_data_size (ParrilladaMedium *medium,
			      gint64 *bytes,
			      gint64 *blocks)
{
	GSList *iter;
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track = NULL;

	g_return_if_fail (medium != NULL);
	g_return_if_fail (PARRILLADA_IS_MEDIUM (medium));

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	if (!priv->tracks) {
		/* that's probably because it wasn't possible to retrieve info */
		if (bytes)
			*bytes = 0;

		if (blocks)
			*blocks = 0;

		return;
	}

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == PARRILLADA_MEDIUM_TRACK_LEADOUT)
			break;

		track = iter->data;
	}

	if (bytes)
		*bytes = track ? (track->start + track->blocks_num) * priv->block_size: 0;

	if (blocks)
		*blocks = track ? track->start + track->blocks_num: 0;
}

/**
 * parrillada_medium_get_free_space:
 * @medium: #ParrilladaMedium
 * @bytes: a #gint64 * or NULL
 * @blocks: a #gint64 * or NULL
 *
 * Stores in either @size (in bytes) or @blocks (the number of blocks) the space
 * on the disc that can be used for writing.
 *
 **/
void
parrillada_medium_get_free_space (ParrilladaMedium *medium,
			       gint64 *bytes,
			       gint64 *blocks)
{
	GSList *iter;
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track = NULL;

	g_return_if_fail (medium != NULL);
	g_return_if_fail (PARRILLADA_IS_MEDIUM (medium));

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	if (!priv->tracks) {
		/* that's probably because it wasn't possible to retrieve info.
		 * maybe it also happens with unformatted DVD+RW */

		if (priv->info & PARRILLADA_MEDIUM_CLOSED) {
			if (bytes)
				*bytes = 0;

			if (blocks)
				*blocks = 0;
		}
		else {
			if (bytes)
				*bytes = priv->block_num * priv->block_size;

			if (blocks)
				*blocks = priv->block_num;
		}

		return;
	}

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == PARRILLADA_MEDIUM_TRACK_LEADOUT) {
			track = iter->data;
			break;
		}
	}

	if (bytes) {
		if (!track) {
			/* No leadout was found so the disc is probably closed:
			 * no free space left. */
			*bytes = 0;
		}
		else if (track->blocks_num <= 0)
			*bytes = (priv->block_num - track->start) * priv->block_size;
		else
			*bytes = track->blocks_num * priv->block_size;
	}

	if (blocks) {
		if (!track) {
			/* No leadout was found so the disc is probably closed:
			 * no free space left. */
			*blocks = 0;
		}
		else if (track->blocks_num <= 0)
			*blocks = priv->block_num - track->blocks_num;
		else
			*blocks = track->blocks_num;
	}
}

/**
 * parrillada_medium_get_capacity:
 * @medium: #ParrilladaMedium
 * @bytes: a #gint64 * or NULL
 * @blocks: a #gint64 * or NULL
 *
 * Stores in either @size (in bytes) or @blocks (the number of blocks) the total
 * disc space.
 * Note that when the disc is closed this space is the one occupied by data. 
 * Otherwise it is the sum of free and used space.
 *
 **/
void
parrillada_medium_get_capacity (ParrilladaMedium *medium,
			     gint64 *bytes,
			     gint64 *blocks)
{
	ParrilladaMediumPrivate *priv;

	g_return_if_fail (medium != NULL);
	g_return_if_fail (PARRILLADA_IS_MEDIUM (medium));

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	if (priv->info & PARRILLADA_MEDIUM_REWRITABLE) {
		if (bytes)
			*bytes = priv->block_num * priv->block_size;

		if (blocks)
			*blocks = priv->block_num;
	}
	else  if (priv->info & PARRILLADA_MEDIUM_CLOSED)
		parrillada_medium_get_data_size (medium, bytes, blocks);
	else
		parrillada_medium_get_free_space (medium, bytes, blocks);
}

/**
 * Test presence of simulate burning/ SAO/ DAO
 */

static gboolean
parrillada_medium_set_write_mode_page_tao (ParrilladaMedium *self,
                                        ParrilladaDeviceHandle *handle,
                                        ParrilladaScsiErrCode *code)
{
	ParrilladaScsiModeData *data = NULL;
	ParrilladaScsiWritePage *wrt_page;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	PARRILLADA_MEDIA_LOG ("Setting write mode page");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* NOTE: this works for CDR, DVDR+-, BDR-SRM */
	/* make sure the current write mode is TAO. Otherwise the drive will
	 * return the first sector of the pregap instead of the first user
	 * accessible sector. */
	result = parrillada_spc1_mode_sense_get_page (handle,
						   PARRILLADA_SPC_PAGE_WRITE,
						   &data,
						   &size,
						   code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("MODE SENSE failed");
		/* This isn't necessarily a problem! we better try the rest */
		return FALSE;
	}

	wrt_page = (ParrilladaScsiWritePage *) &data->page;

	PARRILLADA_MEDIA_LOG ("Former write type %d", wrt_page->write_type);
	PARRILLADA_MEDIA_LOG ("Former track mode %d", wrt_page->track_mode);
	PARRILLADA_MEDIA_LOG ("Former data block type %d", wrt_page->data_block_type);

	/* "reset some stuff to be on the safe side" (words and ideas
	 * taken from k3b:)). */
	wrt_page->ps = 0;
	wrt_page->BUFE = 0;
	wrt_page->multisession = 0;
	wrt_page->testwrite = 0;
	wrt_page->LS_V = 0;
	wrt_page->copy = 0;
	wrt_page->FP = 0;
	wrt_page->session_format = 0;
	PARRILLADA_SET_16 (wrt_page->pause_len, 150);

	if (priv->info & PARRILLADA_MEDIUM_CD) {
		wrt_page->write_type = PARRILLADA_SCSI_WRITE_TAO;
		wrt_page->track_mode = 4;
	}
	else if (priv->info & PARRILLADA_MEDIUM_DVD) {
		wrt_page->write_type = PARRILLADA_SCSI_WRITE_PACKET_INC;
		wrt_page->track_mode = 5;
	}

	wrt_page->data_block_type = 8;

	result = parrillada_spc1_mode_select (handle, data, size, code);
	g_free (data);

	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("MODE SELECT failed");

		/* This isn't necessarily a problem! we better try */
		return FALSE;
	}

	return TRUE;
}

static gboolean
parrillada_medium_test_CD_TAO_simulate (ParrilladaMedium *self,
				     ParrilladaDeviceHandle *handle,
				     ParrilladaScsiErrCode *code)
{
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaScsiCDTAODesc *tao_desc;
	ParrilladaScsiFeatureDesc *desc;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* Try TAO and then SAO if it isn't persistent */
	PARRILLADA_MEDIA_LOG ("Checking simulate (CD TAO)");
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_WRT_TAO,
							 &hdr,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	desc = hdr->desc;
	priv->tao = (desc->current != 0);
	PARRILLADA_MEDIA_LOG ("TAO feature is %s", priv->tao? "supported":"not supported");

	tao_desc = (ParrilladaScsiCDTAODesc *) desc->data;
	priv->dummy_tao = tao_desc->dummy != 0;
	priv->burnfree = tao_desc->buf != 0;

	/* See if CD-RW is supported which means in
	 * this case that we can blank */
	priv->blank_command = (tao_desc->CDRW != 0);
	PARRILLADA_MEDIA_LOG ("Medium %s be blanked", priv->blank_command? "can":"cannot");

	g_free (hdr);
	return TRUE;
}

static gboolean
parrillada_medium_test_CD_SAO_simulate (ParrilladaMedium *self,
				     ParrilladaDeviceHandle *handle,
				     ParrilladaScsiErrCode *code)
{
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaScsiCDSAODesc *sao_desc;
	ParrilladaScsiFeatureDesc *desc;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Checking simulate (CD SAO)");
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_WRT_SAO_RAW,
							 &hdr,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	desc = hdr->desc;
	priv->sao = (desc->current != 0);
	PARRILLADA_MEDIA_LOG ("SAO feature is %s", priv->sao? "supported":"not supported");

	sao_desc = (ParrilladaScsiCDSAODesc *) desc->data;
	priv->dummy_sao = sao_desc->dummy != 0;
	priv->burnfree = sao_desc->buf != 0;

	g_free (hdr);
	return TRUE;
}

static gboolean
parrillada_medium_test_DVDRW_incremental_simulate (ParrilladaMedium *self,
                                                ParrilladaDeviceHandle *handle,
                                                ParrilladaScsiErrCode *code)
{
	ParrilladaScsiDVDRWlessWrtDesc *less_wrt_desc;
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaScsiFeatureDesc *desc;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* Try incremental feature */
	PARRILLADA_MEDIA_LOG ("Checking incremental and simulate feature");
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_WRT_INCREMENT,
							 &hdr,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	priv->tao = (hdr->desc->current != 0);
	g_free (hdr);
	hdr = NULL;

	PARRILLADA_MEDIA_LOG ("Incremental feature is %s", priv->tao? "supported":"not supported");

	/* Only DVD-R(W) support simulation */
	PARRILLADA_MEDIA_LOG ("Checking (DVD-R(W) simulate)");
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_WRT_DVD_LESS,
							 &hdr,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	desc = hdr->desc;

	/* NOTE: SAO feature is always supported if this feature is current
	 * See MMC5 5.3.25 Write feature parameters */
	priv->sao = (desc->current != 0);
	PARRILLADA_MEDIA_LOG ("SAO feature is %s", priv->sao? "supported":"not supported");

	less_wrt_desc = (ParrilladaScsiDVDRWlessWrtDesc *) desc->data;
	priv->dummy_sao = less_wrt_desc->dummy != 0;
	priv->dummy_tao = less_wrt_desc->dummy != 0;
	priv->burnfree = less_wrt_desc->buf != 0;

	/* NOTE: it's said that this is only valid when the current
	 * bit is set which is always the case in this function */
	priv->blank_command = (less_wrt_desc->rw_DVD != 0);
	PARRILLADA_MEDIA_LOG ("Medium %s be blanked", priv->blank_command? "can":"cannot");

	g_free (hdr);
	return TRUE;
}

/**
 * This is a last resort when the initialization has failed.
 */

static void
parrillada_medium_test_2A_simulate (ParrilladaMedium *self,
				 ParrilladaDeviceHandle *handle,
				 ParrilladaScsiErrCode *code)
{
	ParrilladaScsiStatusPage *page_2A = NULL;
	ParrilladaScsiModeData *data = NULL;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size = 0;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* FIXME: we need to get a way to get the write types */
	result = parrillada_spc1_mode_sense_get_page (handle,
						   PARRILLADA_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("MODE SENSE failed");
		return;
	}

	/* NOTE: this bit is only valid:
	 * - for CDs when mode write is TAO or SAO
	 * - for DVDs when mode write is incremental or SAO */

	page_2A = (ParrilladaScsiStatusPage *) &data->page;
	priv->dummy_sao = page_2A->dummy != 0;
	priv->dummy_tao = page_2A->dummy != 0;
	priv->burnfree = page_2A->buffer != 0;

	priv->blank_command = (page_2A->wr_CDRW != 0);
	PARRILLADA_MEDIA_LOG ("Medium %s be blanked", priv->blank_command? "can":"cannot");

	g_free (data);
}

static void
parrillada_medium_init_caps (ParrilladaMedium *self,
			  ParrilladaDeviceHandle *handle,
			  ParrilladaScsiErrCode *code)
{
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult res;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* These special media don't support/need burnfree, simulation, tao/sao */
	if (priv->info & (PARRILLADA_MEDIUM_PLUS|PARRILLADA_MEDIUM_BD))
		return;

	if (priv->info & PARRILLADA_MEDIUM_CD) {
		/* we have to do both */
		res = parrillada_medium_test_CD_SAO_simulate (self, handle, code);
		if (res)
			parrillada_medium_test_CD_TAO_simulate (self, handle, code);
	}
	else
		res = parrillada_medium_test_DVDRW_incremental_simulate (self, handle, code);

	PARRILLADA_MEDIA_LOG ("Tested simulation %d %d, burnfree %d",
			  priv->dummy_tao,
			  priv->dummy_sao,
			  priv->burnfree);

	if (res)
		return;

	/* it didn't work out as expected use fallback */
	PARRILLADA_MEDIA_LOG ("Using fallback 2A page for testing simulation and burnfree");
	parrillada_medium_test_2A_simulate (self, handle, code);

	PARRILLADA_MEDIA_LOG ("Re-tested simulation %d %d, burnfree %d",
			  priv->dummy_tao,
			  priv->dummy_sao,
			  priv->burnfree);
}

/**
 * Function to retrieve the capacity of a media
 */

static gboolean
parrillada_medium_get_capacity_CD_RW (ParrilladaMedium *self,
				   ParrilladaDeviceHandle *handle,
				   ParrilladaScsiErrCode *code)
{
	ParrilladaScsiAtipData *atip_data = NULL;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size = 0;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Retrieving capacity from atip");

	result = parrillada_mmc1_read_atip (handle,
					 &atip_data,
					 &size,
					 NULL);

	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ ATIP failed (scsi error)");
		return FALSE;
	}

	/* check the size of the structure: it must be at least 16 bytes long */
	if (size < 16) {
		if (size)
			g_free (atip_data);

		PARRILLADA_MEDIA_LOG ("READ ATIP failed (wrong size)");
		return FALSE;
	}

	priv->block_num = PARRILLADA_MSF_TO_LBA (atip_data->desc->leadout_mn,
					      atip_data->desc->leadout_sec,
					      atip_data->desc->leadout_frame);
	g_free (atip_data);

	PARRILLADA_MEDIA_LOG ("Format capacity %lli %lli",
			   priv->block_num,
			   priv->block_size);

	return TRUE;
}

static gboolean
parrillada_medium_get_capacity_DVD_RW (ParrilladaMedium *self,
				    ParrilladaDeviceHandle *handle,
				    ParrilladaScsiErrCode *code)
{
	ParrilladaScsiFormatCapacitiesHdr *hdr = NULL;
	ParrilladaScsiFormattableCapacityDesc *desc;
	ParrilladaScsiMaxCapacityDesc *current;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	gint i, max;
	gint size;

	PARRILLADA_MEDIA_LOG ("Retrieving format capacity");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);
	result = parrillada_mmc2_read_format_capacities (handle,
						      &hdr,
						      &size,
						      code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ FORMAT CAPACITIES failed");
		return FALSE;
	}

	/* NOTE: for BD-RE there is a slight problem to determine the exact
	 * capacity of the medium when it is unformatted. Indeed the final size
	 * of the User Data Area will depend on the size of the Spare areas.
	 * On the other hand if it's formatted then that's OK, just take the 
	 * current one.
	 * NOTE: that could work also for BD-R SRM+POW and BD-R RRM */

	/* see if the media is already formatted */
	current = hdr->max_caps;
	if (!(current->type & PARRILLADA_SCSI_DESC_FORMATTED)) {
		PARRILLADA_MEDIA_LOG ("Unformatted media");
		/* If it's sequential, it's not unformatted */
		if (!(priv->info & PARRILLADA_MEDIUM_SEQUENTIAL))
			priv->info |= PARRILLADA_MEDIUM_UNFORMATTED;

		/* if unformatted, a DVD-RAM will return its maximum formattable
		 * size in this descriptor and that's what we're looking for. */
		if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVD_RAM)) {
			priv->block_num = PARRILLADA_GET_32 (current->blocks_num);
			priv->block_size = 2048;
			goto end;
		}
	}
	else if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BDRE)) {
		priv->block_num = PARRILLADA_GET_32 (current->blocks_num);
		priv->block_size = 2048;
		goto end;
	}

	max = (hdr->len - 
	      sizeof (ParrilladaScsiMaxCapacityDesc)) /
	      sizeof (ParrilladaScsiFormattableCapacityDesc);

	desc = hdr->desc;
	for (i = 0; i < max; i ++, desc ++) {
		/* search for the correct descriptor */
		if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_PLUS)) {
			if (desc->format_type == PARRILLADA_SCSI_DVDRW_PLUS) {
				priv->block_num = PARRILLADA_GET_32 (desc->blocks_num);
				priv->block_size = PARRILLADA_GET_24 (desc->type_param);

				/* that can happen */
				if (!priv->block_size)
					priv->block_size = 2048;

				break;
			}
		}
		else if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BDRE)) {
			/* This is for unformatted BDRE: since we can't know the
			 * size of the Spare Area in advance, we take the vendor
			 * preferred one. Always following are the smallest one
			 * and the biggest one. */
			if (desc->format_type == PARRILLADA_SCSI_BDRE_FORMAT) {
				priv->block_num = PARRILLADA_GET_32 (desc->blocks_num);
				break;
			}
		}
		else if (desc->format_type == PARRILLADA_SCSI_MAX_PACKET_SIZE_FORMAT) {
			priv->block_num = PARRILLADA_GET_32 (desc->blocks_num);
			break;
		}
	}

end:

	PARRILLADA_MEDIA_LOG ("Format capacity %lli %lli",
			  priv->block_num,
			  priv->block_size);

	g_free (hdr);
	return TRUE;
}

static gboolean
parrillada_medium_get_capacity_by_type (ParrilladaMedium *self,
				     ParrilladaDeviceHandle *handle,
				     ParrilladaScsiErrCode *code)
{
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* For DVDs/BDs that's always that block size */
	priv->block_size = 2048;

	if (!(priv->info & PARRILLADA_MEDIUM_REWRITABLE))
		return TRUE;

	if (priv->info & PARRILLADA_MEDIUM_CD)
		parrillada_medium_get_capacity_CD_RW (self, handle, code);
	else	/* Works for BD-RE as well */
		parrillada_medium_get_capacity_DVD_RW (self, handle, code);

	return TRUE;
}

/**
 * Functions to retrieve the speed
 */

static gboolean
parrillada_medium_get_speed_mmc3 (ParrilladaMedium *self,
			       ParrilladaDeviceHandle *handle,
			       ParrilladaScsiErrCode *code)
{
	int size = 0;
	int num_desc, i;
	gint max_rd, max_wrt;
	ParrilladaScsiResult result;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiWrtSpdDesc *desc;
	ParrilladaScsiGetPerfData *wrt_perf = NULL;

	PARRILLADA_MEDIA_LOG ("Retrieving speed (Get Performance)");

	/* NOTE: this only work if there is RT streaming feature with
	 * wspd bit set to 1. At least an MMC3 drive. */
	priv = PARRILLADA_MEDIUM_PRIVATE (self);
	result = parrillada_mmc3_get_performance_wrt_spd_desc (handle,
							    &wrt_perf,
							    &size,
							    code);

	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET PERFORMANCE failed");
		return FALSE;
	}

	PARRILLADA_MEDIA_LOG ("Successfully retrieved a header: size %d, address %p", size, wrt_perf);

	/* Choose the smallest value for size */
	size = MIN (size, PARRILLADA_GET_32 (wrt_perf->hdr.len) + sizeof (wrt_perf->hdr.len));
	PARRILLADA_MEDIA_LOG ("Updated header size = %d", size);

	/* NOTE: I don't know why but on some architecture/with some compilers
	 * when size < sizeof (ParrilladaScsiGetPerfHdr) the whole operation below
	 * is treated as signed which leads to have an outstanding number of 
	 * descriptors instead of a negative one. So be anal when checking. */
	if (size <= (sizeof (ParrilladaScsiGetPerfHdr) + sizeof (ParrilladaScsiWrtSpdDesc))) {
		PARRILLADA_MEDIA_LOG ("No descriptors");
		goto end;
	}

	/* Calculate the number of descriptors */
	num_desc = (size - sizeof (ParrilladaScsiGetPerfHdr)) / sizeof (ParrilladaScsiWrtSpdDesc);
	PARRILLADA_MEDIA_LOG ("Got %d descriptor(s)", num_desc);

	if (num_desc <= 0)
		goto end; 

	priv->rd_speeds = g_new0 (guint, num_desc + 1);
	priv->wr_speeds = g_new0 (guint, num_desc + 1);

	max_rd = 0;
	max_wrt = 0;

	desc = (ParrilladaScsiWrtSpdDesc*) &wrt_perf->data;

	for (i = 0; i < num_desc; i ++) {
		PARRILLADA_MEDIA_LOG ("Descriptor n° %d, address = %p", i, (desc + i));

		priv->rd_speeds [i] = PARRILLADA_GET_32 (desc [i].rd_speed);
		priv->wr_speeds [i] = PARRILLADA_GET_32 (desc [i].wr_speed);

		PARRILLADA_MEDIA_LOG ("RD = %u / WRT = %u",
				   priv->rd_speeds [i],
				   priv->wr_speeds [i]);

		max_rd = MAX (max_rd, priv->rd_speeds [i]);
		max_wrt = MAX (max_wrt, priv->wr_speeds [i]);
	}

	priv->max_rd = max_rd;
	priv->max_wrt = max_wrt;

	PARRILLADA_MEDIA_LOG ("Maximum Speed (mmc3) %i", max_wrt);

end:

	g_free (wrt_perf);

	/* strangely there are so drives (I know one case) which support this
	 * function but don't report any speed. So if our top speed is 0 then
	 * use the other way to get the speed. It was a Teac */
	if (!priv->max_wrt)
		return FALSE;

	return TRUE;
}

static gboolean
parrillada_medium_get_page_2A_write_speed_desc (ParrilladaMedium *self,
					     ParrilladaDeviceHandle *handle,
					     ParrilladaScsiErrCode *code)
{
	ParrilladaScsiStatusPage *page_2A = NULL;
	ParrilladaScsiStatusWrSpdDesc *desc;
	ParrilladaScsiModeData *data = NULL;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	gint desc_num, i;
	gint max_wrt = 0;
	gint max_num;
	int size = 0;

	PARRILLADA_MEDIA_LOG ("Retrieving speed (2A speeds)");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);
	result = parrillada_spc1_mode_sense_get_page (handle,
						   PARRILLADA_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("MODE SENSE failed");
		return FALSE;
	}

	page_2A = (ParrilladaScsiStatusPage *) &data->page;

	/* Reminder: size = sizeof (ParrilladaScsiStatusPage) + sizeof (ParrilladaScsiModeHdr) */
 	size = MIN (size, sizeof (data->hdr.len) + PARRILLADA_GET_16 (data->hdr.len));

	if (size < (G_STRUCT_OFFSET (ParrilladaScsiStatusPage, copy_mngt_rev) + sizeof (ParrilladaScsiModeHdr))) {
		g_free (data);
		PARRILLADA_MEDIA_LOG ("wrong page size");
		return FALSE;
	}

	priv->max_rd = PARRILLADA_GET_16 (page_2A->rd_max_speed);
	priv->max_wrt = PARRILLADA_GET_16 (page_2A->wr_max_speed);

	/* Check if we can use the speed descriptors. There must be at least one
	 * available; if not use maximum speed member. */
	if (size < (G_STRUCT_OFFSET (ParrilladaScsiStatusPage, wr_spd_desc) +
		    sizeof (ParrilladaScsiModeHdr) +
		    sizeof (ParrilladaScsiWrtSpdDesc))) {
		PARRILLADA_MEDIA_LOG ("Maximum Speed (Page 2A [old]) %i", priv->max_wrt);

		/* also add fake speed descriptors */
		priv->wr_speeds = g_new0 (guint, 2);
		priv->wr_speeds [0] = PARRILLADA_GET_16 (page_2A->wr_max_speed);
		priv->rd_speeds = g_new0 (guint, 2);
		priv->rd_speeds [0] = PARRILLADA_GET_16 (page_2A->rd_max_speed);

		g_free (data);
		return TRUE;
	}

	desc_num = PARRILLADA_GET_16 (page_2A->wr_speed_desc_num);
	max_num = size -
		  sizeof (ParrilladaScsiStatusPage) -
		  sizeof (ParrilladaScsiModeHdr);
	max_num /= sizeof (ParrilladaScsiWrtSpdDesc);

	if (max_num < 0)
		max_num = 0;

	if (desc_num > max_num)
		desc_num = max_num;

	priv->wr_speeds = g_new0 (guint, desc_num + 1);

	desc = page_2A->wr_spd_desc;
	for (i = 0; i < desc_num; i ++) {
		/* It happens (I have such a drive) that it returns descriptors
		 * with the same speeds each (in this case the maximum) */
		if (i > 0 && priv->wr_speeds [i-1] == PARRILLADA_GET_16 (desc [i].speed))
			continue;

		priv->wr_speeds [i] = PARRILLADA_GET_16 (desc [i].speed);
		max_wrt = MAX (max_wrt, priv->wr_speeds [i]);
	}

	if (max_wrt)
		priv->max_wrt = max_wrt;

	PARRILLADA_MEDIA_LOG ("Maximum Speed (Page 2A) %i", priv->max_wrt);
	g_free (data);

	return TRUE;
}

static gboolean
parrillada_medium_get_speed (ParrilladaMedium *self,
			  ParrilladaDeviceHandle *handle,
			  ParrilladaScsiErrCode *code)
{
	ParrilladaScsiResult result;

	PARRILLADA_MEDIA_LOG ("Retrieving media available speeds");

	result = parrillada_medium_get_speed_mmc3 (self, handle, code);
	if (result == TRUE)
		return result;

	/* Fallback */
	result = parrillada_medium_get_page_2A_write_speed_desc (self, handle, code);
	return result;
}

/**
 * Functions to get information about disc contents
 */

static gboolean
parrillada_medium_track_volume_size (ParrilladaMedium *self,
				  ParrilladaMediumTrack *track,
				  ParrilladaDeviceHandle *handle)
{
	GError *error = NULL;
	ParrilladaVolSrc *vol;
	gint64 nb_blocks;
	gboolean res;

	if (!track)
		return FALSE;

	/* This is a special case. For DVD+RW and DVD-RW in restricted
	 * mode, there is only one session that takes the whole disc size
	 * once formatted. That doesn't necessarily means they have data
	 * Note also that they are reported as complete though you can
	 * still add data (with growisofs). It is nevertheless on the 
	 * condition that the fs is valid.
	 * So we check if their first and only volume is valid. 
	 * That's also used when the track size is reported 300 KiB
	 * see below */
	vol = parrillada_volume_source_open_device_handle (handle, NULL);
	res = parrillada_volume_get_size (vol,
				       track->start,
				       &nb_blocks,
				       &error);
	parrillada_volume_source_close (vol);

	if (!res) {
		PARRILLADA_MEDIA_LOG ("Failed to retrieve the volume size: %s",
				  error && error->message ? 
				  error->message:"unknown error");

		if (error)
			g_error_free (error);

		return FALSE;
	}

	track->blocks_num = nb_blocks;
	return TRUE;
}

static gboolean
parrillada_medium_track_written_SAO (ParrilladaDeviceHandle *handle,
				  int track_num,
				  int track_start)
{
	ParrilladaScsiErrCode error = PARRILLADA_SCSI_ERROR_NONE;
	unsigned char buffer [2048];
	ParrilladaScsiResult result;

	PARRILLADA_MEDIA_LOG ("Checking for TDBs in track pregap.");

	/* To avoid blocking try to check whether it is readable */
	result = parrillada_mmc1_read_block (handle,
					  TRUE,
					  PARRILLADA_SCSI_BLOCK_TYPE_ANY,
					  PARRILLADA_SCSI_BLOCK_HEADER_NONE,
					  PARRILLADA_SCSI_BLOCK_NO_SUBCHANNEL,
					  track_start - 1,
					  1,
					  NULL,
					  0,
					  &error);
	if (result != PARRILLADA_SCSI_OK || error != PARRILLADA_SCSI_ERROR_NONE)
		return TRUE;

	result = parrillada_mmc1_read_block (handle,
					  TRUE,
					  PARRILLADA_SCSI_BLOCK_TYPE_ANY,
					  PARRILLADA_SCSI_BLOCK_HEADER_NONE,
					  PARRILLADA_SCSI_BLOCK_NO_SUBCHANNEL,
					  track_start - 1,
					  1,
					  buffer,
					  sizeof (buffer),
					  &error);
	if (result == PARRILLADA_SCSI_OK && error == PARRILLADA_SCSI_ERROR_NONE) {
		int i;

		if (buffer [0] != 'T' || buffer [1] != 'D' || buffer [2] != 'I') {
			PARRILLADA_MEDIA_LOG ("Track was probably recorded in SAO mode - no TDB.");
			return TRUE;
		}

		/* Find the TDU (16 bytes) for the track (there can be for other tracks).
		 * i must be < 128 = ((2048 - 8 (size TDB)) / 16 (size TDU). */
		for (i = 0; i < 128; i ++) {
			if (PARRILLADA_GET_BCD (buffer [8 + i * 16]) != track_num)
				break;
		}

		if (i >= 128) {
			PARRILLADA_MEDIA_LOG ("No appropriate TDU for track");
			return TRUE;
		}

		if (buffer [8 + i * 16] == 0x80 || buffer [8 + i * 16] == 0x00) {
			PARRILLADA_MEDIA_LOG ("Track was recorded in TAO mode.");
			return FALSE;
		}

		PARRILLADA_MEDIA_LOG ("Track was recorded in Packet mode.");
		return FALSE;
	}

	PARRILLADA_MEDIA_LOG ("No pregap. That track must have been recorded in SAO mode.");
	return TRUE;
}

static gboolean
parrillada_medium_track_get_info (ParrilladaMedium *self,
			       gboolean multisession,
			       ParrilladaMediumTrack *track,
			       int track_num,
			       ParrilladaDeviceHandle *handle,
			       ParrilladaScsiErrCode *code)
{
	ParrilladaScsiTrackInfo track_info;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	PARRILLADA_MEDIA_LOG ("Retrieving track information for %i", track_num);

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* at this point we know the type of the disc that's why we set the 
	 * size according to this type. That may help to avoid outrange address
	 * errors. */
	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DUAL_L|PARRILLADA_MEDIUM_WRITABLE))
		size = 48;
	else if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_PLUS|PARRILLADA_MEDIUM_WRITABLE))
		size = 40;
	else
		size = 36;

	result = parrillada_mmc1_read_track_info (handle,
					       track_num,
					       &track_info,
					       &size,
					       code);

	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ TRACK INFO failed");
		return FALSE;
	}

	track->blocks_num = PARRILLADA_GET_32 (track_info.track_size);
	track->session = PARRILLADA_SCSI_SESSION_NUM (track_info);

	if (track->blocks_num <= 300) {
		/* Now here is a potential bug: we can write tracks (data or
		 * not) shorter than 300 KiB /2 sec but they will be padded to
		 * reach this floor value. It means that blocks_num is always
		 * 300 blocks even if the data length on the track is actually
		 * shorter.
		 * So we read the volume descriptor. If it works, good otherwise
		 * use the old value.
		 * That's important for checksuming to have a perfect account of
		 * the data size. */
		PARRILLADA_MEDIA_LOG ("300 sectors size. Checking for real size");
		parrillada_medium_track_volume_size (self, track, handle);
	}
	/* NOTE: for multisession CDs only
	 * if the session was incremental (TAO/packet/...) by opposition to DAO
	 * and SAO, then 2 blocks (run-out) have been added at the end of user
	 * track for linking. That's why we have 2 additional sectors when the
	 * track has been recorded in TAO mode
	 * See MMC5
	 * 6.44.3.2 CD-R Fixed Packet, Variable Packet, Track-At-Once
	 * Now, strangely track_get_info always removes two blocks, whereas read
	 * raw toc adds them (always) and this, whatever the mode, the position.
	 * It means that when we detect a SAO session we have to add 2 blocks to
	 * all tracks in it. 
	 * See # for any information:
	 * if first track is recorded in SAO/DAO then the length will be two sec
	 * shorter. If not, if it was recorded in TAO, that's fine.
	 * The other way would be to use read raw toc but then that's the
	 * opposite that happens and that latter will return two more bytes for
	 * TAO recorded session.
	 * So there are 2 workarounds:
	 * - read the volume size (can be unreliable)
	 * - read the 2 last blocks and see if they are run-outs
	 * here we do solution 2 but only for CDRW, not blank, and for first
	 * session only since that's the only one that can be recorded in DAO. */
	else if (track->session == 1
	     && (track->type & PARRILLADA_MEDIUM_TRACK_DATA)
	     &&  multisession
	     &&  (priv->info & PARRILLADA_MEDIUM_CD)
	     && !(priv->info & PARRILLADA_MEDIUM_ROM)) {
		PARRILLADA_MEDIA_LOG ("Data track belongs to first session of multisession CD. "
				   "Checking for real size (%i sectors currently).",
				   track->blocks_num);

		/* we test the pregaps blocks for TDB: these are special blocks
		 * filling the pregap of a track when it was recorded as TAO or
		 * as Packet.
		 * NOTE: in this case we need to skip 7 sectors before since if
		 * it was recorded incrementally then there is also 4 runins,
		 * 1 link sector and 2 runouts (at end of pregap). 
		 * we also make sure that the two blocks we're adding are
		 * actually readable. */
		/* Test the last block, the before last and the one before before last */
		result = parrillada_mmc1_read_block (handle,
						  FALSE,
						  PARRILLADA_SCSI_BLOCK_TYPE_ANY,
						  PARRILLADA_SCSI_BLOCK_HEADER_NONE,
						  PARRILLADA_SCSI_BLOCK_NO_SUBCHANNEL,
						  track->blocks_num + track->start,
						  2,
						  NULL,
						  0,
						  NULL);

		if (result == PARRILLADA_SCSI_OK) {
			PARRILLADA_MEDIA_LOG ("Following two sectors are readable.");

			if (parrillada_medium_track_written_SAO (handle, track_num, track->start)) {
				track->blocks_num += 2;
				PARRILLADA_MEDIA_LOG ("Correcting track size (now %i)", track->blocks_num);
			}
		}
		else
			PARRILLADA_MEDIA_LOG ("Detected runouts");
	}

	/* NOTE: DVD+RW, DVD-RW (restricted overwrite) never reach this function */
	PARRILLADA_MEDIA_LOG ("Track %i (session %i): type = %i start = %llu size = %llu",
			  track_num,
			  track->session,
			  track->type,
			  track->start,
			  track->blocks_num);

	return TRUE;
}

static gboolean
parrillada_medium_track_set_leadout_DVDR_blank (ParrilladaMedium *self,
					     ParrilladaDeviceHandle *handle,
					     ParrilladaMediumTrack *leadout,
					     ParrilladaScsiErrCode *code)
{
	ParrilladaScsiFormatCapacitiesHdr *hdr = NULL;
	ParrilladaScsiMaxCapacityDesc *current;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Using fallback method for blank CDR to retrieve NWA and leadout information");

	/* NWA is easy for blank DVD-Rs, it's 0. So far, so good... */
	priv->next_wr_add = 0;

	result = parrillada_mmc2_read_format_capacities (handle,
						      &hdr,
						      &size,
						      code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ FORMAT CAPACITIES failed");
		return FALSE;
	}

	/* See if the media is already formatted which means for -R media that 
	 * they are blank. */
	current = hdr->max_caps;
	if (current->type & PARRILLADA_SCSI_DESC_FORMATTED) {
		PARRILLADA_MEDIA_LOG ("Formatted medium");
		g_free (hdr);
		return FALSE;
	}
		
	PARRILLADA_MEDIA_LOG ("Unformatted medium");

	/* of course it starts at 0 since it's empty */
	leadout->start = 0;
	leadout->blocks_num = PARRILLADA_GET_32 (current->blocks_num);

	PARRILLADA_MEDIA_LOG ("Leadout (through READ FORMAT CAPACITIES): start = %llu size = %llu",
			  leadout->start,
			  leadout->blocks_num);

	g_free (hdr);
	return TRUE;
}

static gboolean
parrillada_medium_track_set_leadout_CDR_blank (ParrilladaMedium *self,
					    ParrilladaDeviceHandle *handle,
					    ParrilladaMediumTrack *leadout,
					    ParrilladaScsiErrCode *code)
{
	ParrilladaScsiAtipData *atip = NULL;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size = 0;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Using fallback method for blank CDR to retrieve NWA and leadout information");

	/* NWA is easy for blank CDRs, it's 0. So far, so good... */
	priv->next_wr_add = 0;

	result = parrillada_mmc1_read_atip (handle, &atip, &size, code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ ATIP failed");
		return FALSE;
	}

	leadout->blocks_num = atip->desc->leadout_mn * 60 * 75 +
			      atip->desc->leadout_sec * 75 +
			      atip->desc->leadout_frame;

	/* of course it starts at 0 since it's empty */
	leadout->start = 0;

	PARRILLADA_MEDIA_LOG ("Leadout (through READ ATIP): start = %llu size = %llu",
			  leadout->start,
			  leadout->blocks_num);

	g_free (atip);

	return TRUE;
}

static gboolean
parrillada_medium_track_set_leadout (ParrilladaMedium *self,
				  ParrilladaDeviceHandle *handle,
				  ParrilladaMediumTrack *leadout,
				  ParrilladaScsiErrCode *code)
{
	ParrilladaScsiTrackInfo track_info;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	gint track_num;
	int size;

	PARRILLADA_MEDIA_LOG ("Retrieving NWA and leadout information");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	if (PARRILLADA_MEDIUM_RANDOM_WRITABLE (priv->info)) {
		PARRILLADA_MEDIA_LOG ("Overwritable medium  => skipping");
		return TRUE;
	}

	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDR)) {
		/* This is necessary to make sure nwa won't be the start of the
		 * pregap if the current write mode is SAO with blank CDR.
		 * Carry on even if it fails.
		 * This can work with CD-R/W and DVD-R/W. + media don't use the
		 * write mode page anyway. */
		result = parrillada_medium_set_write_mode_page_tao (self, handle, code);
		if (result == FALSE
		&&  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDR|PARRILLADA_MEDIUM_BLANK))
			return parrillada_medium_track_set_leadout_CDR_blank (self,
									   handle,
									   leadout,
									   code);
	}

	/* At this point we know the type of the disc that's why we set the 
	 * size according to this type. That may help to avoid outrange address
	 * errors. */
	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DUAL_L|PARRILLADA_MEDIUM_WRITABLE))
		size = 48;
	else if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_PLUS|PARRILLADA_MEDIUM_WRITABLE))
		size = 40;
	else
		size = 36;

	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDR)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDRW)
	/* The following includes DL */
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDR_PLUS)) 
		track_num = 0xFF;
	else if (priv->first_open_track >= 0)
		track_num = priv->first_open_track;
	else {
		PARRILLADA_MEDIA_LOG ("There aren't any open session set");
		return FALSE;
	}

	result = parrillada_mmc1_read_track_info (handle,
					       track_num,
					       &track_info,
					       &size,
					       code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ TRACK INFO failed");

		/* This only for CD-R */
		if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDR|PARRILLADA_MEDIUM_BLANK))
			return parrillada_medium_track_set_leadout_CDR_blank (self,
									   handle,
									   leadout,
									   code);
		else if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BLANK))
			return parrillada_medium_track_set_leadout_DVDR_blank (self,
									    handle,
									    leadout,
									    code);
			 
		return FALSE;
	}

	PARRILLADA_MEDIA_LOG ("Next Writable Address is %d", PARRILLADA_GET_32 (track_info.next_wrt_address));
	if (track_info.next_wrt_address_valid)
		priv->next_wr_add = PARRILLADA_GET_32 (track_info.next_wrt_address);
	else
		PARRILLADA_MEDIA_LOG ("Next Writable Address is not valid");

	/* Set free space */
	PARRILLADA_MEDIA_LOG ("Free blocks %d", PARRILLADA_GET_32 (track_info.free_blocks));
	leadout->blocks_num = PARRILLADA_GET_32 (track_info.free_blocks);

	if (!leadout->blocks_num) {
		leadout->blocks_num = PARRILLADA_GET_32 (track_info.track_size);
		PARRILLADA_MEDIA_LOG ("Using track size %d", leadout->blocks_num);
	}

	if (!leadout->blocks_num
	&&   PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BLANK))
		return parrillada_medium_track_set_leadout_DVDR_blank (self,
								    handle,
								    leadout,
								    code);

	PARRILLADA_MEDIA_LOG ("Leadout: start = %llu size = %llu",
			  leadout->start,
			  leadout->blocks_num);

	return TRUE;
}

/**
 * NOTE: for DVD-R multisession we lose 28688 blocks for each session
 * so the capacity is the addition of all session sizes + 28688 for each
 * For all multisession DVD-/+R and CDR-RW the remaining size is given 
 * in the leadout. One exception though with DVD+/-RW.
 */

static void
parrillada_medium_add_DVD_plus_RW_leadout (ParrilladaMedium *self)
{
	ParrilladaMediumTrack *leadout;
	ParrilladaMediumPrivate *priv;
	gint64 blocks_num;
	gint32 start;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* determine the start */
	if (priv->tracks) {
		ParrilladaMediumTrack *track;

		track = priv->tracks->data;
		start = track->start + track->blocks_num;
		blocks_num = priv->block_num - ((track->blocks_num > 300) ? track->blocks_num : 300);
	}
	else {
		start = 0;
		blocks_num = priv->block_num;
	}

	leadout = g_new0 (ParrilladaMediumTrack, 1);
	priv->tracks = g_slist_append (priv->tracks, leadout);

	leadout->start = start;
	leadout->blocks_num = blocks_num;
	leadout->type = PARRILLADA_MEDIUM_TRACK_LEADOUT;

	/* we fabricate the leadout here. We don't really need one in 
	 * fact since it is always at the last sector whatever the
	 * amount of data written. So we need in fact to read the file
	 * system and get the last sector from it. Hopefully it won't be
	 * buggy */
	priv->next_wr_add = 0;

	PARRILLADA_MEDIA_LOG ("Adding fabricated leadout start = %llu length = %llu",
			  leadout->start,
			  leadout->blocks_num);
}

static gboolean
parrillada_medium_get_sessions_info (ParrilladaMedium *self,
				  ParrilladaDeviceHandle *handle,
				  ParrilladaScsiErrCode *code)
{
	int num, i, size;
	gboolean multisession;
	ParrilladaScsiResult result;
	ParrilladaScsiTocDesc *desc;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiFormattedTocData *toc = NULL;

	PARRILLADA_MEDIA_LOG ("Reading Toc");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

tryagain:

	result = parrillada_mmc1_read_toc_formatted (handle,
						  0,
						  &toc,
						  &size,
						  code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ TOC failed");
		return FALSE;
	}

	if (priv->probe_cancelled) {
		g_free (toc);
		return FALSE;
	}

	/* My drive with some Video CDs gets a size of 2 (basically the size
	 * member of the structure) without any error. Consider the drive is not
	 * ready and needs retrying */
	if (size < sizeof (ParrilladaScsiFormattedTocData)) {
		g_free (toc);
		toc = NULL;
		goto tryagain;
	}

	num = (size - sizeof (ParrilladaScsiFormattedTocData)) /
	       sizeof (ParrilladaScsiTocDesc);

	/* remove 1 for leadout */
	multisession = !(priv->info & PARRILLADA_MEDIUM_BLANK) && num > 0;

	/* NOTE: in the case of DVD- there are always only 3 sessions if they
	 * are open: all first concatenated sessions, the last session, and the
	 * leadout. */
	
	PARRILLADA_MEDIA_LOG ("%i track(s) found", num);

	desc = toc->desc;
	for (i = 0; i < num; i ++, desc ++) {
		ParrilladaMediumTrack *track;

		if (desc->track_num == PARRILLADA_SCSI_TRACK_LEADOUT_START) {
			PARRILLADA_MEDIA_LOG ("Leadout reached %d",
					   PARRILLADA_GET_32 (desc->track_start));
			break;
		}

		track = g_new0 (ParrilladaMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = PARRILLADA_GET_32 (desc->track_start);

		/* we shouldn't request info on a track if the disc is closed */
		if (desc->control & PARRILLADA_SCSI_TRACK_COPY)
			track->type |= PARRILLADA_MEDIUM_TRACK_COPY;

		if (!(desc->control & PARRILLADA_SCSI_TRACK_DATA)) {
			track->type |= PARRILLADA_MEDIUM_TRACK_AUDIO;
			priv->info |= PARRILLADA_MEDIUM_HAS_AUDIO;

			if (desc->control & PARRILLADA_SCSI_TRACK_PREEMP)
				track->type |= PARRILLADA_MEDIUM_TRACK_PREEMP;

			if (desc->control & PARRILLADA_SCSI_TRACK_4_CHANNELS)
				track->type |= PARRILLADA_MEDIUM_TRACK_4_CHANNELS;
		}
		else {
			track->type |= PARRILLADA_MEDIUM_TRACK_DATA;
			priv->info |= PARRILLADA_MEDIUM_HAS_DATA;

			if (desc->control & PARRILLADA_SCSI_TRACK_DATA_INCREMENTAL)
				track->type |= PARRILLADA_MEDIUM_TRACK_INCREMENTAL;
		}

		if (PARRILLADA_MEDIUM_RANDOM_WRITABLE (priv->info)) {
			gboolean result;

			/* A special case for these kinds of media (DVD+RW, ...)
			 * which have only one track: the first. Since it's not
			 * possible to know the amount of data that were really
			 * written in this session, read the filesystem. */
			PARRILLADA_MEDIA_LOG ("DVD+RW (DL) or DVD-RW (restricted overwrite) checking volume size (start = %i)", track->start);
			track->session = 1;
			track->start = 0;
			result = parrillada_medium_track_volume_size (self, 
								   track,
								   handle);
			if (result != TRUE) {
				priv->tracks = g_slist_remove (priv->tracks, track);
				g_free (track);

				priv->info |= PARRILLADA_MEDIUM_BLANK;
				priv->info &= ~(PARRILLADA_MEDIUM_CLOSED|
					        PARRILLADA_MEDIUM_HAS_DATA);

				PARRILLADA_MEDIA_LOG ("Empty first session.");
			}
			else {
				priv->next_wr_add = 0;
				PARRILLADA_MEDIA_LOG ("Track 1 (session %i): type = %i start = %llu size = %llu",
						  track->session,
						  track->type,
						  track->start,
						  track->blocks_num);
			}

			/* NOTE: the next track should be the leadout */
			continue;
		}

		if (priv->probe_cancelled) {
			g_free (toc);
			return FALSE;
		}

		parrillada_medium_track_get_info (self,
					       multisession,
					       track,
					       g_slist_length (priv->tracks),
					       handle,
					       code);
	}

	if (priv->probe_cancelled) {
		g_free (toc);
		return FALSE;
	}

	/* put the tracks in the right order */
	priv->tracks = g_slist_reverse (priv->tracks);

	if (PARRILLADA_MEDIUM_RANDOM_WRITABLE (priv->info))
		parrillada_medium_add_DVD_plus_RW_leadout (self);
	else if (!(priv->info & PARRILLADA_MEDIUM_CLOSED)) {
		ParrilladaMediumTrack *leadout;

		/* we shouldn't request info on leadout if the disc is closed
		 * (except for DVD+/- (restricted) RW (see above) */
		leadout = g_new0 (ParrilladaMediumTrack, 1);
		leadout->start = PARRILLADA_GET_32 (desc->track_start);
		leadout->type = PARRILLADA_MEDIUM_TRACK_LEADOUT;
		priv->tracks = g_slist_append (priv->tracks, leadout);

		parrillada_medium_track_set_leadout (self,
						  handle,
						  leadout,
						  code);
	}

	g_free (toc);

	return TRUE;
}

static void
parrillada_medium_get_DVD_id (ParrilladaMedium *self,
			   ParrilladaDeviceHandle *handle,
			   ParrilladaScsiErrCode *code)
{
	gint size = 0;
	ParrilladaScsiResult result;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiReadDiscStructureHdr *hdr = NULL;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* This should be only possible for DVD-R(W) and not with all drives */
	result = parrillada_mmc2_read_generic_structure (handle,
						      PARRILLADA_SCSI_FORMAT_LESS_MEDIA_ID_DVD,
						      &hdr,
						      &size,
						      code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("Retrieval of DVD id failed");
		return;
	}

	PARRILLADA_MEDIA_LOG ("DVD id %d", PARRILLADA_GET_16 (hdr->data + 2));
	priv->id = g_strdup_printf ("%d", PARRILLADA_GET_16 (hdr->data + 2));
	g_free (hdr);
}

static gboolean
parrillada_medium_set_blank (ParrilladaMedium *self,
			  ParrilladaDeviceHandle *handle,
			  gint first_open_track,
			  ParrilladaScsiErrCode *code)
{
	ParrilladaMediumPrivate *priv;
	ParrilladaMediumTrack *track;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Empty media");

	priv->info |= PARRILLADA_MEDIUM_BLANK;
	priv->block_size = 2048;

	priv->first_open_track = first_open_track;
	PARRILLADA_MEDIA_LOG ("First open track %d", priv->first_open_track);

	if (PARRILLADA_MEDIUM_RANDOM_WRITABLE (priv->info))
		parrillada_medium_add_DVD_plus_RW_leadout (self);
	else {
		track = g_new0 (ParrilladaMediumTrack, 1);
		track->start = 0;
		track->type = PARRILLADA_MEDIUM_TRACK_LEADOUT;
		priv->tracks = g_slist_prepend (priv->tracks, track);
			
		parrillada_medium_track_set_leadout (self,
						  handle,
						  track,
						  code);
	}

	return TRUE;
}

static gboolean
parrillada_medium_get_contents (ParrilladaMedium *self,
			     ParrilladaDeviceHandle *handle,
			     ParrilladaScsiErrCode *code)
{
	int size;
	gboolean res = TRUE;
	ParrilladaScsiResult result;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiDiscInfoStd *info = NULL;

	PARRILLADA_MEDIA_LOG ("Retrieving media status");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	result = parrillada_mmc1_read_disc_information_std (handle,
							 &info,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("READ DISC INFORMATION failed");
		return FALSE;
	}

	if (info->disc_id_valid) {
		/* Try to get the disc identification if possible (CDs only) */
		PARRILLADA_MEDIA_LOG ("Disc id %i", PARRILLADA_GET_32 (info->disc_id));
		priv->id = g_strdup_printf ("%d", PARRILLADA_GET_32 (info->disc_id));
	}
	else if (priv->info & PARRILLADA_MEDIUM_DVD)
		parrillada_medium_get_DVD_id (self, handle, code);

	if (info->erasable)
		priv->info |= PARRILLADA_MEDIUM_REWRITABLE;

	priv->first_open_track = -1;

	if (info->status == PARRILLADA_SCSI_DISC_EMPTY) {
		res = parrillada_medium_set_blank (self,
						handle,
						PARRILLADA_FIRST_TRACK_IN_LAST_SESSION (info),
						code);
	}
	else if (info->status == PARRILLADA_SCSI_DISC_INCOMPLETE) {
		if (!PARRILLADA_MEDIUM_RANDOM_WRITABLE (priv->info)) {
			priv->info |= PARRILLADA_MEDIUM_APPENDABLE;

			/* This is just to make sure the disc is in a correct
			 * state as I saw some drive being flagged as unformatted
			 * appendable */
			priv->info &= ~(PARRILLADA_MEDIUM_UNFORMATTED);

			PARRILLADA_MEDIA_LOG ("Appendable media");

			priv->first_open_track = PARRILLADA_FIRST_TRACK_IN_LAST_SESSION (info);
			PARRILLADA_MEDIA_LOG ("First track in last open session %d", priv->first_open_track);

			res = parrillada_medium_get_sessions_info (self, handle, code);
		}
		else {
			/* if that type of media is in incomplete state that
			 * means it has just been formatted. And therefore it's
			 * blank. */
			res = parrillada_medium_set_blank (self,
							handle,
							PARRILLADA_FIRST_TRACK_IN_LAST_SESSION (info),
							code);
		}
	}
	else if (info->status == PARRILLADA_SCSI_DISC_FINALIZED) {
		priv->info |= PARRILLADA_MEDIUM_CLOSED;
		PARRILLADA_MEDIA_LOG ("Closed media");

		res = parrillada_medium_get_sessions_info (self, handle, code);
	}

	g_free (info);
	return res;
}

/**
 * Some identification functions
 */

static gboolean
parrillada_medium_get_medium_type (ParrilladaMedium *self,
				ParrilladaDeviceHandle *handle,
				ParrilladaScsiErrCode *code)
{
	ParrilladaScsiProfile profile;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;

	PARRILLADA_MEDIA_LOG ("Retrieving media profile");

	priv = PARRILLADA_MEDIUM_PRIVATE (self);
	result = parrillada_mmc2_get_profile (handle, &profile, code);

	if (result != PARRILLADA_SCSI_OK) {
		ParrilladaScsiAtipData *data = NULL;
		int size = 0;

		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");

		/* This could be a MMC1 drive since this command was
		 * introduced in MMC2 and is supported onward. So it
		 * has to be a CD (R/RW). The rest of the information
		 * will be provided by read_disc_information. */

		/* retrieve the speed */
		result = parrillada_medium_get_page_2A_write_speed_desc (self,
								      handle,
								      code);

		/* If this fails it means that this drive is probably older than
		 * MMC1 spec or does not conform to it. */
		if (result != TRUE) {
			priv->info = PARRILLADA_MEDIUM_NONE;
			return FALSE;
		}

		/* The only thing here left to determine is if that's a WRITABLE
		 * or a REWRITABLE. To determine that information, we need to
		 * read TocPmaAtip. It if fails that's a ROM, if it succeeds.
		 * No need to set error code since we consider that it's a ROM
		 * if a failure happens. */
		result = parrillada_mmc1_read_atip (handle,
						 &data,
						 &size,
						 NULL);
		if (result != PARRILLADA_SCSI_OK) {
			/* CD-ROM */
			priv->info = PARRILLADA_MEDIUM_CDROM;
			priv->type = types [1];
		}
		else {
			/* check the size of the structure: it must be at least 8 bytes long */
			if (size < 8) {
				if (size)
					g_free (data);

				PARRILLADA_MEDIA_LOG ("READ ATIP failed (wrong size)");
				return FALSE;
			}

			if (data->desc->erasable) {
				/* CDRW */
				priv->info = PARRILLADA_MEDIUM_CDRW;
				priv->type = types [3];
			}
			else {
				/* CDR */
				priv->info = PARRILLADA_MEDIUM_CDR;
				priv->type = types [2];
			}

			g_free (data);
		}

		return result;
	}

	switch (profile) {
	case PARRILLADA_SCSI_PROF_EMPTY:
		priv->info = PARRILLADA_MEDIUM_NONE;
		return FALSE;

	case PARRILLADA_SCSI_PROF_CDROM:
		priv->info = PARRILLADA_MEDIUM_CDROM;
		priv->type = types [1];
		break;

	case PARRILLADA_SCSI_PROF_CDR:
		priv->info = PARRILLADA_MEDIUM_CDR;
		priv->type = types [2];
		break;

	case PARRILLADA_SCSI_PROF_CDRW:
		priv->info = PARRILLADA_MEDIUM_CDRW;
		priv->type = types [3];
		break;

	case PARRILLADA_SCSI_PROF_DVD_ROM:
		priv->info = PARRILLADA_MEDIUM_DVD_ROM;
		priv->type = types [4];
		break;

	case PARRILLADA_SCSI_PROF_DVD_R:
		priv->info = PARRILLADA_MEDIUM_DVDR;
		priv->type = types [5];
		break;

	case PARRILLADA_SCSI_PROF_DVD_RW_RESTRICTED:
		priv->info = PARRILLADA_MEDIUM_DVDRW_RESTRICTED;
		priv->type = types [6];
		break;

	case PARRILLADA_SCSI_PROF_DVD_RW_SEQUENTIAL:
		priv->info = PARRILLADA_MEDIUM_DVDRW;
		priv->type = types [6];
		break;

	case PARRILLADA_SCSI_PROF_DVD_R_PLUS:
		priv->info = PARRILLADA_MEDIUM_DVDR_PLUS;
		priv->type = types [7];
		break;

	case PARRILLADA_SCSI_PROF_DVD_RW_PLUS:
		priv->info = PARRILLADA_MEDIUM_DVDRW_PLUS;
		priv->type = types [8];
		break;

	case PARRILLADA_SCSI_PROF_DVD_R_PLUS_DL:
		priv->info = PARRILLADA_MEDIUM_DVDR_PLUS_DL;
		priv->type = types [9];
		break;

	case PARRILLADA_SCSI_PROF_DVD_RW_PLUS_DL:
		priv->info = PARRILLADA_MEDIUM_DVDRW_PLUS_DL;
		priv->type = types [10];
		break;

	case PARRILLADA_SCSI_PROF_DVD_R_DL_SEQUENTIAL:
		priv->info = PARRILLADA_MEDIUM_DVDR_DL;
		priv->type = types [11];
		break;

	case PARRILLADA_SCSI_PROF_DVD_R_DL_JUMP:
		priv->info = PARRILLADA_MEDIUM_DVDR_JUMP_DL;
		priv->type = types [11];
		break;

	case PARRILLADA_SCSI_PROF_BD_ROM:
		priv->info = PARRILLADA_MEDIUM_BD_ROM;
		priv->type = types [13];
		break;

	case PARRILLADA_SCSI_PROF_BR_R_SEQUENTIAL:
		/* check if that's a POW later */
		priv->info = PARRILLADA_MEDIUM_BDR_SRM;
		priv->type = types [14];
		break;

	case PARRILLADA_SCSI_PROF_BR_R_RANDOM:
		priv->info = PARRILLADA_MEDIUM_BDR_RANDOM;
		priv->type = types [14];
		break;

	case PARRILLADA_SCSI_PROF_BD_RW:
		priv->info = PARRILLADA_MEDIUM_BDRE;
		priv->type = types [15];
		break;

	case PARRILLADA_SCSI_PROF_DVD_RAM:
		priv->info = PARRILLADA_MEDIUM_DVD_RAM;
		priv->type = types [12];
		break;

	/* WARNING: these types are recognized, no more */
	case PARRILLADA_SCSI_PROF_NON_REMOVABLE:
	case PARRILLADA_SCSI_PROF_REMOVABLE:
	case PARRILLADA_SCSI_PROF_MO_ERASABLE:
	case PARRILLADA_SCSI_PROF_MO_WRITE_ONCE:
	case PARRILLADA_SCSI_PROF_MO_ADVANCED_STORAGE:
	case PARRILLADA_SCSI_PROF_DDCD_ROM:
	case PARRILLADA_SCSI_PROF_DDCD_R:
	case PARRILLADA_SCSI_PROF_DDCD_RW:
	case PARRILLADA_SCSI_PROF_HD_DVD_ROM:
	case PARRILLADA_SCSI_PROF_HD_DVD_R:
	case PARRILLADA_SCSI_PROF_HD_DVD_RAM:
		priv->info = PARRILLADA_MEDIUM_UNSUPPORTED;
		return FALSE;
	}

	/* Get a more precise idea of what sequential BD-R type we have here */
	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BDR_SRM)) {
		ParrilladaScsiGetConfigHdr *hdr = NULL;
		int size = 0;

		/* check for POW type */
		result = parrillada_mmc2_get_configuration_feature (handle,
								 PARRILLADA_SCSI_FEAT_BDR_POW,
								 &hdr,
								 &size,
								 code);
		if (result == PARRILLADA_SCSI_OK) {
			if (hdr->desc->current) {
				PARRILLADA_MEDIA_LOG ("POW formatted medium detected");
				priv->info |= PARRILLADA_MEDIUM_POW;
			}

			g_free (hdr);			
		}
		else {
			ParrilladaScsiFormatCapacitiesHdr *hdr = NULL;

			/* NOTE: the disc status as far as format is concerned
			 * is done later for all rewritable media. */
			/* check for unformatted media (if it's POW or RANDOM
			 * there is no need of course) */
			result = parrillada_mmc2_read_format_capacities (handle,
								      &hdr,
								      &size,
								      NULL);
			if (result == PARRILLADA_SCSI_OK) {
				ParrilladaScsiMaxCapacityDesc *current;

				current = hdr->max_caps;
				if (!(current->type & PARRILLADA_SCSI_DESC_FORMATTED)) {
					PARRILLADA_MEDIA_LOG ("Unformatted BD-R");
					priv->info |= PARRILLADA_MEDIUM_UNFORMATTED;
				}

				g_free (hdr);
			}
		}		
	}

	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BD)) {
		/* FIXME: check for dual layer BD */
	}

	return TRUE;
}

static gboolean
parrillada_medium_get_css_feature (ParrilladaMedium *self,
				ParrilladaDeviceHandle *handle,
				ParrilladaScsiErrCode *code)
{
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiResult result;
	int size;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	PARRILLADA_MEDIA_LOG ("Testing for Css encrypted media");
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_DVD_CSS,
							 &hdr,
							 &size,
							 code);
	if (result != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	if (hdr->desc->add_len < sizeof (ParrilladaScsiDVDCssDesc)) {
		g_free (hdr);
		return TRUE;
	}

	/* here we just need to see if this feature is current or not */
	if (hdr->desc->current) {
		priv->info |= PARRILLADA_MEDIUM_PROTECTED;
		PARRILLADA_MEDIA_LOG ("media is Css protected");
	}

	g_free (hdr);
	return TRUE;
}

static gboolean
parrillada_medium_get_CD_TEXT (ParrilladaMedium *medium,
			    int type,
			    int track_num,
			    guint charset_CD_TEXT,
			    gboolean double_byte,
			    const char *string)
{
	char *utf8_string;
	ParrilladaMediumPrivate *priv;
	const gchar *charset = NULL;

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	/* For the moment we're only interested in medium title but that could
	 * be extented to all tracks information. */
	switch (type) {
	case PARRILLADA_SCSI_CD_TEXT_ALBUM_TITLE:
		if (track_num)
			return FALSE;

		break;

	case PARRILLADA_SCSI_CD_TEXT_PERFORMER_NAME:
	case PARRILLADA_SCSI_CD_TEXT_SONGWRITER_NAME:
	case PARRILLADA_SCSI_CD_TEXT_COMPOSER_NAME:
	case PARRILLADA_SCSI_CD_TEXT_ARRANGER_NAME:
	case PARRILLADA_SCSI_CD_TEXT_ARTIST_NAME:
	case PARRILLADA_SCSI_CD_TEXT_DISC_ID_INFO:
	case PARRILLADA_SCSI_CD_TEXT_GENRE_ID_INFO:
	case PARRILLADA_SCSI_CD_TEXT_UPC_EAN_ISRC:
	default:
		return FALSE;
	}

	g_get_charset (&charset);

	/* It's ASCII so convert to locale */
	switch (charset_CD_TEXT) {
	case PARRILLADA_CD_TEXT_8859_1:
		utf8_string = g_convert_with_fallback (string,
						       -1,
						       charset,
						       "ISO-8859-1",
						       "_",
						       NULL,
						       NULL,
						       NULL);
		break;
	case PARRILLADA_CD_TEXT_KANJI:
		utf8_string = g_convert_with_fallback (string,
						       -1,
						       charset,
						       "EUC-JP",
						       "_",
						       NULL,
						       NULL,
						       NULL);
		break;
	case PARRILLADA_CD_TEXT_KOREAN:
		utf8_string = g_convert_with_fallback (string,
						       -1,
						       charset,
						       "EUC-KR",
						       "_",
						       NULL,
						       NULL,
						       NULL);
		break;
	case PARRILLADA_CD_TEXT_CHINESE:
		utf8_string = g_convert_with_fallback (string,
						       -1,
						       charset,
						       "GB2312",
						       "_",
						       NULL,
						       NULL,
						       NULL);
		break;
	default:
	case PARRILLADA_CD_TEXT_ASCII:
		utf8_string = g_convert_with_fallback (string,
						       -1,
						       charset,
						       "ASCII",
						       "_",
						       NULL,
						       NULL,
						       NULL);
	}


	if (priv->CD_TEXT_title)
		g_free (priv->CD_TEXT_title);

	if (!utf8_string) {
		PARRILLADA_MEDIA_LOG ("Charset convertion failed");
		priv->CD_TEXT_title = g_strdup (string);
	}
	else
		priv->CD_TEXT_title = utf8_string;

	PARRILLADA_MEDIA_LOG ("CD-TEXT title %s", priv->CD_TEXT_title);
	return TRUE;
}

static int
_next_CD_TEXT_pack (ParrilladaScsiCDTextData *cd_text,
		    int current,
		    int max)
{
	current ++;
	if (current >= max)
		return -1;

	/* Skip all packs we're not interested or are not valid */
	while (cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_ALBUM_TITLE &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_PERFORMER_NAME &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_SONGWRITER_NAME &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_COMPOSER_NAME &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_ARRANGER_NAME &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_ARTIST_NAME &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_DISC_ID_INFO &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_GENRE_ID_INFO &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_UPC_EAN_ISRC &&
	       cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_BLOCK_SIZE) {
		current ++;
		if (current >= max)
			return -1;
	}

	return current;
}

static gboolean
parrillada_medium_read_CD_TEXT_block_info (ParrilladaScsiCDTextData *cd_text,
					int current,
					int max,
					gchar *buffer)
{
	while ((current = _next_CD_TEXT_pack (cd_text, current, max)) != -1) {
		off_t offset = 0;

		if (cd_text->pack [current].type != PARRILLADA_SCSI_CD_TEXT_BLOCK_SIZE)
			continue;

		do {
			memcpy (buffer + offset,
				cd_text->pack [current].text,
				sizeof (cd_text->pack [current].text));

			offset += sizeof (cd_text->pack [current].text);
			current = _next_CD_TEXT_pack (cd_text, current, max);
		} while (current != -1 && cd_text->pack [current].type == PARRILLADA_SCSI_CD_TEXT_BLOCK_SIZE);

		return TRUE;
	}

	return FALSE;
}

static void
parrillada_medium_read_CD_TEXT (ParrilladaMedium *self,
			     ParrilladaDeviceHandle *handle,
			     ParrilladaScsiErrCode *code)
{
	int off;
	gint charset;
	int track_num;
	int num, size, i;
	char buffer [256]; /* mmc specs advise no more than 160 */
	gboolean find_block_info;
	ParrilladaScsiCDTextData *cd_text;

	PARRILLADA_MEDIA_LOG ("Getting CD-TEXT");
	if (parrillada_mmc3_read_cd_text (handle, &cd_text, &size, code) != PARRILLADA_SCSI_OK) {
		PARRILLADA_MEDIA_LOG ("GET CD-TEXT failed");
		return;
	}

	/* Get the number of CD-Text Data Packs.
	 * Some drives seem to report an idiotic cd_text->hdr->len. So use size
	 * to be on a safer side. */
	if (size < sizeof (ParrilladaScsiTocPmaAtipHdr)) {
		g_free (cd_text);
		return;
	}

	num = (size - sizeof (ParrilladaScsiTocPmaAtipHdr)) / sizeof (ParrilladaScsiCDTextPackData);
	if (num <= 0) {
		g_free (cd_text);
		return;
	}

	off = 0;
	track_num = 0;
	charset = PARRILLADA_CD_TEXT_ASCII;

	i = -1;
	find_block_info = TRUE;
	while ((i = _next_CD_TEXT_pack (cd_text, i, num)) != -1) {
		int j;
		gboolean is_double_byte;

		/* skip these until the start of another language block or the end */
		if (cd_text->pack [i].type == PARRILLADA_SCSI_CD_TEXT_BLOCK_SIZE) {
			find_block_info = TRUE;
			continue;
		}

		if (find_block_info) {
			find_block_info = FALSE;

			/* This pack is important since it holds the charset. */
			/* NOTE: it's always the last in a block (max 255
			 * CD-TEXT pack data). So find it first. */
			if (parrillada_medium_read_CD_TEXT_block_info (cd_text, i, num, buffer)) {
				ParrilladaScsiCDTextPackCharset *pack;

				pack = (ParrilladaScsiCDTextPackCharset *) buffer;
				charset = pack->charset;

				PARRILLADA_MEDIA_LOG ("Found language pack. Charset = %d. Start %d. End %d",
						  charset, pack->first_track, pack->last_track);
			}
		}

		track_num = cd_text->pack [i].track_num;
		is_double_byte = cd_text->pack [i].double_byte;

		for (j = 0; j < sizeof (cd_text->pack [i].text); j++) {
			if (!off
			&&   cd_text->pack [i].text [j] == '\t'
			&& (!is_double_byte 
			|| (j+1 < sizeof (cd_text->pack [i].text) && cd_text->pack [i].text [j + 1] == '\t'))) {
				/* Specs say that tab character means that's the
				 * same string as before. So if buffer is not
				 * empty send the same string. */
				if (buffer [0] != '\0')
					parrillada_medium_get_CD_TEXT (self,
								    cd_text->pack [i].type,
								    track_num,
								    charset,
								    cd_text->pack [i].double_byte,
								    buffer);
				track_num ++;
				continue;
			}

			buffer [off] = cd_text->pack [i].text [j];
			off++;

			if (cd_text->pack [i].text [j] == '\0'
			&& (!is_double_byte 
			|| (j+1 < sizeof (cd_text->pack [i].text) && cd_text->pack [i].text [j + 1] == '\0'))) {
				/* Make sure we actually wrote something to the
				 * buffer and that it's not empty. */
				if (buffer [0] != '\0')
					parrillada_medium_get_CD_TEXT (self,
								    cd_text->pack [i].type,
								    track_num,
								    charset,
								    cd_text->pack [i].double_byte,
								    buffer);

				/* End of encapsulated Text Pack. Skip to the next. */
				track_num ++;
				off = 0;
			}
		}
	}

	g_free (cd_text);
}

static void
parrillada_medium_init_real (ParrilladaMedium *object,
			  ParrilladaDeviceHandle *handle)
{
	guint i;
	gchar *name;
	gboolean result;
	ParrilladaMediumPrivate *priv;
	ParrilladaScsiErrCode code = 0;
	gchar buffer [256] = { 0, };

	priv = PARRILLADA_MEDIUM_PRIVATE (object);

	name = parrillada_drive_get_display_name (priv->drive);
	PARRILLADA_MEDIA_LOG ("Initializing information for medium in %s", name);
	g_free (name);

	if (priv->probe_cancelled)
		return;

	result = parrillada_medium_get_medium_type (object, handle, &code);
	if (result != TRUE)
		return;

	if (priv->probe_cancelled)
		return;

	result = parrillada_medium_get_speed (object, handle, &code);
	if (result != TRUE)
		return;

	if (priv->probe_cancelled)
		return;

	parrillada_medium_get_capacity_by_type (object, handle, &code);
	if (priv->probe_cancelled)
		return;

	parrillada_medium_init_caps (object, handle, &code);
	if (priv->probe_cancelled)
		return;

	if (!parrillada_medium_get_contents (object, handle, &code))
		return;

	if (priv->probe_cancelled)
		return;

	/* assume that css feature is only for DVD-ROM which might be wrong but
	 * some drives wrongly reports that css is enabled for blank DVD+R/W */
	if (PARRILLADA_MEDIUM_IS (priv->info, (PARRILLADA_MEDIUM_DVD|PARRILLADA_MEDIUM_ROM)))
		parrillada_medium_get_css_feature (object, handle, &code);

	if (priv->probe_cancelled)
		return;

	/* read CD-TEXT title */
	if (priv->info & PARRILLADA_MEDIUM_HAS_AUDIO)
		parrillada_medium_read_CD_TEXT (object, handle, &code);

	if (priv->probe_cancelled)
		return;

	parrillada_media_to_string (priv->info, buffer);
	PARRILLADA_MEDIA_LOG ("media is %s", buffer);

	if (!priv->wr_speeds)
		return;

	/* sort write speeds */
	for (i = 0; priv->wr_speeds [i] != 0; i ++) {
		guint j;

		for (j = 0; priv->wr_speeds [j] != 0; j ++) {
			if (priv->wr_speeds [i] > priv->wr_speeds [j]) {
				gint64 tmp;

				tmp = priv->wr_speeds [i];
				priv->wr_speeds [i] = priv->wr_speeds [j];
				priv->wr_speeds [j] = tmp;
			}
		}
	}
}

gboolean
parrillada_medium_probing (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->probe != NULL;
}

static gboolean
parrillada_medium_probed (gpointer data)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (data), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (data);

	priv->probe_id = 0;

	/* This signal must be emitted in the main thread */
	GDK_THREADS_ENTER ();
	g_signal_emit (data,
		       medium_signals [PROBED],
		       0);
	GDK_THREADS_LEAVE ();

	return FALSE;
}

static gpointer
parrillada_medium_probe_thread (gpointer self)
{
	gint counter = 0;
	GTimeVal wait_time;
	const gchar *device;
	ParrilladaScsiErrCode code;
	ParrilladaMediumPrivate *priv;
	ParrilladaDeviceHandle *handle;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	priv->info = PARRILLADA_MEDIUM_BUSY;

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	device = parrillada_drive_get_device (priv->drive);
	PARRILLADA_MEDIA_LOG ("Trying to open device %s", device);

	handle = parrillada_device_handle_open (device, FALSE, &code);
	while (!handle && counter <= PARRILLADA_MEDIUM_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->probe_cancelled)
			goto end;

		counter ++;
		handle = parrillada_device_handle_open (device, FALSE, &code);
	}

	if (!handle) {
		PARRILLADA_MEDIA_LOG ("Open () failed: medium busy");
		goto end;
	}

	if (priv->probe_cancelled) {
		parrillada_device_handle_close (handle);
		goto end;
	}

	PARRILLADA_MEDIA_LOG ("Open () succeeded");

	/* NOTE: if we wanted to know the status we'd need to read the 
	 * error code variable which is currently NULL */
	while (parrillada_spc1_test_unit_ready (handle, &code) != PARRILLADA_SCSI_OK) {
		if (code == PARRILLADA_SCSI_NO_MEDIUM) {
			PARRILLADA_MEDIA_LOG ("No medium inserted");
			priv->info = PARRILLADA_MEDIUM_NONE;

			parrillada_device_handle_close (handle);
			goto end;
		}
		else if (code != PARRILLADA_SCSI_NOT_READY) {
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

	PARRILLADA_MEDIA_LOG ("Device ready");

	parrillada_medium_init_real (PARRILLADA_MEDIUM (self), handle);
	parrillada_device_handle_close (handle);

end:

	g_mutex_lock (priv->mutex);

	priv->probe = NULL;
	if (!priv->probe_cancelled)
		priv->probe_id = g_idle_add (parrillada_medium_probed, self);

	g_cond_broadcast (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (0);

	return NULL;
}

static void
parrillada_medium_probe (ParrilladaMedium *self)
{
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	/* NOTE: why a thread? Because in case of a damaged medium, parrillada can
	 * block on some functions until timeout and if we do this in the main
	 * thread then our whole UI blocks. This medium won't be exported by the
	 * ParrilladaDrive that exported until it returns PROBED signal.
	 * One (good) side effect is that it also improves start time. */
	g_mutex_lock (priv->mutex);
	priv->probe = g_thread_create (parrillada_medium_probe_thread,
				       self,
				       FALSE,
				       NULL);
	g_mutex_unlock (priv->mutex);
}

static void
parrillada_medium_init_file (ParrilladaMedium *self)
{
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (self);

	priv->info = PARRILLADA_MEDIUM_FILE;
	priv->type = types [0];
}

static void
parrillada_medium_init (ParrilladaMedium *object)
{
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (object);
	priv->next_wr_add = -1;

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
	priv->cond_probe = g_cond_new ();

	/* we can't do anything here since properties haven't been set yet */
}

static void
parrillada_medium_finalize (GObject *object)
{
	ParrilladaMediumPrivate *priv;

	priv = PARRILLADA_MEDIUM_PRIVATE (object);

	PARRILLADA_MEDIA_LOG ("Finalizing Medium object");

	g_mutex_lock (priv->mutex);
	if (priv->probe) {
		/* This to signal that we are cancelling */
		priv->probe_cancelled = TRUE;

		/* This is to wake up the thread if it
		 * was asleep waiting to retry to get
		 * hold of a handle to probe the drive */
		g_cond_signal (priv->cond_probe);

		/* Wait for the end of the thread */
		g_cond_wait (priv->cond, priv->mutex);
	}
	g_mutex_unlock (priv->mutex);

	if (priv->probe_id) {
		g_source_remove (priv->probe_id);
		priv->probe_id = 0;
	}

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

	if (priv->id) {
		g_free (priv->id);
		priv->id = NULL;
	}

	if (priv->CD_TEXT_title) {
		g_free (priv->CD_TEXT_title);
		priv->CD_TEXT_title = NULL;
	}

	g_free (priv->rd_speeds);
	priv->rd_speeds = NULL;

	g_free (priv->wr_speeds);
	priv->wr_speeds = NULL;

	g_slist_foreach (priv->tracks, (GFunc) g_free, NULL);
	g_slist_free (priv->tracks);
	priv->tracks = NULL;

	priv->drive = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_medium_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ParrilladaMediumPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_MEDIUM (object));

	priv = PARRILLADA_MEDIUM_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		/* we don't ref the drive here as it would create a circular
		 * dependency where the drive would hold a reference on the 
		 * medium and the medium on the drive */
		priv->drive = g_value_get_object (value);

		if (parrillada_drive_is_fake (priv->drive)) {
			parrillada_medium_init_file (PARRILLADA_MEDIUM (object));
			break;
		}

		parrillada_medium_probe (PARRILLADA_MEDIUM (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_medium_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ParrilladaMediumPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_MEDIUM (object));

	priv = PARRILLADA_MEDIUM_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		g_value_set_object (value, priv->drive);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_medium_class_init (ParrilladaMediumClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (ParrilladaMediumPrivate));

	object_class->finalize = parrillada_medium_finalize;
	object_class->set_property = parrillada_medium_set_property;
	object_class->get_property = parrillada_medium_get_property;

	/**
 	* ParrilladaMedium::probed:
 	* @medium: the object which received the signal
	*
 	* This signal gets emitted when the medium inside the drive has been
	* fully probed. This is mostly for internal use.
 	*
 	*/
	medium_signals[PROBED] =
		g_signal_new ("probed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);

	g_object_class_install_property (object_class,
	                                 PROP_DRIVE,
	                                 g_param_spec_object ("drive",
	                                                      "Drive",
	                                                      "Drive in which medium is inserted",
	                                                      PARRILLADA_TYPE_DRIVE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * parrillada_medium_can_be_written:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium can be written. It also checks that the medium can
 * write the medium.
 *
 * Return value: a #gboolean. TRUE if the medium can be rewritten, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_be_written (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	return parrillada_drive_can_write_media (priv->drive, priv->info);
}

/**
 * parrillada_medium_can_be_rewritten:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium can be rewritten. Note: it also checks that the drive
 * can rewrite the medium type.
 *
 * Return value: a #gboolean. TRUE if the medium can be rewritten, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_be_rewritten (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);

	if (!(priv->info & PARRILLADA_MEDIUM_REWRITABLE)
	||   (priv->info & PARRILLADA_MEDIUM_FILE))
		return FALSE;

	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_CDRW)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW))
		return priv->blank_command != 0;

	if (PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_RESTRICTED)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_PLUS)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVDRW_PLUS_DL)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_DVD_RAM)
	||  PARRILLADA_MEDIUM_IS (priv->info, PARRILLADA_MEDIUM_BDRE))
		return TRUE;

	return FALSE;
}
/**
 * parrillada_medium_can_use_sao:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium supports SAO.
 *
 * Since 2.29
 *
 * Return value: a #gboolean. TRUE if the medium can use SAO write mode , FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_use_sao (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->sao;
}

/**
 * parrillada_medium_can_use_tao:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium supports TAO.
 *
 * Since 2.29
 *
 * Return value: a #gboolean. TRUE if the medium can use TAO write mode, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_use_tao (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->tao;
}

/**
 * parrillada_medium_can_use_dummy_for_sao:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium supports doing a test write with SAO on.
 *
 * Return value: a #gboolean. TRUE if the medium can use SAO write mode during a test write, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_use_dummy_for_sao (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->dummy_sao;
}

/**
 * parrillada_medium_can_use_dummy_for_tao:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium supports doing a test write with TAO on.
 *
 * Return value: a #gboolean. TRUE if the medium can use TAO write mode during a test write, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_use_dummy_for_tao (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->dummy_tao;
}

/**
 * parrillada_medium_can_use_burnfree:
 * @medium: #ParrilladaMedium
 *
 * Gets whether the medium supports any burnfree technology.
 *
 * Return value: a #gboolean. TRUE if the medium can use any burnfree technology, FALSE otherwise.
 *
 **/
gboolean
parrillada_medium_can_use_burnfree (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), FALSE);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->burnfree;
}

/**
 * parrillada_medium_get_drive:
 * @medium: #ParrilladaMedium
 *
 * Gets the #ParrilladaDrive in which the medium is inserted.
 *
 * Return value: (transfer none): a #ParrilladaDrive. No need to unref after use.
 *
 **/
ParrilladaDrive *
parrillada_medium_get_drive (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	if (!medium)
		return NULL;

	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), NULL);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->drive;
}

/**
 * parrillada_medium_get_CD_TEXT_title:
 * @medium: #ParrilladaMedium
 *
 * Gets the CD-TEXT title for @Medium.
 *
 * Return value: a #gchar *.
 *
 **/
const gchar *
parrillada_medium_get_CD_TEXT_title (ParrilladaMedium *medium)
{
	ParrilladaMediumPrivate *priv;

	g_return_val_if_fail (medium != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_MEDIUM (medium), NULL);

	priv = PARRILLADA_MEDIUM_PRIVATE (medium);
	return priv->CD_TEXT_title;

}

GType
parrillada_medium_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (ParrilladaMediumClass), /* class_size */
			(GBaseInitFunc) NULL, /* base_init */
			(GBaseFinalizeFunc) NULL, /* base_finalize */
			(GClassInitFunc) parrillada_medium_class_init, /* class_init */
			(GClassFinalizeFunc) NULL, /* class_finalize */
			NULL /* class_data */,
			sizeof (ParrilladaMedium), /* instance_size */
			0, /* n_preallocs */
			(GInstanceInitFunc) parrillada_medium_init, /* instance_init */
			NULL /* value_table */
		};

		our_type = g_type_register_static (G_TYPE_OBJECT, "ParrilladaMedium",
		                                   &our_info, 0);
	}

	return our_type;
}

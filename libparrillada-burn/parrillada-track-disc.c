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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "parrillada-track-disc.h"

typedef struct _ParrilladaTrackDiscPrivate ParrilladaTrackDiscPrivate;
struct _ParrilladaTrackDiscPrivate
{
	ParrilladaDrive *drive;

	guint track_num;

	glong src_removed_sig;
	glong src_added_sig;
};

#define PARRILLADA_TRACK_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_DISC, ParrilladaTrackDiscPrivate))

G_DEFINE_TYPE (ParrilladaTrackDisc, parrillada_track_disc, PARRILLADA_TYPE_TRACK);

/**
 * parrillada_track_disc_set_track_num:
 * @track: a #ParrilladaTrackDisc
 * @num: a #guint
 *
 * Sets a track number which can be used
 * to copy only one specific session on a multisession disc
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_disc_set_track_num (ParrilladaTrackDisc *track,
				  guint num)
{
	ParrilladaTrackDiscPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DISC (track), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);
	priv->track_num = num;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_disc_get_track_num:
 * @track: a #ParrilladaTrackDisc
 *
 * Gets the track number which will be used
 * to copy only one specific session on a multisession disc
 *
 * Return value: a #guint. 0 if none is set, any other number otherwise.
 **/

guint
parrillada_track_disc_get_track_num (ParrilladaTrackDisc *track)
{
	ParrilladaTrackDiscPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DISC (track), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);
	return priv->track_num;
}

static void
parrillada_track_disc_remove_drive (ParrilladaTrackDisc *track)
{
	ParrilladaTrackDiscPrivate *priv;

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);

	if (priv->src_added_sig) {
		g_signal_handler_disconnect (priv->drive, priv->src_added_sig);
		priv->src_added_sig = 0;
	}

	if (priv->src_removed_sig) {
		g_signal_handler_disconnect (priv->drive, priv->src_removed_sig);
		priv->src_removed_sig = 0;
	}

	if (priv->drive) {
		g_object_unref (priv->drive);
		priv->drive = NULL;
	}
}

static void
parrillada_track_disc_medium_changed (ParrilladaDrive *drive,
				   ParrilladaMedium *medium,
				   ParrilladaTrack *track)
{
	parrillada_track_changed (track);
}

/**
 * parrillada_track_disc_set_drive:
 * @track: a #ParrilladaTrackDisc
 * @drive: a #ParrilladaDrive
 *
 * Sets @drive to be the #ParrilladaDrive that will be used
 * as the source when copying
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_disc_set_drive (ParrilladaTrackDisc *track,
			      ParrilladaDrive *drive)
{
	ParrilladaTrackDiscPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DISC (track), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);

	parrillada_track_disc_remove_drive (track);
	if (!drive) {
		parrillada_track_changed (PARRILLADA_TRACK (track));
		return PARRILLADA_BURN_OK;
	}

	priv->drive = drive;
	g_object_ref (drive);

	priv->src_added_sig = g_signal_connect (drive,
						"medium-added",
						G_CALLBACK (parrillada_track_disc_medium_changed),
						track);
	priv->src_removed_sig = g_signal_connect (drive,
						  "medium-removed",
						  G_CALLBACK (parrillada_track_disc_medium_changed),
						  track);

	parrillada_track_changed (PARRILLADA_TRACK (track));

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_disc_get_drive:
 * @track: a #ParrilladaTrackDisc
 *
 * Gets the #ParrilladaDrive object that will be used as
 * the source when copying.
 *
 * Return value: a #ParrilladaDrive or NULL. Don't unref or free it.
 **/

ParrilladaDrive *
parrillada_track_disc_get_drive (ParrilladaTrackDisc *track)
{
	ParrilladaTrackDiscPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DISC (track), NULL);

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);
	return priv->drive;
}

/**
 * parrillada_track_disc_get_medium_type:
 * @track: a #ParrilladaTrackDisc
 *
 * Gets the #ParrilladaMedia for the medium that is
 * currently inserted into the drive assigned for @track
 * with parrillada_track_disc_set_drive ().
 *
 * Return value: a #ParrilladaMedia.
 **/

ParrilladaMedia
parrillada_track_disc_get_medium_type (ParrilladaTrackDisc *track)
{
	ParrilladaTrackDiscPrivate *priv;
	ParrilladaMedium *medium;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DISC (track), PARRILLADA_MEDIUM_NONE);

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);
	medium = parrillada_drive_get_medium (priv->drive);
	if (!medium)
		return PARRILLADA_MEDIUM_NONE;

	return parrillada_medium_get_status (medium);
}

static ParrilladaBurnResult
parrillada_track_disc_get_size (ParrilladaTrack *track,
			     goffset *blocks,
			     goffset *block_size)
{
	ParrilladaMedium *medium;
	goffset medium_size = 0;
	goffset medium_blocks = 0;
	ParrilladaTrackDiscPrivate *priv;

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);
	medium = parrillada_drive_get_medium (priv->drive);
	if (!medium)
		return PARRILLADA_BURN_NOT_READY;

	parrillada_medium_get_data_size (medium, &medium_size, &medium_blocks);

	if (blocks)
		*blocks = medium_blocks;

	if (block_size)
		*block_size = medium_blocks? (medium_size / medium_blocks):0;

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_disc_get_track_type (ParrilladaTrack *track,
				   ParrilladaTrackType *type)
{
	ParrilladaTrackDiscPrivate *priv;
	ParrilladaMedium *medium;

	priv = PARRILLADA_TRACK_DISC_PRIVATE (track);

	medium = parrillada_drive_get_medium (priv->drive);

	parrillada_track_type_set_has_medium (type);
	parrillada_track_type_set_medium_type (type, parrillada_medium_get_status (medium));

	return PARRILLADA_BURN_OK;
}

static void
parrillada_track_disc_init (ParrilladaTrackDisc *object)
{ }

static void
parrillada_track_disc_finalize (GObject *object)
{
	parrillada_track_disc_remove_drive (PARRILLADA_TRACK_DISC (object));

	G_OBJECT_CLASS (parrillada_track_disc_parent_class)->finalize (object);
}

static void
parrillada_track_disc_class_init (ParrilladaTrackDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass* track_class = PARRILLADA_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackDiscPrivate));

	object_class->finalize = parrillada_track_disc_finalize;

	track_class->get_size = parrillada_track_disc_get_size;
	track_class->get_type = parrillada_track_disc_get_track_type;
}

/**
 * parrillada_track_disc_new:
 *
 * Creates a new #ParrilladaTrackDisc object.
 *
 * This type of tracks is used to copy media either
 * to a disc image file or to another medium.
 *
 * Return value: a #ParrilladaTrackDisc.
 **/

ParrilladaTrackDisc *
parrillada_track_disc_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_DISC, NULL);
}

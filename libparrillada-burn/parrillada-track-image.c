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

#include <string.h>

#include "parrillada-track-image.h"
#include "parrillada-enums.h"
#include "parrillada-track.h"

#include "burn-debug.h"
#include "burn-image-format.h"

typedef struct _ParrilladaTrackImagePrivate ParrilladaTrackImagePrivate;
struct _ParrilladaTrackImagePrivate
{
	gchar *image;
	gchar *toc;

	guint64 blocks;

	ParrilladaImageFormat format;
};

#define PARRILLADA_TRACK_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_IMAGE, ParrilladaTrackImagePrivate))


G_DEFINE_TYPE (ParrilladaTrackImage, parrillada_track_image, PARRILLADA_TYPE_TRACK);

static ParrilladaBurnResult
parrillada_track_image_set_source_real (ParrilladaTrackImage *track,
				     const gchar *image,
				     const gchar *toc,
				     ParrilladaImageFormat format)
{
	ParrilladaTrackImagePrivate *priv;

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	priv->format = format;

	if (priv->image)
		g_free (priv->image);

	if (priv->toc)
		g_free (priv->toc);

	priv->image = g_strdup (image);
	priv->toc = g_strdup (toc);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_image_set_source:
 * @track: a #ParrilladaTrackImage
 * @image: a #gchar or NULL
 * @toc: a #gchar or NULL
 * @format: a #ParrilladaImageFormat
 *
 * Sets the image source path (and its toc if need be)
 * as well as its format.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it is successful.
 **/

ParrilladaBurnResult
parrillada_track_image_set_source (ParrilladaTrackImage *track,
				const gchar *image,
				const gchar *toc,
				ParrilladaImageFormat format)
{
	ParrilladaTrackImagePrivate *priv;
	ParrilladaTrackImageClass *klass;
	ParrilladaBurnResult res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), PARRILLADA_BURN_ERR);

	/* See if it has changed */
	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	klass = PARRILLADA_TRACK_IMAGE_GET_CLASS (track);
	if (!klass->set_source)
		return PARRILLADA_BURN_ERR;

	res = klass->set_source (track, image, toc, format);
	if (res != PARRILLADA_BURN_OK)
		return res;

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_image_set_block_num_real (ParrilladaTrackImage *track,
					goffset blocks)
{
	ParrilladaTrackImagePrivate *priv;

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);
	priv->blocks = blocks;
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_image_set_block_num:
 * @track: a #ParrilladaTrackImage
 * @blocks: a #goffset
 *
 * Sets the image size (in sectors).
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it is successful.
 **/

ParrilladaBurnResult
parrillada_track_image_set_block_num (ParrilladaTrackImage *track,
				   goffset blocks)
{
	ParrilladaTrackImagePrivate *priv;
	ParrilladaTrackImageClass *klass;
	ParrilladaBurnResult res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);
	if (priv->blocks == blocks)
		return PARRILLADA_BURN_OK;

	klass = PARRILLADA_TRACK_IMAGE_GET_CLASS (track);
	if (!klass->set_block_num)
		return PARRILLADA_BURN_ERR;

	res = klass->set_block_num (track, blocks);
	if (res != PARRILLADA_BURN_OK)
		return res;

	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_image_get_source:
 * @track: a #ParrilladaTrackImage
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE) of the
 * source image file.
 *
 * Return value: a #gchar
 **/

gchar *
parrillada_track_image_get_source (ParrilladaTrackImage *track,
				gboolean uri)
{
	ParrilladaTrackImagePrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), NULL);

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	if (!priv->image) {
		gchar *complement;
		gchar *retval;
		gchar *toc;

		if (!priv->toc) {
			PARRILLADA_BURN_LOG ("Image nor toc were set");
			return NULL;
		}

		toc = parrillada_string_get_localpath (priv->toc);
		complement = parrillada_image_format_get_complement (priv->format, toc);
		g_free (toc);

		if (!complement) {
			PARRILLADA_BURN_LOG ("No complement could be retrieved");
			return NULL;
		}

		PARRILLADA_BURN_LOG ("Complement file retrieved %s", complement);
		if (uri)
			retval = parrillada_string_get_uri (complement);
		else
			retval = parrillada_string_get_localpath (complement);

		g_free (complement);
		return retval;
	}

	if (uri)
		return parrillada_string_get_uri (priv->image);
	else
		return parrillada_string_get_localpath (priv->image);
}

/**
 * parrillada_track_image_get_toc_source:
 * @track: a #ParrilladaTrackImage
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE) of the
 * source toc file.
 *
 * Return value: a #gchar
 **/

gchar *
parrillada_track_image_get_toc_source (ParrilladaTrackImage *track,
				    gboolean uri)
{
	ParrilladaTrackImagePrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), NULL);

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	/* Don't use file complement retrieval here as it's not possible */
	if (uri)
		return parrillada_string_get_uri (priv->toc);
	else
		return parrillada_string_get_localpath (priv->toc);
}

/**
 * parrillada_track_image_get_format:
 * @track: a #ParrilladaTrackImage
 *
 * This function returns the format of the
 * source image.
 *
 * Return value: a #ParrilladaImageFormat
 **/

ParrilladaImageFormat
parrillada_track_image_get_format (ParrilladaTrackImage *track)
{
	ParrilladaTrackImagePrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), PARRILLADA_IMAGE_FORMAT_NONE);

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);
	return priv->format;
}

/**
 * parrillada_track_image_need_byte_swap:
 * @track: a #ParrilladaTrackImage
 *
 * This function returns whether the data bytes need swapping. Some .bin files
 * associated with .cue files are little endian for audio whereas they should
 * be big endian.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_image_need_byte_swap (ParrilladaTrackImage *track)
{
	ParrilladaTrackImagePrivate *priv;
	gchar *cueuri;
	gboolean res;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_IMAGE (track), PARRILLADA_IMAGE_FORMAT_NONE);

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);
	if (priv->format != PARRILLADA_IMAGE_FORMAT_CUE)
		return FALSE;

	cueuri = parrillada_string_get_uri (priv->toc);
	res = parrillada_image_format_cue_bin_byte_swap (cueuri, NULL, NULL);
	g_free (cueuri);

	return res;
}

static ParrilladaBurnResult
parrillada_track_image_get_track_type (ParrilladaTrack *track,
				    ParrilladaTrackType *type)
{
	ParrilladaTrackImagePrivate *priv;

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	parrillada_track_type_set_has_image (type);
	parrillada_track_type_set_image_format (type, priv->format);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_image_get_size (ParrilladaTrack *track,
			      goffset *blocks,
			      goffset *block_size)
{
	ParrilladaTrackImagePrivate *priv;

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (track);

	if (priv->format == PARRILLADA_IMAGE_FORMAT_BIN) {
		if (block_size)
			*block_size = 2048;
	}
	else if (priv->format == PARRILLADA_IMAGE_FORMAT_CLONE) {
		if (block_size)
			*block_size = 2448;
	}
	else if (priv->format == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
		if (block_size)
			*block_size = 2352;
	}
	else if (priv->format == PARRILLADA_IMAGE_FORMAT_CUE) {
		if (block_size)
			*block_size = 2352;
	}
	else if (block_size)
		*block_size = 0;

	if (blocks)
		*blocks = priv->blocks;

	return PARRILLADA_BURN_OK;
}

static void
parrillada_track_image_init (ParrilladaTrackImage *object)
{ }

static void
parrillada_track_image_finalize (GObject *object)
{
	ParrilladaTrackImagePrivate *priv;

	priv = PARRILLADA_TRACK_IMAGE_PRIVATE (object);
	if (priv->image) {
		g_free (priv->image);
		priv->image = NULL;
	}

	if (priv->toc) {
		g_free (priv->toc);
		priv->toc = NULL;
	}

	G_OBJECT_CLASS (parrillada_track_image_parent_class)->finalize (object);
}

static void
parrillada_track_image_class_init (ParrilladaTrackImageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass *track_class = PARRILLADA_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackImagePrivate));

	object_class->finalize = parrillada_track_image_finalize;

	track_class->get_size = parrillada_track_image_get_size;
	track_class->get_type = parrillada_track_image_get_track_type;

	klass->set_source = parrillada_track_image_set_source_real;
	klass->set_block_num = parrillada_track_image_set_block_num_real;
}

/**
 * parrillada_track_image_new:
 *
 * Creates a new #ParrilladaTrackImage object.
 *
 * This type of tracks is used to burn disc images.
 *
 * Return value: a #ParrilladaTrackImage object.
 **/

ParrilladaTrackImage *
parrillada_track_image_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_IMAGE, NULL);
}

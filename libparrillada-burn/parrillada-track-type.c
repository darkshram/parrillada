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

#include "parrillada-medium.h"
#include "parrillada-drive.h"
#include "parrillada-track-type.h"
#include "parrillada-track-type-private.h"

/**
 * parrillada_track_type_new:
 *
 * Creates a new #ParrilladaTrackType structure.
 * Free it with parrillada_track_type_free ().
 *
 * Return value: a #ParrilladaTrackType pointer.
 **/

ParrilladaTrackType *
parrillada_track_type_new (void)
{
	return g_new0 (ParrilladaTrackType, 1);
}

/**
 * parrillada_track_type_free:
 * @type: a #ParrilladaTrackType.
 *
 * Frees #ParrilladaTrackType structure.
 *
 **/

void
parrillada_track_type_free (ParrilladaTrackType *type)
{
	if (!type)
		return;

	g_free (type);
}

/**
 * parrillada_track_type_get_image_format:
 * @type: a #ParrilladaTrackType.
 *
 * Returns the format of an image when
 * parrillada_track_type_get_has_image () returned
 * TRUE.
 *
 * Return value: a #ParrilladaImageFormat
 **/

ParrilladaImageFormat
parrillada_track_type_get_image_format (const ParrilladaTrackType *type) 
{
	g_return_val_if_fail (type != NULL, PARRILLADA_IMAGE_FORMAT_NONE);

	if (type->type != PARRILLADA_TRACK_TYPE_IMAGE)
		return PARRILLADA_IMAGE_FORMAT_NONE;

	return type->subtype.img_format;
}

/**
 * parrillada_track_type_get_data_fs:
 * @type: a #ParrilladaTrackType.
 *
 * Returns the parameters for the image generation
 * when parrillada_track_type_get_has_data () returned
 * TRUE.
 *
 * Return value: a #ParrilladaImageFS
 **/

ParrilladaImageFS
parrillada_track_type_get_data_fs (const ParrilladaTrackType *type) 
{
	g_return_val_if_fail (type != NULL, PARRILLADA_IMAGE_FS_NONE);

	if (type->type != PARRILLADA_TRACK_TYPE_DATA)
		return PARRILLADA_IMAGE_FS_NONE;

	return type->subtype.fs_type;
}

/**
 * parrillada_track_type_get_stream_format:
 * @type: a #ParrilladaTrackType.
 *
 * Returns the format for a stream (song or video)
 * when parrillada_track_type_get_has_stream () returned
 * TRUE.
 *
 * Return value: a #ParrilladaStreamFormat
 **/

ParrilladaStreamFormat
parrillada_track_type_get_stream_format (const ParrilladaTrackType *type) 
{
	g_return_val_if_fail (type != NULL, PARRILLADA_AUDIO_FORMAT_NONE);

	if (type->type != PARRILLADA_TRACK_TYPE_STREAM)
		return PARRILLADA_AUDIO_FORMAT_NONE;

	return type->subtype.stream_format;
}

/**
 * parrillada_track_type_get_medium_type:
 * @type: a #ParrilladaTrackType.
 *
 * Returns the medium type
 * when parrillada_track_type_get_has_medium () returned
 * TRUE.
 *
 * Return value: a #ParrilladaMedia
 **/

ParrilladaMedia
parrillada_track_type_get_medium_type (const ParrilladaTrackType *type) 
{
	g_return_val_if_fail (type != NULL, PARRILLADA_MEDIUM_NONE);

	if (type->type != PARRILLADA_TRACK_TYPE_DISC)
		return PARRILLADA_MEDIUM_NONE;

	return type->subtype.media;
}

/**
 * parrillada_track_type_set_image_format:
 * @type: a #ParrilladaTrackType.
 * @format: a #ParrilladaImageFormat
 *
 * Sets the #ParrilladaImageFormat. Must be called
 * after parrillada_track_type_set_has_image ().
 *
 **/

void
parrillada_track_type_set_image_format (ParrilladaTrackType *type,
				     ParrilladaImageFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != PARRILLADA_TRACK_TYPE_IMAGE)
		return;

	type->subtype.img_format = format;
}

/**
 * parrillada_track_type_set_data_fs:
 * @type: a #ParrilladaTrackType.
 * @fs_type: a #ParrilladaImageFS
 *
 * Sets the #ParrilladaImageFS. Must be called
 * after parrillada_track_type_set_has_data ().
 *
 **/

void
parrillada_track_type_set_data_fs (ParrilladaTrackType *type,
				ParrilladaImageFS fs_type) 
{
	g_return_if_fail (type != NULL);

	if (type->type != PARRILLADA_TRACK_TYPE_DATA)
		return;

	type->subtype.fs_type = fs_type;
}

/**
 * parrillada_track_type_set_stream_format:
 * @type: a #ParrilladaTrackType.
 * @format: a #ParrilladaImageFormat
 *
 * Sets the #ParrilladaStreamFormat. Must be called
 * after parrillada_track_type_set_has_stream ().
 *
 **/

void
parrillada_track_type_set_stream_format (ParrilladaTrackType *type,
				      ParrilladaStreamFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != PARRILLADA_TRACK_TYPE_STREAM)
		return;

	type->subtype.stream_format = format;
}

/**
 * parrillada_track_type_set_medium_type:
 * @type: a #ParrilladaTrackType.
 * @media: a #ParrilladaMedia
 *
 * Sets the #ParrilladaMedia. Must be called
 * after parrillada_track_type_set_has_medium ().
 *
 **/

void
parrillada_track_type_set_medium_type (ParrilladaTrackType *type,
				    ParrilladaMedia media) 
{
	g_return_if_fail (type != NULL);

	if (type->type != PARRILLADA_TRACK_TYPE_DISC)
		return;

	type->subtype.media = media;
}

/**
 * parrillada_track_type_is_empty:
 * @type: a #ParrilladaTrackType.
 *
 * Returns TRUE if no type was set.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_is_empty (const ParrilladaTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return (type->type == PARRILLADA_TRACK_TYPE_NONE);
}

/**
 * parrillada_track_type_get_has_data:
 * @type: a #ParrilladaTrackType.
 *
 * Returns TRUE if DATA type (see parrillada_track_data_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_get_has_data (const ParrilladaTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == PARRILLADA_TRACK_TYPE_DATA;
}

/**
 * parrillada_track_type_get_has_image:
 * @type: a #ParrilladaTrackType.
 *
 * Returns TRUE if IMAGE type (see parrillada_track_image_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_get_has_image (const ParrilladaTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == PARRILLADA_TRACK_TYPE_IMAGE;
}

/**
 * parrillada_track_type_get_has_stream:
 * @type: a #ParrilladaTrackType.
 *
 * This function returns %TRUE if IMAGE type (see parrillada_track_stream_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_get_has_stream (const ParrilladaTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == PARRILLADA_TRACK_TYPE_STREAM;
}

/**
 * parrillada_track_type_get_has_medium:
 * @type: a #ParrilladaTrackType.
 *
 * Returns TRUE if MEDIUM type (see parrillada_track_disc_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_get_has_medium (const ParrilladaTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == PARRILLADA_TRACK_TYPE_DISC;
}

/**
 * parrillada_track_type_set_has_data:
 * @type: a #ParrilladaTrackType.
 *
 * Set DATA type for @type.
 *
 **/

void
parrillada_track_type_set_has_data (ParrilladaTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = PARRILLADA_TRACK_TYPE_DATA;
}

/**
 * parrillada_track_type_set_has_image:
 * @type: a #ParrilladaTrackType.
 *
 * Set IMAGE type for @type.
 *
 **/

void
parrillada_track_type_set_has_image (ParrilladaTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = PARRILLADA_TRACK_TYPE_IMAGE;
}

/**
 * parrillada_track_type_set_has_stream:
 * @type: a #ParrilladaTrackType.
 *
 * Set STREAM type for @type
 *
 **/

void
parrillada_track_type_set_has_stream (ParrilladaTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = PARRILLADA_TRACK_TYPE_STREAM;
}

/**
 * parrillada_track_type_set_has_medium:
 * @type: a #ParrilladaTrackType.
 *
 * Set MEDIUM type for @type.
 *
 **/

void
parrillada_track_type_set_has_medium (ParrilladaTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = PARRILLADA_TRACK_TYPE_DISC;
}

/**
 * parrillada_track_type_equal:
 * @type_A: a #ParrilladaTrackType.
 * @type_B: a #ParrilladaTrackType.
 *
 * Returns TRUE if @type_A and @type_B represents
 * the same type and subtype.
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_equal (const ParrilladaTrackType *type_A,
			  const ParrilladaTrackType *type_B)
{
	g_return_val_if_fail (type_A != NULL, FALSE);
	g_return_val_if_fail (type_B != NULL, FALSE);

	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case PARRILLADA_TRACK_TYPE_DATA:
		if (type_A->subtype.fs_type != type_B->subtype.fs_type)
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_DISC:
		if (type_B->subtype.media != type_A->subtype.media)
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_IMAGE:
		if (type_A->subtype.img_format != type_B->subtype.img_format)
			return FALSE;
		break;

	case PARRILLADA_TRACK_TYPE_STREAM:
		if (type_A->subtype.stream_format != type_B->subtype.stream_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

#if 0
/**
 * parrillada_track_type_match:
 * @type_A: a #ParrilladaTrackType.
 * @type_B: a #ParrilladaTrackType.
 *
 * Returns TRUE if @type_A and @type_B match.
 *
 * (Used internally)
 *
 * Return value: a #gboolean
 **/

gboolean
parrillada_track_type_match (const ParrilladaTrackType *type_A,
			  const ParrilladaTrackType *type_B)
{
	g_return_val_if_fail (type_A != NULL, FALSE);
	g_return_val_if_fail (type_B != NULL, FALSE);

	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case PARRILLADA_TRACK_TYPE_DATA:
		if (!(type_A->subtype.fs_type & type_B->subtype.fs_type))
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_DISC:
		if (!(type_A->subtype.media & type_B->subtype.media))
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_IMAGE:
		if (!(type_A->subtype.img_format & type_B->subtype.img_format))
			return FALSE;
		break;

	case PARRILLADA_TRACK_TYPE_STREAM:
		if (!(type_A->subtype.stream_format & type_B->subtype.stream_format))
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

#endif

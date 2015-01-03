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

#ifndef _PARRILLADA_ENUM_H_
#define _PARRILLADA_ENUM_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_BURN_OK,
	PARRILLADA_BURN_ERR,
	PARRILLADA_BURN_RETRY,
	PARRILLADA_BURN_CANCEL,
	PARRILLADA_BURN_RUNNING,
	PARRILLADA_BURN_DANGEROUS,
	PARRILLADA_BURN_NOT_READY,
	PARRILLADA_BURN_NOT_RUNNING,
	PARRILLADA_BURN_NEED_RELOAD,
	PARRILLADA_BURN_NOT_SUPPORTED,
} ParrilladaBurnResult;

/* These flags are sorted by importance. That's done to solve the problem of
 * exclusive flags: that way MERGE will always win over any other flag if they
 * are exclusive. On the other hand DAO will always lose. */
typedef enum {
	PARRILLADA_BURN_FLAG_NONE			= 0,

	/* These flags should always be supported */
	PARRILLADA_BURN_FLAG_CHECK_SIZE		= 1,
	PARRILLADA_BURN_FLAG_NOGRACE		= 1 << 1,
	PARRILLADA_BURN_FLAG_EJECT			= 1 << 2,

	/* These are of great importance for the result */
	PARRILLADA_BURN_FLAG_MERGE			= 1 << 3,
	PARRILLADA_BURN_FLAG_MULTI			= 1 << 4,
	PARRILLADA_BURN_FLAG_APPEND		= 1 << 5,

	PARRILLADA_BURN_FLAG_BURNPROOF		= 1 << 6,
	PARRILLADA_BURN_FLAG_NO_TMP_FILES		= 1 << 7,
	PARRILLADA_BURN_FLAG_DUMMY			= 1 << 8,

	PARRILLADA_BURN_FLAG_OVERBURN		= 1 << 9,

	PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE	= 1 << 10,
	PARRILLADA_BURN_FLAG_FAST_BLANK		= 1 << 11,

	/* NOTE: these two are contradictory? */
	PARRILLADA_BURN_FLAG_DAO			= 1 << 13,
	PARRILLADA_BURN_FLAG_RAW			= 1 << 14,

	PARRILLADA_BURN_FLAG_LAST
} ParrilladaBurnFlag;

typedef enum {
	PARRILLADA_BURN_ACTION_NONE		= 0,
	PARRILLADA_BURN_ACTION_GETTING_SIZE,
	PARRILLADA_BURN_ACTION_CREATING_IMAGE,
	PARRILLADA_BURN_ACTION_RECORDING,
	PARRILLADA_BURN_ACTION_BLANKING,
	PARRILLADA_BURN_ACTION_CHECKSUM,
	PARRILLADA_BURN_ACTION_DRIVE_COPY,
	PARRILLADA_BURN_ACTION_FILE_COPY,
	PARRILLADA_BURN_ACTION_ANALYSING,
	PARRILLADA_BURN_ACTION_TRANSCODING,
	PARRILLADA_BURN_ACTION_PREPARING,
	PARRILLADA_BURN_ACTION_LEADIN,
	PARRILLADA_BURN_ACTION_RECORDING_CD_TEXT,
	PARRILLADA_BURN_ACTION_FIXATING,
	PARRILLADA_BURN_ACTION_LEADOUT,
	PARRILLADA_BURN_ACTION_START_RECORDING,
	PARRILLADA_BURN_ACTION_FINISHED,
	PARRILLADA_BURN_ACTION_EJECTING,
	PARRILLADA_BURN_ACTION_LAST
} ParrilladaBurnAction;

typedef enum {
	PARRILLADA_IMAGE_FS_NONE			= 0,
	PARRILLADA_IMAGE_FS_ISO			= 1,
	PARRILLADA_IMAGE_FS_UDF			= 1 << 1,
	PARRILLADA_IMAGE_FS_JOLIET			= 1 << 2,
	PARRILLADA_IMAGE_FS_VIDEO			= 1 << 3,

	/* The following one conflict with UDF and JOLIET */
	PARRILLADA_IMAGE_FS_SYMLINK		= 1 << 4,

	PARRILLADA_IMAGE_ISO_FS_LEVEL_3		= 1 << 5,
	PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY	= 1 << 6,
	PARRILLADA_IMAGE_FS_ANY			= PARRILLADA_IMAGE_FS_ISO|
						  PARRILLADA_IMAGE_FS_UDF|
						  PARRILLADA_IMAGE_FS_JOLIET|
						  PARRILLADA_IMAGE_FS_SYMLINK|
						  PARRILLADA_IMAGE_ISO_FS_LEVEL_3|
						  PARRILLADA_IMAGE_FS_VIDEO|
						  PARRILLADA_IMAGE_ISO_FS_DEEP_DIRECTORY
} ParrilladaImageFS;

typedef enum {
	PARRILLADA_AUDIO_FORMAT_NONE		= 0,
	PARRILLADA_AUDIO_FORMAT_UNDEFINED		= 1,
	PARRILLADA_AUDIO_FORMAT_DTS		= 1 << 1,
	PARRILLADA_AUDIO_FORMAT_RAW		= 1 << 2,
	PARRILLADA_AUDIO_FORMAT_AC3		= 1 << 3,
	PARRILLADA_AUDIO_FORMAT_MP2		= 1 << 4,

	PARRILLADA_AUDIO_FORMAT_44100		= 1 << 5,
	PARRILLADA_AUDIO_FORMAT_48000		= 1 << 6,


	PARRILLADA_VIDEO_FORMAT_UNDEFINED		= 1 << 7,
	PARRILLADA_VIDEO_FORMAT_VCD		= 1 << 8,
	PARRILLADA_VIDEO_FORMAT_VIDEO_DVD		= 1 << 9,

	PARRILLADA_METADATA_INFO			= 1 << 10,

	PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN  = 1 << 11,

	/* Deprecated */
	PARRILLADA_AUDIO_FORMAT_4_CHANNEL		= 0
} ParrilladaStreamFormat;

#define PARRILLADA_STREAM_FORMAT_AUDIO(stream_FORMAT)	((stream_FORMAT) & 0x087F)
#define PARRILLADA_STREAM_FORMAT_VIDEO(stream_FORMAT)	((stream_FORMAT) & 0x0380)

#define	PARRILLADA_MIN_STREAM_LENGTH			((gint64) 6 * 1000000000LL)
#define PARRILLADA_STREAM_LENGTH(start_MACRO, end_MACRO)					\
	((end_MACRO) - (start_MACRO) > PARRILLADA_MIN_STREAM_LENGTH) ?			\
	((end_MACRO) - (start_MACRO)) : PARRILLADA_MIN_STREAM_LENGTH

#define PARRILLADA_STREAM_FORMAT_HAS_VIDEO(format_MACRO)				\
	 ((format_MACRO) & (PARRILLADA_VIDEO_FORMAT_UNDEFINED|			\
	 		    PARRILLADA_VIDEO_FORMAT_VCD|				\
	 		    PARRILLADA_VIDEO_FORMAT_VIDEO_DVD))

typedef enum {
	PARRILLADA_IMAGE_FORMAT_NONE = 0,
	PARRILLADA_IMAGE_FORMAT_BIN = 1,
	PARRILLADA_IMAGE_FORMAT_CUE = 1 << 1,
	PARRILLADA_IMAGE_FORMAT_CLONE = 1 << 2,
	PARRILLADA_IMAGE_FORMAT_CDRDAO = 1 << 3,
	PARRILLADA_IMAGE_FORMAT_ANY = PARRILLADA_IMAGE_FORMAT_BIN|
	PARRILLADA_IMAGE_FORMAT_CUE|
	PARRILLADA_IMAGE_FORMAT_CDRDAO|
	PARRILLADA_IMAGE_FORMAT_CLONE,
} ParrilladaImageFormat; 

typedef enum {
	PARRILLADA_PLUGIN_ERROR_NONE					= 0,
	PARRILLADA_PLUGIN_ERROR_MODULE,
	PARRILLADA_PLUGIN_ERROR_MISSING_APP,
	PARRILLADA_PLUGIN_ERROR_WRONG_APP_VERSION,
	PARRILLADA_PLUGIN_ERROR_SYMBOLIC_LINK_APP,
	PARRILLADA_PLUGIN_ERROR_MISSING_LIBRARY,
	PARRILLADA_PLUGIN_ERROR_LIBRARY_VERSION,
	PARRILLADA_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN,
} ParrilladaPluginErrorType;

G_END_DECLS

#endif


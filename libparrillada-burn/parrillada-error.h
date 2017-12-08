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

#ifndef _PARRILLADA_ERROR_H_
#define _PARRILLADA_ERROR_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_BURN_ERROR_NONE,
	PARRILLADA_BURN_ERROR_GENERAL,

	PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,

	PARRILLADA_BURN_ERROR_SLOW_DMA,
	PARRILLADA_BURN_ERROR_PERMISSION,
	PARRILLADA_BURN_ERROR_DRIVE_BUSY,
	PARRILLADA_BURN_ERROR_DISK_SPACE,

	PARRILLADA_BURN_ERROR_EMPTY,
	PARRILLADA_BURN_ERROR_INPUT_INVALID,

	PARRILLADA_BURN_ERROR_OUTPUT_NONE,

	PARRILLADA_BURN_ERROR_FILE_INVALID,
	PARRILLADA_BURN_ERROR_FILE_FOLDER,
	PARRILLADA_BURN_ERROR_FILE_PLAYLIST,
	PARRILLADA_BURN_ERROR_FILE_NOT_FOUND,
	PARRILLADA_BURN_ERROR_FILE_NOT_LOCAL,

	PARRILLADA_BURN_ERROR_WRITE_MEDIUM,
	PARRILLADA_BURN_ERROR_WRITE_IMAGE,

	PARRILLADA_BURN_ERROR_IMAGE_INVALID,
	PARRILLADA_BURN_ERROR_IMAGE_JOLIET,
	PARRILLADA_BURN_ERROR_IMAGE_LAST_SESSION,

	PARRILLADA_BURN_ERROR_MEDIUM_NONE,
	PARRILLADA_BURN_ERROR_MEDIUM_INVALID,
	PARRILLADA_BURN_ERROR_MEDIUM_SPACE,
	PARRILLADA_BURN_ERROR_MEDIUM_NO_DATA,
	PARRILLADA_BURN_ERROR_MEDIUM_NOT_WRITABLE,
	PARRILLADA_BURN_ERROR_MEDIUM_NOT_REWRITABLE,
	PARRILLADA_BURN_ERROR_MEDIUM_NEED_RELOADING,

	PARRILLADA_BURN_ERROR_BAD_CHECKSUM,

	PARRILLADA_BURN_ERROR_MISSING_APP_AND_PLUGIN,

	/* these are not necessarily error */
	PARRILLADA_BURN_WARNING_CHECKSUM,
	PARRILLADA_BURN_WARNING_INSERT_AFTER_COPY,

	PARRILLADA_BURN_ERROR_TMP_DIRECTORY,
	PARRILLADA_BURN_ERROR_ENCRYPTION_KEY
} ParrilladaBurnError;

/**
 * Error handling and defined return values
 */

GQuark parrillada_burn_quark (void);

#define PARRILLADA_BURN_ERROR				\
	parrillada_burn_quark()

G_END_DECLS

#endif /* _PARRILLADA_ERROR_H_ */


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

#include <stdio.h>

#include <errno.h>
#include <string.h>

#include <glib.h>

#ifndef _BURN_ERROR_H
#define _BURN_ERROR_H

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_SCSI_ERROR_NONE	   = 0,
	PARRILLADA_SCSI_ERR_UNKNOWN,
	PARRILLADA_SCSI_SIZE_MISMATCH,
	PARRILLADA_SCSI_TYPE_MISMATCH,
	PARRILLADA_SCSI_BAD_ARGUMENT,
	PARRILLADA_SCSI_NOT_READY,
	PARRILLADA_SCSI_OUTRANGE_ADDRESS,
	PARRILLADA_SCSI_INVALID_ADDRESS,
	PARRILLADA_SCSI_INVALID_COMMAND,
	PARRILLADA_SCSI_INVALID_PARAMETER,
	PARRILLADA_SCSI_INVALID_FIELD,
	PARRILLADA_SCSI_TIMEOUT,
	PARRILLADA_SCSI_KEY_NOT_ESTABLISHED,
	PARRILLADA_SCSI_INVALID_TRACK_MODE,
	PARRILLADA_SCSI_ERRNO,
	PARRILLADA_SCSI_NO_MEDIUM,
	PARRILLADA_SCSI_ERROR_LAST
} ParrilladaScsiErrCode;

typedef enum {
	PARRILLADA_SCSI_OK,
	PARRILLADA_SCSI_FAILURE,
	PARRILLADA_SCSI_RECOVERABLE,
} ParrilladaScsiResult;

const gchar *
parrillada_scsi_strerror (ParrilladaScsiErrCode code);

void
parrillada_scsi_set_error (GError **error, ParrilladaScsiErrCode code);

G_END_DECLS

#endif /* _BURN_ERROR_H */

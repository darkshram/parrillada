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

#include <glib.h>

#include "scsi-mmc2.h"

#include "parrillada-media-private.h"

#include "scsi-error.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-disc-structure.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaReadDiscStructureCDB {
	uchar opcode;

	uchar media_type		:4;
	uchar reserved0			:4;

	/* for formats 0x83 */
	uchar address			[4];
	uchar layer_num;

	uchar format;

	uchar alloc_len			[2];

	uchar reserved1			:6;

	/* for formats 0x02, 0x06, 0x07, 0x80, 0x82 */
	uchar agid			:2;

	uchar ctl;
};

#else

struct _ParrilladaReadDiscStructureCDB {
	uchar opcode;

	uchar reserved0			:4;
	uchar media_type		:4;

	uchar address			[4];

	uchar layer_num;
	uchar format;

	uchar alloc_len			[2];

	uchar agid			:2;
	uchar reserved1			:6;

	uchar ctl;
};

#endif

typedef struct _ParrilladaReadDiscStructureCDB ParrilladaReadDiscStructureCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaReadDiscStructureCDB,
			     READ_DISC_STRUCTURE,
			     PARRILLADA_SCSI_READ);

typedef enum {
PARRILLADA_MEDIA_DVD_HD_DVD			= 0x00,
PARRILLADA_MEDIA_BD				= 0x01
	/* reserved */
} ParrilladaScsiMediaType;

static ParrilladaScsiResult
parrillada_read_disc_structure (ParrilladaReadDiscStructureCDB *cdb,
			     ParrilladaScsiReadDiscStructureHdr **data,
			     int *size,
			     ParrilladaScsiErrCode *error)
{
	ParrilladaScsiReadDiscStructureHdr *buffer;
	ParrilladaScsiReadDiscStructureHdr hdr;
	ParrilladaScsiResult res;
	int request_size;

	if (!data || !size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (hdr));

	memset (&hdr, 0, sizeof (hdr));
	res = parrillada_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	request_size = PARRILLADA_GET_16 (hdr.len) + sizeof (hdr.len);
	buffer = (ParrilladaScsiReadDiscStructureHdr *) g_new0 (uchar, request_size);

	PARRILLADA_SET_16 (cdb->alloc_len, request_size);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return res;
	}

	if (request_size < PARRILLADA_GET_16 (buffer->len) + sizeof (buffer->len)) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_SIZE_MISMATCH);
		g_free (buffer);
		return PARRILLADA_SCSI_FAILURE;
	}

	*data = buffer;
	*size = request_size;

	return res;
}

ParrilladaScsiResult
parrillada_mmc2_read_generic_structure (ParrilladaDeviceHandle *handle,
				     ParrilladaScsiGenericFormatType type,
				     ParrilladaScsiReadDiscStructureHdr **data,
				     int *size,
				     ParrilladaScsiErrCode *error)
{
	ParrilladaReadDiscStructureCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = type;

	res = parrillada_read_disc_structure (cdb, data, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}

#if 0

/* So far this function only creates a warning at
 * build time and is not used but may be in the
 * future. */

ParrilladaScsiResult
parrillada_mmc2_read_dvd_structure (ParrilladaDeviceHandle *handle,
				 int address,
				 ParrilladaScsiDVDFormatType type,
				 ParrilladaScsiReadDiscStructureHdr **data,
				 int *size,
				 ParrilladaScsiErrCode *error)
{
	ParrilladaReadDiscStructureCDB *cdb;
	ParrilladaScsiResult res;

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = type;
	cdb->media_type = PARRILLADA_MEDIA_DVD_HD_DVD;
	PARRILLADA_SET_32 (cdb->address, address);

	res = parrillada_read_disc_structure (cdb, data, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}

ParrilladaScsiResult
parrillada_mmc5_read_bd_structure (ParrilladaDeviceHandle *handle,
				ParrilladaScsiBDFormatType type,
				ParrilladaScsiReadDiscStructureHdr **data,
				int *size,
				ParrilladaScsiErrCode *error)
{
	ParrilladaReadDiscStructureCDB *cdb;
	ParrilladaScsiResult res;

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = type;
	cdb->media_type = PARRILLADA_MEDIA_BD;

	res = parrillada_read_disc_structure (cdb, data, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}

#endif

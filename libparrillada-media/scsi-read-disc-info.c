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

#include "parrillada-media-private.h"

#include "scsi-mmc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-disc-info.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaRdDiscInfoCDB {
	uchar opcode;

	uchar data_type		:3;
	uchar reserved0		:5;

	uchar reserved1		[5];
	uchar alloc_len		[2];

	uchar ctl;
};

#else

struct _ParrilladaRdDiscInfoCDB {
	uchar opcode;

	uchar reserved0		:5;
	uchar data_type		:3;

	uchar reserved1		[5];
	uchar alloc_len		[2];

	uchar ctl;
};

#endif

typedef struct _ParrilladaRdDiscInfoCDB ParrilladaRdDiscInfoCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaRdDiscInfoCDB,
			     READ_DISC_INFORMATION,
			     PARRILLADA_SCSI_READ);

typedef enum {
PARRILLADA_DISC_INFO_STD		= 0x00,
PARRILLADA_DISC_INFO_TRACK_RES	= 0x01,
PARRILLADA_DISC_INFO_POW_RES	= 0x02,
	/* reserved */
} ParrilladaDiscInfoType;


ParrilladaScsiResult
parrillada_mmc1_read_disc_information_std (ParrilladaDeviceHandle *handle,
					ParrilladaScsiDiscInfoStd **info_return,
					int *size,
					ParrilladaScsiErrCode *error)
{
	ParrilladaScsiDiscInfoStd std_info;
	ParrilladaScsiDiscInfoStd *buffer;
	ParrilladaRdDiscInfoCDB *cdb;
	ParrilladaScsiResult res;
	int request_size;
	int buffer_size;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	if (!info_return || !size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->data_type = PARRILLADA_DISC_INFO_STD;
	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (ParrilladaScsiDiscInfoStd));

	memset (&std_info, 0, sizeof (ParrilladaScsiDiscInfoStd));
	res = parrillada_scsi_command_issue_sync (cdb,
					       &std_info,
					       sizeof (ParrilladaScsiDiscInfoStd),
					       error);
	if (res)
		goto end;

	request_size = PARRILLADA_GET_16 (std_info.len) + 
		       sizeof (std_info.len);
	
	buffer = (ParrilladaScsiDiscInfoStd *) g_new0 (uchar, request_size);

	PARRILLADA_SET_16 (cdb->alloc_len, request_size);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		goto end;
	}

	buffer_size = PARRILLADA_GET_16 (buffer->len) +
		      sizeof (buffer->len);

	if (request_size != buffer_size)
		PARRILLADA_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				  request_size,
				  buffer_size);

	*info_return = buffer;
	*size = MIN (request_size, buffer_size);

end:

	parrillada_scsi_command_free (cdb);
	return res;
}

#if 0

/* These functions are not used for now but may
 * be one day. So keep them around but turn 
 * them off to avoid build warnings */
 
ParrilladaScsiResult
parrillada_mmc5_read_disc_information_tracks (ParrilladaDeviceHandle *handle,
					   ParrilladaScsiTrackResInfo *info_return,
					   int size,
					   ParrilladaScsiErrCode *error)
{
	ParrilladaRdDiscInfoCDB *cdb;
	ParrilladaScsiResult res;

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->data_type = PARRILLADA_DISC_INFO_TRACK_RES;
	PARRILLADA_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = parrillada_scsi_command_issue_sync (cdb, info_return, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}

ParrilladaScsiResult
parrillada_mmc5_read_disc_information_pows (ParrilladaDeviceHandle *handle,
					 ParrilladaScsiPOWResInfo *info_return,
					 int size,
					 ParrilladaScsiErrCode *error)
{
	ParrilladaRdDiscInfoCDB *cdb;
	ParrilladaScsiResult res;

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->data_type = PARRILLADA_DISC_INFO_POW_RES;
	PARRILLADA_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = parrillada_scsi_command_issue_sync (cdb, info_return, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}

#endif

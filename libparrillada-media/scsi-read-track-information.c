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
#include "scsi-read-track-information.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaRdTrackInfoCDB {
	uchar opcode;

	uchar addr_num_type		:2;
	uchar open			:1;	/* MMC5 field only */
	uchar reserved0			:5;

	uchar blk_addr_trk_ses_num	[4];

	uchar reserved1;

	uchar alloc_len			[2];
	uchar ctl;
};

#else

struct _ParrilladaRdTrackInfoCDB {
	uchar opcode;

	uchar reserved0			:5;
	uchar open			:1;
	uchar addr_num_type		:2;

	uchar blk_addr_trk_ses_num	[4];

	uchar reserved1;

	uchar alloc_len			[2];
	uchar ctl;
};

#endif

typedef struct _ParrilladaRdTrackInfoCDB ParrilladaRdTrackInfoCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaRdTrackInfoCDB,
			     READ_TRACK_INFORMATION,
			     PARRILLADA_SCSI_READ);

typedef enum {
PARRILLADA_FIELD_LBA			= 0x00,
PARRILLADA_FIELD_TRACK_NUM			= 0x01,
PARRILLADA_FIELD_SESSION_NUM		= 0x02,
	/* reserved */
} ParrilladaFieldType;

#define PARRILLADA_SCSI_INCOMPLETE_TRACK	0xFF

static ParrilladaScsiResult
parrillada_read_track_info (ParrilladaRdTrackInfoCDB *cdb,
			 ParrilladaScsiTrackInfo *info,
			 int *size,
			 ParrilladaScsiErrCode *error)
{
	ParrilladaScsiTrackInfo hdr;
	ParrilladaScsiResult res;
	int datasize;

	if (!info || !size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	/* first ask the drive how long should the data be and then ... */
	datasize = 4;
	memset (&hdr, 0, sizeof (info));
	PARRILLADA_SET_16 (cdb->alloc_len, datasize);
	res = parrillada_scsi_command_issue_sync (cdb, &hdr, datasize, error);
	if (res)
		return res;

	/* ... check the size in case of a buggy firmware ... */
	if (PARRILLADA_GET_16 (hdr.len) + sizeof (hdr.len) >= datasize) {
		datasize = PARRILLADA_GET_16 (hdr.len) + sizeof (hdr.len);

		if (datasize > *size) {
			/* it must not be over sizeof (ParrilladaScsiTrackInfo) */
			if (datasize > sizeof (ParrilladaScsiTrackInfo)) {
				PARRILLADA_MEDIA_LOG ("Oversized data received (%i) setting to %i", datasize, *size);
				datasize = *size;
			}
			else
				*size = datasize;
		}
		else if (*size < datasize) {
			PARRILLADA_MEDIA_LOG ("Oversized data required (%i) setting to %i", *size, datasize);
			*size = datasize;
		}
	}
	else {
		PARRILLADA_MEDIA_LOG ("Undersized data received (%i) setting to %i", datasize, *size);
		datasize = *size;
	}

	/* ... and re-issue the command */
	memset (info, 0, sizeof (ParrilladaScsiTrackInfo));
	PARRILLADA_SET_16 (cdb->alloc_len, datasize);
	res = parrillada_scsi_command_issue_sync (cdb, info, datasize, error);
	if (res == PARRILLADA_SCSI_OK) {
		if (datasize != PARRILLADA_GET_16 (info->len) + sizeof (info->len))
			PARRILLADA_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
					  datasize,
					  PARRILLADA_GET_16 (info->len) + sizeof (info->len));

		*size = MIN (datasize, PARRILLADA_GET_16 (info->len) + sizeof (info->len));
	}

	return res;
}

/**
 * 
 * NOTE: if media is a CD and track_num = 0 then returns leadin
 * but since the other media don't have a leadin they error out.
 * if track_num = 255 returns last incomplete track.
 */
 
ParrilladaScsiResult
parrillada_mmc1_read_track_info (ParrilladaDeviceHandle *handle,
			      int track_num,
			      ParrilladaScsiTrackInfo *track_info,
			      int *size,
			      ParrilladaScsiErrCode *error)
{
	ParrilladaRdTrackInfoCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->addr_num_type = PARRILLADA_FIELD_TRACK_NUM;
	PARRILLADA_SET_32 (cdb->blk_addr_trk_ses_num, track_num);

	res = parrillada_read_track_info (cdb, track_info, size, error);
	parrillada_scsi_command_free (cdb);

	return res;
}

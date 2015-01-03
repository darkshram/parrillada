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
#include "scsi-mmc3.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-toc-pma-atip.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaRdTocPmaAtipCDB {
	uchar opcode;

	uchar reserved0		:1;
	uchar msf		:1;
	uchar reserved1		:6;

	uchar format		:4;
	uchar reserved2		:4;

	uchar reserved3		[3];
	uchar track_session_num;

	uchar alloc_len		[2];
	uchar ctl;
};

#else

struct _ParrilladaRdTocPmaAtipCDB {
	uchar opcode;

	uchar reserved1		:6;
	uchar msf		:1;
	uchar reserved0		:1;

	uchar reserved2		:4;
	uchar format		:4;

	uchar reserved3		[3];
	uchar track_session_num;

	uchar alloc_len		[2];
	uchar ctl;
};

#endif

typedef struct _ParrilladaRdTocPmaAtipCDB ParrilladaRdTocPmaAtipCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaRdTocPmaAtipCDB,
			     READ_TOC_PMA_ATIP,
			     PARRILLADA_SCSI_READ);

typedef enum {
PARRILLADA_RD_TAP_FORMATTED_TOC		= 0x00,
PARRILLADA_RD_TAP_MULTISESSION_INFO	= 0x01,
PARRILLADA_RD_TAP_RAW_TOC			= 0x02,
PARRILLADA_RD_TAP_PMA			= 0x03,
PARRILLADA_RD_TAP_ATIP			= 0x04,
PARRILLADA_RD_TAP_CD_TEXT			= 0x05	/* Introduced in MMC3 */
} ParrilladaReadTocPmaAtipType;

static ParrilladaScsiResult
parrillada_read_toc_pma_atip (ParrilladaRdTocPmaAtipCDB *cdb,
			   int desc_size,
			   ParrilladaScsiTocPmaAtipHdr **data,
			   int *size,
			   ParrilladaScsiErrCode *error)
{
	ParrilladaScsiTocPmaAtipHdr *buffer;
	ParrilladaScsiTocPmaAtipHdr hdr;
	ParrilladaScsiResult res;
	int request_size;
	int buffer_size;

	if (!data || !size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (ParrilladaScsiTocPmaAtipHdr));

	memset (&hdr, 0, sizeof (ParrilladaScsiTocPmaAtipHdr));
	res = parrillada_scsi_command_issue_sync (cdb,
					       &hdr,
					       sizeof (ParrilladaScsiTocPmaAtipHdr),
					       error);
	if (res) {
		*size = 0;
		return res;
	}

	request_size = PARRILLADA_GET_16 (hdr.len) + sizeof (hdr.len);

	/* NOTE: if size is not valid use the maximum possible size */
	if ((request_size - sizeof (hdr)) % desc_size) {
		PARRILLADA_MEDIA_LOG ("Unaligned data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}
	else if (request_size - sizeof (hdr) < desc_size) {
		PARRILLADA_MEDIA_LOG ("Undersized data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}

	buffer = (ParrilladaScsiTocPmaAtipHdr *) g_new0 (uchar, request_size);

	PARRILLADA_SET_16 (cdb->alloc_len, request_size);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		*size = 0;
		return res;
	}

	buffer_size = PARRILLADA_GET_16 (buffer->len) + sizeof (buffer->len);

	*data = buffer;
	*size = MIN (buffer_size, request_size);

	return res;
}

/**
 * Returns TOC data for all the sessions starting with track_num
 */

ParrilladaScsiResult
parrillada_mmc1_read_toc_formatted (ParrilladaDeviceHandle *handle,
				 int track_num,
				 ParrilladaScsiFormattedTocData **data,
				 int *size,
				 ParrilladaScsiErrCode *error)
{
	ParrilladaRdTocPmaAtipCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = PARRILLADA_RD_TAP_FORMATTED_TOC;

	/* first track for which this function will return information */
	cdb->track_session_num = track_num;

	res = parrillada_read_toc_pma_atip (cdb,
					 sizeof (ParrilladaScsiTocDesc),
					(ParrilladaScsiTocPmaAtipHdr **) data,
					 size,
					 error);
	parrillada_scsi_command_free (cdb);
	return res;
}

/**
 * Returns RAW TOC data
 */

ParrilladaScsiResult
parrillada_mmc1_read_toc_raw (ParrilladaDeviceHandle *handle,
			   int session_num,
			   ParrilladaScsiRawTocData **data,
			   int *size,
			   ParrilladaScsiErrCode *error)
{
	ParrilladaRdTocPmaAtipCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = PARRILLADA_RD_TAP_RAW_TOC;

	/* first session for which this function will return information */
	cdb->track_session_num = session_num;

	res = parrillada_read_toc_pma_atip (cdb,
					 sizeof (ParrilladaScsiRawTocDesc),
					(ParrilladaScsiTocPmaAtipHdr **) data,
					 size,
					 error);
	parrillada_scsi_command_free (cdb);
	return res;
}

/**
 * Returns CD-TEXT information recorded in the leadin area as R-W sub-channel
 */

ParrilladaScsiResult
parrillada_mmc3_read_cd_text (ParrilladaDeviceHandle *handle,
			   ParrilladaScsiCDTextData **data,
			   int *size,
			   ParrilladaScsiErrCode *error)
{
	ParrilladaRdTocPmaAtipCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = PARRILLADA_RD_TAP_CD_TEXT;

	res = parrillada_read_toc_pma_atip (cdb,
					 sizeof (ParrilladaScsiCDTextPackData),
					(ParrilladaScsiTocPmaAtipHdr **) data,
					 size,
					 error);
	parrillada_scsi_command_free (cdb);
	return res;
}

/**
 * Returns ATIP information
 */

ParrilladaScsiResult
parrillada_mmc1_read_atip (ParrilladaDeviceHandle *handle,
			ParrilladaScsiAtipData **data,
			int *size,
			ParrilladaScsiErrCode *error)
{
	ParrilladaRdTocPmaAtipCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	/* In here we have to ask how many bytes the drive wants to return first
	 * indeed there is a difference in the descriptor size between MMC1/MMC2
	 * and MMC3. */
	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->format = PARRILLADA_RD_TAP_ATIP;
	cdb->msf = 1;				/* specs says it's compulsory */

	/* FIXME: sizeof (ParrilladaScsiTocDesc) is not really good here but that
	 * avoids the unaligned message */
	res = parrillada_read_toc_pma_atip (cdb,
					 sizeof (ParrilladaScsiTocDesc),
					(ParrilladaScsiTocPmaAtipHdr **) data,
					 size,
					 error);
	parrillada_scsi_command_free (cdb);
	return res;
}

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

#include "scsi-mmc3.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-get-performance.h"

/**
 * GET PERMORMANCE command description	(MMC2)
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaGetPerformanceCDB {
	uchar opcode		:8;

	uchar except		:2;
	uchar write		:1;
	uchar tolerance		:2;
	uchar reserved0		:3;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#else

struct _ParrilladaGetPerformanceCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar tolerance		:2;
	uchar write		:1;
	uchar except		:2;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#endif

typedef struct _ParrilladaGetPerformanceCDB ParrilladaGetPerformanceCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaGetPerformanceCDB,
			     GET_PERFORMANCE,
			     PARRILLADA_SCSI_READ);

/* used to choose which GET PERFORMANCE response we want */
#define PARRILLADA_GET_PERFORMANCE_PERF_TYPE		0x00
#define PARRILLADA_GET_PERFORMANCE_UNUSABLE_AREA_TYPE	0x01
#define PARRILLADA_GET_PERFORMANCE_DEFECT_STATUS_TYPE	0x02
#define PARRILLADA_GET_PERFORMANCE_WR_SPEED_TYPE		0x03
#define PARRILLADA_GET_PERFORMANCE_DBI_TYPE		0x04
#define PARRILLADA_GET_PERFORMANCE_DBI_CACHE_TYPE		0x05

static ParrilladaScsiGetPerfData *
parrillada_get_performance_get_buffer (ParrilladaGetPerformanceCDB *cdb,
				    gint sizeof_descriptors,
				    ParrilladaScsiGetPerfHdr *hdr,
				    ParrilladaScsiErrCode *error)
{
	ParrilladaScsiGetPerfData *buffer;
	ParrilladaScsiResult res;
	int request_size;
	int desc_num;

	/* ... check the request size ... */
	request_size = PARRILLADA_GET_32 (hdr->len) +
		       G_STRUCT_OFFSET (ParrilladaScsiGetPerfHdr, len) +
		       sizeof (hdr->len);

	/* ... check the request size ... */
	if (request_size > 2048) {
		PARRILLADA_MEDIA_LOG ("Oversized data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}
	else if ((request_size - sizeof (hdr)) % sizeof_descriptors) {
		PARRILLADA_MEDIA_LOG ("Unaligned data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}
	else if (request_size < sizeof (hdr)) {
		PARRILLADA_MEDIA_LOG ("Undersized data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}

	desc_num = (request_size - sizeof (ParrilladaScsiGetPerfHdr)) / sizeof_descriptors;

	/* ... allocate a buffer and re-issue the command */
	buffer = (ParrilladaScsiGetPerfData *) g_new0 (uchar, request_size);

	PARRILLADA_SET_16 (cdb->max_desc, desc_num);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return NULL;
	}

	return buffer;
}

static ParrilladaScsiResult
parrillada_get_performance (ParrilladaGetPerformanceCDB *cdb,
			 gint sizeof_descriptors,
			 ParrilladaScsiGetPerfData **data,
			 int *data_size,
			 ParrilladaScsiErrCode *error)
{
	ParrilladaScsiGetPerfData *buffer;
	ParrilladaScsiGetPerfHdr hdr;
	ParrilladaScsiResult res;
	int request_size;
	int buffer_size;

	if (!data || !data_size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	/* Issue the command once to get the size ... */
	memset (&hdr, 0, sizeof (hdr));
	PARRILLADA_SET_16 (cdb->max_desc, 0);
	res = parrillada_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res != PARRILLADA_SCSI_OK)
		return res;

	/* ... get the request size ... */
	request_size = PARRILLADA_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (ParrilladaScsiGetPerfHdr, len) +
		       sizeof (hdr.len);

	/* ... get the answer itself. */
	buffer = parrillada_get_performance_get_buffer (cdb,
						     sizeof_descriptors,
						     &hdr,
						     error);
	if (!buffer)
		return PARRILLADA_SCSI_FAILURE;

	/* make sure the response has the requested size */
	buffer_size = PARRILLADA_GET_32 (buffer->hdr.len) +
		      G_STRUCT_OFFSET (ParrilladaScsiGetPerfHdr, len) +
		      sizeof (buffer->hdr.len);

	if (request_size < buffer_size) {
		ParrilladaScsiGetPerfData *new_buffer;

		/* Strangely some drives returns a buffer size that is bigger
		 * than the one they returned on the first time. So redo whole
		 * operation again but this time with the new size we got */
		PARRILLADA_MEDIA_LOG ("Sizes mismatch asked %i / received %i\n"
				   "Re-issuing the command with received size",
				   request_size,
				   buffer_size);

		/* Try to get a new buffer of the new size */
		memcpy (&hdr, &buffer->hdr, sizeof (hdr));
		new_buffer = parrillada_get_performance_get_buffer (cdb,
		                                         	 sizeof_descriptors,
		                                                 &hdr,
		                                                 error);
		if (new_buffer) {
			g_free (buffer);
			buffer = new_buffer;

			request_size = buffer_size;
			buffer_size = PARRILLADA_GET_32 (buffer->hdr.len) +
				      G_STRUCT_OFFSET (ParrilladaScsiGetPerfHdr, len) +
				      sizeof (buffer->hdr.len);
		}
	}
	else if (request_size > buffer_size)
		PARRILLADA_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				   request_size,
				   buffer_size);
	*data = buffer;
	*data_size = MIN (buffer_size, request_size);

	return res;
}

/**
 * MMC3 command extension
 */

ParrilladaScsiResult
parrillada_mmc3_get_performance_wrt_spd_desc (ParrilladaDeviceHandle *handle,
					   ParrilladaScsiGetPerfData **data,
					   int *size,
					   ParrilladaScsiErrCode *error)
{
	ParrilladaGetPerformanceCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->type = PARRILLADA_GET_PERFORMANCE_WR_SPEED_TYPE;

	res = parrillada_get_performance (cdb, sizeof (ParrilladaScsiWrtSpdDesc), data, size, error);
	parrillada_scsi_command_free (cdb);
	return res;
}


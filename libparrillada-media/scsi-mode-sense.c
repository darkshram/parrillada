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

#include "scsi-spc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-mode-pages.h"

/**
 * MODE SENSE command description (defined in SPC, Scsi Primary Commands) 
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaModeSenseCDB {
	uchar opcode		:8;

	uchar reserved1		:3;
	uchar dbd		:1;
	uchar llbaa		:1;
	uchar reserved0		:3;

	uchar page_code		:8;
	uchar subpage_code	:8;

	uchar reserved2		[3];

	uchar alloc_len		[2];
	uchar ctl;
};

#else

struct _ParrilladaModeSenseCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar llbaa		:1;
	uchar dbd		:1;
	uchar reserved1		:3;

	uchar page_code		:8;
	uchar subpage_code	:8;

	uchar reserved2		[3];

	uchar alloc_len		[2];
	uchar ctl;
};

#endif

typedef struct _ParrilladaModeSenseCDB ParrilladaModeSenseCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaModeSenseCDB,
			     MODE_SENSE,
			     PARRILLADA_SCSI_READ);

#define PARRILLADA_MODE_DATA(data)			((ParrilladaScsiModeData *) (data))

ParrilladaScsiResult
parrillada_spc1_mode_sense_get_page (ParrilladaDeviceHandle *handle,
				  ParrilladaSPCPageType num,
				  ParrilladaScsiModeData **data,
				  int *data_size,
				  ParrilladaScsiErrCode *error)
{
	int page_size;
	int buffer_size;
	int request_size;
	ParrilladaScsiResult res;
	ParrilladaModeSenseCDB *cdb;
	ParrilladaScsiModeData header;
	ParrilladaScsiModeData *buffer;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	if (!data || !data_size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	/* issue a first command to get the size of the page ... */
	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->dbd = 1;
	cdb->page_code = num;
	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (header));
	bzero (&header, sizeof (header));

	PARRILLADA_MEDIA_LOG ("Getting page size");
	res = parrillada_scsi_command_issue_sync (cdb, &header, sizeof (header), error);
	if (res)
		goto end;

	if (!header.hdr.len) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_SIZE_MISMATCH);
		res = PARRILLADA_SCSI_FAILURE;
		goto end;
	}

	/* Paranoïa, make sure:
	 * - the size given in header, the one of the page returned are coherent
	 * - the block descriptors are actually disabled */
	if (PARRILLADA_GET_16 (header.hdr.bdlen)) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		PARRILLADA_MEDIA_LOG ("Block descriptors not disabled %i", PARRILLADA_GET_16 (header.hdr.bdlen));
		res = PARRILLADA_SCSI_FAILURE;
		goto end;
	}

	request_size = PARRILLADA_GET_16 (header.hdr.len) +
		       G_STRUCT_OFFSET (ParrilladaScsiModeHdr, len) +
		       sizeof (header.hdr.len);

	page_size = header.page.len +
		    G_STRUCT_OFFSET (ParrilladaScsiModePage, len) +
		    sizeof (header.page.len);

	if (request_size != page_size + sizeof (ParrilladaScsiModeHdr)) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_SIZE_MISMATCH);
		PARRILLADA_MEDIA_LOG ("Incoherent answer sizes: request %i, page %i", request_size, page_size);
		res = PARRILLADA_SCSI_FAILURE;
		goto end;
	}

	/* ... allocate an appropriate buffer ... */
	buffer = (ParrilladaScsiModeData *) g_new0 (uchar, request_size);

	/* ... re-issue the command */
	PARRILLADA_MEDIA_LOG("Getting page (size = %i)", request_size);

	PARRILLADA_SET_16 (cdb->alloc_len, request_size);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		goto end;
	}

	/* Paranoïa, some more checks:
	 * - the size of the page returned is the one we requested
	 * - block descriptors are actually disabled
	 * - header claimed size == buffer size
	 * - header claimed size == sizeof (header) + sizeof (page) */
	buffer_size = PARRILLADA_GET_16 (buffer->hdr.len) +
		      G_STRUCT_OFFSET (ParrilladaScsiModeHdr, len) +
		      sizeof (buffer->hdr.len);

	page_size = buffer->page.len +
		    G_STRUCT_OFFSET (ParrilladaScsiModePage, len) +
		    sizeof (buffer->page.len);

	if (request_size != buffer_size
	||  PARRILLADA_GET_16 (buffer->hdr.bdlen)
	||  buffer_size != page_size + sizeof (ParrilladaScsiModeHdr)) {
		g_free (buffer);

		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_SIZE_MISMATCH);
		res = PARRILLADA_SCSI_FAILURE;
		goto end;
	}

	*data = buffer;
	*data_size = request_size;

end:
	parrillada_scsi_command_free (cdb);
	return res;
}

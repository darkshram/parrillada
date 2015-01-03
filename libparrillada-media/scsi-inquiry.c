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

#include "scsi-spc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-inquiry.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaInquiryCDB {
	uchar opcode;

	uchar evpd			:1;
	uchar cmd_dt			:1;
	uchar reserved0		:6;

	uchar op_code;

	uchar reserved1;

	uchar alloc_len;

	uchar ctl;
};

#else

struct _ParrilladaInquiryCDB {
	uchar opcode;

	uchar reserved0		:6;
	uchar cmd_dt			:1;
	uchar evpd			:1;

	uchar op_code;

	uchar reserved1;

	uchar alloc_len;

	uchar ctl;
};

#endif

typedef struct _ParrilladaInquiryCDB ParrilladaInquiryCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaInquiryCDB,
			     INQUIRY,
			     PARRILLADA_SCSI_READ);

ParrilladaScsiResult
parrillada_spc1_inquiry (ParrilladaDeviceHandle *handle,
                      ParrilladaScsiInquiry *hdr,
                      ParrilladaScsiErrCode *error)
{
	ParrilladaInquiryCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->alloc_len = sizeof (ParrilladaScsiInquiry);

	memset (hdr, 0, sizeof (ParrilladaScsiInquiry));
	res = parrillada_scsi_command_issue_sync (cdb,
					       hdr,
					       sizeof (ParrilladaScsiInquiry),
					       error);
	parrillada_scsi_command_free (cdb);
	return res;
}

ParrilladaScsiResult
parrillada_spc1_inquiry_is_optical_drive (ParrilladaDeviceHandle *handle,
                                       ParrilladaScsiErrCode *error)
{
	ParrilladaInquiryCDB *cdb;
	ParrilladaScsiInquiry hdr;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	cdb->alloc_len = sizeof (hdr);

	memset (&hdr, 0, sizeof (hdr));
	res = parrillada_scsi_command_issue_sync (cdb,
					       &hdr,
					       sizeof (hdr),
					       error);
	parrillada_scsi_command_free (cdb);

	if (res != PARRILLADA_SCSI_OK)
		return res;

	/* NOTE: 0x05 is for CD/DVD players */
	return hdr.type == 0x05? PARRILLADA_SCSI_OK:PARRILLADA_SCSI_RECOVERABLE;
}

 

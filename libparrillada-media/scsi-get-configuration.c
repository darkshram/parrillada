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

#include "scsi-mmc2.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-get-configuration.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaGetConfigCDB {
	uchar opcode		:8;

	uchar returned_data	:2;
	uchar reserved0		:6;

	uchar feature_num	[2];

	uchar reserved1		[3];

	uchar alloc_len		[2];

	uchar ctl;
};

#else

struct _ParrilladaGetConfigCDB {
	uchar opcode		:8;

	uchar reserved1		:6;
	uchar returned_data	:2;

	uchar feature_num	[2];

	uchar reserved0		[3];

	uchar alloc_len		[2];

	uchar ctl;
};

#endif

typedef struct _ParrilladaGetConfigCDB ParrilladaGetConfigCDB;

PARRILLADA_SCSI_COMMAND_DEFINE (ParrilladaGetConfigCDB,
			     GET_CONFIGURATION,
			     PARRILLADA_SCSI_READ);

typedef enum {
PARRILLADA_GET_CONFIG_RETURN_ALL_FEATURES	= 0x00,
PARRILLADA_GET_CONFIG_RETURN_ALL_CURRENT	= 0x01,
PARRILLADA_GET_CONFIG_RETURN_ONLY_FEATURE	= 0x02,
} ParrilladaGetConfigReturnedData;

static ParrilladaScsiResult
parrillada_get_configuration (ParrilladaGetConfigCDB *cdb,
			   ParrilladaScsiGetConfigHdr **data,
			   int *size,
			   ParrilladaScsiErrCode *error)
{
	ParrilladaScsiGetConfigHdr *buffer;
	ParrilladaScsiGetConfigHdr hdr;
	ParrilladaScsiResult res;
	int request_size;
	int buffer_size;

	if (!data || !size) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_BAD_ARGUMENT);
		return PARRILLADA_SCSI_FAILURE;
	}

	/* first issue the command once ... */
	memset (&hdr, 0, sizeof (hdr));
	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (hdr));
	res = parrillada_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	/* ... check the size returned ... */
	request_size = PARRILLADA_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (ParrilladaScsiGetConfigHdr, len) +
		       sizeof (hdr.len);

	/* NOTE: if size is not valid use the maximum possible size */
	if ((request_size - sizeof (hdr)) % 8) {
		PARRILLADA_MEDIA_LOG ("Unaligned data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}
	else if (request_size <= sizeof (hdr)) {
		/* NOTE: if there is a feature, the size must be larger than the
		 * header size. */
		PARRILLADA_MEDIA_LOG ("Undersized data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}

	/* ... allocate a buffer and re-issue the command */
	buffer = (ParrilladaScsiGetConfigHdr *) g_new0 (uchar, request_size);

	PARRILLADA_SET_16 (cdb->alloc_len, request_size);
	res = parrillada_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return res;
	}

	/* make sure the response has the requested size */
	buffer_size = PARRILLADA_GET_32 (buffer->len) +
		      G_STRUCT_OFFSET (ParrilladaScsiGetConfigHdr, len) +
		      sizeof (hdr.len);

	if (buffer_size < sizeof (ParrilladaScsiGetConfigHdr) + 2) {
		/* we can't have a size less or equal to that of the header */
		PARRILLADA_MEDIA_LOG ("Size of buffer is less or equal to size of header");
		g_free (buffer);
		return PARRILLADA_SCSI_FAILURE;
	}

	if (buffer_size != request_size)
		PARRILLADA_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				  request_size,
				  buffer_size);

	*data = buffer;
	*size = MIN (buffer_size, request_size);
	return PARRILLADA_SCSI_OK;
}

ParrilladaScsiResult
parrillada_mmc2_get_configuration_feature (ParrilladaDeviceHandle *handle,
					ParrilladaScsiFeatureType type,
					ParrilladaScsiGetConfigHdr **data,
					int *size,
					ParrilladaScsiErrCode *error)
{
	ParrilladaScsiGetConfigHdr *hdr = NULL;
	ParrilladaGetConfigCDB *cdb;
	ParrilladaScsiResult res;
	int hdr_size = 0;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);
	g_return_val_if_fail (data != NULL, PARRILLADA_SCSI_FAILURE);
	g_return_val_if_fail (size != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	PARRILLADA_SET_16 (cdb->feature_num, type);
	cdb->returned_data = PARRILLADA_GET_CONFIG_RETURN_ONLY_FEATURE;

	res = parrillada_get_configuration (cdb, &hdr, &hdr_size, error);
	parrillada_scsi_command_free (cdb);

	/* make sure the desc is the one we want */
	if (hdr && PARRILLADA_GET_16 (hdr->desc->code) != type) {
		PARRILLADA_MEDIA_LOG ("Wrong type returned %d", hdr->desc->code);
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_TYPE_MISMATCH);

		g_free (hdr);
		return PARRILLADA_SCSI_FAILURE;
	}

	*data = hdr;
	*size = hdr_size;
	return res;
}

ParrilladaScsiResult
parrillada_mmc2_get_profile (ParrilladaDeviceHandle *handle,
			  ParrilladaScsiProfile *profile,
			  ParrilladaScsiErrCode *error)
{
	ParrilladaScsiGetConfigHdr hdr;
	ParrilladaGetConfigCDB *cdb;
	ParrilladaScsiResult res;

	g_return_val_if_fail (handle != NULL, PARRILLADA_SCSI_FAILURE);
	g_return_val_if_fail (profile != NULL, PARRILLADA_SCSI_FAILURE);

	cdb = parrillada_scsi_command_new (&info, handle);
	PARRILLADA_SET_16 (cdb->feature_num, PARRILLADA_SCSI_FEAT_CORE);
	cdb->returned_data = PARRILLADA_GET_CONFIG_RETURN_ONLY_FEATURE;

	memset (&hdr, 0, sizeof (hdr));
	PARRILLADA_SET_16 (cdb->alloc_len, sizeof (hdr));
	res = parrillada_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	parrillada_scsi_command_free (cdb);

	if (res)
		return res;

	*profile = PARRILLADA_GET_16 (hdr.current_profile);
	return PARRILLADA_SCSI_OK;
}


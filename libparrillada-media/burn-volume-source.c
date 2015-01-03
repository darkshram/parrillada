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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "burn-volume-source.h"
#include "burn-iso9660.h"
#include "parrillada-media.h"
#include "parrillada-media-private.h"

#include "scsi-mmc1.h"
#include "scsi-mmc2.h"
#include "scsi-sbc.h"

static gint64
parrillada_volume_source_seek_device_handle (ParrilladaVolSrc *src,
					  guint block,
					  gint whence,
					  GError **error)
{
	gint64 oldpos;

	oldpos = src->position;

	if (whence == SEEK_CUR)
		src->position += block;
	else if (whence == SEEK_SET)
		src->position = block;

	return oldpos;
}

static gint64
parrillada_volume_source_seek_fd (ParrilladaVolSrc *src,
			       guint block,
			       int whence,
			       GError **error)
{
	gint64 oldpos;

	oldpos = ftello (src->data);
	if (fseeko (src->data, (guint64) (block * ISO9660_BLOCK_SIZE), whence) == -1) {
		int errsv = errno;

		PARRILLADA_MEDIA_LOG ("fseeko () failed at block %i (= %lli bytes) (%s)",
				   block,
				   (guint64) (block * ISO9660_BLOCK_SIZE),
				   g_strerror (errsv));
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     "%s",
			     g_strerror (errsv));
		return -1;
	}

	return oldpos / ISO9660_BLOCK_SIZE;
}

static gboolean
parrillada_volume_source_read_fd (ParrilladaVolSrc *src,
			       gchar *buffer,
			       guint blocks,
			       GError **error)
{
	guint64 bytes_read;

	PARRILLADA_MEDIA_LOG ("Using fread()");

	bytes_read = fread (buffer, 1, ISO9660_BLOCK_SIZE * blocks, src->data);
	if (bytes_read != ISO9660_BLOCK_SIZE * blocks) {
		int errsv = errno;

		PARRILLADA_MEDIA_LOG ("fread () failed (%s)", g_strerror (errsv));
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     "%s",
			     g_strerror (errsv));
		return FALSE;
	}

	return TRUE;
}

static gboolean
parrillada_volume_source_readcd_device_handle (ParrilladaVolSrc *src,
					    gchar *buffer,
					    guint blocks,
					    GError **error)
{
	ParrilladaScsiResult result;
	ParrilladaScsiErrCode code;

	PARRILLADA_MEDIA_LOG ("Using READCD. Reading with track mode %i", src->data_mode);
	result = parrillada_mmc1_read_block (src->data,
					  TRUE,
					  src->data_mode,
					  PARRILLADA_SCSI_BLOCK_HEADER_NONE,
					  PARRILLADA_SCSI_BLOCK_NO_SUBCHANNEL,
					  src->position,
					  blocks,
					  (unsigned char *) buffer,
					  blocks * ISO9660_BLOCK_SIZE,
					  &code);
	if (result == PARRILLADA_SCSI_OK) {
		src->position += blocks;
		return TRUE;
	}

	/* Give it a last chance if the code is PARRILLADA_SCSI_INVALID_TRACK_MODE */
	if (code == PARRILLADA_SCSI_INVALID_TRACK_MODE) {
		PARRILLADA_MEDIA_LOG ("Wrong track mode autodetecting mode for block %i",
				  src->position);

		for (src->data_mode = PARRILLADA_SCSI_BLOCK_TYPE_CDDA;
		     src->data_mode <= PARRILLADA_SCSI_BLOCK_TYPE_MODE2_FORM2;
		     src->data_mode ++) {
			PARRILLADA_MEDIA_LOG ("Re-trying with track mode %i", src->data_mode);
			result = parrillada_mmc1_read_block (src->data,
							  TRUE,
							  src->data_mode,
							  PARRILLADA_SCSI_BLOCK_HEADER_NONE,
							  PARRILLADA_SCSI_BLOCK_NO_SUBCHANNEL,
							  src->position,
							  blocks,
							  (unsigned char *) buffer,
							  blocks * ISO9660_BLOCK_SIZE,
							  &code);

			if (result == PARRILLADA_SCSI_OK) {
				src->position += blocks;
				return TRUE;
			}

			if (code != PARRILLADA_SCSI_INVALID_TRACK_MODE) {
				PARRILLADA_MEDIA_LOG ("Failed with error code %i", code);
				src->data_mode = PARRILLADA_SCSI_BLOCK_TYPE_ANY;
				break;
			}
		}
	}

	g_set_error (error,
		     PARRILLADA_MEDIA_ERROR,
		     PARRILLADA_MEDIA_ERROR_GENERAL,
		     "%s",
		     parrillada_scsi_strerror (code));

	return FALSE;
}

static gboolean
parrillada_volume_source_read10_device_handle (ParrilladaVolSrc *src,
					    gchar *buffer,
					    guint blocks,
					    GError **error)
{
	ParrilladaScsiResult result;
	ParrilladaScsiErrCode code;

	PARRILLADA_MEDIA_LOG ("Using READ10");
	result = parrillada_sbc_read10_block (src->data,
					   src->position,
					   blocks,
					   (unsigned char *) buffer,
					   blocks * ISO9660_BLOCK_SIZE,
					   &code);
	if (result == PARRILLADA_SCSI_OK) {
		src->position += blocks;
		return TRUE;
	}

	PARRILLADA_MEDIA_LOG ("READ10 failed %s at %i",
			  parrillada_scsi_strerror (code),
			  src->position);
	g_set_error (error,
		     PARRILLADA_MEDIA_ERROR,
		     PARRILLADA_MEDIA_ERROR_GENERAL,
		     "%s",
		     parrillada_scsi_strerror (code));

	return FALSE;
}

void
parrillada_volume_source_close (ParrilladaVolSrc *src)
{
	src->ref --;
	if (src->ref > 0)
		return;

	if (src->seek == parrillada_volume_source_seek_fd)
		fclose (src->data);

	g_free (src);
}

ParrilladaVolSrc *
parrillada_volume_source_open_file (const gchar *path,
				 GError **error)
{
	ParrilladaVolSrc *src;
	FILE *file;

	file = fopen (path, "r");
	if (!file) {
		int errsv = errno;

		PARRILLADA_MEDIA_LOG ("open () failed (%s)", g_strerror (errsv));
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     "%s",
			     g_strerror (errsv));
		return FALSE;
	}

	src = g_new0 (ParrilladaVolSrc, 1);
	src->ref = 1;
	src->data = file;
	src->seek = parrillada_volume_source_seek_fd;
	src->read = parrillada_volume_source_read_fd;
	return src;
}

ParrilladaVolSrc *
parrillada_volume_source_open_fd (int fd,
			       GError **error)
{
	ParrilladaVolSrc *src;
	int dup_fd;
	FILE *file;

	dup_fd = dup (fd);
	if (dup_fd == -1) {
		int errsv = errno;

		PARRILLADA_MEDIA_LOG ("dup () failed (%s)", g_strerror (errsv));
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     "%s",
			     g_strerror (errsv));
		return FALSE;
	}

	file = fdopen (dup_fd, "r");
	if (!file) {
		int errsv = errno;

		close (dup_fd);

		PARRILLADA_MEDIA_LOG ("fdopen () failed (%s)", g_strerror (errsv));
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_GENERAL,
			     "%s",
			     g_strerror (errsv));
		return FALSE;
	}

	src = g_new0 (ParrilladaVolSrc, 1);
	src->ref = 1;
	src->data = file;
	src->seek = parrillada_volume_source_seek_fd;
	src->read = parrillada_volume_source_read_fd;
	return src;
}

ParrilladaVolSrc *
parrillada_volume_source_open_device_handle (ParrilladaDeviceHandle *handle,
					  GError **error)
{
	int size;
	ParrilladaVolSrc *src;
	ParrilladaScsiResult result;
	ParrilladaScsiGetConfigHdr *hdr = NULL;

	g_return_val_if_fail (handle != NULL, NULL);

	src = g_new0 (ParrilladaVolSrc, 1);
	src->ref = 1;
	src->data = handle;
	src->seek = parrillada_volume_source_seek_device_handle;

	/* check which read function should be used. */
	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_RD_CD,
							 &hdr,
							 &size,
							 NULL);
	if (result == PARRILLADA_SCSI_OK && hdr->desc->current) {
		PARRILLADA_MEDIA_LOG ("READ CD current. Using READCD");
		src->read = parrillada_volume_source_readcd_device_handle;
		g_free (hdr);
		return src;
	}

	/* clean and retry */
	g_free (hdr);
	hdr = NULL;

	result = parrillada_mmc2_get_configuration_feature (handle,
							 PARRILLADA_SCSI_FEAT_RD_RANDOM,
							 &hdr,
							 &size,
							 NULL);
	if (result == PARRILLADA_SCSI_OK && hdr->desc->current) {
		PARRILLADA_MEDIA_LOG ("READ DVD current. Using READ10");
		src->read = parrillada_volume_source_read10_device_handle;
		g_free (hdr);
	}
	else {
		PARRILLADA_MEDIA_LOG ("READ DVD not current. Using READCD.");
		src->read = parrillada_volume_source_readcd_device_handle;
		g_free (hdr);
	}

	return src;
}

void
parrillada_volume_source_ref (ParrilladaVolSrc *vol)
{
	vol->ref ++;
}


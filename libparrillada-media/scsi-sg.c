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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "parrillada-media-private.h"

#include "scsi-command.h"
#include "scsi-utils.h"
#include "scsi-error.h"
#include "scsi-sense-data.h"

struct _ParrilladaDeviceHandle {
	int fd;
};

struct _ParrilladaScsiCmd {
	uchar cmd [PARRILLADA_SCSI_CMD_MAX_LEN];
	ParrilladaDeviceHandle *handle;

	const ParrilladaScsiCmdInfo *info;
};
typedef struct _ParrilladaScsiCmd ParrilladaScsiCmd;

#define PARRILLADA_SCSI_CMD_OPCODE_OFF			0
#define PARRILLADA_SCSI_CMD_SET_OPCODE(command)		(command->cmd [PARRILLADA_SCSI_CMD_OPCODE_OFF] = command->info->opcode)

#define OPEN_FLAGS			O_RDWR /*|O_EXCL */|O_NONBLOCK

/**
 * This is to send a command
 */

static void
parrillada_sg_command_setup (struct sg_io_hdr *transport,
			  uchar *sense_data,
			  ParrilladaScsiCmd *cmd,
			  uchar *buffer,
			  int size)
{
	memset (sense_data, 0, PARRILLADA_SENSE_DATA_SIZE);
	memset (transport, 0, sizeof (struct sg_io_hdr));
	
	transport->interface_id = 'S';				/* mandatory */
//	transport->flags = SG_FLAG_LUN_INHIBIT|SG_FLAG_DIRECT_IO;
	transport->cmdp = cmd->cmd;
	transport->cmd_len = cmd->info->size;
	transport->dxferp = buffer;
	transport->dxfer_len = size;

	/* where to output the scsi sense buffer */
	transport->sbp = sense_data;
	transport->mx_sb_len = PARRILLADA_SENSE_DATA_SIZE;

	if (cmd->info->direction & PARRILLADA_SCSI_READ)
		transport->dxfer_direction = SG_DXFER_FROM_DEV;
	else if (cmd->info->direction & PARRILLADA_SCSI_WRITE)
		transport->dxfer_direction = SG_DXFER_TO_DEV;
}

ParrilladaScsiResult
parrillada_scsi_command_issue_sync (gpointer command,
				 gpointer buffer,
				 int size,
				 ParrilladaScsiErrCode *error)
{
	uchar sense_buffer [PARRILLADA_SENSE_DATA_SIZE];
	struct sg_io_hdr transport;
	ParrilladaScsiResult res;
	ParrilladaScsiCmd *cmd;

	g_return_val_if_fail (command != NULL, PARRILLADA_SCSI_FAILURE);

	cmd = command;
	parrillada_sg_command_setup (&transport,
				  sense_buffer,
				  cmd,
				  buffer,
				  size);

	/* NOTE on SG_IO: only for TEST UNIT READY, REQUEST/MODE SENSE, INQUIRY,
	 * READ CAPACITY, READ BUFFER, READ and LOG SENSE are allowed with it */
	res = ioctl (cmd->handle->fd, SG_IO, &transport);
	if (res) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_ERRNO);
		return PARRILLADA_SCSI_FAILURE;
	}

	if ((transport.info & SG_INFO_OK_MASK) == SG_INFO_OK)
		return PARRILLADA_SCSI_OK;

	if ((transport.masked_status & CHECK_CONDITION) && transport.sb_len_wr)
		return parrillada_sense_data_process (sense_buffer, error);

	return PARRILLADA_SCSI_FAILURE;
}

gpointer
parrillada_scsi_command_new (const ParrilladaScsiCmdInfo *info,
			  ParrilladaDeviceHandle *handle) 
{
	ParrilladaScsiCmd *cmd;

	g_return_val_if_fail (handle != NULL, NULL);

	/* make sure we can set the flags of the descriptor */

	/* allocate the command */
	cmd = g_new0 (ParrilladaScsiCmd, 1);
	cmd->info = info;
	cmd->handle = handle;

	PARRILLADA_SCSI_CMD_SET_OPCODE (cmd);
	return cmd;
}

ParrilladaScsiResult
parrillada_scsi_command_free (gpointer cmd)
{
	g_free (cmd);
	return PARRILLADA_SCSI_OK;
}

/**
 * This is to open a device
 */

ParrilladaDeviceHandle *
parrillada_device_handle_open (const gchar *path,
			    gboolean exclusive,
			    ParrilladaScsiErrCode *code)
{
	int fd;
	int flags = OPEN_FLAGS;
	ParrilladaDeviceHandle *handle;

	if (exclusive)
		flags |= O_EXCL;

	PARRILLADA_MEDIA_LOG ("Getting handle");
	fd = open (path, flags);
	if (fd < 0) {
		PARRILLADA_MEDIA_LOG ("No handle: %s", strerror (errno));
		if (code) {
			if (errno == EAGAIN
			||  errno == EWOULDBLOCK
			||  errno == EBUSY)
				*code = PARRILLADA_SCSI_NOT_READY;
			else
				*code = PARRILLADA_SCSI_ERRNO;
		}

		return NULL;
	}

	handle = g_new (ParrilladaDeviceHandle, 1);
	handle->fd = fd;

	PARRILLADA_MEDIA_LOG ("Handle ready");
	return handle;
}

void
parrillada_device_handle_close (ParrilladaDeviceHandle *handle)
{
	close (handle->fd);
	g_free (handle);
}

char *
parrillada_device_get_bus_target_lun (const gchar *device)
{
	return strdup (device);
}

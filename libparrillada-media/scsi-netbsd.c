/* $NetBSD: scsi-netbsd.c,v 1.2 2009/03/22 09:30:39 wiz Exp $ */
/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *               Jared D. McNeill 2009 <jmcneill@NetBSD.org>
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

#include <sys/scsiio.h>

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

#define OPEN_FLAGS			(O_RDWR|O_NONBLOCK)
#define SCSIREQ_TIMEOUT			(30 * 1000)

/**
 * This is to send a command
 */

static void
parrillada_sg_command_setup (scsireq_t *req,
			  ParrilladaScsiCmd *cmd,
			  uchar *buffer,
			  int size)
{
	memset (req, 0, sizeof (scsireq_t));

	req->cmdlen = cmd->info->size;
	memcpy(req->cmd, cmd->cmd, req->cmdlen);
	req->databuf = buffer;
	req->datalen = size;
	req->timeout = SCSIREQ_TIMEOUT;

	/* where to output the scsi sense buffer */
	req->senselen = PARRILLADA_SENSE_DATA_SIZE;

	if (cmd->info->direction & PARRILLADA_SCSI_READ)
		req->flags = SCCMD_READ;
	else if (cmd->info->direction & PARRILLADA_SCSI_WRITE)
		req->flags = SCCMD_WRITE;
}

ParrilladaScsiResult
parrillada_scsi_command_issue_sync (gpointer command,
				 gpointer buffer,
				 int size,
				 ParrilladaScsiErrCode *error)
{
	scsireq_t req;
	ParrilladaScsiResult res;
	ParrilladaScsiCmd *cmd;

	cmd = command;
	parrillada_sg_command_setup (&req,
				  cmd,
				  buffer,
				  size);

	res = ioctl (cmd->handle->fd, SCIOCCOMMAND, &req);
	if (res == -1) {
		PARRILLADA_SCSI_SET_ERRCODE (error, PARRILLADA_SCSI_ERRNO);
		return PARRILLADA_SCSI_FAILURE;
	}

	if (req.retsts == SCCMD_OK)
		return PARRILLADA_SCSI_OK;

	if (req.retsts == SCCMD_SENSE)
		return parrillada_sense_data_process (req.sense, error);

	return PARRILLADA_SCSI_FAILURE;
}

gpointer
parrillada_scsi_command_new (const ParrilladaScsiCmdInfo *info,
			  ParrilladaDeviceHandle *handle) 
{
	ParrilladaScsiCmd *cmd;

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
	gchar *rdevnode;

	if (exclusive)
		flags |= O_EXCL;

	rdevnode = g_strdup_printf ("/dev/r%s", path + strlen ("/dev/"));
	fd = open (rdevnode, flags);
	g_free (rdevnode);
	if (fd < 0) {
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

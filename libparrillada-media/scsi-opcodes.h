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

#include <glib.h>

#ifndef _SCSI_CMD_OPCODES_H
#define _SCSI_CMD_OPCODES_H

G_BEGIN_DECLS

/**
 *	SBC1
 */

#define PARRILLADA_PREVENT_ALLOW_MEDIUM_REMOVAL_OPCODE		0x1E

/**
 *	SPC1
 */

#define PARRILLADA_TEST_UNIT_READY_OPCODE			0x00
#define PARRILLADA_INQUIRY_OPCODE				0x12
#define PARRILLADA_MODE_SENSE_OPCODE			0x5a
#define PARRILLADA_MODE_SELECT_OPCODE			0x55


/**
 *	MMC1
 */

#define PARRILLADA_MECHANISM_STATUS_OPCODE			0xBD
#define PARRILLADA_READ_DISC_INFORMATION_OPCODE		0x51
#define PARRILLADA_READ_TRACK_INFORMATION_OPCODE		0x52
#define PARRILLADA_READ_TOC_PMA_ATIP_OPCODE		0x43
#define PARRILLADA_READ_BUFFER_CAPACITY_OPCODE		0x5C
#define PARRILLADA_READ_HEADER_OPCODE			0x44
#define PARRILLADA_READ_SUB_CHANNEL_OPCODE			0x42
#define PARRILLADA_READ_MASTER_CUE_OPCODE			0x59
#define PARRILLADA_LOAD_CD_OPCODE				0xA6
#define PARRILLADA_MECH_STATUS_OPCODE			0xBD
#define PARRILLADA_READ_CD_OPCODE				0xBE

/**
 *	MMC2
 */

#define PARRILLADA_GET_PERFORMANCE_OPCODE			0xac
#define PARRILLADA_GET_CONFIGURATION_OPCODE		0x46
#define PARRILLADA_READ_CAPACITY_OPCODE			0x25
#define PARRILLADA_READ_FORMAT_CAPACITIES_OPCODE		0x23
#define PARRILLADA_READ10_OPCODE				0x28

/**
 *	MMC3
 */

#define PARRILLADA_READ_DISC_STRUCTURE_OPCODE		0xAD

G_END_DECLS

#endif /* _SCSI_CMD-OPCODES_H */

 

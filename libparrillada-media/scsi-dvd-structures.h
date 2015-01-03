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

#include "scsi-base.h"

#ifndef _SCSI_DVD_STRUCTURES_H
#define _SCSI_DVD_STRUCTURES_H

G_BEGIN_DECLS

typedef enum {
PARRILLADA_SCSI_NONE				= 0x00,
PARRILLADA_SCSI_CSS				= 0x01,
PARRILLADA_SCSI_CPRM				= 0x02,
PARRILLADA_SCSI_AACS				= 0x03
	/* reserved */
} ParrilladaScsiProtectionSystem;

typedef enum {
PARRILLADA_SCSI_USA_CANADA				= 0x01,
PARRILLADA_SCSI_WEUROPE_JAPAN_MIDDLE_EAST		= 0x02,
PARRILLADA_SCSI_SE_ASIA				= 0x03,
PARRILLADA_SCSI_AUSTRALIA_LATIN_AMERICA		= 0x04,
PARRILLADA_SCSI_EEUROPE_RUSSIA_INDIA_AFRICA	= 0x05,
PARRILLADA_SCSI_CHINA				= 0x06,

} ParrilladaScsiRegions;

typedef enum {
	/* reserved */
PARRILLADA_SCSI_FORMAT_LAYER_CD			= 0x0008,
	/* reserved */
PARRILLADA_SCSI_FORMAT_LAYER_DVD			= 0x0010,
	/* reserved */
PARRILLADA_SCSI_FORMAT_LAYER_BD			= 0x0040,
	/* reserved */
PARRILLADA_SCSI_FORMAT_LAYER_HD_DVD		= 0x0050
	/* reserved */
} ParrilladaScsiFormatLayersType;

/* the following 3 need key established state (AACS authentification) */
struct _ParrilladaScsiAACSVolID {
	uchar volume_identifier		[4];
};
typedef struct _ParrilladaScsiAACSVolID ParrilladaScsiAACSVolID;

struct _ParrilladaScsiAACSMediaSerial {
	uchar serial_num		[4];
};
typedef struct _ParrilladaScsiAACSMediaSerial ParrilladaScsiAACSMediaSerial;

/* the following two are variable length structures */
struct _ParrilladaScsiAACSMediaID {
	uchar media_id			[1];
};
typedef struct _ParrilladaScsiAACSMediaID ParrilladaScsiAACSMediaID;

struct _ParrilladaScsiAACSKey {
	uchar key_block			[1];
};
typedef struct _ParrilladaScsiAACSKey ParrilladaScsiAACSKey;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaScsiLayerFormatList {
	uchar format_layer_num;

	uchar online_format_layer	:2;
	uchar reserved0			:2;
	uchar default_format_layer	:2;
	uchar reserved1			:2;

	uchar format_layer_0		[0];
};

struct _ParrilladaScsiDiscWrtProtection {
	uchar soft_wrt_protect		:1;
	uchar persistent_wrt_protect	:1;
	uchar cartridge_wrt_protect	:1;
	uchar media_spec_wrt_protect	:1;
	uchar reserved0			:4;

	uchar reserved1			[3];
};

struct _ParrilladaScsiDiscStructureEntry {
	uchar format_code;

	uchar reserved0			:6;
	uchar read_disc			:1;
	uchar send_disc			:1;

	uchar len			[2];
};

struct _ParrilladaScsiPhysicalFormat {
	/* from MMC specs */
	uchar part_version		:4;
	uchar category			:4;

	uchar max_rate			:4;
	uchar size			:4;

	uchar layer_type		:4;
	uchar track			:1;
	uchar layers_num		:2;
	uchar reserved0			:1;

	uchar track_density		:4;
	uchar linear_density		:4;

	uchar zero_1;

	uchar user_data_start		[3];

	uchar zero_2;

	uchar user_data_end		[3];

	uchar zero_3;

	uchar user_data_end_layer0	[3];

	uchar reserved1			:7;
	uchar burst_cutting_area	:1;

	uchar media_specific		[0];
};

struct _ParrilladaScsiCopyrightManagement {
	union {
		struct {
		uchar protection_mode		:4;
		uchar copy_rights		:2;
		uchar protected_sector		:1;
		uchar copyright_material	:1;
		} dvdrom;

		struct {
		uchar reserved0			:4;
		uchar copy_rights		:2;
		uchar reserved1			:1;
		uchar copyright_material	:1;
		} dvd_r_rw_1;

		struct {
		uchar reserved;
		} dvd_ram_dvd_r_2;

		struct {
		uchar reserved0			:2;
		uchar adp_ty			:2;
		uchar reserved1			:4;
		} dvd_r_rw_dl;

	} copyright_status;
	uchar reserved				[3];
};

#else

struct _ParrilladaScsiLayerFormatList {
	uchar format_layer_num;

	uchar reserved1			:2;
	uchar default_format_layer	:2;
	uchar reserved0			:2;
	uchar online_format_layer	:2;

	uchar format_layer_0		[0];
};

struct _ParrilladaScsiDiscWrtProtection {
	uchar reserved0			:4;
	uchar media_spec_wrt_protect	:1;
	uchar cartridge_wrt_protect	:1;
	uchar persistent_wrt_protect	:1;
	uchar soft_wrt_protect		:1;

	uchar reserved1			[3];
};

struct _ParrilladaScsiDiscStructureEntry {
	uchar format_code;

	uchar send_disc			:1;
	uchar read_disc			:1;
	uchar reserved0			:6;

	uchar len			[2];
};

struct _ParrilladaScsiPhysicalFormat {
	/* from MMC specs */
	uchar disk_category		:4;
	uchar part_version		:4;

	uchar disc_size			:4;
	uchar max_rate			:4;

	uchar reserved0			:1;
	uchar layers_num		:2;
	uchar track			:1;
	uchar layer_type		:4;

	uchar linear_density		:4;
	uchar track_density		:4;

	uchar zero_1;

	uchar user_data_start		[3];

	uchar zero_2;

	uchar user_data_end		[3];

	uchar zero_3;

	uchar user_data_end_layer0	[3];

	uchar reserved1			:7;
	uchar burst_cutting_area	:1;

	uchar media_specific		[2031];
};

struct _ParrilladaScsiCopyrightManagement {
	union {
		struct {
		uchar copyright_material	:1;
		uchar protected_sector		:1;
		uchar copy_rights		:2;
		uchar protection_mode		:4;
		} dvdrom;

		struct {
		uchar copyright_material	:1;
		uchar reserved1			:1;
		uchar copy_rights		:2;
		uchar reserved0			:4;
		} dvd_r_rw_1;

		struct {
		uchar reserved;
		} dvd_ram_dvd_r_2;

		struct {
		uchar reserved1			:4;
		uchar adp_ty			:2;
		uchar reserved0			:2;
		} dvd_r_rw_dl;

	} copyright_status;
	uchar reserved				[3];
};

#endif

struct _ParrilladaScsiDVDCopyright {
	uchar protection_system;
	uchar region_management;
	uchar reserved				[2];
};
/* variable length 12 to 188 bytes for DVD, max is 76 for HD DVD */
/*
struct _ParrilladaScsiBurstCuttingArea {
	uchar burst_cutting_area		[0];
};
*/

struct _ParrilladaScsiManufacturingInfo {
	uchar data				[2048];
};

/* variable length */
/*
struct _ParrilladaScsiCopyrightDataSection {
	uchar data				[0];
};
*/

typedef struct _ParrilladaScsiLayerFormatList ParrilladaScsiLayerFormatList;
typedef struct _ParrilladaScsiDiscWrtProtection ParrilladaScsiDiscWrtProtection;
typedef struct _ParrilladaScsiDiscStructureEntry ParrilladaScsiDiscStructureEntry;
typedef struct _ParrilladaScsiPhysicalFormat ParrilladaScsiPhysicalFormat;
typedef struct _ParrilladaScsiCopyrightManagement ParrilladaScsiCopyrightManagement;
typedef struct _ParrilladaScsiDVDCopyright ParrilladaScsiDVDCopyright;
/* typedef struct _ParrilladaScsiBurstCuttingArea ParrilladaScsiBurnCuttingArea; */
typedef struct _ParrilladaScsiManufacturingInfo ParrilladaScsiManufacturingInfo;

G_END_DECLS

#endif /* _SCSI_DVD_STRUCTURES_H */

 

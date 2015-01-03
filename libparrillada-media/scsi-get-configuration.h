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

#ifndef _SCSI_GET_CONFIGURATION_H
#define _SCSI_GET_CONFIGURATION_H

G_BEGIN_DECLS

typedef enum {
PARRILLADA_SCSI_PROF_EMPTY				= 0x0000,
PARRILLADA_SCSI_PROF_NON_REMOVABLE		= 0x0001,
PARRILLADA_SCSI_PROF_REMOVABLE		= 0x0002,
PARRILLADA_SCSI_PROF_MO_ERASABLE		= 0x0003,
PARRILLADA_SCSI_PROF_MO_WRITE_ONCE		= 0x0004,
PARRILLADA_SCSI_PROF_MO_ADVANCED_STORAGE	= 0x0005,
	/* reserved */
PARRILLADA_SCSI_PROF_CDROM			= 0x0008,
PARRILLADA_SCSI_PROF_CDR			= 0x0009,
PARRILLADA_SCSI_PROF_CDRW			= 0x000A,
	/* reserved */
PARRILLADA_SCSI_PROF_DVD_ROM		= 0x0010,
PARRILLADA_SCSI_PROF_DVD_R			= 0x0011,
PARRILLADA_SCSI_PROF_DVD_RAM		= 0x0012,
PARRILLADA_SCSI_PROF_DVD_RW_RESTRICTED	= 0x0013,
PARRILLADA_SCSI_PROF_DVD_RW_SEQUENTIAL	= 0x0014,
PARRILLADA_SCSI_PROF_DVD_R_DL_SEQUENTIAL	= 0x0015,
PARRILLADA_SCSI_PROF_DVD_R_DL_JUMP		= 0x0016,
	/* reserved */
PARRILLADA_SCSI_PROF_DVD_RW_PLUS		= 0x001A,
PARRILLADA_SCSI_PROF_DVD_R_PLUS		= 0x001B,
	/* reserved */
PARRILLADA_SCSI_PROF_DDCD_ROM		= 0x0020,
PARRILLADA_SCSI_PROF_DDCD_R		= 0x0021,
PARRILLADA_SCSI_PROF_DDCD_RW		= 0x0022,
	/* reserved */
PARRILLADA_SCSI_PROF_DVD_RW_PLUS_DL	= 0x002A,
PARRILLADA_SCSI_PROF_DVD_R_PLUS_DL		= 0x002B,
	/* reserved */
PARRILLADA_SCSI_PROF_BD_ROM		= 0x0040,
PARRILLADA_SCSI_PROF_BR_R_SEQUENTIAL	= 0x0041,
PARRILLADA_SCSI_PROF_BR_R_RANDOM		= 0x0042,
PARRILLADA_SCSI_PROF_BD_RW			= 0x0043,
PARRILLADA_SCSI_PROF_HD_DVD_ROM		= 0x0050,
PARRILLADA_SCSI_PROF_HD_DVD_R		= 0x0051,
PARRILLADA_SCSI_PROF_HD_DVD_RAM		= 0x0052,
	/* reserved */
} ParrilladaScsiProfile;

typedef enum {
PARRILLADA_SCSI_INTERFACE_NONE		= 0x00000000,
PARRILLADA_SCSI_INTERFACE_SCSI		= 0x00000001,
PARRILLADA_SCSI_INTERFACE_ATAPI		= 0x00000002,
PARRILLADA_SCSI_INTERFACE_FIREWIRE_95	= 0x00000003,
PARRILLADA_SCSI_INTERFACE_FIREWIRE_A	= 0x00000004,
PARRILLADA_SCSI_INTERFACE_FCP		= 0x00000005,
PARRILLADA_SCSI_INTERFACE_FIREWIRE_B	= 0x00000006,
PARRILLADA_SCSI_INTERFACE_SERIAL_ATAPI	= 0x00000007,
PARRILLADA_SCSI_INTERFACE_USB		= 0x00000008
} ParrilladaScsiInterface;

typedef enum {
PARRILLADA_SCSI_LOADING_CADDY		= 0x000,
PARRILLADA_SCSI_LOADING_TRAY		= 0x001,
PARRILLADA_SCSI_LOADING_POPUP		= 0x002,
PARRILLADA_SCSI_LOADING_EMBED_CHANGER_IND	= 0X004,
PARRILLADA_SCSI_LOADING_EMBED_CHANGER_MAG	= 0x005
} ParrilladaScsiLoadingMech;

typedef enum {
PARRILLADA_SCSI_FEAT_PROFILES		= 0x0000,
PARRILLADA_SCSI_FEAT_CORE			= 0x0001,
PARRILLADA_SCSI_FEAT_MORPHING		= 0x0002,
PARRILLADA_SCSI_FEAT_REMOVABLE		= 0x0003,
PARRILLADA_SCSI_FEAT_WRT_PROTECT		= 0x0004,
	/* reserved */
PARRILLADA_SCSI_FEAT_RD_RANDOM		= 0x0010,
	/* reserved */
PARRILLADA_SCSI_FEAT_RD_MULTI		= 0x001D,
PARRILLADA_SCSI_FEAT_RD_CD			= 0x001E,
PARRILLADA_SCSI_FEAT_RD_DVD		= 0x001F,
PARRILLADA_SCSI_FEAT_WRT_RANDOM		= 0x0020,
PARRILLADA_SCSI_FEAT_WRT_INCREMENT		= 0x0021,
PARRILLADA_SCSI_FEAT_WRT_ERASE		= 0x0022,
PARRILLADA_SCSI_FEAT_WRT_FORMAT		= 0x0023,
PARRILLADA_SCSI_FEAT_DEFECT_MNGT		= 0x0024,
PARRILLADA_SCSI_FEAT_WRT_ONCE		= 0x0025,
PARRILLADA_SCSI_FEAT_RESTRICT_OVERWRT	= 0x0026,
PARRILLADA_SCSI_FEAT_WRT_CAV_CDRW		= 0x0027,
PARRILLADA_SCSI_FEAT_MRW			= 0x0028,
PARRILLADA_SCSI_FEAT_DEFECT_REPORT		= 0x0029,
PARRILLADA_SCSI_FEAT_WRT_DVDRW_PLUS	= 0x002A,
PARRILLADA_SCSI_FEAT_WRT_DVDR_PLUS		= 0x002B,
PARRILLADA_SCSI_FEAT_RIGID_OVERWRT		= 0x002C,
PARRILLADA_SCSI_FEAT_WRT_TAO		= 0x002D,
PARRILLADA_SCSI_FEAT_WRT_SAO_RAW		= 0x002E,
PARRILLADA_SCSI_FEAT_WRT_DVD_LESS		= 0x002F,
PARRILLADA_SCSI_FEAT_RD_DDCD		= 0x0030,
PARRILLADA_SCSI_FEAT_WRT_DDCD		= 0x0031,
PARRILLADA_SCSI_FEAT_RW_DDCD		= 0x0032,
PARRILLADA_SCSI_FEAT_LAYER_JUMP		= 0x0033,
PARRILLADA_SCSI_FEAT_WRT_CDRW		= 0x0037,
PARRILLADA_SCSI_FEAT_BDR_POW		= 0x0038,
	/* reserved */
PARRILLADA_SCSI_FEAT_WRT_DVDRW_PLUS_DL		= 0x003A,
PARRILLADA_SCSI_FEAT_WRT_DVDR_PLUS_DL		= 0x003B,
	/* reserved */
PARRILLADA_SCSI_FEAT_RD_BD			= 0x0040,
PARRILLADA_SCSI_FEAT_WRT_BD		= 0x0041,
PARRILLADA_SCSI_FEAT_TSR			= 0x0042,
	/* reserved */
PARRILLADA_SCSI_FEAT_RD_HDDVD		= 0x0050,
PARRILLADA_SCSI_FEAT_WRT_HDDVD		= 0x0051,
	/* reserved */
PARRILLADA_SCSI_FEAT_HYBRID_DISC		= 0x0080,
	/* reserved */
PARRILLADA_SCSI_FEAT_PWR_MNGT		= 0x0100,
PARRILLADA_SCSI_FEAT_SMART			= 0x0101,
PARRILLADA_SCSI_FEAT_EMBED_CHNGR		= 0x0102,
PARRILLADA_SCSI_FEAT_AUDIO_PLAY		= 0x0103,
PARRILLADA_SCSI_FEAT_FIRM_UPGRADE		= 0x0104,
PARRILLADA_SCSI_FEAT_TIMEOUT		= 0x0105,
PARRILLADA_SCSI_FEAT_DVD_CSS		= 0x0106,
PARRILLADA_SCSI_FEAT_REAL_TIME_STREAM	= 0x0107,
PARRILLADA_SCSI_FEAT_DRIVE_SERIAL_NB	= 0x0108,
PARRILLADA_SCSI_FEAT_MEDIA_SERIAL_NB	= 0x0109,
PARRILLADA_SCSI_FEAT_DCB			= 0x010A,
PARRILLADA_SCSI_FEAT_DVD_CPRM		= 0x010B,
PARRILLADA_SCSI_FEAT_FIRMWARE_INFO		= 0x010C,
PARRILLADA_SCSI_FEAT_AACS			= 0x010D,
	/* reserved */
PARRILLADA_SCSI_FEAT_VCPS			= 0x0110,
} ParrilladaScsiFeatureType;


#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _ParrilladaScsiFeatureDesc {
	uchar code		[2];

	uchar current		:1;
	uchar persistent	:1;
	uchar version		:4;
	uchar reserved		:2;

	uchar add_len;
	uchar data		[0];
};

struct _ParrilladaScsiCoreDescMMC4 {
	/* this is for MMC4 & MMC5 */
	uchar dbe		:1;
	uchar inq2		:1;
	uchar reserved0		:6;

	uchar reserved1		[3];
};

struct _ParrilladaScsiCoreDescMMC3 {
	uchar interface		[4];
};

struct _ParrilladaScsiProfileDesc {
	uchar number		[2];

	uchar currentp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _ParrilladaScsiMorphingDesc {
	uchar async		:1;
	uchar op_chge_event	:1;
	uchar reserved0		:6;

	uchar reserved1		[3];
};

struct _ParrilladaScsiMediumDesc {
	uchar lock		:1;
	uchar reserved		:1;
	uchar prevent_jmp	:1;
	uchar eject		:1;
	uchar reserved1		:1;
	uchar loading_mech	:3;

	uchar reserved2		[3];
};

struct _ParrilladaScsiWrtProtectDesc {
	uchar sswpp		:1;
	uchar spwp		:1;
	uchar wdcb		:1;
	uchar dwp		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];
};

struct _ParrilladaScsiRandomReadDesc {
	uchar block_size	[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _ParrilladaScsiCDReadDesc {
	uchar cdtext		:1;
	uchar c2flags		:1;
	uchar reserved0		:5;
	uchar dap		:1;

	uchar reserved1		[3];
};

/* MMC5 only otherwise just the header */
struct _ParrilladaScsiDVDReadDesc {
	uchar multi110		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar dual_R		:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _ParrilladaScsiRandomWriteDesc {
	/* MMC4/MMC5 only */
	uchar last_lba		[4];

	uchar block_size	[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _ParrilladaScsiIncrementalWrtDesc {
	uchar block_type	[2];

	uchar buf		:1;
	uchar arsv		:1;		/* MMC5 */
	uchar trio		:1;		/* MMC5 */
	uchar reserved0		:5;

	uchar num_link_sizes;
	uchar links		[0];
};

/* MMC5 only */
struct _ParrilladaScsiFormatDesc {
	uchar cert		:1;
	uchar qcert		:1;
	uchar expand		:1;
	uchar renosa		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];

	uchar rrm		:1;
	uchar reserved2		:7;

	uchar reserved3		[3];
};

struct _ParrilladaScsiDefectMngDesc {
	uchar reserved0		:7;
	uchar ssa		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiWrtOnceDesc {
	uchar lba_size		[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _ParrilladaScsiMRWDesc {
	uchar wrt_CD		:1;
	uchar rd_DVDplus	:1;
	uchar wrt_DVDplus	:1;
	uchar reserved0		:5;

	uchar reserved1		[3];
};

struct _ParrilladaScsiDefectReportDesc {
	uchar drt_dm		:1;
	uchar reserved0		:7;

	uchar dbi_zones_num;
	uchar num_entries	[2];
};

struct _ParrilladaScsiDVDRWplusDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar close		:1;
	uchar quick_start	:1;
	uchar reserved1		:6;

	uchar reserved2		[2];
};

struct _ParrilladaScsiDVDRplusDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiRigidOverwrtDesc {
	uchar blank		:1;
	uchar intermediate	:1;
	uchar dsdr		:1;
	uchar dsdg		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];
};

struct _ParrilladaScsiCDTAODesc {
	uchar RW_subcode	:1;
	uchar CDRW		:1;
	uchar dummy		:1;
	uchar RW_pack		:1;
	uchar RW_raw		:1;
	uchar reserved0		:1;
	uchar buf		:1;
	uchar reserved1		:1;

	uchar reserved2;

	uchar data_type		[2];
};

struct _ParrilladaScsiCDSAODesc {
	uchar rw_sub_chan	:1;
	uchar rw_CD		:1;
	uchar dummy		:1;
	uchar raw		:1;
	uchar raw_multi		:1;
	uchar sao		:1;
	uchar buf		:1;
	uchar reserved		:1;

	uchar max_cue_size	[3];
};

struct _ParrilladaScsiDVDRWlessWrtDesc {
	uchar reserved0		:1;
	uchar rw_DVD		:1;
	uchar dummy		:1;
	uchar dual_layer_r	:1;
	uchar reserved1		:2;
	uchar buf		:1;
	uchar reserved2		:1;

	uchar reserved3		[3];
};

struct _ParrilladaScsiCDRWWrtDesc {
	uchar reserved0;

	uchar sub0		:1;
	uchar sub1		:1;
	uchar sub2		:1;
	uchar sub3		:1;
	uchar sub4		:1;
	uchar sub5		:1;
	uchar sub6		:1;
	uchar sub7		:1;

	uchar reserved1		[2];
};

struct _ParrilladaScsiDVDRWDLDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar close		:1;
	uchar quick_start	:1;
	uchar reserved1		:6;

	uchar reserved2		[2];
};

struct _ParrilladaScsiDVDRDLDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiBDReadDesc {
	uchar reserved		[4];

	uchar class0_RE_v8	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v15	:1;

	uchar class0_RE_v0	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v7	:1;
	
	uchar class1_RE_v8	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v15	:1;
	
	uchar class1_RE_v0	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v7	:1;
	
	uchar class2_RE_v8	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v15	:1;
	
	uchar class2_RE_v0	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v7	:1;
	
	uchar class3_RE_v8	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v15	:1;
	
	uchar class3_RE_v0	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v7	:1;

	uchar class0_R_v8	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v15	:1;
	
	uchar class0_R_v0	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v7	:1;
	
	uchar class1_R_v8	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v15	:1;
	
	uchar class1_R_v0	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v7	:1;
	
	uchar class2_R_v8	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v15	:1;
	
	uchar class2_R_v0	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v7	:1;
	
	uchar class3_R_v8	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v15	:1;
	
	uchar class3_R_v0	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v7	:1;
};

struct _ParrilladaScsiBDWriteDesc {
	uchar reserved		[4];

	uchar class0_RE_v8	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v15	:1;
	
	uchar class0_RE_v0	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v7	:1;
	
	uchar class1_RE_v8	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v15	:1;
	
	uchar class1_RE_v0	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v7	:1;
	
	uchar class2_RE_v8	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v15	:1;
	
	uchar class2_RE_v0	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v7	:1;
	
	uchar class3_RE_v8	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v15	:1;
	
	uchar class3_RE_v0	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v7	:1;

	uchar class0_R_v8	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v15	:1;
	
	uchar class0_R_v0	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v7	:1;
	
	uchar class1_R_v8	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v15	:1;
	
	uchar class1_R_v0	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v7	:1;
	
	uchar class2_R_v8	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v15	:1;
	
	uchar class2_R_v0	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v7	:1;
	
	uchar class3_R_v8	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v15	:1;
	
	uchar class3_R_v0	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v7	:1;
};

struct _ParrilladaScsiHDDVDReadDesc {
	uchar hd_dvd_r		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar hd_dvd_ram	:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _ParrilladaScsiHDDVDWriteDesc {
	uchar hd_dvd_r		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar hd_dvd_ram	:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _ParrilladaScsiHybridDiscDesc {
	uchar ri		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiSmartDesc {
	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiEmbedChngDesc {
	uchar reserved0		:1;
	uchar sdp		:1;
	uchar reserved1		:1;
	uchar scc		:1;
	uchar reserved2		:3;

	uchar reserved3		[2];

	uchar slot_num		:5;
	uchar reserved4		:3;
};

struct _ParrilladaScsiExtAudioPlayDesc {
	uchar separate_vol	:1;
	uchar separate_chnl_mute:1;
	uchar scan_command	:1;
	uchar reserved0		:5;

	uchar reserved1;

	uchar number_vol	[2];
};

struct _ParrilladaScsiFirmwareUpgrDesc {
	uchar m5		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiTimeoutDesc {
	uchar group3		:1;
	uchar reserved0		:7;

	uchar reserved1;
	uchar unit_len		[2];
};

struct _ParrilladaScsiRTStreamDesc {
	uchar stream_wrt	:1;
	uchar wrt_spd		:1;
	uchar mp2a		:1;
	uchar set_cd_spd	:1;
	uchar rd_buf_caps_block	:1;
	uchar reserved0		:3;

	uchar reserved1		[3];
};

struct _ParrilladaScsiAACSDesc {
	uchar bng		:1;
	uchar reserved0		:7;

	uchar block_count;

	uchar agids_num		:4;
	uchar reserved1		:4;

	uchar version;
};

#else

struct _ParrilladaScsiFeatureDesc {
	uchar code		[2];

	uchar current		:1;
	uchar persistent	:1;
	uchar version		:4;
	uchar reserved		:2;

	uchar add_len;
	uchar data		[0];
};

struct _ParrilladaScsiProfileDesc {
	uchar number		[2];

	uchar reserved0		:7;
	uchar currentp		:1;

	uchar reserved1;
};

struct _ParrilladaScsiCoreDescMMC4 {
	uchar reserved0		:6;
	uchar inq2		:1;
	uchar dbe		:1;

  	uchar mmc4		[0];
	uchar reserved1		[3];
};

struct _ParrilladaScsiCoreDescMMC3 {
	uchar interface		[4];
};

struct _ParrilladaScsiMorphingDesc {
	uchar reserved0		:6;
	uchar op_chge_event	:1;
	uchar async		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiMediumDesc {
	uchar loading_mech	:3;
	uchar reserved1		:1;
	uchar eject		:1;
	uchar prevent_jmp	:1;
	uchar reserved		:1;
	uchar lock		:1;

	uchar reserved2		[3];
};

struct _ParrilladaScsiWrtProtectDesc {
	uchar reserved0		:4;
	uchar dwp		:1;
	uchar wdcb		:1;
	uchar spwp		:1;
	uchar sswpp		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiRandomReadDesc {
	uchar block_size	[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _ParrilladaScsiCDReadDesc {
	uchar dap		:1;
	uchar reserved0		:5;
	uchar c2flags		:1;
	uchar cdtext		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiDVDReadDesc {
	uchar reserved0		:7;
	uchar multi110		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar dual_R		:1;

	uchar reserved3;
};

struct _ParrilladaScsiRandomWriteDesc {
	uchar last_lba		[4];
	uchar block_size	[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _ParrilladaScsiIncrementalWrtDesc {
	uchar block_type	[2];

	uchar reserved0		:5;
	uchar trio		:1;
	uchar arsv		:1;
	uchar buf		:1;

	uchar num_link_sizes;
	uchar links;
};

struct _ParrilladaScsiFormatDesc {
	uchar reserved0		:4;
	uchar renosa		:1;
	uchar expand		:1;
	uchar qcert		:1;
	uchar cert		:1;

	uchar reserved1		[3];

	uchar reserved2		:7;
	uchar rrm		:1;

	uchar reserved3		[3];
};

struct _ParrilladaScsiDefectMngDesc {
	uchar ssa		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _ParrilladaScsiWrtOnceDesc {
	uchar lba_size		[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _ParrilladaScsiMRWDesc {
	uchar reserved0		:5;
	uchar wrt_DVDplus	:1;
	uchar rd_DVDplus	:1;
	uchar wrt_CD		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiDefectReportDesc {
	uchar reserved0		:7;
	uchar drt_dm		:1;

	uchar dbi_zones_num;
	uchar num_entries	[2];
};

struct _ParrilladaScsiDVDRWplusDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		:6;
	uchar quick_start	:1;
	uchar close		:1;

	uchar reserved2		[2];
};

struct _ParrilladaScsiDVDRplusDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiRigidOverwrtDesc {
	uchar reserved0		:4;
	uchar dsdg		:1;
	uchar dsdr		:1;
	uchar intermediate	:1;
	uchar blank		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiCDTAODesc {
	uchar reserved1		:1;
	uchar buf		:1;
	uchar reserved0		:1;
	uchar RW_raw		:1;
	uchar RW_pack		:1;
	uchar dummy		:1;
	uchar CDRW		:1;
	uchar RW_subcode	:1;

	uchar reserved2;

	uchar data_type		[2];
};

struct _ParrilladaScsiCDSAODesc {
	uchar reserved		:1;
	uchar buf		:1;
	uchar sao		:1;
	uchar raw_multi		:1;
	uchar raw		:1;
	uchar dummy		:1;
	uchar rw_CD		:1;
	uchar rw_sub_chan	:1;

	uchar max_cue_size	[3];
};

struct _ParrilladaScsiDVDRWlessWrtDesc {
	uchar reserved2		:1;
	uchar buf		:1;
	uchar reserved1		:2;
	uchar dual_layer_r	:1;
	uchar dummy		:1;
	uchar rw_DVD		:1;
	uchar reserved0		:1;

	uchar reserved3		[3];
};

struct _ParrilladaScsiCDRWWrtDesc {
	uchar reserved0;

	uchar sub7		:1;
	uchar sub6		:1;
	uchar sub5		:1;
	uchar sub4		:1;
	uchar sub3		:1;
	uchar sub2		:1;
	uchar sub1		:1;
	uchar sub0		:1;

	uchar reserved1		[2];
};

struct _ParrilladaScsiDVDRWDLDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		:6;
	uchar quick_start	:1;
	uchar close		:1;

	uchar reserved2		[2];
};

struct _ParrilladaScsiDVDRDLDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiBDReadDesc {
	uchar reserved		[4];

	uchar class0_RE_v15	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v8	:1;

	uchar class0_RE_v7	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v0	:1;

	uchar class1_RE_v15	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v8	:1;
	
	uchar class1_RE_v7	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v0	:1;
	
	uchar class2_RE_v15	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v8	:1;
	
	uchar class2_RE_v7	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v0	:1;
	
	uchar class3_RE_v15	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v8	:1;
	
	uchar class3_RE_v7	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v0	:1;

	uchar class0_R_v15	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v8	:1;

	uchar class0_R_v7	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v0	:1;

	uchar class1_R_v15	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v8	:1;
	
	uchar class1_R_v7	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v0	:1;
	
	uchar class2_R_v15	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v8	:1;
	
	uchar class2_R_v7	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v0	:1;
	
	uchar class3_R_v15	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v8	:1;
	
	uchar class3_R_v7	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v0	:1;
};

struct _ParrilladaScsiBDWriteDesc {
	uchar reserved		[4];

	uchar class0_RE_v15	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v8	:1;

	uchar class0_RE_v7	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v0	:1;

	uchar class1_RE_v15	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v8	:1;
	
	uchar class1_RE_v7	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v0	:1;
	
	uchar class2_RE_v15	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v8	:1;
	
	uchar class2_RE_v7	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v0	:1;
	
	uchar class3_RE_v15	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v8	:1;
	
	uchar class3_RE_v7	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v0	:1;

	uchar class0_R_v15	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v8	:1;

	uchar class0_R_v7	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v0	:1;

	uchar class1_R_v15	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v8	:1;
	
	uchar class1_R_v7	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v0	:1;
	
	uchar class2_R_v15	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v8	:1;
	
	uchar class2_R_v7	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v0	:1;
	
	uchar class3_R_v15	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v8	:1;
	
	uchar class3_R_v7	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v0	:1;
};

struct _ParrilladaScsiHDDVDReadDesc {
	uchar reserved0		:7;
	uchar hd_dvd_r		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar hd_dvd_ram	:1;

	uchar reserved3;
};

struct _ParrilladaScsiHDDVDWriteDesc {
	uchar reserved0		:7;
	uchar hd_dvd_r		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar hd_dvd_ram	:1;

	uchar reserved3;
};

struct _ParrilladaScsiHybridDiscDesc {
	uchar reserved0		:7;
	uchar ri		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiSmartDesc {
	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiEmbedChngDesc {
	uchar reserved2		:3;
	uchar scc		:1;
	uchar reserved1		:1;
	uchar sdp		:1;
	uchar reserved0		:1;

	uchar reserved3		[2];

	uchar reserved4		:3;
	uchar slot_num		:5;
};

struct _ParrilladaScsiExtAudioPlayDesc {
	uchar reserved0		:5;
	uchar scan_command	:1;
	uchar separate_chnl_mute:1;
	uchar separate_vol	:1;

	uchar reserved1;

	uchar number_vol	[2];
};

struct _ParrilladaScsiFirmwareUpgrDesc {
	uchar reserved0		:7;
	uchar m5		:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiTimeoutDesc {
	uchar reserved0		:7;
	uchar group3		:1;

	uchar reserved1;
	uchar unit_len		[2];
};

struct _ParrilladaScsiRTStreamDesc {
	uchar reserved0		:3;
	uchar rd_buf_caps_block	:1;
	uchar set_cd_spd	:1;
	uchar mp2a		:1;
	uchar wrt_spd		:1;
	uchar stream_wrt	:1;

	uchar reserved1		[3];
};

struct _ParrilladaScsiAACSDesc {
	uchar reserved0		:7;
	uchar bng		:1;

	uchar block_count;

	uchar reserved1		:4;
	uchar agids_num		:4;

	uchar version;
};

#endif

struct _ParrilladaScsiInterfaceDesc {
	uchar code		[4];
};

struct _ParrilladaScsiCDrwCavDesc {
	uchar reserved		[4];
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _ParrilladaScsiLayerJmpDesc {
	uchar reserved0		[3];
	uchar num_link_sizes;
	uchar links		[0];
};

struct _ParrilladaScsiPOWDesc {
	uchar reserved		[4];
};

struct _ParrilladaScsiDVDCssDesc {
	uchar reserved		[3];
	uchar version;
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _ParrilladaScsiDriveSerialNumDesc {
	uchar serial		[4];
};

struct _ParrilladaScsiMediaSerialNumDesc {
	uchar serial		[4];
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _ParrilladaScsiDiscCtlBlocksDesc {
	uchar entry		[1][4];
};

struct _ParrilladaScsiDVDCprmDesc {
	uchar reserved0 	[3];
	uchar version;
};

struct _ParrilladaScsiFirmwareDesc {
	uchar century		[2];
	uchar year		[2];
	uchar month		[2];
	uchar day		[2];
	uchar hour		[2];
	uchar minute		[2];
	uchar second		[2];
	uchar reserved		[2];
};

struct _ParrilladaScsiVPSDesc {
	uchar reserved		[4];
};

typedef struct _ParrilladaScsiFeatureDesc ParrilladaScsiFeatureDesc;
typedef struct _ParrilladaScsiProfileDesc ParrilladaScsiProfileDesc;
typedef struct _ParrilladaScsiCoreDescMMC3 ParrilladaScsiCoreDescMMC3;
typedef struct _ParrilladaScsiCoreDescMMC4 ParrilladaScsiCoreDescMMC4;
typedef struct _ParrilladaScsiInterfaceDesc ParrilladaScsiInterfaceDesc;
typedef struct _ParrilladaScsiMorphingDesc ParrilladaScsiMorphingDesc;
typedef struct _ParrilladaScsiMediumDesc ParrilladaScsiMediumDesc;
typedef struct _ParrilladaScsiWrtProtectDesc ParrilladaScsiWrtProtectDesc;
typedef struct _ParrilladaScsiRandomReadDesc ParrilladaScsiRandomReadDesc;
typedef struct _ParrilladaScsiCDReadDesc ParrilladaScsiCDReadDesc;
typedef struct _ParrilladaScsiDVDReadDesc ParrilladaScsiDVDReadDesc;
typedef struct _ParrilladaScsiRandomWriteDesc ParrilladaScsiRandomWriteDesc;
typedef struct _ParrilladaScsiIncrementalWrtDesc ParrilladaScsiIncrementalWrtDesc;
typedef struct _ParrilladaScsiFormatDesc ParrilladaScsiFormatDesc;
typedef struct _ParrilladaScsiDefectMngDesc ParrilladaScsiDefectMngDesc;
typedef struct _ParrilladaScsiWrtOnceDesc ParrilladaScsiWrtOnceDesc;
typedef struct _ParrilladaScsiCDrwCavDesc ParrilladaScsiCDrwCavDesc;
typedef struct _ParrilladaScsiMRWDesc ParrilladaScsiMRWDesc;
typedef struct _ParrilladaScsiDefectReportDesc ParrilladaScsiDefectReportDesc;
typedef struct _ParrilladaScsiDVDRWplusDesc ParrilladaScsiDVDRWplusDesc;
typedef struct _ParrilladaScsiDVDRplusDesc ParrilladaScsiDVDRplusDesc;
typedef struct _ParrilladaScsiRigidOverwrtDesc ParrilladaScsiRigidOverwrtDesc;
typedef struct _ParrilladaScsiCDTAODesc ParrilladaScsiCDTAODesc;
typedef struct _ParrilladaScsiCDSAODesc ParrilladaScsiCDSAODesc;
typedef struct _ParrilladaScsiDVDRWlessWrtDesc ParrilladaScsiDVDRWlessWrtDesc;
typedef struct _ParrilladaScsiLayerJmpDesc ParrilladaScsiLayerJmpDesc;
typedef struct _ParrilladaScsiCDRWWrtDesc ParrilladaScsiCDRWWrtDesc;
typedef struct _ParrilladaScsiDVDRWDLDesc ParrilladaScsiDVDRWDLDesc;
typedef struct _ParrilladaScsiDVDRDLDesc ParrilladaScsiDVDRDLDesc;
typedef struct _ParrilladaScsiBDReadDesc ParrilladaScsiBDReadDesc;
typedef struct _ParrilladaScsiBDWriteDesc ParrilladaScsiBDWriteDesc;
typedef struct _ParrilladaScsiHDDVDReadDesc ParrilladaScsiHDDVDReadDesc;
typedef struct _ParrilladaScsiHDDVDWriteDesc ParrilladaScsiHDDVDWriteDesc;
typedef struct _ParrilladaScsiHybridDiscDesc ParrilladaScsiHybridDiscDesc;
typedef struct _ParrilladaScsiSmartDesc ParrilladaScsiSmartDesc;
typedef struct _ParrilladaScsiEmbedChngDesc ParrilladaScsiEmbedChngDesc;
typedef struct _ParrilladaScsiExtAudioPlayDesc ParrilladaScsiExtAudioPlayDesc;
typedef struct _ParrilladaScsiFirmwareUpgrDesc ParrilladaScsiFirmwareUpgrDesc;
typedef struct _ParrilladaScsiTimeoutDesc ParrilladaScsiTimeoutDesc;
typedef struct _ParrilladaScsiRTStreamDesc ParrilladaScsiRTStreamDesc;
typedef struct _ParrilladaScsiAACSDesc ParrilladaScsiAACSDesc;
typedef struct _ParrilladaScsiPOWDesc ParrilladaScsiPOWDesc;
typedef struct _ParrilladaScsiDVDCssDesc ParrilladaScsiDVDCssDesc;
typedef struct _ParrilladaScsiDriveSerialNumDesc ParrilladaScsiDriveSerialNumDesc;
typedef struct _ParrilladaScsiMediaSerialNumDesc ParrilladaScsiMediaSerialNumDesc;
typedef struct _ParrilladaScsiDiscCtlBlocksDesc ParrilladaScsiDiscCtlBlocksDesc;
typedef struct _ParrilladaScsiDVDCprmDesc ParrilladaScsiDVDCprmDesc;
typedef struct _ParrilladaScsiFirmwareDesc ParrilladaScsiFirmwareDesc;
typedef struct _ParrilladaScsiVPSDesc ParrilladaScsiVPSDesc;

struct _ParrilladaScsiGetConfigHdr {
	uchar len			[4];
	uchar reserved			[2];
	uchar current_profile		[2];

	ParrilladaScsiFeatureDesc desc 	[0];
};
typedef struct _ParrilladaScsiGetConfigHdr ParrilladaScsiGetConfigHdr;

G_END_DECLS

#endif /* _SCSI_GET_CONFIGURATION_H */

 

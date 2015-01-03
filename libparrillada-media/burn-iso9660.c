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
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "parrillada-units.h"
#include "parrillada-media.h"
#include "parrillada-media-private.h"
#include "burn-iso9660.h"
#include "burn-iso-field.h"
#include "burn-susp.h"
#include "parrillada-media.h"
#include "burn-volume.h"

struct _ParrilladaIsoCtx {
	gint num_blocks;

	gchar buffer [ISO9660_BLOCK_SIZE];
	gint offset;
	ParrilladaVolSrc *vol;

	gchar *spare_record;

	guint64 data_blocks;
	GError *error;

	guchar susp_skip;

	guint is_root:1;
	guint has_susp:1;
	guint has_RR:1;
};
typedef struct _ParrilladaIsoCtx ParrilladaIsoCtx;

typedef enum {
PARRILLADA_ISO_FILE_EXISTENCE		= 1,
PARRILLADA_ISO_FILE_DIRECTORY		= 1 << 1,
PARRILLADA_ISO_FILE_ASSOCIATED		= 1 << 2,
PARRILLADA_ISO_FILE_RECORD			= 1 << 3,
PARRILLADA_ISO_FILE_PROTECTION		= 1 << 4,
	/* Reserved */
PARRILLADA_ISO_FILE_MULTI_EXTENT_FINAL	= 1 << 7
} ParrilladaIsoFileFlag;

struct _ParrilladaIsoDirRec {
	guchar record_size;
	guchar x_attr_size;
	guchar address			[8];
	guchar file_size		[8];
	guchar date_time		[7];
	guchar flags;
	guchar file_unit;
	guchar gap_size;
	guchar volseq_num		[4];
	guchar id_size;
	gchar id			[0];
};
typedef struct _ParrilladaIsoDirRec ParrilladaIsoDirRec;

struct _ParrilladaIsoPrimary {
	guchar type;
	gchar id			[5];
	guchar version;

	guchar unused_0;

	gchar system_id			[32];
	gchar vol_id			[32];

	guchar unused_1			[8];

	guchar vol_size			[8];

	guchar escapes			[32];
	guchar volset_size		[4];
	guchar sequence_num		[4];
	guchar block_size		[4];
	guchar path_table_size		[8];
	guchar L_table_loc		[4];
	guchar opt_L_table_loc		[4];
	guchar M_table_loc		[4];
	guchar opt_M_table_loc		[4];

	/* the following has a fixed size of 34 bytes */
	ParrilladaIsoDirRec root_rec	[0];

	/* to be continued if needed */
};
typedef struct _ParrilladaIsoPrimary ParrilladaIsoPrimary;

typedef enum {
	PARRILLADA_ISO_OK,
	PARRILLADA_ISO_END,
	PARRILLADA_ISO_ERROR
} ParrilladaIsoResult;

#define ISO9660_BYTES_TO_BLOCKS(size)			PARRILLADA_BYTES_TO_SECTORS ((size), ISO9660_BLOCK_SIZE)

static GList *
parrillada_iso9660_load_directory_records (ParrilladaIsoCtx *ctx,
					ParrilladaVolFile *parent,
					ParrilladaIsoDirRec *record,
					gboolean recursive);

static ParrilladaVolFile *
parrillada_iso9660_lookup_directory_records (ParrilladaIsoCtx *ctx,
					  const gchar *path,
					  gint address);

gboolean
parrillada_iso9660_is_primary_descriptor (const char *buffer,
				       GError **error)
{
	ParrilladaVolDesc *vol;

	/* must be CD001 */
	vol = (ParrilladaVolDesc *) buffer;
	if (memcmp (vol->id, "CD001", 5)) {
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
			     _("It does not appear to be a valid ISO image"));
		return FALSE;
	}

	/* must be "1" for primary volume */
	if (vol->type != 1) {
		g_set_error (error,
			     PARRILLADA_MEDIA_ERROR,
			     PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
			     _("It does not appear to be a valid ISO image"));
		return FALSE;
	}

	return TRUE;
}

gboolean
parrillada_iso9660_get_size (const gchar *block,
			  gint64 *nb_blocks,
			  GError **error)
{
	ParrilladaIsoPrimary *vol;

	/* read the size of the volume */
	vol = (ParrilladaIsoPrimary *) block;
	*nb_blocks = (gint64) parrillada_iso9660_get_733_val (vol->vol_size);

	return TRUE;
}

gboolean
parrillada_iso9660_get_label (const gchar *block,
			   gchar **label,
			   GError **error)
{
	ParrilladaIsoPrimary *vol;

	/* read the identifier */
	vol = (ParrilladaIsoPrimary *) block;
	*label = g_strndup (vol->vol_id, sizeof (vol->vol_id));

	return TRUE;	
}

static ParrilladaIsoResult
parrillada_iso9660_seek (ParrilladaIsoCtx *ctx, gint address)
{
	ctx->offset = 0;
	ctx->num_blocks = 1;

	/* The size of all the records is given by size member and its location
	 * by its address member. In a set of directory records the first two 
	 * records are: '.' (id == 0) and '..' (id == 1). So since we've got
	 * the address of the set load the block. */
	if (PARRILLADA_VOL_SRC_SEEK (ctx->vol, address, SEEK_SET, &(ctx->error)) == -1)
		return PARRILLADA_ISO_ERROR;

	if (!PARRILLADA_VOL_SRC_READ (ctx->vol, ctx->buffer, 1, &(ctx->error)))
		return PARRILLADA_ISO_ERROR;

	return PARRILLADA_ISO_OK;
}

static ParrilladaIsoResult
parrillada_iso9660_next_block (ParrilladaIsoCtx *ctx)
{
	ctx->offset = 0;
	ctx->num_blocks ++;

	if (!PARRILLADA_VOL_SRC_READ (ctx->vol, ctx->buffer, 1, &(ctx->error)))
		return PARRILLADA_ISO_ERROR;

	return PARRILLADA_ISO_OK;
}

static gboolean
parrillada_iso9660_read_susp (ParrilladaIsoCtx *ctx,
			   ParrilladaSuspCtx *susp_ctx,
			   gchar *susp,
			   gint susp_len)
{
	gboolean result = TRUE;
	guint64 current_position = -1;

	memset (susp_ctx, 0, sizeof (ParrilladaSuspCtx));
	if (!parrillada_susp_read (susp_ctx, susp, susp_len)) {
		PARRILLADA_MEDIA_LOG ("Could not read susp area");
		return FALSE;
	}

	while (susp_ctx->CE_address) {
		gchar CE_block [ISO9660_BLOCK_SIZE];
		gint64 seek_res;
		guint32 offset;
		guint32 len;

		PARRILLADA_MEDIA_LOG ("Continuation Area");

		/* we need to move to another block */
		seek_res = PARRILLADA_VOL_SRC_SEEK (ctx->vol, susp_ctx->CE_address, SEEK_SET, NULL);
		if (seek_res == -1) {
			PARRILLADA_MEDIA_LOG ("Could not seek to continuation area");
			result = FALSE;
			break;
		}

		if (current_position == -1)
			current_position = seek_res;

		if (!PARRILLADA_VOL_SRC_READ (ctx->vol, CE_block, 1, NULL)) {
			PARRILLADA_MEDIA_LOG ("Could not get continuation area");
			result = FALSE;
			break;
		}

		offset = susp_ctx->CE_offset;
		len = MIN (susp_ctx->CE_len, sizeof (CE_block) - offset);

		/* reset information about the CE area */
		memset (&susp_ctx->CE_address, 0, sizeof (susp_ctx->CE_address));
		memset (&susp_ctx->CE_offset, 0, sizeof (susp_ctx->CE_offset));
		memset (&susp_ctx->CE_len, 0, sizeof (susp_ctx->CE_len));

		/* read all information contained in the CE area */
		if (!parrillada_susp_read (susp_ctx, CE_block + offset, len)) {
			PARRILLADA_MEDIA_LOG ("Could not read continuation area");
			result = FALSE;
			break;
		}
	}

	/* reset the reading address properly */
	if (current_position != -1
	&&  PARRILLADA_VOL_SRC_SEEK (ctx->vol, current_position, SEEK_SET, NULL) == -1) {
		PARRILLADA_MEDIA_LOG ("Could not rewind to previous position");
		result = FALSE;
	}

	return result;
}

static gchar *
parrillada_iso9660_get_susp (ParrilladaIsoCtx *ctx,
			  ParrilladaIsoDirRec *record,
			  guint *susp_len)
{
	gchar *susp_block;
	gint start;
	gint len;

	start = sizeof (ParrilladaIsoDirRec) + record->id_size;
	/* padding byte when id_size is an even number */
	if (start & 1)
		start ++;

	if (ctx->susp_skip)
		start += ctx->susp_skip;

	/* we don't want to go beyond end of buffer */
	if (start >= record->record_size)
		return NULL;

	len = record->record_size - start;

	if (len <= 0)
		return NULL;

	if (susp_len)
		*susp_len = len;

	susp_block = ((gchar *) record) + start;

	PARRILLADA_MEDIA_LOG ("Got susp block");
	return susp_block;
}

static ParrilladaIsoResult
parrillada_iso9660_next_record (ParrilladaIsoCtx *ctx, ParrilladaIsoDirRec **retval)
{
	ParrilladaIsoDirRec *record;

	if (ctx->offset > sizeof (ctx->buffer)) {
		PARRILLADA_MEDIA_LOG ("Invalid record size");
		goto error;
	}

	if (ctx->offset == sizeof (ctx->buffer)) {
		PARRILLADA_MEDIA_LOG ("No next record");
		return PARRILLADA_ISO_END;
	}

	/* record_size already checked last time function was called */
	record = (ParrilladaIsoDirRec *) (ctx->buffer + ctx->offset);
	if (!record->record_size) {
		PARRILLADA_MEDIA_LOG ("Last record");
		return PARRILLADA_ISO_END;
	}

	if (record->record_size > (sizeof (ctx->buffer) - ctx->offset)) {
		gint part_one, part_two;

		/* This is for cross sector boundary records */
		PARRILLADA_MEDIA_LOG ("Cross sector boundary record");

		/* some implementations write across block boundary which is
		 * "forbidden" by ECMA-119. But linux kernel accepts it, so ...
		 */
/*		ctx->error = g_error_new (PARRILLADA_MEDIA_ERROR,
					  PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
					  _("It does not appear to be a valid ISO image"));
		goto error;
*/
		if (ctx->spare_record)
			g_free (ctx->spare_record);

		ctx->spare_record = g_new0 (gchar, record->record_size);

		part_one = sizeof (ctx->buffer) - ctx->offset;
		part_two = record->record_size - part_one;
		
		memcpy (ctx->spare_record,
			ctx->buffer + ctx->offset,
			part_one);
		
		if (parrillada_iso9660_next_block (ctx) == PARRILLADA_ISO_ERROR)
			goto error;

		memcpy (ctx->spare_record + part_one,
			ctx->buffer,
			part_two);
		ctx->offset = part_two;

		record = (ParrilladaIsoDirRec *) ctx->spare_record;
	}
	else
		ctx->offset += record->record_size;

	*retval = record;
	return PARRILLADA_ISO_OK;

error:
	if (!ctx->error)
		ctx->error = g_error_new (PARRILLADA_MEDIA_ERROR,
					  PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
					  _("It does not appear to be a valid ISO image"));
	return PARRILLADA_ISO_ERROR;
}

static ParrilladaIsoResult
parrillada_iso9660_get_first_directory_record (ParrilladaIsoCtx *ctx,
					    ParrilladaIsoDirRec **record,
					    gint address)
{
	ParrilladaIsoResult result;

	PARRILLADA_MEDIA_LOG ("Reading directory record");

	result = parrillada_iso9660_seek (ctx, address);
	if (result != PARRILLADA_ISO_OK)
		return PARRILLADA_ISO_ERROR;

	/* load "." */
	result = parrillada_iso9660_next_record (ctx, record);
	if (result != PARRILLADA_ISO_OK)
		return PARRILLADA_ISO_ERROR;

	return PARRILLADA_ISO_OK;
}

static ParrilladaVolFile *
parrillada_iso9660_read_file_record (ParrilladaIsoCtx *ctx,
				  ParrilladaIsoDirRec *record,
				  ParrilladaSuspCtx *susp_ctx)
{
	ParrilladaVolFile *file;
	ParrilladaVolFileExtent *extent;

	if (record->id_size > record->record_size - sizeof (ParrilladaIsoDirRec)) {
		PARRILLADA_MEDIA_LOG ("Filename is too long");
		ctx->error = g_error_new (PARRILLADA_MEDIA_ERROR,
					  PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
					  _("It does not appear to be a valid ISO image"));
		return NULL;
	}

	file = g_new0 (ParrilladaVolFile, 1);
	file->isdir = 0;
	file->name = g_new0 (gchar, record->id_size + 1);
	memcpy (file->name, record->id, record->id_size);

	file->specific.file.size_bytes = parrillada_iso9660_get_733_val (record->file_size);

	/* NOTE: a file can be in multiple places */
	extent = g_new (ParrilladaVolFileExtent, 1);
	extent->block = parrillada_iso9660_get_733_val (record->address);
	extent->size = parrillada_iso9660_get_733_val (record->file_size);
	file->specific.file.extents = g_slist_prepend (file->specific.file.extents, extent);

	/* see if we've got a susp area */
	if (!susp_ctx) {
		PARRILLADA_MEDIA_LOG ("New file %s", file->name);
		return file;
	}

	PARRILLADA_MEDIA_LOG ("New file %s with a suspend area", file->name);

	if (susp_ctx->rr_name) {
		PARRILLADA_MEDIA_LOG ("Got a susp (RR) %s", susp_ctx->rr_name);
		file->rr_name = susp_ctx->rr_name;
		susp_ctx->rr_name = NULL;
	}

	return file;
}

static ParrilladaVolFile *
parrillada_iso9660_read_directory_record (ParrilladaIsoCtx *ctx,
				       ParrilladaIsoDirRec *record,
				       gboolean recursive)
{
	gchar *susp;
	gint address;
	guint susp_len = 0;
	ParrilladaSuspCtx susp_ctx;
	ParrilladaVolFile *directory;

	if (record->id_size > record->record_size - sizeof (ParrilladaIsoDirRec)) {
		PARRILLADA_MEDIA_LOG ("Filename is too long");
		ctx->error = g_error_new (PARRILLADA_MEDIA_ERROR,
					  PARRILLADA_MEDIA_ERROR_IMAGE_INVALID,
					  _("It does not appear to be a valid ISO image"));
		return NULL;
	}

	/* create the directory and set information */
	directory = g_new0 (ParrilladaVolFile, 1);
	directory->isdir = TRUE;
	directory->isdir_loaded = FALSE;
	directory->name = g_new0 (gchar, record->id_size + 1);
	memcpy (directory->name, record->id, record->id_size);

	if (ctx->has_susp && ctx->has_RR) {
		/* See if we've got a susp area. Do it now to see if it has a CL
		 * entry. The rest will be checked later after reading contents.
		 */
		susp = parrillada_iso9660_get_susp (ctx, record, &susp_len);
		if (!parrillada_iso9660_read_susp (ctx, &susp_ctx, susp, susp_len)) {
			PARRILLADA_MEDIA_LOG ("Could not read susp area");
			parrillada_volume_file_free (directory);
			return NULL;
		}

		/* look for a "CL" SUSP entry in case the directory was relocated */
		if (susp_ctx.CL_address) {
			PARRILLADA_MEDIA_LOG ("Entry has a CL entry");
			address = susp_ctx.CL_address;
		}
		else
			address = parrillada_iso9660_get_733_val (record->address);

		PARRILLADA_MEDIA_LOG ("New directory %s with susp area", directory->name);

		/* if this directory has a "RE" susp entry then drop it; it's 
		 * not at the right place in the Rock Ridge file hierarchy. It
		 * will probably be skipped */
		if (susp_ctx.has_RE) {
			PARRILLADA_MEDIA_LOG ("Rock Ridge relocated directory. Skipping entry.");
			directory->relocated = TRUE;
		}

		if (susp_ctx.rr_name) {
			PARRILLADA_MEDIA_LOG ("Got a susp (RR) %s", susp_ctx.rr_name);
			directory->rr_name = susp_ctx.rr_name;
			susp_ctx.rr_name = NULL;
		}

		parrillada_susp_ctx_clean (&susp_ctx);
	}
	else
		address = parrillada_iso9660_get_733_val (record->address);

	/* load contents if recursive */
	if (recursive) {
		GList *children;

		parrillada_iso9660_get_first_directory_record (ctx,
							    &record,
							    address);
		children = parrillada_iso9660_load_directory_records (ctx,
								   directory,
								   record,
								   TRUE);
		if (!children && ctx->error) {
			parrillada_volume_file_free (directory);
			if (ctx->has_susp && ctx->has_RR)
				parrillada_susp_ctx_clean (&susp_ctx);

			return NULL;
		}

		directory->isdir_loaded = TRUE;
		directory->specific.dir.children = children;
	}
	else	/* store the address of contents for later use */
		directory->specific.dir.address = address;

	PARRILLADA_MEDIA_LOG ("New directory %s", directory->name);
	return directory;
}

static GList *
parrillada_iso9660_load_directory_records (ParrilladaIsoCtx *ctx,
					ParrilladaVolFile *parent,
					ParrilladaIsoDirRec *record,
					gboolean recursive)
{
	GSList *iter;
	gint max_block;
	gint max_record_size;
	ParrilladaVolFile *entry;
	GList *children = NULL;
	ParrilladaIsoResult result;
	GSList *directories = NULL;

	max_record_size = parrillada_iso9660_get_733_val (record->file_size);
	max_block = ISO9660_BYTES_TO_BLOCKS (max_record_size);
	PARRILLADA_MEDIA_LOG ("Maximum directory record length %i block (= %i bytes)", max_block, max_record_size);

	/* skip ".." */
	result = parrillada_iso9660_next_record (ctx, &record);
	if (result != PARRILLADA_ISO_OK)
		return NULL;

	PARRILLADA_MEDIA_LOG ("Skipped '.' and '..'");

	while (1) {
		ParrilladaIsoResult result;

		result = parrillada_iso9660_next_record (ctx, &record);
		if (result == PARRILLADA_ISO_END) {
			if (ctx->num_blocks >= max_block)
				break;

			result = parrillada_iso9660_next_block (ctx);
			if (result != PARRILLADA_ISO_OK)
				goto error;

			continue;
		}
		else if (result == PARRILLADA_ISO_ERROR)
			goto error;

		if (!record)
			break;

		/* if it's a directory, keep the record for later (we don't 
		 * want to change the reading offset for the moment) */
		if (record->flags & PARRILLADA_ISO_FILE_DIRECTORY) {
			gpointer copy;

			copy = g_new0 (gchar, record->record_size);
			memcpy (copy, record, record->record_size);
			directories = g_slist_prepend (directories, copy);
			continue;
		}

		if (ctx->has_RR) {
			ParrilladaSuspCtx susp_ctx = { NULL, };
			guint susp_len = 0;
			gchar *susp;

			/* See if we've got a susp area. Do it now to see if it
			 * has a CL entry. The rest will be checked later after
			 * reading contents. Otherwise we wouldn't be able to 
			 * get deep directories that are flagged as files. */
			susp = parrillada_iso9660_get_susp (ctx, record, &susp_len);
			if (!parrillada_iso9660_read_susp (ctx, &susp_ctx, susp, susp_len)) {
				PARRILLADA_MEDIA_LOG ("Could not read susp area");
				goto error;
			}

			/* look for a "CL" SUSP entry in case the directory was
			 * relocated. If it has, it's a directory and keep it
			 * for later. */
			if (susp_ctx.CL_address) {
				gpointer copy;

				PARRILLADA_MEDIA_LOG ("Entry has a CL entry, keeping for later");
				copy = g_new0 (gchar, record->record_size);
				memcpy (copy, record, record->record_size);
				directories = g_slist_prepend (directories, copy);

				parrillada_susp_ctx_clean (&susp_ctx);
				memset (&susp_ctx, 0, sizeof (ParrilladaSuspCtx));
				continue;
			}

			entry = parrillada_iso9660_read_file_record (ctx, record, &susp_ctx);
			parrillada_susp_ctx_clean (&susp_ctx);
		}
		else
			entry = parrillada_iso9660_read_file_record (ctx, record, NULL);

		if (!entry)
			goto error;

		entry->parent = parent;

		/* check that we don't have another file record for the
		 * same file (usually files > 4G). It always follows
		 * its sibling */
		if (children) {
			ParrilladaVolFile *last;

			last = children->data;
			if (!last->isdir && !strcmp (PARRILLADA_VOLUME_FILE_NAME (last), PARRILLADA_VOLUME_FILE_NAME (entry))) {
				/* add size and addresses */
				ctx->data_blocks += ISO9660_BYTES_TO_BLOCKS (entry->specific.file.size_bytes);
				last = parrillada_volume_file_merge (last, entry);
				PARRILLADA_MEDIA_LOG ("Multi extent file");
				continue;
			}
		}

		children = g_list_prepend (children, entry);
		ctx->data_blocks += ISO9660_BYTES_TO_BLOCKS (entry->specific.file.size_bytes);
	}

	/* Takes care of the directories: we accumulate them not to change the
	 * offset of file descriptor FILE */
	for (iter = directories; iter; iter = iter->next) {
		record = iter->data;

		entry = parrillada_iso9660_read_directory_record (ctx, record, recursive);
		if (!entry)
			goto error;

		if (entry->relocated) {
			parrillada_volume_file_free (entry);
			continue;
		}

		entry->parent = parent;
		children = g_list_prepend (children, entry);
	}
	g_slist_foreach (directories, (GFunc) g_free, NULL);
	g_slist_free (directories);

	return children;

error:

	g_list_foreach (children, (GFunc) parrillada_volume_file_free, NULL);
	g_list_free (children);

	g_slist_foreach (directories, (GFunc) g_free, NULL);
	g_slist_free (directories);

	return NULL;
}

static gboolean
parrillada_iso9660_check_SUSP_RR_use (ParrilladaIsoCtx *ctx,
				   ParrilladaIsoDirRec *record)
{
	ParrilladaSuspCtx susp_ctx;
	guint susp_len = 0;
	gchar *susp;

	susp = parrillada_iso9660_get_susp (ctx, record, &susp_len);
	parrillada_iso9660_read_susp (ctx, &susp_ctx, susp, susp_len);

	ctx->has_susp = susp_ctx.has_SP;
	ctx->has_RR = susp_ctx.has_RockRidge;
	ctx->susp_skip = susp_ctx.skip;
	ctx->is_root = FALSE;

	if (ctx->has_susp)
		PARRILLADA_MEDIA_LOG ("File system supports system use sharing protocol");

	if (ctx->has_RR)
		PARRILLADA_MEDIA_LOG ("File system has Rock Ridge extension");

	parrillada_susp_ctx_clean (&susp_ctx);
	return TRUE;
}

static void
parrillada_iso9660_ctx_init (ParrilladaIsoCtx *ctx, ParrilladaVolSrc *vol)
{
	memset (ctx, 0, sizeof (ParrilladaIsoCtx));

	ctx->is_root = TRUE;
	ctx->vol = vol;
	ctx->offset = 0;

	/* to fully initialize the context we need the root directory record */
}

ParrilladaVolFile *
parrillada_iso9660_get_contents (ParrilladaVolSrc *vol,
			      const gchar *block,
			      gint64 *data_blocks,
			      GError **error)
{
	ParrilladaIsoPrimary *primary;
	ParrilladaIsoDirRec *record;
	ParrilladaVolFile *volfile;
	ParrilladaIsoDirRec *root;
	ParrilladaIsoCtx ctx;
	GList *children;
	gint address;

	primary = (ParrilladaIsoPrimary *) block;
	root = primary->root_rec;

	/* check settings */
	address = parrillada_iso9660_get_733_val (root->address);
	parrillada_iso9660_ctx_init (&ctx, vol);
	parrillada_iso9660_get_first_directory_record (&ctx, &record, address);
	parrillada_iso9660_check_SUSP_RR_use (&ctx, record);

	/* create volume file */
	volfile = g_new0 (ParrilladaVolFile, 1);
	volfile->isdir = TRUE;
	volfile->isdir_loaded = FALSE;

	children = parrillada_iso9660_load_directory_records (&ctx,
							   volfile,
							   record,
							   TRUE);
	volfile->specific.dir.children = children;

	if (ctx.spare_record)
		g_free (ctx.spare_record);

	if (data_blocks)
		*data_blocks = ctx.data_blocks;

	if (!children && ctx.error) {
		if (error)
			g_propagate_error (error, ctx.error);

		parrillada_volume_file_free (volfile);
		volfile = NULL;
	}

	return volfile;
}

static ParrilladaVolFile *
parrillada_iso9660_lookup_directory_record_RR (ParrilladaIsoCtx *ctx,
					    const gchar *path,
					    guint len,
					    ParrilladaIsoDirRec *record)
{
	ParrilladaVolFile *entry = NULL;
	ParrilladaSuspCtx susp_ctx;
	gchar record_name [256];
	guint susp_len = 0;
	gchar *susp;

	/* See if we've got a susp area. Do it now to see if it
	 * has a CL entry and rr_name. */
	susp = parrillada_iso9660_get_susp (ctx, record, &susp_len);
	if (!parrillada_iso9660_read_susp (ctx, &susp_ctx, susp, susp_len)) {
		PARRILLADA_MEDIA_LOG ("Could not read susp area");
		return NULL;
	}

	/* set name */
	if (!susp_ctx.rr_name) {
		memcpy (record_name, record->id, record->id_size);
		record_name [record->id_size] = '\0';
	}
	else
		strcpy (record_name, susp_ctx.rr_name);

	if (!(record->flags & PARRILLADA_ISO_FILE_DIRECTORY)) {
		if (len) {
			/* Look for a "CL" SUSP entry in case it was
			 * relocated. If it has, it's a directory. */
			if (susp_ctx.CL_address && !strncmp (record_name, path, len)) {
				/* move path forward */
				path += len;
				path ++;

				entry = parrillada_iso9660_lookup_directory_records (ctx,
										  path,
										  susp_ctx.CL_address);
			}
		}
		else if (!strcmp (record_name, path))
			entry = parrillada_iso9660_read_file_record (ctx,
								  record,
								  &susp_ctx);
	}
	else if (len && !strncmp (record_name, path, len)) {
		gint address;

		/* move path forward */
		path += len;
		path ++;

		address = parrillada_iso9660_get_733_val (record->address);
		entry = parrillada_iso9660_lookup_directory_records (ctx,
								  path,
								  address);
	}

	parrillada_susp_ctx_clean (&susp_ctx);
	return entry;
}

static ParrilladaVolFile *
parrillada_iso9660_lookup_directory_record_ISO (ParrilladaIsoCtx *ctx,
					     const gchar *path,
					     guint len,
					     ParrilladaIsoDirRec *record)
{
	ParrilladaVolFile *entry = NULL;

	if (!(record->flags & PARRILLADA_ISO_FILE_DIRECTORY)) {
		if (!len && !strncmp (record->id, path, record->id_size))
			entry = parrillada_iso9660_read_file_record (ctx,
								  record,
								  NULL);
	}
	else if (len && !strncmp (record->id, path, record->id_size)) {
		gint address;

		/* move path forward */
		path += len;
		path ++;

		address = parrillada_iso9660_get_733_val (record->address);
		entry = parrillada_iso9660_lookup_directory_records (ctx,
								  path,
								  address);
	}

	return entry;
}

static ParrilladaVolFile *
parrillada_iso9660_lookup_directory_records (ParrilladaIsoCtx *ctx,
					  const gchar *path,
					  gint address)
{
	guint len;
	gchar *end;
	gint max_block;
	gint max_record_size;
	ParrilladaIsoResult result;
	ParrilladaIsoDirRec *record;
	ParrilladaVolFile *file = NULL;

	PARRILLADA_MEDIA_LOG ("Reading directory record");

	result = parrillada_iso9660_seek (ctx, address);
	if (result != PARRILLADA_ISO_OK)
		return NULL;

	/* "." */
	result = parrillada_iso9660_next_record (ctx, &record);
	if (result != PARRILLADA_ISO_OK)
		return NULL;

	/* Look for "SP" SUSP if it's root directory. Also look for "ER" which
	 * should tell us whether Rock Ridge could be used. */
	if (ctx->is_root) {
		ParrilladaSuspCtx susp_ctx;
		guint susp_len = 0;
		gchar *susp;

		susp = parrillada_iso9660_get_susp (ctx, record, &susp_len);
		parrillada_iso9660_read_susp (ctx, &susp_ctx, susp, susp_len);

		ctx->has_susp = susp_ctx.has_SP;
		ctx->has_RR = susp_ctx.has_RockRidge;
		ctx->susp_skip = susp_ctx.skip;
		ctx->is_root = FALSE;

		parrillada_susp_ctx_clean (&susp_ctx);

		if (ctx->has_susp)
			PARRILLADA_MEDIA_LOG ("File system supports system use sharing protocol");

		if (ctx->has_RR)
			PARRILLADA_MEDIA_LOG ("File system has Rock Ridge extension");
	}

	max_record_size = parrillada_iso9660_get_733_val (record->file_size);
	max_block = ISO9660_BYTES_TO_BLOCKS (max_record_size);
	PARRILLADA_MEDIA_LOG ("Maximum directory record length %i block (= %i bytes)", max_block, max_record_size);

	/* skip ".." */
	result = parrillada_iso9660_next_record (ctx, &record);
	if (result != PARRILLADA_ISO_OK)
		return NULL;

	PARRILLADA_MEDIA_LOG ("Skipped '.' and '..'");

	end = strchr (path, '/');
	if (!end)
		/* reached the final file */
		len = 0;
	else
		len = end - path;

	while (1) {
		ParrilladaIsoResult result;
		ParrilladaVolFile *entry;

		result = parrillada_iso9660_next_record (ctx, &record);
		if (result == PARRILLADA_ISO_END) {
			if (ctx->num_blocks >= max_block) {
				PARRILLADA_MEDIA_LOG ("Reached the end of directory record");
				break;
			}

			result = parrillada_iso9660_next_block (ctx);
			if (result != PARRILLADA_ISO_OK) {
				PARRILLADA_MEDIA_LOG ("Failed to load next block");
				return NULL;
			}

			continue;
		}
		else if (result == PARRILLADA_ISO_ERROR) {
			PARRILLADA_MEDIA_LOG ("Error retrieving next record");
			return NULL;
		}

		if (!record) {
			PARRILLADA_MEDIA_LOG ("No record !!!");
			break;
		}

		if (ctx->has_RR)
			entry = parrillada_iso9660_lookup_directory_record_RR (ctx,
									    path,
									    len,
									    record);
		else
			entry = parrillada_iso9660_lookup_directory_record_ISO (ctx,
									     path,
									     len,
									     record);

		if (!entry)
			continue;

		if (file) {
			/* add size and addresses */
			file = parrillada_volume_file_merge (file, entry);
			PARRILLADA_MEDIA_LOG ("Multi extent file");
		}
		else
			file = entry;

		/* carry on in case that's a multi extent file */
	}

	return file;
}

ParrilladaVolFile *
parrillada_iso9660_get_file (ParrilladaVolSrc *vol,
			  const gchar *path,
			  const gchar *block,
			  GError **error)
{
	ParrilladaIsoPrimary *primary;
	ParrilladaIsoDirRec *root;
	ParrilladaVolFile *entry;
	ParrilladaIsoCtx ctx;
	gint address;

	primary = (ParrilladaIsoPrimary *) block;
	root = primary->root_rec;

	address = parrillada_iso9660_get_733_val (root->address);
	parrillada_iso9660_ctx_init (&ctx, vol);

	/* now that we have root block address, skip first "/" and go. */
	path ++;
	entry = parrillada_iso9660_lookup_directory_records (&ctx,
							  path,
							  address);

	/* clean context */
	if (ctx.spare_record)
		g_free (ctx.spare_record);

	if (error && ctx.error)
		g_propagate_error (error, ctx.error);

	return entry;
}

GList *
parrillada_iso9660_get_directory_contents (ParrilladaVolSrc *vol,
					const gchar *vol_desc,
					gint address,
					GError **error)
{
	ParrilladaIsoDirRec *record = NULL;
	ParrilladaIsoPrimary *primary;
	ParrilladaIsoDirRec *root;
	ParrilladaIsoCtx ctx;
	GList *children;

	/* Check root "." for use of RR and things like that */
	primary = (ParrilladaIsoPrimary *) vol_desc;
	root = primary->root_rec;

	parrillada_iso9660_ctx_init (&ctx, vol);
	parrillada_iso9660_get_first_directory_record (&ctx,
						    &record,
						    parrillada_iso9660_get_733_val (root->address));
	parrillada_iso9660_check_SUSP_RR_use (&ctx, record);

	/* Seek up to the contents of the directory */
	if (address > 0)
		parrillada_iso9660_get_first_directory_record (&ctx,
							    &record,
							    address);

	/* load */
	children = parrillada_iso9660_load_directory_records (&ctx,
							   NULL,
							   record,
							   FALSE);
	if (ctx.error && error)
		g_propagate_error (error, ctx.error);

	return children;
}

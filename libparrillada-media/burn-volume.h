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

#ifndef BURN_VOLUME_H
#define BURN_VOLUME_H

G_BEGIN_DECLS

#include "burn-volume-source.h"

struct _ParrilladaVolDesc {
	guchar type;
	gchar id			[5];
	guchar version;
};
typedef struct _ParrilladaVolDesc ParrilladaVolDesc;

struct _ParrilladaVolFileExtent {
	guint block;
	guint size;
};
typedef struct _ParrilladaVolFileExtent ParrilladaVolFileExtent;

typedef struct _ParrilladaVolFile ParrilladaVolFile;
struct _ParrilladaVolFile {
	ParrilladaVolFile *parent;

	gchar *name;
	gchar *rr_name;

	union {

	struct {
		GSList *extents;
		guint64 size_bytes;
	} file;

	struct {
		GList *children;
		guint address;
	} dir;

	} specific;

	guint isdir:1;
	guint isdir_loaded:1;

	/* mainly used internally */
	guint has_RR:1;
	guint relocated:1;
};

gboolean
parrillada_volume_is_valid (ParrilladaVolSrc *src,
			 GError **error);

gboolean
parrillada_volume_get_size (ParrilladaVolSrc *src,
			 gint64 block,
			 gint64 *nb_blocks,
			 GError **error);

ParrilladaVolFile *
parrillada_volume_get_files (ParrilladaVolSrc *src,
			  gint64 block,
			  gchar **label,
			  gint64 *nb_blocks,
			  gint64 *data_blocks,
			  GError **error);

ParrilladaVolFile *
parrillada_volume_get_file (ParrilladaVolSrc *src,
			 const gchar *path,
			 gint64 volume_start_block,
			 GError **error);

GList *
parrillada_volume_load_directory_contents (ParrilladaVolSrc *vol,
					gint64 session_block,
					gint64 block,
					GError **error);


#define PARRILLADA_VOLUME_FILE_NAME(file)			((file)->rr_name?(file)->rr_name:(file)->name)
#define PARRILLADA_VOLUME_FILE_SIZE(file)			((file)->isdir?0:(file)->specific.file.size_bytes)

void
parrillada_volume_file_free (ParrilladaVolFile *file);

gchar *
parrillada_volume_file_to_path (ParrilladaVolFile *file);

ParrilladaVolFile *
parrillada_volume_file_from_path (const gchar *ptr,
			       ParrilladaVolFile *parent);

gint64
parrillada_volume_file_size (ParrilladaVolFile *file);

ParrilladaVolFile *
parrillada_volume_file_merge (ParrilladaVolFile *file1,
			   ParrilladaVolFile *file2);

G_END_DECLS

#endif /* BURN_VOLUME_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-misc is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_IO_H_
#define _PARRILLADA_IO_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-async-task-manager.h"

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_IO_INFO_NONE			= 0,
	PARRILLADA_IO_INFO_MIME			= 1,
	PARRILLADA_IO_INFO_ICON			= 1,
	PARRILLADA_IO_INFO_PERM			= 1 << 1,
	PARRILLADA_IO_INFO_METADATA		= 1 << 2,
	PARRILLADA_IO_INFO_METADATA_THUMBNAIL	= 1 << 3,
	PARRILLADA_IO_INFO_RECURSIVE		= 1 << 4,
	PARRILLADA_IO_INFO_CHECK_PARENT_SYMLINK	= 1 << 5,
	PARRILLADA_IO_INFO_METADATA_MISSING_CODEC	= 1 << 6,

	PARRILLADA_IO_INFO_FOLLOW_SYMLINK		= 1 << 7,

	PARRILLADA_IO_INFO_URGENT			= 1 << 9,
	PARRILLADA_IO_INFO_IDLE			= 1 << 10
} ParrilladaIOFlags;


typedef enum {
	PARRILLADA_IO_PHASE_START		= 0,
	PARRILLADA_IO_PHASE_DOWNLOAD,
	PARRILLADA_IO_PHASE_END
} ParrilladaIOPhase;

#define PARRILLADA_IO_XFER_DESTINATION	"xfer::destination"

#define PARRILLADA_IO_PLAYLIST_TITLE	"playlist::title"
#define PARRILLADA_IO_IS_PLAYLIST		"playlist::is_playlist"
#define PARRILLADA_IO_PLAYLIST_ENTRIES_NUM	"playlist::entries_num"

#define PARRILLADA_IO_COUNT_NUM		"count::num"
#define PARRILLADA_IO_COUNT_SIZE		"count::size"
#define PARRILLADA_IO_COUNT_INVALID	"count::invalid"

#define PARRILLADA_IO_THUMBNAIL		"metadata::thumbnail"

#define PARRILLADA_IO_LEN			"metadata::length"
#define PARRILLADA_IO_ISRC			"metadata::isrc"
#define PARRILLADA_IO_TITLE		"metadata::title"
#define PARRILLADA_IO_ARTIST		"metadata::artist"
#define PARRILLADA_IO_ALBUM		"metadata::album"
#define PARRILLADA_IO_GENRE		"metadata::genre"
#define PARRILLADA_IO_COMPOSER		"metadata::composer"
#define PARRILLADA_IO_HAS_AUDIO		"metadata::has_audio"
#define PARRILLADA_IO_HAS_VIDEO		"metadata::has_video"
#define PARRILLADA_IO_IS_SEEKABLE		"metadata::is_seekable"

#define PARRILLADA_IO_HAS_DTS			"metadata::audio::wav::has_dts"

#define PARRILLADA_IO_CHANNELS		"metadata::audio::channels"
#define PARRILLADA_IO_RATE				"metadata::audio::rate"

#define PARRILLADA_IO_DIR_CONTENTS_ADDR	"image::directory::address"

typedef struct _ParrilladaIOJobProgress ParrilladaIOJobProgress;

typedef void		(*ParrilladaIOResultCallback)	(GObject *object,
							 GError *error,
							 const gchar *uri,
							 GFileInfo *info,
							 gpointer callback_data);

typedef void		(*ParrilladaIOProgressCallback)	(GObject *object,
							 ParrilladaIOJobProgress *info,
							 gpointer callback_data);

typedef void		(*ParrilladaIODestroyCallback)	(GObject *object,
							 gboolean cancel,
							 gpointer callback_data);

typedef gboolean	(*ParrilladaIOCompareCallback)	(gpointer data,
							 gpointer user_data);


struct _ParrilladaIOJobCallbacks {
	ParrilladaIOResultCallback callback;
	ParrilladaIODestroyCallback destroy;
	ParrilladaIOProgressCallback progress;

	guint ref;

	/* Whether we are returning something for this base */
	guint in_use:1;
};
typedef struct _ParrilladaIOJobCallbacks ParrilladaIOJobCallbacks;

struct _ParrilladaIOJobBase {
	GObject *object;
	ParrilladaIOJobCallbacks *methods;
};
typedef struct _ParrilladaIOJobBase ParrilladaIOJobBase;

struct _ParrilladaIOResultCallbackData {
	gpointer callback_data;
	gint ref;
};
typedef struct _ParrilladaIOResultCallbackData ParrilladaIOResultCallbackData;

struct _ParrilladaIOJob {
	gchar *uri;
	ParrilladaIOFlags options;

	const ParrilladaIOJobBase *base;
	ParrilladaIOResultCallbackData *callback_data;
};
typedef struct _ParrilladaIOJob ParrilladaIOJob;

#define PARRILLADA_IO_JOB(data)	((ParrilladaIOJob *) (data))

void
parrillada_io_job_free (gboolean cancelled,
		     ParrilladaIOJob *job);

void
parrillada_io_set_job (ParrilladaIOJob *self,
		    const ParrilladaIOJobBase *base,
		    const gchar *uri,
		    ParrilladaIOFlags options,
		    ParrilladaIOResultCallbackData *callback_data);

void
parrillada_io_push_job (ParrilladaIOJob *job,
		     const ParrilladaAsyncTaskType *type);

void
parrillada_io_return_result (const ParrilladaIOJobBase *base,
			  const gchar *uri,
			  GFileInfo *info,
			  GError *error,
			  ParrilladaIOResultCallbackData *callback_data);


typedef GtkWindow *	(* ParrilladaIOGetParentWinCb)	(gpointer user_data);

void
parrillada_io_set_parent_window_callback (ParrilladaIOGetParentWinCb callback,
                                       gpointer user_data);

void
parrillada_io_shutdown (void);

/* NOTE: The split in methods and objects was
 * done to prevent jobs sharing the same methods
 * to return their results concurently. In other
 * words only one job among those sharing the
 * same methods can return its results. */
 
ParrilladaIOJobBase *
parrillada_io_register (GObject *object,
		     ParrilladaIOResultCallback callback,
		     ParrilladaIODestroyCallback destroy,
		     ParrilladaIOProgressCallback progress);

ParrilladaIOJobBase *
parrillada_io_register_with_methods (GObject *object,
                                  ParrilladaIOJobCallbacks *methods);

ParrilladaIOJobCallbacks *
parrillada_io_register_job_methods (ParrilladaIOResultCallback callback,
                                 ParrilladaIODestroyCallback destroy,
                                 ParrilladaIOProgressCallback progress);

void
parrillada_io_job_base_free (ParrilladaIOJobBase *base);

void
parrillada_io_cancel_by_base (ParrilladaIOJobBase *base);

void
parrillada_io_find_urgent (const ParrilladaIOJobBase *base,
			ParrilladaIOCompareCallback callback,
			gpointer callback_data);			

void
parrillada_io_load_directory (const gchar *uri,
			   const ParrilladaIOJobBase *base,
			   ParrilladaIOFlags options,
			   gpointer callback_data);
void
parrillada_io_get_file_info (const gchar *uri,
			  const ParrilladaIOJobBase *base,
			  ParrilladaIOFlags options,
			  gpointer callback_data);
void
parrillada_io_get_file_count (GSList *uris,
			   const ParrilladaIOJobBase *base,
			   ParrilladaIOFlags options,
			   gpointer callback_data);
void
parrillada_io_parse_playlist (const gchar *uri,
			   const ParrilladaIOJobBase *base,
			   ParrilladaIOFlags options,
			   gpointer callback_data);

guint64
parrillada_io_job_progress_get_read (ParrilladaIOJobProgress *progress);

guint64
parrillada_io_job_progress_get_total (ParrilladaIOJobProgress *progress);

ParrilladaIOPhase
parrillada_io_job_progress_get_phase (ParrilladaIOJobProgress *progress);

guint
parrillada_io_job_progress_get_file_processed (ParrilladaIOJobProgress *progress);

G_END_DECLS

#endif /* _PARRILLADA_IO_H_ */

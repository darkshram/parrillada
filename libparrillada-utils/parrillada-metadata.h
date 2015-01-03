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

#ifndef _METADATA_H
#define _METADATA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gst/gst.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_METADATA_FLAG_NONE			= 0,
	PARRILLADA_METADATA_FLAG_SILENCES			= 1 << 1,
	PARRILLADA_METADATA_FLAG_MISSING			= 1 << 2,
	PARRILLADA_METADATA_FLAG_THUMBNAIL			= 1 << 3
} ParrilladaMetadataFlag;

#define PARRILLADA_TYPE_METADATA         (parrillada_metadata_get_type ())
#define PARRILLADA_METADATA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_METADATA, ParrilladaMetadata))
#define PARRILLADA_METADATA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_METADATA, ParrilladaMetadataClass))
#define PARRILLADA_IS_METADATA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_METADATA))
#define PARRILLADA_IS_METADATA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_METADATA))
#define PARRILLADA_METADATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_METADATA, ParrilladaMetadataClass))

typedef struct {
	gint64 start;
	gint64 end;
} ParrilladaMetadataSilence;

typedef struct {
	gchar *uri;
	gchar *type;
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *genre;
	gchar *composer;
	gchar *musicbrainz_id;
	int isrc;
	guint64 len;

	gint channels;
	gint rate;

	GSList *silences;

	GdkPixbuf *snapshot;

	guint is_seekable:1;
	guint has_audio:1;
	guint has_video:1;
	guint has_dts:1;
} ParrilladaMetadataInfo;

void
parrillada_metadata_info_copy (ParrilladaMetadataInfo *dest,
			    ParrilladaMetadataInfo *src);
void
parrillada_metadata_info_clear (ParrilladaMetadataInfo *info);
void
parrillada_metadata_info_free (ParrilladaMetadataInfo *info);

typedef struct _ParrilladaMetadataClass ParrilladaMetadataClass;
typedef struct _ParrilladaMetadata ParrilladaMetadata;

struct _ParrilladaMetadataClass {
	GObjectClass parent_class;

	void		(*completed)	(ParrilladaMetadata *meta,
					 const GError *error);
	void		(*progress)	(ParrilladaMetadata *meta,
					 gdouble progress);

};

struct _ParrilladaMetadata {
	GObject parent;
};

GType parrillada_metadata_get_type (void) G_GNUC_CONST;

ParrilladaMetadata *parrillada_metadata_new (void);

gboolean
parrillada_metadata_get_info_async (ParrilladaMetadata *metadata,
				 const gchar *uri,
				 ParrilladaMetadataFlag flags);

void
parrillada_metadata_cancel (ParrilladaMetadata *metadata);

gboolean
parrillada_metadata_set_uri (ParrilladaMetadata *metadata,
			  ParrilladaMetadataFlag flags,
			  const gchar *uri,
			  GError **error);

void
parrillada_metadata_wait (ParrilladaMetadata *metadata,
		       GCancellable *cancel);
void
parrillada_metadata_increase_listener_number (ParrilladaMetadata *metadata);

gboolean
parrillada_metadata_decrease_listener_number (ParrilladaMetadata *metadata);

const gchar *
parrillada_metadata_get_uri (ParrilladaMetadata *metadata);

ParrilladaMetadataFlag
parrillada_metadata_get_flags (ParrilladaMetadata *metadata);

gboolean
parrillada_metadata_get_result (ParrilladaMetadata *metadata,
			     ParrilladaMetadataInfo *info,
			     GError **error);

typedef int	(*ParrilladaMetadataGetXidCb)	(gpointer user_data);

void
parrillada_metadata_set_get_xid_callback (ParrilladaMetadata *metadata,
                                       ParrilladaMetadataGetXidCb callback,
                                       gpointer user_data);
G_END_DECLS

#endif				/* METADATA_H */

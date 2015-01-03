/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-burn is distributed in the hope that it will be useful,
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

#ifndef _BURN_TRACK_H
#define _BURN_TRACK_H

#include <glib.h>
#include <glib-object.h>

#include <parrillada-drive.h>
#include <parrillada-medium.h>

#include <parrillada-enums.h>
#include <parrillada-error.h>
#include <parrillada-status.h>

#include <parrillada-track-type.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TRACK             (parrillada_track_get_type ())
#define PARRILLADA_TRACK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK, ParrilladaTrack))
#define PARRILLADA_TRACK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK, ParrilladaTrackClass))
#define PARRILLADA_IS_TRACK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK))
#define PARRILLADA_IS_TRACK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK))
#define PARRILLADA_TRACK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK, ParrilladaTrackClass))

typedef struct _ParrilladaTrackClass ParrilladaTrackClass;
typedef struct _ParrilladaTrack ParrilladaTrack;

struct _ParrilladaTrackClass
{
	GObjectClass parent_class;

	/* Virtual functions */
	ParrilladaBurnResult	(* get_status)		(ParrilladaTrack *track,
							 ParrilladaStatus *status);

	ParrilladaBurnResult	(* get_size)		(ParrilladaTrack *track,
							 goffset *blocks,
							 goffset *block_size);

	ParrilladaBurnResult	(* get_type)		(ParrilladaTrack *track,
							 ParrilladaTrackType *type);

	/* Signals */
	void			(* changed)		(ParrilladaTrack *track);
};

struct _ParrilladaTrack
{
	GObject parent_instance;
};

GType parrillada_track_get_type (void) G_GNUC_CONST;

void
parrillada_track_changed (ParrilladaTrack *track);



ParrilladaBurnResult
parrillada_track_get_size (ParrilladaTrack *track,
			goffset *blocks,
			goffset *bytes);

ParrilladaBurnResult
parrillada_track_get_track_type (ParrilladaTrack *track,
			      ParrilladaTrackType *type);

ParrilladaBurnResult
parrillada_track_get_status (ParrilladaTrack *track,
			  ParrilladaStatus *status);


/** 
 * Checksums
 */

typedef enum {
	PARRILLADA_CHECKSUM_NONE			= 0,
	PARRILLADA_CHECKSUM_DETECT			= 1,		/* means the plugin handles detection of checksum type */
	PARRILLADA_CHECKSUM_MD5			= 1 << 1,
	PARRILLADA_CHECKSUM_MD5_FILE		= 1 << 2,
	PARRILLADA_CHECKSUM_SHA1			= 1 << 3,
	PARRILLADA_CHECKSUM_SHA1_FILE		= 1 << 4,
	PARRILLADA_CHECKSUM_SHA256			= 1 << 5,
	PARRILLADA_CHECKSUM_SHA256_FILE		= 1 << 6,
} ParrilladaChecksumType;

ParrilladaBurnResult
parrillada_track_set_checksum (ParrilladaTrack *track,
			    ParrilladaChecksumType type,
			    const gchar *checksum);

const gchar *
parrillada_track_get_checksum (ParrilladaTrack *track);

ParrilladaChecksumType
parrillada_track_get_checksum_type (ParrilladaTrack *track);

ParrilladaBurnResult
parrillada_track_tag_add (ParrilladaTrack *track,
		       const gchar *tag,
		       GValue *value);

ParrilladaBurnResult
parrillada_track_tag_lookup (ParrilladaTrack *track,
			  const gchar *tag,
			  GValue **value);

void
parrillada_track_tag_copy_missing (ParrilladaTrack *dest,
				ParrilladaTrack *src);

/**
 * Convenience functions for tags
 */

ParrilladaBurnResult
parrillada_track_tag_add_string (ParrilladaTrack *track,
			      const gchar *tag,
			      const gchar *string);

const gchar *
parrillada_track_tag_lookup_string (ParrilladaTrack *track,
				 const gchar *tag);

ParrilladaBurnResult
parrillada_track_tag_add_int (ParrilladaTrack *track,
			   const gchar *tag,
			   int value);

int
parrillada_track_tag_lookup_int (ParrilladaTrack *track,
			      const gchar *tag);

G_END_DECLS

#endif /* _BURN_TRACK_H */

 

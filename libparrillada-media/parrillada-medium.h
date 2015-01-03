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

#include <glib-object.h>

#include <parrillada-media.h>

#ifndef _BURN_MEDIUM_H_
#define _BURN_MEDIUM_H_

G_BEGIN_DECLS

#define PARRILLADA_TYPE_MEDIUM             (parrillada_medium_get_type ())
#define PARRILLADA_MEDIUM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_MEDIUM, ParrilladaMedium))
#define PARRILLADA_MEDIUM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_MEDIUM, ParrilladaMediumClass))
#define PARRILLADA_IS_MEDIUM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_MEDIUM))
#define PARRILLADA_IS_MEDIUM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_MEDIUM))
#define PARRILLADA_MEDIUM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_MEDIUM, ParrilladaMediumClass))

typedef struct _ParrilladaMediumClass ParrilladaMediumClass;

/**
 * ParrilladaMedium:
 *
 * Represents a physical medium currently inserted in a #ParrilladaDrive.
 **/
typedef struct _ParrilladaMedium ParrilladaMedium;

/**
 * ParrilladaDrive:
 *
 * Represents a physical drive currently connected.
 **/
typedef struct _ParrilladaDrive ParrilladaDrive;

struct _ParrilladaMediumClass
{
	GObjectClass parent_class;
};

struct _ParrilladaMedium
{
	GObject parent_instance;
};

GType parrillada_medium_get_type (void) G_GNUC_CONST;


ParrilladaMedia
parrillada_medium_get_status (ParrilladaMedium *medium);

guint64
parrillada_medium_get_max_write_speed (ParrilladaMedium *medium);

guint64 *
parrillada_medium_get_write_speeds (ParrilladaMedium *medium);

void
parrillada_medium_get_free_space (ParrilladaMedium *medium,
			       goffset *bytes,
			       goffset *blocks);

void
parrillada_medium_get_capacity (ParrilladaMedium *medium,
			     goffset *bytes,
			     goffset *blocks);

void
parrillada_medium_get_data_size (ParrilladaMedium *medium,
			      goffset *bytes,
			      goffset *blocks);

gint64
parrillada_medium_get_next_writable_address (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_be_rewritten (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_be_written (ParrilladaMedium *medium);

const gchar *
parrillada_medium_get_CD_TEXT_title (ParrilladaMedium *medium);

const gchar *
parrillada_medium_get_type_string (ParrilladaMedium *medium);

gchar *
parrillada_medium_get_tooltip (ParrilladaMedium *medium);

ParrilladaDrive *
parrillada_medium_get_drive (ParrilladaMedium *medium);

guint
parrillada_medium_get_track_num (ParrilladaMedium *medium);

gboolean
parrillada_medium_get_last_data_track_space (ParrilladaMedium *medium,
					  goffset *bytes,
					  goffset *sectors);

gboolean
parrillada_medium_get_last_data_track_address (ParrilladaMedium *medium,
					    goffset *bytes,
					    goffset *sectors);

gboolean
parrillada_medium_get_track_space (ParrilladaMedium *medium,
				guint num,
				goffset *bytes,
				goffset *sectors);

gboolean
parrillada_medium_get_track_address (ParrilladaMedium *medium,
				  guint num,
				  goffset *bytes,
				  goffset *sectors);

gboolean
parrillada_medium_can_use_dummy_for_sao (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_use_dummy_for_tao (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_use_burnfree (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_use_sao (ParrilladaMedium *medium);

gboolean
parrillada_medium_can_use_tao (ParrilladaMedium *medium);

G_END_DECLS

#endif /* _BURN_MEDIUM_H_ */

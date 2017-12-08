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

#ifndef _PARRILLADA_TRACK_DATA_H_
#define _PARRILLADA_TRACK_DATA_H_

#include <glib-object.h>

#include <parrillada-track.h>

G_BEGIN_DECLS

/**
 * ParrilladaGraftPt:
 * @uri: a URI
 * @path: a file path
 *
 * A pair of strings describing:
 * @uri the actual current location of the file
 * @path the path of the file on the future ISO9660/UDF/... filesystem
 **/
typedef struct _ParrilladaGraftPt {
	gchar *uri;
	gchar *path;
} ParrilladaGraftPt;

void
parrillada_graft_point_free (ParrilladaGraftPt *graft);

ParrilladaGraftPt *
parrillada_graft_point_copy (ParrilladaGraftPt *graft);


#define PARRILLADA_TYPE_TRACK_DATA             (parrillada_track_data_get_type ())
#define PARRILLADA_TRACK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TRACK_DATA, ParrilladaTrackData))
#define PARRILLADA_TRACK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TRACK_DATA, ParrilladaTrackDataClass))
#define PARRILLADA_IS_TRACK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TRACK_DATA))
#define PARRILLADA_IS_TRACK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TRACK_DATA))
#define PARRILLADA_TRACK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TRACK_DATA, ParrilladaTrackDataClass))

typedef struct _ParrilladaTrackDataClass ParrilladaTrackDataClass;
typedef struct _ParrilladaTrackData ParrilladaTrackData;

struct _ParrilladaTrackDataClass
{
	ParrilladaTrackClass parent_class;

	/* virtual functions */

	/**
	 * set_source:
	 * @track: a #ParrilladaTrackData.
	 * @grafts: (element-type ParrilladaBurn.GraftPt) (transfer full): a #GSList of #ParrilladaGraftPt.
	 * @unreadable: (element-type utf8) (transfer full) (allow-none): a #GSList of URIs (as strings) or %NULL.
	 *
	 * Return value: a #ParrilladaBurnResult
	 **/
	ParrilladaBurnResult	(*set_source)		(ParrilladaTrackData *track,
							 GSList *grafts,
							 GSList *unreadable);
	ParrilladaBurnResult       (*add_fs)		(ParrilladaTrackData *track,
							 ParrilladaImageFS fstype);

	ParrilladaBurnResult       (*rm_fs)		(ParrilladaTrackData *track,
							 ParrilladaImageFS fstype);

	ParrilladaImageFS		(*get_fs)		(ParrilladaTrackData *track);
	GSList*			(*get_grafts)		(ParrilladaTrackData *track);
	GSList*			(*get_excluded)		(ParrilladaTrackData *track);
	guint64			(*get_file_num)		(ParrilladaTrackData *track);
};

struct _ParrilladaTrackData
{
	ParrilladaTrack parent_instance;
};

GType parrillada_track_data_get_type (void) G_GNUC_CONST;

ParrilladaTrackData *
parrillada_track_data_new (void);

ParrilladaBurnResult
parrillada_track_data_set_source (ParrilladaTrackData *track,
			       GSList *grafts,
			       GSList *unreadable);

ParrilladaBurnResult
parrillada_track_data_add_fs (ParrilladaTrackData *track,
			   ParrilladaImageFS fstype);

ParrilladaBurnResult
parrillada_track_data_rm_fs (ParrilladaTrackData *track,
			  ParrilladaImageFS fstype);

ParrilladaBurnResult
parrillada_track_data_set_data_blocks (ParrilladaTrackData *track,
				    goffset blocks);

ParrilladaBurnResult
parrillada_track_data_set_file_num (ParrilladaTrackData *track,
				 guint64 number);

GSList *
parrillada_track_data_get_grafts (ParrilladaTrackData *track);

GSList *
parrillada_track_data_get_excluded_list (ParrilladaTrackData *track);

ParrilladaBurnResult
parrillada_track_data_write_to_paths (ParrilladaTrackData *track,
                                   const gchar *grafts_path,
                                   const gchar *excluded_path,
                                   const gchar *emptydir,
                                   const gchar *videodir,
                                   GError **error);

ParrilladaBurnResult
parrillada_track_data_get_file_num (ParrilladaTrackData *track,
				 guint64 *file_num);

ParrilladaImageFS
parrillada_track_data_get_fs (ParrilladaTrackData *track);

G_END_DECLS

#endif /* _PARRILLADA_TRACK_DATA_H_ */

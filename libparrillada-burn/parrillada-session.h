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

#ifndef BURN_SESSION_H
#define BURN_SESSION_H

#include <glib.h>
#include <glib-object.h>

#include <parrillada-drive.h>

#include <parrillada-error.h>
#include <parrillada-status.h>
#include <parrillada-track.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_BURN_SESSION         (parrillada_burn_session_get_type ())
#define PARRILLADA_BURN_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_BURN_SESSION, ParrilladaBurnSession))
#define PARRILLADA_BURN_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_BURN_SESSION, ParrilladaBurnSessionClass))
#define PARRILLADA_IS_BURN_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_BURN_SESSION))
#define PARRILLADA_IS_BURN_SESSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_BURN_SESSION))
#define PARRILLADA_BURN_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_BURN_SESSION, ParrilladaBurnSessionClass))

typedef struct _ParrilladaBurnSession ParrilladaBurnSession;
typedef struct _ParrilladaBurnSessionClass ParrilladaBurnSessionClass;

struct _ParrilladaBurnSession {
	GObject parent;
};

struct _ParrilladaBurnSessionClass {
	GObjectClass parent_class;

	/** Virtual functions **/
	ParrilladaBurnResult	(*set_output_image)	(ParrilladaBurnSession *session,
							 ParrilladaImageFormat format,
							 const gchar *image,
							 const gchar *toc);
	ParrilladaBurnResult	(*get_output_path)	(ParrilladaBurnSession *session,
							 gchar **image,
							 gchar **toc);
	ParrilladaImageFormat	(*get_output_format)	(ParrilladaBurnSession *session);

	/** Signals **/
	void			(*tag_changed)		(ParrilladaBurnSession *session,
					                 const gchar *tag);
	void			(*track_added)		(ParrilladaBurnSession *session,
							 ParrilladaTrack *track);
	void			(*track_removed)	(ParrilladaBurnSession *session,
							 ParrilladaTrack *track,
							 guint former_position);
	void			(*track_changed)	(ParrilladaBurnSession *session,
							 ParrilladaTrack *track);
	void			(*output_changed)	(ParrilladaBurnSession *session,
							 ParrilladaMedium *former_medium);
};

GType parrillada_burn_session_get_type (void);

ParrilladaBurnSession *parrillada_burn_session_new (void);


/**
 * Used to manage tracks for input
 */

ParrilladaBurnResult
parrillada_burn_session_add_track (ParrilladaBurnSession *session,
				ParrilladaTrack *new_track,
				ParrilladaTrack *sibling);

ParrilladaBurnResult
parrillada_burn_session_move_track (ParrilladaBurnSession *session,
				 ParrilladaTrack *track,
				 ParrilladaTrack *sibling);

ParrilladaBurnResult
parrillada_burn_session_remove_track (ParrilladaBurnSession *session,
				   ParrilladaTrack *track);

GSList *
parrillada_burn_session_get_tracks (ParrilladaBurnSession *session);

/**
 * Get some information about the session
 */

ParrilladaBurnResult
parrillada_burn_session_get_status (ParrilladaBurnSession *session,
				 ParrilladaStatus *status);

ParrilladaBurnResult
parrillada_burn_session_get_size (ParrilladaBurnSession *session,
			       goffset *blocks,
			       goffset *bytes);

ParrilladaBurnResult
parrillada_burn_session_get_input_type (ParrilladaBurnSession *session,
				     ParrilladaTrackType *type);

/**
 * This is to set additional arbitrary information
 */

ParrilladaBurnResult
parrillada_burn_session_tag_lookup (ParrilladaBurnSession *session,
				 const gchar *tag,
				 GValue **value);

ParrilladaBurnResult
parrillada_burn_session_tag_add (ParrilladaBurnSession *session,
			      const gchar *tag,
			      GValue *value);

ParrilladaBurnResult
parrillada_burn_session_tag_remove (ParrilladaBurnSession *session,
				 const gchar *tag);

ParrilladaBurnResult
parrillada_burn_session_tag_add_int (ParrilladaBurnSession *self,
                                  const gchar *tag,
                                  gint value);
gint
parrillada_burn_session_tag_lookup_int (ParrilladaBurnSession *self,
                                     const gchar *tag);

/**
 * Destination 
 */
ParrilladaBurnResult
parrillada_burn_session_get_output_type (ParrilladaBurnSession *self,
                                      ParrilladaTrackType *output);

ParrilladaDrive *
parrillada_burn_session_get_burner (ParrilladaBurnSession *session);

void
parrillada_burn_session_set_burner (ParrilladaBurnSession *session,
				 ParrilladaDrive *drive);

ParrilladaBurnResult
parrillada_burn_session_set_image_output_full (ParrilladaBurnSession *session,
					    ParrilladaImageFormat format,
					    const gchar *image,
					    const gchar *toc);

ParrilladaBurnResult
parrillada_burn_session_get_output (ParrilladaBurnSession *session,
				 gchar **image,
				 gchar **toc);

ParrilladaBurnResult
parrillada_burn_session_set_image_output_format (ParrilladaBurnSession *self,
					    ParrilladaImageFormat format);

ParrilladaImageFormat
parrillada_burn_session_get_output_format (ParrilladaBurnSession *session);

const gchar *
parrillada_burn_session_get_label (ParrilladaBurnSession *session);

void
parrillada_burn_session_set_label (ParrilladaBurnSession *session,
				const gchar *label);

ParrilladaBurnResult
parrillada_burn_session_set_rate (ParrilladaBurnSession *session,
			       guint64 rate);

guint64
parrillada_burn_session_get_rate (ParrilladaBurnSession *session);

/**
 * Session flags
 */

void
parrillada_burn_session_set_flags (ParrilladaBurnSession *session,
			        ParrilladaBurnFlag flags);

void
parrillada_burn_session_add_flag (ParrilladaBurnSession *session,
			       ParrilladaBurnFlag flags);

void
parrillada_burn_session_remove_flag (ParrilladaBurnSession *session,
				  ParrilladaBurnFlag flags);

ParrilladaBurnFlag
parrillada_burn_session_get_flags (ParrilladaBurnSession *session);


/**
 * Used to deal with the temporary files (mostly used by plugins)
 */

ParrilladaBurnResult
parrillada_burn_session_set_tmpdir (ParrilladaBurnSession *session,
				 const gchar *path);
const gchar *
parrillada_burn_session_get_tmpdir (ParrilladaBurnSession *session);

/**
 * Test the supported or compulsory flags for a given session
 */

ParrilladaBurnResult
parrillada_burn_session_get_burn_flags (ParrilladaBurnSession *session,
				     ParrilladaBurnFlag *supported,
				     ParrilladaBurnFlag *compulsory);

ParrilladaBurnResult
parrillada_burn_session_get_blank_flags (ParrilladaBurnSession *session,
				      ParrilladaBurnFlag *supported,
				      ParrilladaBurnFlag *compulsory);

/**
 * Used to test the possibilities offered for a given session
 */

void
parrillada_burn_session_set_strict_support (ParrilladaBurnSession *session,
                                         gboolean strict_check);

gboolean
parrillada_burn_session_get_strict_support (ParrilladaBurnSession *session);

ParrilladaBurnResult
parrillada_burn_session_can_blank (ParrilladaBurnSession *session);

ParrilladaBurnResult
parrillada_burn_session_can_burn (ParrilladaBurnSession *session,
                               gboolean check_flags);

typedef ParrilladaBurnResult	(* ParrilladaForeachPluginErrorCb)	(ParrilladaPluginErrorType type,
		                                                 const gchar *detail,
		                                                 gpointer user_data);

ParrilladaBurnResult
parrillada_session_foreach_plugin_error (ParrilladaBurnSession *session,
                                      ParrilladaForeachPluginErrorCb callback,
                                      gpointer user_data);

ParrilladaBurnResult
parrillada_burn_session_input_supported (ParrilladaBurnSession *session,
				      ParrilladaTrackType *input,
                                      gboolean check_flags);

ParrilladaBurnResult
parrillada_burn_session_output_supported (ParrilladaBurnSession *session,
				       ParrilladaTrackType *output);

ParrilladaMedia
parrillada_burn_session_get_required_media_type (ParrilladaBurnSession *session);

guint
parrillada_burn_session_get_possible_output_formats (ParrilladaBurnSession *session,
						  ParrilladaImageFormat *formats);

ParrilladaImageFormat
parrillada_burn_session_get_default_output_format (ParrilladaBurnSession *session);


G_END_DECLS

#endif /* BURN_SESSION_H */

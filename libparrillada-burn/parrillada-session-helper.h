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

#ifndef _BURN_SESSION_HELPER_H_
#define _BURN_SESSION_HELPER_H_

#include <glib.h>

#include "parrillada-media.h"
#include "parrillada-drive.h"

#include "parrillada-session.h"

G_BEGIN_DECLS


/**
 * Some convenience functions used internally
 */

ParrilladaBurnResult
parrillada_caps_session_get_image_flags (ParrilladaTrackType *input,
                                     ParrilladaTrackType *output,
                                     ParrilladaBurnFlag *supported,
                                     ParrilladaBurnFlag *compulsory);

goffset
parrillada_burn_session_get_available_medium_space (ParrilladaBurnSession *session);

ParrilladaMedia
parrillada_burn_session_get_dest_media (ParrilladaBurnSession *session);

ParrilladaDrive *
parrillada_burn_session_get_src_drive (ParrilladaBurnSession *session);

ParrilladaMedium *
parrillada_burn_session_get_src_medium (ParrilladaBurnSession *session);

gboolean
parrillada_burn_session_is_dest_file (ParrilladaBurnSession *session);

gboolean
parrillada_burn_session_same_src_dest_drive (ParrilladaBurnSession *session);

#define PARRILLADA_BURN_SESSION_EJECT(session)					\
(parrillada_burn_session_get_flags ((session)) & PARRILLADA_BURN_FLAG_EJECT)

#define PARRILLADA_BURN_SESSION_CHECK_SIZE(session)				\
(parrillada_burn_session_get_flags ((session)) & PARRILLADA_BURN_FLAG_CHECK_SIZE)

#define PARRILLADA_BURN_SESSION_NO_TMP_FILE(session)				\
(parrillada_burn_session_get_flags ((session)) & PARRILLADA_BURN_FLAG_NO_TMP_FILES)

#define PARRILLADA_BURN_SESSION_OVERBURN(session)					\
(parrillada_burn_session_get_flags ((session)) & PARRILLADA_BURN_FLAG_OVERBURN)

#define PARRILLADA_BURN_SESSION_APPEND(session)					\
(parrillada_burn_session_get_flags ((session)) & (PARRILLADA_BURN_FLAG_APPEND|PARRILLADA_BURN_FLAG_MERGE))

ParrilladaBurnResult
parrillada_burn_session_get_tmp_image (ParrilladaBurnSession *session,
				    ParrilladaImageFormat format,
				    gchar **image,
				    gchar **toc,
				    GError **error);

ParrilladaBurnResult
parrillada_burn_session_get_tmp_file (ParrilladaBurnSession *session,
				   const gchar *suffix,
				   gchar **path,
				   GError **error);

ParrilladaBurnResult
parrillada_burn_session_get_tmp_dir (ParrilladaBurnSession *session,
				  gchar **path,
				  GError **error);

ParrilladaBurnResult
parrillada_burn_session_get_tmp_image_type_same_src_dest (ParrilladaBurnSession *session,
                                                       ParrilladaTrackType *image_type);

/**
 * This is to log a session
 * (used internally)
 */

const gchar *
parrillada_burn_session_get_log_path (ParrilladaBurnSession *session);

gboolean
parrillada_burn_session_start (ParrilladaBurnSession *session);

void
parrillada_burn_session_stop (ParrilladaBurnSession *session);

void
parrillada_burn_session_logv (ParrilladaBurnSession *session,
			   const gchar *format,
			   va_list arg_list);
void
parrillada_burn_session_log (ParrilladaBurnSession *session,
			  const gchar *format,
			  ...);

/**
 * Allow to save a whole session settings/source and restore it later.
 * (used internally)
 */

void
parrillada_burn_session_push_settings (ParrilladaBurnSession *session);
void
parrillada_burn_session_pop_settings (ParrilladaBurnSession *session);

void
parrillada_burn_session_push_tracks (ParrilladaBurnSession *session);
ParrilladaBurnResult
parrillada_burn_session_pop_tracks (ParrilladaBurnSession *session);


G_END_DECLS

#endif

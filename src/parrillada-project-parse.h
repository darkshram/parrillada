/***************************************************************************
 *            disc.h
 *
 *  dim nov 27 14:58:13 2005
 *  Copyright  2005  Rouquier Philippe
 *  parrillada-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Parrillada is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_PROJECT_PARSE_H_
#define _PARRILLADA_PROJECT_PARSE_H_

#include <glib.h>

#include "parrillada-track.h"
#include "parrillada-session.h"

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_PROJECT_SAVE_XML			= 0,
	PARRILLADA_PROJECT_SAVE_PLAIN			= 1,
	PARRILLADA_PROJECT_SAVE_PLAYLIST_PLS		= 2,
	PARRILLADA_PROJECT_SAVE_PLAYLIST_M3U		= 3,
	PARRILLADA_PROJECT_SAVE_PLAYLIST_XSPF		= 4,
	PARRILLADA_PROJECT_SAVE_PLAYLIST_IRIVER_PLA	= 5
} ParrilladaProjectSave;

typedef enum {
	PARRILLADA_PROJECT_TYPE_INVALID,
	PARRILLADA_PROJECT_TYPE_COPY,
	PARRILLADA_PROJECT_TYPE_ISO,
	PARRILLADA_PROJECT_TYPE_AUDIO,
	PARRILLADA_PROJECT_TYPE_DATA,
	PARRILLADA_PROJECT_TYPE_VIDEO
} ParrilladaProjectType;

gboolean
parrillada_project_open_project_xml (const gchar *uri,
				  ParrilladaBurnSession *session,
				  gboolean warn_user);

gboolean
parrillada_project_open_audio_playlist_project (const gchar *uri,
					     ParrilladaBurnSession *session,
					     gboolean warn_user);

gboolean 
parrillada_project_save_project_xml (ParrilladaBurnSession *session,
				  const gchar *uri);

gboolean
parrillada_project_save_audio_project_plain_text (ParrilladaBurnSession *session,
					       const gchar *uri);

gboolean
parrillada_project_save_audio_project_playlist (ParrilladaBurnSession *session,
					     const gchar *uri,
					     ParrilladaProjectSave type);

G_END_DECLS

#endif

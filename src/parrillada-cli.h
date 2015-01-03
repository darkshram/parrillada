/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2010 <bonfire-app@wanadoo.fr>
 * 
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with parrillada.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_CLI_H_
#define _PARRILLADA_CLI_H_

#include <glib.h>

#include "parrillada-app.h"
#include "parrillada-drive.h"

G_BEGIN_DECLS

struct _ParrilladaCLI {
	gchar *burn_project_uri;
	gchar *project_uri;
	gchar *cover_project;
	gchar *playlist_uri;
	gchar *copy_project_path;
	gchar *image_project_uri;

	gint audio_project;
	gint data_project;
	gint video_project;
	gint empty_project;
	gint open_ncb;

	gint disc_blank;
	gint disc_check;

	gint parent_window;
	gint burn_immediately;

	gboolean image_project;
	gboolean copy_project;
	gboolean not_unique;

	ParrilladaDrive *burner;

	gchar **files;
};
typedef struct _ParrilladaCLI ParrilladaCLI;

extern ParrilladaCLI cmd_line_options;
extern const GOptionEntry prog_options [];

void
parrillada_cli_apply_options (ParrilladaApp *app);

G_END_DECLS

#endif
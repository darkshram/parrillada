/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            project.h
 *
 *  mar nov 29 09:32:17 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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

#ifndef PROJECT_H
#define PROJECT_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-session-cfg.h"

#include "parrillada-disc.h"
#include "parrillada-uri-container.h"
#include "parrillada-project-type-chooser.h"
#include "parrillada-jacket-edit.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PROJECT         (parrillada_project_get_type ())
#define PARRILLADA_PROJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_PROJECT, ParrilladaProject))
#define PARRILLADA_PROJECT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_PROJECT, ParrilladaProjectClass))
#define PARRILLADA_IS_PROJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_PROJECT))
#define PARRILLADA_IS_PROJECT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_PROJECT))
#define PARRILLADA_PROJECT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_PROJECT, ParrilladaProjectClass))

typedef struct ParrilladaProjectPrivate ParrilladaProjectPrivate;

typedef struct {
	GtkBox parent;
	ParrilladaProjectPrivate *priv;
} ParrilladaProject;

typedef struct {
	GtkBoxClass parent_class;

	void	(*add_pressed)	(ParrilladaProject *project);
} ParrilladaProjectClass;

GType parrillada_project_get_type (void);
GtkWidget *parrillada_project_new (void);

ParrilladaBurnResult
parrillada_project_confirm_switch (ParrilladaProject *project,
				gboolean keep_files);

void
parrillada_project_set_audio (ParrilladaProject *project);
void
parrillada_project_set_data (ParrilladaProject *project);
void
parrillada_project_set_video (ParrilladaProject *project);
void
parrillada_project_set_none (ParrilladaProject *project);

void
parrillada_project_set_source (ParrilladaProject *project,
			    ParrilladaURIContainer *source);

ParrilladaProjectType
parrillada_project_convert_to_data (ParrilladaProject *project);

ParrilladaProjectType
parrillada_project_convert_to_stream (ParrilladaProject *project,
				   gboolean is_video);

ParrilladaProjectType
parrillada_project_open_session (ParrilladaProject *project,
			      ParrilladaSessionCfg *session);

gboolean
parrillada_project_save_project (ParrilladaProject *project);
gboolean
parrillada_project_save_project_as (ParrilladaProject *project);

gboolean
parrillada_project_save_session (ParrilladaProject *project,
			      const gchar *uri,
			      gchar **saved_uri,
			      gboolean show_cancel);

void
parrillada_project_register_ui (ParrilladaProject *project,
			     GtkUIManager *manager);

void
parrillada_project_create_audio_cover (ParrilladaProject *project);

G_END_DECLS

#endif /* PROJECT_H */

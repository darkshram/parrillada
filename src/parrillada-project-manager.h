/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            parrillada-project-manager.h
 *
 *  mer mai 24 14:22:56 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifndef PARRILLADA_PROJECT_MANAGER_H
#define PARRILLADA_PROJECT_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-medium.h"
#include "parrillada-project-parse.h"
#include "parrillada-project-type-chooser.h"
#include "parrillada-session-cfg.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PROJECT_MANAGER         (parrillada_project_manager_get_type ())
#define PARRILLADA_PROJECT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_PROJECT_MANAGER, ParrilladaProjectManager))
#define PARRILLADA_PROJECT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_PROJECT_MANAGER, ParrilladaProjectManagerClass))
#define PARRILLADA_IS_PROJECT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_PROJECT_MANAGER))
#define PARRILLADA_IS_PROJECT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_PROJECT_MANAGER))
#define PARRILLADA_PROJECT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_PROJECT_MANAGER, ParrilladaProjectManagerClass))

typedef struct ParrilladaProjectManagerPrivate ParrilladaProjectManagerPrivate;

typedef struct {
	GtkNotebook parent;
	ParrilladaProjectManagerPrivate *priv;
} ParrilladaProjectManager;

typedef struct {
	GtkNotebookClass parent_class;	
} ParrilladaProjectManagerClass;

GType parrillada_project_manager_get_type (void);
GtkWidget *parrillada_project_manager_new (void);

gboolean
parrillada_project_manager_open_session (ParrilladaProjectManager *manager,
                                      ParrilladaSessionCfg *session);

void
parrillada_project_manager_empty (ParrilladaProjectManager *manager);

/**
 * returns the path of the project that was saved. NULL otherwise.
 */

gboolean
parrillada_project_manager_save_session (ParrilladaProjectManager *manager,
				      const gchar *path,
				      gchar **saved_uri,
				      gboolean cancellable);

void
parrillada_project_manager_register_ui (ParrilladaProjectManager *manager,
				     GtkUIManager *ui_manager);

void
parrillada_project_manager_switch (ParrilladaProjectManager *manager,
				ParrilladaProjectType type,
				gboolean reset);

G_END_DECLS

#endif /* PARRILLADA_PROJECT_MANAGER_H */

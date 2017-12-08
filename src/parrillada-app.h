/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_APP_H_
#define _PARRILLADA_APP_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-session-cfg.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_APP             (parrillada_app_get_type ())
#define PARRILLADA_APP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_APP, ParrilladaApp))
#define PARRILLADA_APP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_APP, ParrilladaAppClass))
#define PARRILLADA_IS_APP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_APP))
#define PARRILLADA_IS_APP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_APP))
#define PARRILLADA_APP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_APP, ParrilladaAppClass))

typedef struct _ParrilladaAppClass ParrilladaAppClass;
typedef struct _ParrilladaApp ParrilladaApp;

struct _ParrilladaAppClass
{
	GtkWindowClass parent_class;
};

struct _ParrilladaApp
{
	GtkWindow parent_instance;
};

GType parrillada_app_get_type (void) G_GNUC_CONST;

ParrilladaApp *
parrillada_app_new (GApplication *gapp);

ParrilladaApp *
parrillada_app_get_default (void);

void
parrillada_app_set_parent (ParrilladaApp *app,
			guint xid);

void
parrillada_app_set_toplevel (ParrilladaApp *app, GtkWindow *window);

void
parrillada_app_create_mainwin (ParrilladaApp *app);

gboolean
parrillada_app_run_mainwin (ParrilladaApp *app);

gboolean
parrillada_app_is_running (ParrilladaApp *app);

GtkWidget *
parrillada_app_dialog (ParrilladaApp *app,
		    const gchar *primary_message,
		    GtkButtonsType button_type,
		    GtkMessageType msg_type);

void
parrillada_app_alert (ParrilladaApp *app,
		   const gchar *primary_message,
		   const gchar *secondary_message,
		   GtkMessageType type);

gboolean
parrillada_app_burn (ParrilladaApp *app,
		  ParrilladaBurnSession *session,
		  gboolean multi);

void
parrillada_app_burn_uri (ParrilladaApp *app,
		      ParrilladaDrive *burner,
		      gboolean burn);

void
parrillada_app_data (ParrilladaApp *app,
		  ParrilladaDrive *burner,
		  gchar * const *uris,
		  gboolean burn);

void
parrillada_app_stream (ParrilladaApp *app,
		    ParrilladaDrive *burner,
		    gchar * const *uris,
		    gboolean is_video,
		    gboolean burn);

void
parrillada_app_image (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   const gchar *uri,
		   gboolean burn);

void
parrillada_app_copy_disc (ParrilladaApp *app,
		       ParrilladaDrive *burner,
		       const gchar *device,
		       const gchar *cover,
		       gboolean burn);

void
parrillada_app_blank (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   gboolean burn);

void
parrillada_app_check (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   gboolean burn);

gboolean
parrillada_app_open_project (ParrilladaApp *app,
			  ParrilladaDrive *burner,
                          const gchar *uri,
                          gboolean is_playlist,
                          gboolean warn_user,
                          gboolean burn);

gboolean
parrillada_app_open_uri (ParrilladaApp *app,
                      const gchar *uri_arg,
                      gboolean warn_user);

gboolean
parrillada_app_open_uri_drive_detection (ParrilladaApp *app,
                                      ParrilladaDrive *burner,
                                      const gchar *uri,
                                      const gchar *cover_project,
                                      gboolean burn);
GtkWidget *
parrillada_app_get_statusbar1 (ParrilladaApp *app);

GtkWidget *
parrillada_app_get_statusbar2 (ParrilladaApp *app);

GtkWidget *
parrillada_app_get_project_manager (ParrilladaApp *app);

/**
 * Session management
 */

#define PARRILLADA_SESSION_TMP_PROJECT_PATH	"parrillada-tmp-project"

const gchar *
parrillada_app_get_saved_contents (ParrilladaApp *app);

gboolean
parrillada_app_save_contents (ParrilladaApp *app,
			   gboolean cancellable);

G_END_DECLS

#endif /* _PARRILLADA_APP_H_ */

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "parrillada-misc.h"
#include "parrillada-io.h"

#include "parrillada-app.h"
#include "parrillada-setting.h"
#include "parrillada-blank-dialog.h"
#include "parrillada-sum-dialog.h"
#include "parrillada-eject-dialog.h"
#include "parrillada-project-manager.h"
#include "parrillada-xsession.h"
#include "parrillada-pref.h"

#include "parrillada-drive.h"
#include "parrillada-medium.h"
#include "parrillada-volume.h"

#include "parrillada-tags.h"
#include "parrillada-burn.h"
#include "parrillada-track-disc.h"
#include "parrillada-track-image.h"
#include "parrillada-track-data-cfg.h"
#include "parrillada-track-stream-cfg.h"
#include "parrillada-track-image-cfg.h"
#include "parrillada-session.h"
#include "parrillada-burn-lib.h"

#include "parrillada-status-dialog.h"
#include "parrillada-burn-options.h"
#include "parrillada-burn-dialog.h"
#include "parrillada-jacket-edit.h"

#include "burn-plugin-manager.h"
#include "parrillada-drive-settings.h"

typedef struct _ParrilladaAppPrivate ParrilladaAppPrivate;
struct _ParrilladaAppPrivate
{
	UniqueApp *gapp;

	ParrilladaSetting *setting;

	GdkWindow *parent;

	GtkWidget *mainwin;

	GtkWidget *burn_dialog;
	GtkWidget *tool_dialog;

	/* This is the toplevel window currently displayed */
	GtkWidget *toplevel;

	GtkWidget *projects;
	GtkWidget *contents;
	GtkWidget *statusbar1;
	GtkWidget *statusbar2;
	GtkUIManager *manager;

	guint tooltip_ctx;

	gchar *saved_contents;

	guint is_maximized:1;
	guint mainwin_running:1;
};

#define PARRILLADA_APP_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_APP, ParrilladaAppPrivate))


G_DEFINE_TYPE (ParrilladaApp, parrillada_app, G_TYPE_OBJECT);

enum {
	PROP_NONE,
	PROP_GAPP
};

/**
 * Menus and toolbar
 */

static void on_prefs_cb (GtkAction *action, ParrilladaApp *app);
static void on_eject_cb (GtkAction *action, ParrilladaApp *app);
static void on_erase_cb (GtkAction *action, ParrilladaApp *app);
static void on_integrity_check_cb (GtkAction *action, ParrilladaApp *app);

static void on_exit_cb (GtkAction *action, ParrilladaApp *app);

static void on_about_cb (GtkAction *action, ParrilladaApp *app);
static void on_help_cb (GtkAction *action, ParrilladaApp *app);

static GtkActionEntry entries[] = {
	{"ProjectMenu", NULL, N_("_Project")},
	{"ViewMenu", NULL, N_("_View")},
	{"EditMenu", NULL, N_("_Edit")},
	{"ToolMenu", NULL, N_("_Tools")},

	{"HelpMenu", NULL, N_("_Help")},

	{"Plugins", NULL, N_("P_lugins"), NULL,
	 N_("Choose plugins for Parrillada"), G_CALLBACK (on_prefs_cb)},

	{"Eject", "media-eject", N_("E_ject"), NULL,
	 N_("Eject a disc"), G_CALLBACK (on_eject_cb)},

	{"Blank", "media-optical-blank", N_("_Blank…"), NULL,
	 N_("Blank a disc"), G_CALLBACK (on_erase_cb)},

	{"Check", NULL, N_("_Check Integrity…"), NULL,
	 N_("Check data integrity of disc"), G_CALLBACK (on_integrity_check_cb)},

	{"Quit", GTK_STOCK_QUIT, NULL, NULL,
	 N_("Quit Parrillada"), G_CALLBACK (on_exit_cb)},

	{"Contents", GTK_STOCK_HELP, N_("_Contents"), "F1", N_("Display help"),
	 G_CALLBACK (on_help_cb)}, 

	{"About", GTK_STOCK_ABOUT, NULL, NULL, N_("About"),
	 G_CALLBACK (on_about_cb)},
};


static const gchar *description = {
	"<ui>"
	    "<menubar name='menubar' >"
	    "<menu action='ProjectMenu'>"
		"<placeholder name='ProjectPlaceholder'/>"
		"<separator/>"
		"<menuitem action='Quit'/>"
	    "</menu>"
	    "<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'/>"
		"<separator/>"
		"<menuitem action='Plugins'/>"
	    "</menu>"
	    "<menu action='ViewMenu'>"
		"<placeholder name='ViewPlaceholder'/>"
	    "</menu>"
	    "<menu action='ToolMenu'>"
		"<placeholder name='DiscPlaceholder'/>"
		"<menuitem action='Eject'/>"
		"<menuitem action='Blank'/>"
		"<menuitem action='Check'/>"
	    "</menu>"
	    "<menu action='HelpMenu'>"
		"<menuitem action='Contents'/>"
		"<separator/>"
		"<menuitem action='About'/>"
	    "</menu>"
	    "</menubar>"
	"</ui>"
};

static gchar *
parrillada_app_get_path (const gchar *name)
{
	gchar *directory;
	gchar *retval;

	directory = g_build_filename (g_get_user_config_dir (),
				      "parrillada",
				      NULL);
	if (!g_file_test (directory, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (directory, S_IRWXU);

	retval = g_build_filename (directory, name, NULL);
	g_free (directory);
	return retval;
}

static gboolean
parrillada_app_load_window_state (ParrilladaApp *app)
{
	gint width;
	gint height;
	gint state = 0;
	gpointer value;

	GdkScreen *screen;
	GdkRectangle rect;
	gint monitor;

	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	/* Make sure that on first run the window has a default size of at least
	 * 85% of the screen (hardware not GTK+) */
	screen = gtk_window_get_screen (GTK_WINDOW (priv->mainwin));
	monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (GTK_WIDGET (priv->mainwin)));
	gdk_screen_get_monitor_geometry (screen, monitor, &rect);

	parrillada_setting_get_value (parrillada_setting_get_default (),
	                           PARRILLADA_SETTING_WIN_WIDTH,
	                           &value);
	width = GPOINTER_TO_INT (value);

	parrillada_setting_get_value (parrillada_setting_get_default (),
	                           PARRILLADA_SETTING_WIN_HEIGHT,
	                           &value);
	height = GPOINTER_TO_INT (value);

	parrillada_setting_get_value (parrillada_setting_get_default (),
	                           PARRILLADA_SETTING_WIN_MAXIMIZED,
	                           &value);
	state = GPOINTER_TO_INT (value);

	if (width > 0 && height > 0)
		gtk_window_resize (GTK_WINDOW (priv->mainwin),
				   width,
				   height);
	else
		gtk_window_resize (GTK_WINDOW (priv->mainwin),
		                   rect.width / 100 *85,
		                   rect.height / 100 * 85);

	if (state)
		gtk_window_maximize (GTK_WINDOW (priv->mainwin));

	return TRUE;
}

/**
 * returns FALSE when nothing prevents the shutdown
 * returns TRUE when shutdown should be delayed
 */

gboolean
parrillada_app_save_contents (ParrilladaApp *app,
			   gboolean cancellable)
{
	gboolean cancel;
	gchar *project_path;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (priv->burn_dialog) {
		if (cancellable)
			return (parrillada_burn_dialog_cancel (PARRILLADA_BURN_DIALOG (priv->burn_dialog), FALSE) == FALSE);

		gtk_widget_destroy (priv->burn_dialog);
		return FALSE;
	}

	if (priv->tool_dialog) {
		if (cancellable) {
			if (PARRILLADA_IS_TOOL_DIALOG (priv->tool_dialog))
				return (parrillada_tool_dialog_cancel (PARRILLADA_TOOL_DIALOG (priv->tool_dialog)) == FALSE);
			else if (PARRILLADA_IS_EJECT_DIALOG (priv->tool_dialog))
				return (parrillada_eject_dialog_cancel (PARRILLADA_EJECT_DIALOG (priv->tool_dialog)) == FALSE);
		}

		gtk_widget_destroy (priv->tool_dialog);
		return FALSE;
	}

	/* If we are not having a main window there is no point in going further */
	if (!priv->mainwin)
		return FALSE;

	if (priv->saved_contents) {
		g_free (priv->saved_contents);
		priv->saved_contents = NULL;
	}

	project_path = parrillada_app_get_path (PARRILLADA_SESSION_TMP_PROJECT_PATH);
	cancel = parrillada_project_manager_save_session (PARRILLADA_PROJECT_MANAGER (priv->projects),
						       project_path,
						       &priv->saved_contents,
						       cancellable);
	g_free (project_path);

	return cancel;
}

const gchar *
parrillada_app_get_saved_contents (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	return priv->saved_contents;
}

/**
 * These functions are only useful because they set the proper toplevel parent
 * for the message dialog. The following one also sets some properties in case
 * there isn't any toplevel parent (like show in taskbar, ...).
 **/

static void
parrillada_app_toplevel_destroyed_cb (GtkObject *object,
				   ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	priv->toplevel = NULL;
}

GtkWidget *
parrillada_app_dialog (ParrilladaApp *app,
		    const gchar *primary_message,
		    GtkButtonsType button_type,
		    GtkMessageType msg_type)
{
	gboolean is_on_top = FALSE;
	ParrilladaAppPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *dialog;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (priv->mainwin) {
		toplevel = GTK_WINDOW (priv->mainwin);
		gtk_widget_show (priv->mainwin);
	}
	else if (!priv->toplevel) {
		is_on_top = TRUE;
		toplevel = NULL;
	}
	else
		toplevel = GTK_WINDOW (priv->toplevel);

	dialog = gtk_message_dialog_new (toplevel,
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 msg_type,
					 button_type,
					 "%s",
					 primary_message);

	if (!toplevel && priv->parent) {
		gtk_widget_realize (GTK_WIDGET (dialog));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
	}

	if (is_on_top) {
		gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

		priv->toplevel = dialog;
		g_signal_connect (dialog,
				  "destroy",
				  G_CALLBACK (parrillada_app_toplevel_destroyed_cb),
				  app);
	}

	return dialog;
}

void
parrillada_app_alert (ParrilladaApp *app,
		   const gchar *primary_message,
		   const gchar *secondary_message,
		   GtkMessageType type)
{
	GtkWidget *parent = NULL;
	gboolean is_on_top= TRUE;
	ParrilladaAppPrivate *priv;
	GtkWidget *alert;

	priv = PARRILLADA_APP_PRIVATE (app);

	/* Whatever happens, they need a parent or must be in the taskbar */
	if (priv->mainwin) {
		parent = GTK_WIDGET (priv->mainwin);
		is_on_top = FALSE;
	}
	else if (priv->toplevel) {
		parent = priv->toplevel;
		is_on_top = FALSE;
	}

	alert = parrillada_utils_create_message_dialog (parent,
						     primary_message,
						     secondary_message,
						     type);
	if (!parent && priv->parent) {
		is_on_top = FALSE;

		gtk_widget_realize (GTK_WIDGET (alert));
		gtk_window_set_modal (GTK_WINDOW (alert), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (alert)), priv->parent);
	}

	if (is_on_top) {
		gtk_window_set_title (GTK_WINDOW (alert), _("Disc Burner"));
		gtk_window_set_skip_pager_hint (GTK_WINDOW (alert), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (alert), FALSE);
	}

	gtk_dialog_run (GTK_DIALOG (alert));
	gtk_widget_destroy (alert);
}

GtkWidget *
parrillada_app_get_statusbar1 (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	/* FIXME: change with future changes */
	return priv->statusbar1;
}

GtkWidget *
parrillada_app_get_statusbar2 (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	return priv->statusbar2;
}

GtkWidget *
parrillada_app_get_project_manager (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	return priv->projects;
}

static gboolean
on_destroy_cb (GtkWidget *window, ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	if (priv->mainwin)
		gtk_main_quit ();

	return FALSE;
}

static gboolean
on_delete_cb (GtkWidget *window, GdkEvent *event, ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	if (!priv->mainwin)
		return FALSE;

	if (parrillada_app_save_contents (app, TRUE))
		return TRUE;

	return FALSE;
}

static void
on_exit_cb (GtkAction *action, ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (parrillada_app_save_contents (app, TRUE))
		return;

	if (priv->mainwin)
		gtk_widget_destroy (GTK_WIDGET (priv->mainwin));
}

gboolean
parrillada_app_is_running (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	return priv->mainwin_running;
}

void
parrillada_app_set_parent (ParrilladaApp *app,
			guint parent_xid)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	priv->parent = gdk_window_foreign_new (parent_xid);
}

gboolean
parrillada_app_burn (ParrilladaApp *app,
		  ParrilladaBurnSession *session,
		  gboolean multi)
{
	gboolean success;
	GtkWidget *dialog;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	/* now setup the burn dialog */
	dialog = parrillada_burn_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "parrillada");

	priv->burn_dialog = dialog;

	parrillada_app_set_toplevel (app, GTK_WINDOW (dialog));
	if (!multi)
		success = parrillada_burn_dialog_run (PARRILLADA_BURN_DIALOG (dialog),
						   PARRILLADA_BURN_SESSION (session));
	else
		success = parrillada_burn_dialog_run_multi (PARRILLADA_BURN_DIALOG (dialog),
							 PARRILLADA_BURN_SESSION (session));
	priv->burn_dialog = NULL;

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);
	return success;
}

static ParrilladaBurnResult
parrillada_app_burn_options (ParrilladaApp *app,
			  ParrilladaSessionCfg *session)
{
	GtkWidget *dialog;
	GtkResponseType answer;

	dialog = parrillada_burn_options_new (session);
	parrillada_app_set_toplevel (app, GTK_WINDOW (dialog));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "parrillada");

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);
	if (answer == GTK_RESPONSE_OK)
		return PARRILLADA_BURN_OK;

	if (answer == GTK_RESPONSE_ACCEPT)
		return PARRILLADA_BURN_RETRY;

	return PARRILLADA_BURN_CANCEL;

}

static void
parrillada_app_session_burn (ParrilladaApp *app,
			  ParrilladaSessionCfg *session,
			  gboolean burn)
{
	ParrilladaDriveSettings *settings;

	/* Set saved temporary directory for the session.
	 * NOTE: ParrilladaBurnSession can cope with NULL path */
	settings = parrillada_drive_settings_new ();
	parrillada_drive_settings_set_session (settings, PARRILLADA_BURN_SESSION (session));

	/* We need to have a drive to start burning immediately */
	if (burn && parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (session))) {
		ParrilladaStatus *status;
		ParrilladaBurnResult result;

		status = parrillada_status_new ();
		parrillada_burn_session_get_status (PARRILLADA_BURN_SESSION (session), status);

		result = parrillada_status_get_result (status);
		if (result == PARRILLADA_BURN_NOT_READY || result == PARRILLADA_BURN_RUNNING) {
			GtkWidget *status_dialog;

			status_dialog = parrillada_status_dialog_new (PARRILLADA_BURN_SESSION (session), NULL);
			gtk_dialog_run (GTK_DIALOG (status_dialog));
			gtk_widget_destroy (status_dialog);

			parrillada_burn_session_get_status (PARRILLADA_BURN_SESSION (session), status);
			result = parrillada_status_get_result (status);
		}
		g_object_unref (status);

		if (result == PARRILLADA_BURN_CANCEL) {
			g_object_unref (settings);
			return;
		}

		if (result != PARRILLADA_BURN_OK) {
			GError *error;

			error = parrillada_status_get_error (status);
			parrillada_app_alert (app,
					   _("Error while burning."),
					   error? error->message:"",
					   GTK_MESSAGE_ERROR);
			if (error)
				g_error_free (error);

			g_object_unref (settings);
			return;
		}

		result = parrillada_app_burn (app,
		                           PARRILLADA_BURN_SESSION (session),
		                           FALSE);
	}
	else {
		ParrilladaBurnResult result;

		result = parrillada_app_burn_options (app, session);
		if (result == PARRILLADA_BURN_OK || result == PARRILLADA_BURN_RETRY) {
			result = parrillada_app_burn (app,
			                           PARRILLADA_BURN_SESSION (session),
			                           (result == PARRILLADA_BURN_RETRY));
		}
	}

	g_object_unref (settings);
}

void
parrillada_app_copy_disc (ParrilladaApp *app,
		       ParrilladaDrive *burner,
		       const gchar *device,
		       const gchar *cover,
		       gboolean burn)
{
	ParrilladaTrackDisc *track = NULL;
	ParrilladaSessionCfg *session;
	ParrilladaDrive *drive = NULL;

	session = parrillada_session_cfg_new ();
	track = parrillada_track_disc_new ();
	parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session),
					PARRILLADA_TRACK (track),
					NULL);
	g_object_unref (track);

	/* if a device is specified then get the corresponding medium */
	if (device) {
		ParrilladaMediumMonitor *monitor;

		monitor = parrillada_medium_monitor_get_default ();
		drive = parrillada_medium_monitor_get_drive (monitor, device);
		g_object_unref (monitor);

		if (!drive)
			return;

		parrillada_track_disc_set_drive (PARRILLADA_TRACK_DISC (track), drive);
		g_object_unref (drive);
	}

	/* Set a cover if any. */
	if (cover) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, cover);
		parrillada_burn_session_tag_add (PARRILLADA_BURN_SESSION (session),
					      PARRILLADA_COVER_URI,
					      value);
	}

	parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session), burner);
	parrillada_app_session_burn (app, session, burn);
	g_object_unref (session);
}

void
parrillada_app_image (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   const gchar *uri_arg,
		   gboolean burn)
{
	ParrilladaSessionCfg *session;
	ParrilladaTrackImageCfg *track = NULL;

	/* setup, show, and run options dialog */
	session = parrillada_session_cfg_new ();
	track = parrillada_track_image_cfg_new ();
	parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session),
					PARRILLADA_TRACK (track),
					NULL);
	g_object_unref (track);

	if (uri_arg) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uri_arg);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		parrillada_track_image_cfg_set_source (track, uri);
		g_free (uri);
	}

	parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session), burner);
	parrillada_app_session_burn (app, session, burn);
	g_object_unref (session);
}

static void
parrillada_app_process_session (ParrilladaApp *app,
			     ParrilladaSessionCfg *session,
			     gboolean burn)
{
	if (!burn) {
		GtkWidget *manager;
		ParrilladaAppPrivate *priv;

		priv = PARRILLADA_APP_PRIVATE (app);
		if (!priv->mainwin)
			parrillada_app_create_mainwin (app);

		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_open_session (PARRILLADA_PROJECT_MANAGER (manager), session);
	}
	else
		parrillada_app_session_burn (app, session, TRUE);
}

void
parrillada_app_burn_uri (ParrilladaApp *app,
		      ParrilladaDrive *burner,
		      gboolean burn)
{
	GFileEnumerator *enumerator;
	ParrilladaSessionCfg *session;
	ParrilladaTrackDataCfg *track;
	GFileInfo *info = NULL;
	GError *error = NULL;
	GFile *file;

	/* Here we get the contents from the burn:// URI and add them
	 * individually to the data project. This is done in case it is
	 * empty no to start the "Getting Project Size" dialog and then
	 * show the "Project is empty" dialog. Do this synchronously as:
	 * - we only want the top nodes which reduces time needed
	 * - it's always local
	 * - windows haven't been shown yet
	 * NOTE: don't use any file specified on the command line. */
	file = g_file_new_for_uri ("burn://");
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NONE,
						NULL,
						&error);
	if (!enumerator) {
		gchar *string;

		if (error) {
			string = g_strdup (error->message);
			g_error_free (error);
		}
		else
			string = g_strdup (_("An internal error occurred"));

		parrillada_app_alert (app,
				   _("Error while loading the project"),
				   string,
				   GTK_MESSAGE_ERROR);

		g_free (string);
		g_object_unref (file);
		return;
	}

	session = parrillada_session_cfg_new ();

	track = parrillada_track_data_cfg_new ();
	parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session), PARRILLADA_TRACK (track), NULL);
	g_object_unref (track);

	while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
		gchar *uri;

		uri = g_strconcat ("burn:///", g_file_info_get_name (info), NULL);
		g_object_unref (info);

		parrillada_track_data_cfg_add (track, uri, NULL);
		g_free (uri);
	}

	g_object_unref (enumerator);
	g_object_unref (file);

	if (error) {
		g_object_unref (session);

		/* NOTE: this check errors in g_file_enumerator_next_file () */
		parrillada_app_alert (app,
				   _("Error while loading the project"),
				   error->message,
				   GTK_MESSAGE_ERROR);
		return;
	}

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (track), NULL) == 0) {
	        g_object_unref (session);
		parrillada_app_alert (app,
				   _("Please add files to the project."),
				   _("The project is empty"),
				   GTK_MESSAGE_ERROR);
		return;
	}

	parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session), burner);
	parrillada_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
parrillada_app_data (ParrilladaApp *app,
		  ParrilladaDrive *burner,
		  gchar * const *uris,
		  gboolean burn)
{
	ParrilladaTrackDataCfg *track;
	ParrilladaSessionCfg *session;
	ParrilladaAppPrivate *priv;
	int i, num;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (!uris) {
		GtkWidget *manager;

		if (burn) {
			parrillada_app_alert (app,
					   _("Please add files to the project."),
					   _("The project is empty"),
					   GTK_MESSAGE_ERROR);
			return;
		}

		if (!priv->mainwin)
			parrillada_app_create_mainwin (app);

		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_switch (PARRILLADA_PROJECT_MANAGER (manager),
						PARRILLADA_PROJECT_TYPE_DATA,
						TRUE);
		return;
	}

	session = parrillada_session_cfg_new ();
	track = parrillada_track_data_cfg_new ();
	parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session), PARRILLADA_TRACK (track), NULL);
	g_object_unref (track);

	num = g_strv_length ((gchar **) uris);
	for (i = 0; i < num; i ++) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uris [i]);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		/* Ignore the return value */
		parrillada_track_data_cfg_add (track, uri, NULL);
		g_free (uri);
	}

	parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session), burner);
	parrillada_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
parrillada_app_stream (ParrilladaApp *app,
		    ParrilladaDrive *burner,
		    gchar * const *uris,
		    gboolean is_video,
		    gboolean burn)
{
	ParrilladaSessionCfg *session;
	ParrilladaAppPrivate *priv;
	int i, num;

	priv = PARRILLADA_APP_PRIVATE (app);

	session = parrillada_session_cfg_new ();

	if (!uris) {
		GtkWidget *manager;

		if (burn) {
			parrillada_app_alert (app,
					   _("Please add files to the project."),
					   _("The project is empty"),
					   GTK_MESSAGE_ERROR);
			return;
		}

		if (!priv->mainwin)
			parrillada_app_create_mainwin (app);

		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_switch (PARRILLADA_PROJECT_MANAGER (manager),
						is_video? PARRILLADA_PROJECT_TYPE_VIDEO:PARRILLADA_PROJECT_TYPE_AUDIO,
						TRUE);
		return;
	}

	num = g_strv_length ((gchar **) uris);
	for (i = 0; i < num; i ++) {
		ParrilladaTrackStreamCfg *track;
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uris [i]);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		track = parrillada_track_stream_cfg_new ();
		parrillada_track_stream_set_source (PARRILLADA_TRACK_STREAM (track), uri);
		g_free (uri);

		if (is_video)
			parrillada_track_stream_set_format (PARRILLADA_TRACK_STREAM (track),
			                                 PARRILLADA_VIDEO_FORMAT_UNDEFINED);

		parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (session), PARRILLADA_TRACK (track), NULL);
		g_object_unref (track);
	}

	parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session), burner);
	parrillada_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
parrillada_app_blank (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   gboolean burn)
{
	ParrilladaBlankDialog *dialog;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	dialog = parrillada_blank_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "parrillada");

	if (burner) {
		ParrilladaMedium *medium;

		medium = parrillada_drive_get_medium (burner);
		parrillada_tool_dialog_set_medium (PARRILLADA_TOOL_DIALOG (dialog), medium);
	}

	priv->tool_dialog = GTK_WIDGET (dialog);
	if (!priv->mainwin) {
		gtk_widget_realize (GTK_WIDGET (dialog));

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
		}
	}
	else {
		GtkWidget *toplevel;

		/* FIXME! This is a bad idea and needs fixing */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
	priv->tool_dialog = NULL;
}

static void
on_erase_cb (GtkAction *action, ParrilladaApp *app)
{
	parrillada_app_blank (app, NULL, FALSE);
}

static void
on_eject_cb (GtkAction *action, ParrilladaApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	dialog = parrillada_eject_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "parrillada");

	/* FIXME! This is a bad idea and needs fixing */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	priv->tool_dialog = dialog;
	gtk_dialog_run (GTK_DIALOG (dialog));
	priv->tool_dialog = NULL;

	gtk_widget_destroy (dialog);
}

void
parrillada_app_check (ParrilladaApp *app,
		   ParrilladaDrive *burner,
		   gboolean burn)
{
	ParrilladaSumDialog *dialog;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	dialog = parrillada_sum_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "parrillada");

	priv->tool_dialog = GTK_WIDGET (dialog);

	if (burner) {
		ParrilladaMedium *medium;

		medium = parrillada_drive_get_medium (burner);
		parrillada_tool_dialog_set_medium (PARRILLADA_TOOL_DIALOG (dialog), medium);
	}

	if (!priv->mainwin) {
		gtk_widget_realize (GTK_WIDGET (dialog));

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
		}
	}
	else {
		GtkWidget *toplevel;

		/* FIXME! This is a bad idea and needs fixing */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
	priv->tool_dialog = NULL;
}

static void
on_integrity_check_cb (GtkAction *action, ParrilladaApp *app)
{
	parrillada_app_check (app, NULL, FALSE);
}

static void
parrillada_app_current_toplevel_destroyed (GtkWidget *widget,
					ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);
	if (priv->mainwin_running)
		gtk_widget_show (GTK_WIDGET (priv->mainwin));
}

void
parrillada_app_set_toplevel (ParrilladaApp *app, GtkWindow *window)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (!priv->mainwin_running) {
		if (priv->parent) {
			gtk_widget_realize (GTK_WIDGET (window));
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (window)), priv->parent);
		}
		else {
			gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
		}
	}
	else {
		/* hide main dialog if it is shown */
		gtk_widget_hide (GTK_WIDGET (priv->mainwin));

		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	}

	gtk_widget_show (GTK_WIDGET (window));
	g_signal_connect (window,
			  "destroy",
			  G_CALLBACK (parrillada_app_current_toplevel_destroyed),
			  app);
}

static void
on_prefs_cb (GtkAction *action, ParrilladaApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	dialog = parrillada_pref_new ();

	/* FIXME! This is a bad idea and needs fixing */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
on_about_cb (GtkAction *action, ParrilladaApp *app)
{
	const gchar *authors[] = {
		"Philippe Rouquier <bonfire-app@wanadoo.fr>",
		NULL
	};

	const gchar *documenters[] = {
		"Phil Bull <philbull@gmail.com>\n"
		"Milo Casagrande <milo_casagrande@yahoo.it>\n"
		"Andrew Stabeno <stabeno@gmail.com>",
		NULL
	};

	const gchar *license_part[] = {
		N_("Parrillada is free software; you can redistribute "
		   "it and/or modify it under the terms of the GNU "
		   "General Public License as published by the Free "
		   "Software Foundation; either version 2 of the "
		   "License, or (at your option) any later version."),
                N_("Parrillada is distributed in the hope that it will "
		   "be useful, but WITHOUT ANY WARRANTY; without even "
		   "the implied warranty of MERCHANTABILITY or FITNESS "
		   "FOR A PARTICULAR PURPOSE.  See the GNU General "
		   "Public License for more details."),
                N_("You should have received a copy of the GNU General "
		   "Public License along with Parrillada; if not, write "
		   "to the Free Software Foundation, Inc., "
                   "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA"),
		NULL
        };

	gchar  *license, *comments;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	comments = g_strdup (_("A simple to use CD/DVD burning application for MATE"));

	license = g_strjoin ("\n\n",
                             _(license_part[0]),
                             _(license_part[1]),
                             _(license_part[2]),
			     NULL);

	/* This can only be shown from the main window so no need for toplevel */
	gtk_show_about_dialog (GTK_WINDOW (GTK_WIDGET (priv->mainwin)),
			       "program-name", "Parrillada",
			       "comments", comments,
			       "version", VERSION,
			       "copyright", "Copyright © 2005-2010 Philippe Rouquier\nCopyright © 2014 Joel Barrios",
			       "authors", authors,
			       "documenters", documenters,
			       "website", "http://www.alcancelibre.org/staticpages/parrillada",
			       "website-label", _("Parrillada Homepage"),
			       "license", license,
			       "wrap-license", TRUE,
			       "logo-icon-name", "parrillada",
			       /* Translators: This is a special message that shouldn't be translated
                                 * literally. It is used in the about box to give credits to
                                 * the translators.
                                 * Thus, you should translate it to your name and email address.
                                 * You should also include other translators who have contributed to
                                 * this translation; in that case, please write each of them on a separate
                                 * line seperated by newlines (\n).
                                 */
                               "translator-credits", _("translator-credits"),
			       NULL);

	g_free (comments);
	g_free (license);
}

static void
on_help_cb (GtkAction *action, ParrilladaApp *app)
{
	GError *error = NULL;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

 	gtk_show_uri (NULL, "ghelp:parrillada", gtk_get_current_event_time (), &error);
   	if (error) {
		GtkWidget *d;
        
		d = gtk_message_dialog_new (GTK_WINDOW (priv->mainwin),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					    "%s", error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
		error = NULL;
	}
}

static gboolean
on_window_state_changed_cb (GtkWidget *widget,
			    GdkEventWindowState *event,
			    ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
		priv->is_maximized = 1;
		parrillada_setting_set_value (parrillada_setting_get_default (),
		                           PARRILLADA_SETTING_WIN_MAXIMIZED,
		                           GINT_TO_POINTER (1));
	}
	else {
		priv->is_maximized = 0;
		parrillada_setting_set_value (parrillada_setting_get_default (),
		                           PARRILLADA_SETTING_WIN_MAXIMIZED,
		                           GINT_TO_POINTER (0));
	}

	return FALSE;
}

static gboolean
on_configure_event_cb (GtkWidget *widget,
		       GdkEventConfigure *event,
		       ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (!priv->is_maximized) {
		parrillada_setting_set_value (parrillada_setting_get_default (),
		                           PARRILLADA_SETTING_WIN_WIDTH,
		                           GINT_TO_POINTER (event->width));
		parrillada_setting_set_value (parrillada_setting_get_default (),
		                           PARRILLADA_SETTING_WIN_HEIGHT,
		                           GINT_TO_POINTER (event->height));
	}

	return FALSE;
}

gboolean
parrillada_app_open_project (ParrilladaApp *app,
			  ParrilladaDrive *burner,
                          const gchar *uri,
                          gboolean is_playlist,
                          gboolean warn_user,
                          gboolean burn)
{
	ParrilladaSessionCfg *session;
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	session = parrillada_session_cfg_new ();

#ifdef BUILD_PLAYLIST

	if (is_playlist) {
		if (!parrillada_project_open_audio_playlist_project (uri,
								  PARRILLADA_BURN_SESSION (session),
								  warn_user))
			return FALSE;
	}
	else

#endif
	
	if (!parrillada_project_open_project_xml (uri,
					       PARRILLADA_BURN_SESSION (session),
					       warn_user))
		return FALSE;

	parrillada_app_process_session (app, session, burn);
	g_object_unref (session);

	return TRUE;
}

static gboolean
parrillada_app_open_by_mime (ParrilladaApp *app,
                          const gchar *uri,
                          const gchar *mime,
                          gboolean warn_user)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (!mime) {
		/* that can happen when the URI could not be identified */
		return FALSE;
	}

	/* When our files/description of x-parrillada mime type is not properly 
	 * installed, it's returned as application/xml, so check that too. */
	if (!strcmp (mime, "application/x-parrillada")
	||  !strcmp (mime, "application/xml"))
		return parrillada_app_open_project (app,
						 NULL,
						 uri,
						 FALSE,
						 warn_user,
						 FALSE);

#ifdef BUILD_PLAYLIST

	else if (!strcmp (mime, "audio/x-scpls")
	     ||  !strcmp (mime, "audio/x-ms-asx")
	     ||  !strcmp (mime, "audio/x-mp3-playlist")
	     ||  !strcmp (mime, "audio/x-mpegurl"))
		return parrillada_app_open_project (app,
						 NULL,
						 uri,
						 TRUE,
						 warn_user,
						 FALSE);

#endif

	else if (!strcmp (mime, "application/x-cd-image")
	     ||  !strcmp (mime, "application/x-cdrdao-toc")
	     ||  !strcmp (mime, "application/x-toc")
	     ||  !strcmp (mime, "application/x-cue")) {
		parrillada_app_image (app, NULL, uri, FALSE);
		return TRUE;
	}

	return FALSE;
}

static gboolean
parrillada_app_open_uri_file (ParrilladaApp *app,
                           GFile *file,
                           GFileInfo *info,
                           gboolean warn_user)
{
	ParrilladaProjectType type;
	gchar *uri = NULL;

	/* if that's a symlink, redo it on its target to get the real mime type
	 * that usually also depends on the extension of the target:
	 * ex: an iso file with the extension .iso will be seen as octet-stream
	 * if the symlink hasn't got any extention at all */
	while (g_file_info_get_is_symlink (info)) {
		const gchar *target;
		GFileInfo *tmp_info;
		GFile *tmp_file;
		GError *error = NULL;

		target = g_file_info_get_symlink_target (info);
		if (!g_path_is_absolute (target)) {
			gchar *parent;
			gchar *tmp;

			tmp = g_file_get_path (file);
			parent = g_path_get_dirname (tmp);
			g_free (tmp);

			target = g_build_filename (parent, target, NULL);
			g_free (parent);
		}

		tmp_file = g_file_new_for_commandline_arg (target);
		tmp_info = g_file_query_info (tmp_file,
					      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					      G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
					      G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
					      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					      NULL,
					      &error);
		if (!tmp_info) {
			g_object_unref (tmp_file);
			break;
		}

		g_object_unref (info);
		g_object_unref (file);

		info = tmp_info;
		file = tmp_file;
	}

	uri = g_file_get_uri (file);
	if (g_file_query_exists (file, NULL)
	&&  g_file_info_get_content_type (info)) {
		const gchar *mime;

		mime = g_file_info_get_content_type (info);
	  	type = parrillada_app_open_by_mime (app, uri, mime, warn_user);
        } 
	else if (warn_user) {
		gchar *string;

		string = g_strdup_printf (_("The project \"%s\" does not exist"), uri);
		parrillada_app_alert (app,
				   _("Error while loading the project"),
				   string,
				   GTK_MESSAGE_ERROR);
		g_free (string);

		type = PARRILLADA_PROJECT_TYPE_INVALID;
	}
	else
		type = PARRILLADA_PROJECT_TYPE_INVALID;

	g_free (uri);
	return (type != PARRILLADA_PROJECT_TYPE_INVALID);
}

gboolean
parrillada_app_open_uri (ParrilladaApp *app,
                      const gchar *uri_arg,
                      gboolean warn_user)
{
	GFile *file;
	GFileInfo *info;
	gboolean retval;

	/* FIXME: make that asynchronous */
	/* NOTE: don't follow symlink because we want to identify them */
	file = g_file_new_for_commandline_arg (uri_arg);
	if (!file)
		return FALSE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL,
				  NULL);

	if (!info) {
		g_object_unref (file);
		return FALSE;
	}

	retval = parrillada_app_open_uri_file (app, file, info, warn_user);

	g_object_unref (file);
	g_object_unref (info);

	return retval;
}

gboolean
parrillada_app_open_uri_drive_detection (ParrilladaApp *app,
                                      ParrilladaDrive *burner,
                                      const gchar *uri_arg,
                                      const gchar *cover_project,
                                      gboolean burn_immediately)
{
	gchar *uri;
	GFile *file;
	GFileInfo *info;
	gboolean retval = FALSE;

	file = g_file_new_for_commandline_arg (uri_arg);
	if (!file)
		return FALSE;

	/* Note: if the path is the path of a mounted volume the uri returned
	 * will be entirely different like if /path/to/somewhere is where
	 * an audio CD is mounted will return cdda://sr0/ */
	uri = g_file_get_uri (file);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE ","
	                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL,
				  NULL);
	if (!info) {
		g_object_unref (file);
		g_free (uri);
		return FALSE;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_SPECIAL) {
		/* It could be a block device, try */
		if (!g_strcmp0 (g_file_info_get_content_type (info), "inode/blockdevice")) {
			gchar *device;
			ParrilladaMedia media;
			ParrilladaDrive *drive;
			ParrilladaMedium *medium;
			ParrilladaMediumMonitor *monitor;

			g_object_unref (info);
			g_free (uri);

			monitor = parrillada_medium_monitor_get_default ();
			while (parrillada_medium_monitor_is_probing (monitor))
				sleep (1);

			device = g_file_get_path (file);
			drive = parrillada_medium_monitor_get_drive (monitor, device);
			g_object_unref (monitor);

			if (!drive) {
				/* This is not a known optical drive to us. */
				g_object_unref (file);
				return FALSE;
			}

			medium = parrillada_drive_get_medium (drive);
			if (!medium) {
				g_object_unref (file);
				g_object_unref (drive);
				return FALSE;
			}

			media = parrillada_medium_get_status (medium);
			if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_BLANK)) {
				/* This medium is blank so it rules out blanking
				 * copying, checksuming. Open a data project. */
				g_object_unref (file);
				g_object_unref (drive);
				return FALSE;
			}
			g_object_unref (drive);

			/* It seems that we are expected to copy the disc */
			device = g_strdup (g_file_get_path (file));
			g_object_unref (file);
			parrillada_app_copy_disc (app,
					       burner,
					       device,
					       cover_project,
					       burn_immediately != 0);
			g_free (device);
			return TRUE;
		}

		/* The rest are unsupported */
	}
	else if (g_str_has_prefix (uri, "cdda:")) {
		GFile *child;
		gchar *device;
		ParrilladaMediumMonitor *monitor;

		/* Make sure we are talking of the root */
		child = g_file_get_parent (file);
		if (child) {
			g_object_unref (child);
			g_object_unref (info);
			g_object_unref (file);
			g_free (uri);
			return FALSE;
		}

		/* We need to wait for the monitor to be ready */
		monitor = parrillada_medium_monitor_get_default ();
		while (parrillada_medium_monitor_is_probing (monitor))
			sleep (1);
		g_object_unref (monitor);

		if (g_str_has_suffix (uri, "/"))
			device = g_strdup_printf ("/dev/%.*s",
				                  (int) (strrchr (uri, '/') - uri - 7),
				                  uri + 7);
		else
			device = g_strdup_printf ("/dev/%s", uri + 7);
		parrillada_app_copy_disc (app,
				       burner,
				       device,
				       cover_project,
				       burn_immediately != 0);
		g_free (device);

		retval = TRUE;
	}
	else if (g_str_has_prefix (uri, "file:/")
	     &&  g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		ParrilladaMediumMonitor *monitor;
		gchar *directory_path;
		GSList *drives;
		GSList *iter;

		/* Try to detect a mounted optical disc */
		monitor = parrillada_medium_monitor_get_default ();
		while (parrillada_medium_monitor_is_probing (monitor))
			sleep (1);

		/* Check if this is a mount point for an optical disc */
		directory_path = g_file_get_path (file);
		drives = parrillada_medium_monitor_get_drives (monitor, PARRILLADA_DRIVE_TYPE_ALL_BUT_FILE);
		for (iter = drives; iter; iter = iter->next) {
			gchar *mountpoint;
			ParrilladaDrive *drive;
			ParrilladaMedium *medium;

			drive = iter->data;
			medium = parrillada_drive_get_medium (drive);
			mountpoint = parrillada_volume_get_mount_point (PARRILLADA_VOLUME (medium), NULL);
			if (!mountpoint)
				continue;

			if (!g_strcmp0 (mountpoint, directory_path)) {
				g_free (mountpoint);
				parrillada_app_copy_disc (app,
						       burner,
						       parrillada_drive_get_device (drive),
						       cover_project,
						       burn_immediately != 0);
				retval = TRUE;
				break;
			}
			g_free (mountpoint);
		}
		g_slist_foreach (drives, (GFunc) g_object_unref, NULL);
		g_slist_free (drives);

		g_free (directory_path);
	}

	g_object_unref (info);
	g_object_unref (file);
	g_free (uri);
	return retval;
}

static void
parrillada_app_recent_open (GtkRecentChooser *chooser,
			 ParrilladaApp *app)
{
	gchar *uri;
    	const gchar *mime;
    	GtkRecentInfo *item;
	ParrilladaAppPrivate *priv;
	GtkRecentManager *manager;

	priv = PARRILLADA_APP_PRIVATE (app);
	/* This is a workaround since following code doesn't work */
	/*
    	item = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (chooser));
	if (!item)
		return;
	*/

	uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (chooser));
	if (!uri)
		return;

	manager = gtk_recent_manager_get_default ();
	item = gtk_recent_manager_lookup_item (manager, uri, NULL);

	if (!item) {
		g_free (uri);
		return;
	}

	mime = gtk_recent_info_get_mime_type (item);

	if (!mime) {
		g_free (uri);
		g_warning ("Unrecognized mime type");
		return;
	}

	/* Make sure it is no longer one shot */
	parrillada_app_open_by_mime (app,
	                          uri,
	                          mime,
	                          TRUE);
	gtk_recent_info_unref (item);
	g_free (uri);
}

static void
parrillada_app_add_recent (ParrilladaApp *app,
			GtkActionGroup *group)
{
	GtkRecentManager *recent;
	GtkRecentFilter *filter;
	GtkAction *action;

	recent = gtk_recent_manager_get_default ();
	action = gtk_recent_action_new_for_manager ("RecentProjects",
						    _("_Recent Projects"),
						    _("Display the projects recently opened"),
						    NULL,
						    recent);
	filter = gtk_recent_filter_new ();

	gtk_recent_filter_set_name (filter, _("_Recent Projects"));
	gtk_recent_filter_add_mime_type (filter, "application/x-parrillada");
	gtk_recent_filter_add_mime_type (filter, "application/x-cd-image");
	gtk_recent_filter_add_mime_type (filter, "application/x-cdrdao-toc");
	gtk_recent_filter_add_mime_type (filter, "application/x-toc");
	gtk_recent_filter_add_mime_type (filter, "application/x-cue");
	gtk_recent_filter_add_mime_type (filter, "audio/x-scpls");
	gtk_recent_filter_add_mime_type (filter, "audio/x-ms-asx");
	gtk_recent_filter_add_mime_type (filter, "audio/x-mp3-playlist");
	gtk_recent_filter_add_mime_type (filter, "audio/x-mpegurl");

	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (action), filter);
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (action), filter);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (action), 5);

	gtk_recent_chooser_set_show_tips (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (action), GTK_RECENT_SORT_MRU);

	gtk_action_group_add_action (group, action);
	g_object_unref (action);
	g_signal_connect (action,
			  "item-activated",
			  G_CALLBACK (parrillada_app_recent_open),
			  app);
}

static void
parrillada_menu_item_selected_cb (GtkMenuItem *proxy,
			       ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;
	GtkAction *action;
	gchar *message;

	priv = PARRILLADA_APP_PRIVATE (app);

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar2),
				    priv->tooltip_ctx,
				    message);
		g_free (message);

		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar1),
				    priv->tooltip_ctx,
				    "");
	}
}

static void
parrillada_menu_item_deselected_cb (GtkMenuItem *proxy,
				 ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar2),
			   priv->tooltip_ctx);
	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar1),
			   priv->tooltip_ctx);
}

static void
parrillada_connect_ui_manager_proxy_cb (GtkUIManager *manager,
				     GtkAction *action,
				     GtkWidget *proxy,
				     ParrilladaApp *app)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_connect (proxy,
			  "select",
			  G_CALLBACK (parrillada_menu_item_selected_cb),
			  app);
	g_signal_connect (proxy,
			  "deselect",
			  G_CALLBACK (parrillada_menu_item_deselected_cb),
			  app);
}

static void
parrillada_disconnect_ui_manager_proxy_cb (GtkUIManager *manager,
					GtkAction *action,
					GtkWidget *proxy,
					ParrilladaApp *app)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_handlers_disconnect_by_func (proxy,
					      G_CALLBACK (parrillada_menu_item_selected_cb),
					      app);
	g_signal_handlers_disconnect_by_func (proxy,
					      G_CALLBACK (parrillada_menu_item_deselected_cb),
					      app);
}

static void
parrillada_caps_changed_cb (ParrilladaPluginManager *manager,
			 ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;
	GtkWidget *widget;

	priv = PARRILLADA_APP_PRIVATE (app);

	widget = gtk_ui_manager_get_widget (priv->manager, "/menubar/ToolMenu/Check");

	if (!parrillada_burn_library_can_checksum ())
		gtk_widget_set_sensitive (widget, FALSE);
	else
		gtk_widget_set_sensitive (widget, TRUE);
}

void
parrillada_app_create_mainwin (ParrilladaApp *app)
{
	GtkWidget *hbox;
	GtkWidget *menubar;
	GError *error = NULL;
	ParrilladaAppPrivate *priv;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	ParrilladaPluginManager *plugin_manager;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (priv->mainwin)
		return;

	/* New window */
	priv->mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_name (GTK_WINDOW (priv->mainwin), "parrillada");

	g_signal_connect (G_OBJECT (priv->mainwin),
			  "delete-event",
			  G_CALLBACK (on_delete_cb),
			  app);
	g_signal_connect (G_OBJECT (priv->mainwin),
			  "destroy",
			  G_CALLBACK (on_destroy_cb),
			  app);

	/* contents */
	priv->contents = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (priv->contents);

	gtk_container_add (GTK_CONTAINER (priv->mainwin), priv->contents);

	/* menu and toolbar */
	priv->manager = gtk_ui_manager_new ();
	g_signal_connect (priv->manager,
			  "connect-proxy",
			  G_CALLBACK (parrillada_connect_ui_manager_proxy_cb),
			  app);
	g_signal_connect (priv->manager,
			  "disconnect-proxy",
			  G_CALLBACK (parrillada_disconnect_ui_manager_proxy_cb),
			  app);

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      app);

	gtk_ui_manager_insert_action_group (priv->manager, action_group, 0);

	parrillada_app_add_recent (app, action_group);

	if (!gtk_ui_manager_add_ui_from_string (priv->manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	menubar = gtk_ui_manager_get_widget (priv->manager, "/menubar");
	gtk_box_pack_start (GTK_BOX (priv->contents), menubar, FALSE, FALSE, 0);

	/* window contents */
	priv->projects = parrillada_project_manager_new ();
	gtk_widget_show (priv->projects);

	gtk_box_pack_start (GTK_BOX (priv->contents), priv->projects, TRUE, TRUE, 0);

	/* status bar to display the size of selected files */
	hbox = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_end (GTK_BOX (priv->contents), hbox, FALSE, TRUE, 0);

	priv->statusbar2 = gtk_statusbar_new ();
	gtk_widget_show (priv->statusbar2);
	priv->tooltip_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar2), "tooltip_info");
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (priv->statusbar2), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), priv->statusbar2, FALSE, TRUE, 0);

	priv->statusbar1 = gtk_statusbar_new ();
	gtk_widget_show (priv->statusbar1);
	gtk_box_pack_start (GTK_BOX (hbox), priv->statusbar1, FALSE, TRUE, 0);

	/* Update everything */
	parrillada_project_manager_register_ui (PARRILLADA_PROJECT_MANAGER (priv->projects),
					     priv->manager);

	gtk_ui_manager_ensure_update (priv->manager);

	/* check if we can use checksums (we need plugins enabled) */
	if (!parrillada_burn_library_can_checksum ()) {
		GtkWidget *widget;

		widget = gtk_ui_manager_get_widget (priv->manager, "/menubar/ToolMenu/Check");
		gtk_widget_set_sensitive (widget, FALSE);
	}

	plugin_manager = parrillada_plugin_manager_get_default ();
	g_signal_connect (plugin_manager,
			  "caps-changed",
			  G_CALLBACK (parrillada_caps_changed_cb),
			  app);

	/* add accelerators */
	accel_group = gtk_ui_manager_get_accel_group (priv->manager);
	gtk_window_add_accel_group (GTK_WINDOW (priv->mainwin), accel_group);

	/* set up the window geometry */
	gtk_window_set_position (GTK_WINDOW (priv->mainwin),
				 GTK_WIN_POS_CENTER);

	g_signal_connect (priv->mainwin,
			  "window-state-event",
			  G_CALLBACK (on_window_state_changed_cb),
			  app);
	g_signal_connect (priv->mainwin,
			  "configure-event",
			  G_CALLBACK (on_configure_event_cb),
			  app);

	gtk_widget_realize (GTK_WIDGET (priv->mainwin));

	if (priv->parent) {
		gtk_window_set_modal (GTK_WINDOW (priv->mainwin), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (priv->mainwin)), priv->parent);
	}

	parrillada_app_load_window_state (app);
}

static UniqueResponse
parrillada_app_unique_message (UniqueApp *uapp,
			    gint command,
			    UniqueMessageData *message_data,
			    ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (command == UNIQUE_ACTIVATE) {
	       if (priv->mainwin_running) {
		       gtk_widget_show (priv->mainwin);
		       gtk_window_present (GTK_WINDOW (priv->mainwin));
	       }
 	}

	return UNIQUE_RESPONSE_OK;
}

gboolean
parrillada_app_run_mainwin (ParrilladaApp *app)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (app);

	if (priv->mainwin_running)
		return TRUE;

	priv->mainwin_running = 1;
	gtk_widget_show (GTK_WIDGET (priv->mainwin));

	gtk_main ();
	return TRUE;
}

static GtkWindow *
parrillada_app_get_io_parent_window (gpointer user_data)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (user_data);
	if (!priv->mainwin) {
		if (priv->parent)
			return GTK_WINDOW (priv->parent);
	}
	else {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));
		return GTK_WINDOW (toplevel);
	}

	return NULL;
}

static void
parrillada_app_init (ParrilladaApp *object)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (object);

	/* Load settings */
	priv->setting = parrillada_setting_get_default ();
	parrillada_setting_load (priv->setting);

	/* Connect to session */
	parrillada_session_connect (object);

	g_set_application_name (_("Disc Burner"));
	gtk_window_set_default_icon_name ("parrillada");

	parrillada_io_set_parent_window_callback (parrillada_app_get_io_parent_window, object);
}

static void
parrillada_app_finalize (GObject *object)
{
	ParrilladaAppPrivate *priv;

	priv = PARRILLADA_APP_PRIVATE (object);

	parrillada_setting_save (priv->setting);
	g_object_unref (priv->setting);
	priv->setting = NULL;

	parrillada_session_disconnect (PARRILLADA_APP (object));

	if (priv->saved_contents) {
		g_free (priv->saved_contents);
		priv->saved_contents = NULL;
	}

	G_OBJECT_CLASS (parrillada_app_parent_class)->finalize (object);
}
static void
parrillada_app_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	ParrilladaApp *app = PARRILLADA_APP (object);
	ParrilladaAppPrivate *priv = PARRILLADA_APP_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_GAPP:
		priv->gapp = g_value_dup_object (value);
		g_signal_connect (priv->gapp,
				  "message-received",
				  G_CALLBACK (parrillada_app_unique_message),
				  app);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_app_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	ParrilladaAppPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_APP (object));

	priv = PARRILLADA_APP_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_GAPP:
		g_value_set_object (value, priv->gapp);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_app_class_init (ParrilladaAppClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaAppPrivate));

	object_class->finalize = parrillada_app_finalize;
	object_class->set_property = parrillada_app_set_property;
	object_class->get_property = parrillada_app_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_GAPP,
					 g_param_spec_object("gapp",
	                                 "UniqueApplication",
					 "The UniqueApp object",
					 UNIQUE_TYPE_APP,
					 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

}

ParrilladaApp *
parrillada_app_new (UniqueApp *gapp)
{
	return g_object_new (PARRILLADA_TYPE_APP,
	                     "gapp", gapp,
	                     NULL);
}

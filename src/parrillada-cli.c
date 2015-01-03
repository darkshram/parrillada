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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "parrillada-cli.h"
#include "parrillada-app.h"
#include "parrillada-project-manager.h"

#include "parrillada-burn-lib.h"

#include "parrillada-medium-monitor.h"
#include "parrillada-medium.h"

#include "parrillada-misc.h"


ParrilladaCLI cmd_line_options;

static gboolean
parrillada_cli_copy_project (const gchar *option_name,
                          const gchar *value,
                          gpointer data,
                          GError **error);
static gboolean
parrillada_cli_image_project (const gchar *option_name,
                           const gchar *value,
                           gpointer data,
                           GError **error);

static gboolean
parrillada_cli_fake_device (const gchar *option_name,
                         const gchar *value,
                         gpointer data,
                         GError **error);

static gboolean
parrillada_cli_burning_device (const gchar *option_name,
                            const gchar *value,
                            gpointer data,
                            GError **error);

const GOptionEntry prog_options [] = {
	{ "project", 'p', 0, G_OPTION_ARG_FILENAME, &cmd_line_options.project_uri,
	  N_("Open the specified project"),
	  N_("PROJECT") },

#ifdef BUILD_PLAYLIST

	 { "playlist", 'l', 0, G_OPTION_ARG_FILENAME, &cmd_line_options.playlist_uri,
	  N_("Open the specified playlist as an audio project"),
	  N_("PLAYLIST") },

#endif

	{ "device", 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, parrillada_cli_burning_device,
	  N_("Set the drive to be used for burning"),
	  N_("DEVICE PATH") },

	{ "image-file", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, parrillada_cli_fake_device,
	  N_("Create an image file instead of burning"),
	  NULL },

	{ "audio", 'a', 0, G_OPTION_ARG_NONE, &cmd_line_options.audio_project,
	  N_("Open an audio project adding the URIs given on the command line"),
	  NULL },

	{ "data", 'd', 0, G_OPTION_ARG_NONE, &cmd_line_options.data_project,
         N_("Open a data project adding the URIs given on the command line"),
          NULL },

	{ "copy", 'c', G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, parrillada_cli_copy_project,
	  N_("Copy a disc"),
	  N_("PATH TO DEVICE") },

	{ "cover", 'j', 0, G_OPTION_ARG_FILENAME, &cmd_line_options.cover_project,
	  N_("Cover to use"),
	  N_("PATH TO COVER") },

	{ "video", 'o', 0, G_OPTION_ARG_NONE, &cmd_line_options.video_project,
	  N_("Open a video project adding the URIs given on the command line"),
	  NULL },

	{ "image", 'i', G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, parrillada_cli_image_project,
	 N_("URI of an image file to burn (autodetected)"),
          N_("PATH TO IMAGE") },

    	{ "empty", 'e', 0, G_OPTION_ARG_NONE, &cmd_line_options.empty_project,
         N_("Force Parrillada to display the project selection page"),
          NULL },

	{ "blank", 'b', 0, G_OPTION_ARG_NONE, &cmd_line_options.disc_blank,
	  N_("Open the blank disc dialog"),
	  N_("PATH TO DEVICE") },

	{ "check", 'k', 0, G_OPTION_ARG_NONE, &cmd_line_options.disc_check,
	  N_("Open the check disc dialog"),
	  N_("PATH TO DEVICE") },

	{ "ncb", 'n', 0, G_OPTION_ARG_NONE, &cmd_line_options.open_ncb,
	  N_("Burn the contents of the burn:// URI"),
	  NULL },

	{ "immediately", 0, 0, G_OPTION_ARG_NONE, &cmd_line_options.burn_immediately,
	  N_("Start burning immediately."),
	  NULL },

	 { "no-existing-session", 0, 0, G_OPTION_ARG_NONE, &cmd_line_options.not_unique,
	  N_("Don't connect to an already-running instance"),
	  NULL },

	{ "burn-and-remove-project", 'r', 0, G_OPTION_ARG_FILENAME, &cmd_line_options.burn_project_uri,
	  N_("Burn the specified project and remove it.\nThis option is mainly useful for integration with other applications."),
	  N_("PATH") },

	{ "transient-for", 'x', 0, G_OPTION_ARG_INT, &cmd_line_options.parent_window,
	/* Translators: the xid is a number identifying each window in the X11
	 * world (not Windows, MacOS X). The following sentence says that
	 * parrillada will be set to be always on top of the window identified by
	 * xid. In other words, the window with the given xid will become parrillada
	 * parent as if parrillada was a dialog for the parent application */
	  N_("The XID of the parent window"), NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &cmd_line_options.files,
	  NULL, NULL }, /* collects file arguments */

	{ NULL }
};

static gboolean
parrillada_cli_fake_device (const gchar *option_name,
                         const gchar *value,
                         gpointer data,
                         GError **error)
{
	ParrilladaMediumMonitor *monitor;
	GSList *list;

	/* Wait for the libparrillada-media to be ready */
	monitor = parrillada_medium_monitor_get_default ();
	while (parrillada_medium_monitor_is_probing (monitor))
		sleep (1);

	list = parrillada_medium_monitor_get_drives (monitor, PARRILLADA_DRIVE_TYPE_FILE);
	if (!list)
		return FALSE;

	cmd_line_options.burner = list->data;
	g_slist_free (list);

	return TRUE;
}

static gboolean
parrillada_cli_burning_device (const gchar *option_name,
                            const gchar *value,
                            gpointer data,
                            GError **error)
{
	ParrilladaDrive *burner;
	ParrilladaMediumMonitor *monitor;

	if (!value)
		return FALSE;

	/* Wait for the libparrillada-media to be ready */
	monitor = parrillada_medium_monitor_get_default ();
	while (parrillada_medium_monitor_is_probing (monitor))
		sleep (1);

	burner = parrillada_medium_monitor_get_drive (monitor, value);
	g_object_unref (monitor);

	if (burner) {
		if (!parrillada_drive_can_write (burner)) {
			gchar *string;

			/* Translators: %s is the path of drive */
			string = g_strdup_printf (_("\"%s\" cannot write."), value);
			parrillada_utils_message_dialog (NULL,
						      _("Wrong command line option."),
						      string,
						      GTK_MESSAGE_ERROR);

			g_object_unref (burner);
			return FALSE;
		}
	}
	else {
		gchar *string;

		/* Translators: %s is the path of a drive */
		string = g_strdup_printf (_("\"%s\" cannot be found."), value);
		parrillada_utils_message_dialog (NULL,
					      _("Wrong command line option."),
					      string,
					      GTK_MESSAGE_ERROR);
		g_free (string);
		return FALSE;
	}

	return TRUE;
}

static gboolean
parrillada_cli_copy_project (const gchar *option_name,
                          const gchar *value,
                          gpointer data,
                          GError **error)
{
	cmd_line_options.copy_project = TRUE;
	cmd_line_options.copy_project_path = g_strdup (value);

	return TRUE;
}

static gboolean
parrillada_cli_image_project (const gchar *option_name,
                           const gchar *value,
                           gpointer data,
                           GError **error)
{
	cmd_line_options.image_project = TRUE;
	cmd_line_options.image_project_uri = g_strdup (value);

	return TRUE;
}

void
parrillada_cli_apply_options (ParrilladaApp *app)
{
	gint nb = 0;
	GtkWidget *manager = NULL;

	if (cmd_line_options.parent_window)
		parrillada_app_set_parent (app, cmd_line_options.parent_window);

    	if (cmd_line_options.empty_project) {
	    	parrillada_app_create_mainwin (app);
		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_empty (PARRILLADA_PROJECT_MANAGER (manager));
		parrillada_app_run_mainwin (app);
		return;
	}

	/* we first check that only one of the options was given
	 * (except for --debug, cover argument and device) */
	if (cmd_line_options.copy_project)
		nb ++;
	if (cmd_line_options.image_project)
		nb ++;
	if (cmd_line_options.project_uri)
		nb ++;
	if (cmd_line_options.burn_project_uri)
		nb ++;
	if (cmd_line_options.playlist_uri)
		nb ++;
	if (cmd_line_options.audio_project)
		nb ++;
	if (cmd_line_options.data_project)
		nb ++;
	if (cmd_line_options.video_project)
	    	nb ++;
	if (cmd_line_options.disc_blank)
	  	nb ++;
	if (cmd_line_options.open_ncb)
		nb ++;

	if (nb > 1) {
		parrillada_app_create_mainwin (app);
		parrillada_app_alert (app,
				   _("Incompatible command line options used."),
				   _("Only one option can be given at a time"),
				   GTK_MESSAGE_ERROR);

		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_empty (PARRILLADA_PROJECT_MANAGER (manager));
	}
	else if (cmd_line_options.project_uri) {
		parrillada_app_open_project (app,
					  cmd_line_options.burner,
					  cmd_line_options.project_uri,
					  FALSE,
					  TRUE,
					  cmd_line_options.burn_immediately != 0);
		if (cmd_line_options.burn_immediately)
			return;
	}
	else if (cmd_line_options.burn_project_uri) {
		gboolean res;

		res = parrillada_app_open_project (app,
		                                cmd_line_options.burner,
		                                cmd_line_options.burn_project_uri,
		                                FALSE,
		                                TRUE,
		                                FALSE /* This is to keep the current behavior which is open main window */);
		if (res)
			parrillada_app_run_mainwin (app);

		if (g_remove (cmd_line_options.burn_project_uri) != 0) {
			gchar *path;

			path = g_filename_from_uri (cmd_line_options.burn_project_uri, NULL, NULL);
			g_remove (path);
			g_free (path);
		}
		return;
	}

#ifdef BUILD_PLAYLIST

	else if (cmd_line_options.playlist_uri) {
		parrillada_app_open_project (app,
					  cmd_line_options.burner,
					  cmd_line_options.playlist_uri,
					  TRUE,
					  TRUE,
					  cmd_line_options.burn_immediately != 0);
		if (cmd_line_options.burn_immediately)
			return;
	}

#endif
	else if (cmd_line_options.copy_project) {
		parrillada_app_copy_disc (app,
				       cmd_line_options.burner,
				       cmd_line_options.copy_project_path,
				       cmd_line_options.cover_project,
				       cmd_line_options.burn_immediately != 0);
		return;
	}
	else if (cmd_line_options.image_project) {
		parrillada_app_image (app,
				   cmd_line_options.burner,
				   cmd_line_options.image_project_uri,
				   cmd_line_options.burn_immediately != 0);
		return;
	}
	else if (cmd_line_options.open_ncb) {
		parrillada_app_burn_uri (app, cmd_line_options.burner, cmd_line_options.burn_immediately != 0);
		return;
	}
	else if (cmd_line_options.disc_blank) {
		parrillada_app_blank (app, cmd_line_options.burner, cmd_line_options.burn_immediately != 0);
		return;
	}
	else if (cmd_line_options.disc_check) {
		parrillada_app_check (app, cmd_line_options.burner, cmd_line_options.burn_immediately != 0);
		return;
	}
	else if (cmd_line_options.audio_project) {
		parrillada_app_stream (app, cmd_line_options.burner, cmd_line_options.files, FALSE, cmd_line_options.burn_immediately != 0);
		if (cmd_line_options.burn_immediately)
			return;
	}
	else if (cmd_line_options.data_project) {
		parrillada_app_data (app, cmd_line_options.burner, cmd_line_options.files, cmd_line_options.burn_immediately != 0);
		if (cmd_line_options.burn_immediately)
			return;
	}
	else if (cmd_line_options.video_project) {
		parrillada_app_stream (app, cmd_line_options.burner, cmd_line_options.files, TRUE, cmd_line_options.burn_immediately != 0);
		if (cmd_line_options.burn_immediately)
			return;
	}
	else if (cmd_line_options.files) {
		if (g_strv_length (cmd_line_options.files) == 1) {
			gboolean result;

			result = parrillada_app_open_uri_drive_detection (app,
			                                               cmd_line_options.burner,
			                                               cmd_line_options.files [0],
			                                               cmd_line_options.cover_project,
			                                               cmd_line_options.burn_immediately);
			/* Return here if the URI was related to a disc operation */
			if (result)
				return;

			result = parrillada_app_open_uri (app, cmd_line_options.files [0], FALSE);
			if (!result)
				parrillada_app_data (app, cmd_line_options.burner, cmd_line_options.files, cmd_line_options.burn_immediately != 0);
		}
		else
			parrillada_app_data (app, cmd_line_options.burner, cmd_line_options.files, cmd_line_options.burn_immediately != 0);

		if (cmd_line_options.burn_immediately)
			return;
	}
	else {
		parrillada_app_create_mainwin (app);
		manager = parrillada_app_get_project_manager (app);
		parrillada_project_manager_empty (PARRILLADA_PROJECT_MANAGER (manager));
	}

	parrillada_app_run_mainwin (app);
}

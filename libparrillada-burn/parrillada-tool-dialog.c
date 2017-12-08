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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "parrillada-tool-dialog.h"
#include "parrillada-tool-dialog-private.h"

#include "parrillada-progress.h"
#include "parrillada-medium-selection.h"

#include "parrillada-misc.h"

#include "parrillada-burn.h"

#include "parrillada-medium.h"
#include "parrillada-drive.h"

G_DEFINE_TYPE (ParrilladaToolDialog, parrillada_tool_dialog, GTK_TYPE_DIALOG);

typedef struct _ParrilladaToolDialogPrivate ParrilladaToolDialogPrivate;
struct _ParrilladaToolDialogPrivate {
	GtkWidget *upper_box;
	GtkWidget *lower_box;
	GtkWidget *selector;
	GtkWidget *progress;
	GtkWidget *button;
	GtkWidget *options;
	GtkWidget *cancel;

	ParrilladaBurn *burn;

	gboolean running;
};

#define PARRILLADA_TOOL_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TOOL_DIALOG, ParrilladaToolDialogPrivate))

static GtkDialogClass *parent_class = NULL;

static void
parrillada_tool_dialog_media_error (ParrilladaToolDialog *self)
{
	parrillada_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     _("The disc is not supported"),
				     GTK_MESSAGE_ERROR);
}

static void
parrillada_tool_dialog_media_busy (ParrilladaToolDialog *self)
{
	gchar *string;

	string = g_strdup_printf ("%s. %s",
				  _("The drive is busy"),
				  _("Make sure another application is not using it"));
	parrillada_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     string,
				     GTK_MESSAGE_ERROR);
	g_free (string);
}

static void
parrillada_tool_dialog_no_media (ParrilladaToolDialog *self)
{
	parrillada_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     _("The drive is empty"),
				     GTK_MESSAGE_ERROR);
}

void
parrillada_tool_dialog_set_progress (ParrilladaToolDialog *self,
				  gdouble overall_progress,
				  gdouble task_progress,
				  glong remaining,
				  gint size_mb,
				  gint written_mb)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	parrillada_burn_progress_set_status (PARRILLADA_BURN_PROGRESS (priv->progress),
					  FALSE, /* no need for the media here since speed is not specified */
					  overall_progress,
					  task_progress,
					  remaining,
					  -1,
					  -1,
					  -1);
}

void
parrillada_tool_dialog_set_action (ParrilladaToolDialog *self,
				ParrilladaBurnAction action,
				const gchar *string)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	parrillada_burn_progress_set_action (PARRILLADA_BURN_PROGRESS (priv->progress),
					  action,
					  string);
}

static void
parrillada_tool_dialog_progress_changed (ParrilladaBurn *burn,
				      gdouble overall_progress,
				      gdouble progress,
				      glong time_remaining,
				      ParrilladaToolDialog *self)
{
	parrillada_tool_dialog_set_progress (self,
					  -1.0,
					  progress,
					  -1,
					  -1,
					  -1);
}

static void
parrillada_tool_dialog_action_changed (ParrilladaBurn *burn,
				    ParrilladaBurnAction action,
				    ParrilladaToolDialog *self)
{
	gchar *string;

	parrillada_burn_get_action_string (burn, action, &string);
	parrillada_tool_dialog_set_action (self,
					action,
					string);
	g_free (string);
}

ParrilladaBurn *
parrillada_tool_dialog_get_burn (ParrilladaToolDialog *self)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);

	if (priv->burn) {
		parrillada_burn_cancel (priv->burn, FALSE);
		g_object_unref (priv->burn);
	}

	priv->burn = parrillada_burn_new ();
	g_signal_connect (priv->burn,
			  "progress-changed",
			  G_CALLBACK (parrillada_tool_dialog_progress_changed),
			  self);
	g_signal_connect (priv->burn,
			  "action-changed",
			  G_CALLBACK (parrillada_tool_dialog_action_changed),
			  self);

	return priv->burn;
}

static gboolean
parrillada_tool_dialog_run (ParrilladaToolDialog *self)
{
	ParrilladaToolDialogPrivate *priv;
	ParrilladaToolDialogClass *klass;
	gboolean close = FALSE;
	ParrilladaMedium *medium;
	ParrilladaMedia media;
	GdkCursor *cursor;
	GdkWindow *window;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	medium = parrillada_medium_selection_get_active (PARRILLADA_MEDIUM_SELECTION (priv->selector));

	/* set up */
	gtk_widget_set_sensitive (priv->upper_box, FALSE);
	gtk_widget_set_sensitive (priv->lower_box, TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->button), FALSE);

	cursor = gdk_cursor_new (GDK_WATCH);
	window = gtk_widget_get_window (GTK_WIDGET (self));
	gdk_window_set_cursor (window, cursor);
	g_object_unref (cursor);

	gtk_button_set_label (GTK_BUTTON (priv->cancel), GTK_STOCK_CANCEL);

	/* check the contents of the drive */
	media = parrillada_medium_get_status (medium);
	if (media == PARRILLADA_MEDIUM_NONE) {
		parrillada_tool_dialog_no_media (self);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->button), TRUE);
		goto end;
	}
	else if (media == PARRILLADA_MEDIUM_UNSUPPORTED) {
		/* error out */
		gtk_widget_set_sensitive (GTK_WIDGET (priv->button), TRUE);
		parrillada_tool_dialog_media_error (self);
		goto end;
	}
	else if (media == PARRILLADA_MEDIUM_BUSY) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->button), TRUE);
		parrillada_tool_dialog_media_busy (self);
		goto end;
	}

	priv->running = TRUE;

	klass = PARRILLADA_TOOL_DIALOG_GET_CLASS (self);
	if (klass->activate)
		close = klass->activate (self, medium);

	priv->running = FALSE;

	if (medium)
		g_object_unref (medium);

	if (close)
		return TRUE;

end:

	gdk_window_set_cursor (window, NULL);
	gtk_button_set_label (GTK_BUTTON (priv->cancel), GTK_STOCK_CLOSE);

	gtk_widget_set_sensitive (priv->upper_box, TRUE);
	gtk_widget_set_sensitive (priv->lower_box, FALSE);

	parrillada_burn_progress_reset (PARRILLADA_BURN_PROGRESS (priv->progress));

	return FALSE;
}

void
parrillada_tool_dialog_pack_options (ParrilladaToolDialog *self,
				  ...)
{
	gchar *title;
	va_list vlist;
	GtkWidget *child;
	GSList *list = NULL;
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);

	va_start (vlist, self);
	while ((child = va_arg (vlist, GtkWidget *)))
		list = g_slist_prepend (list, child);
	va_end (vlist);

	title = g_strdup_printf ("<b>%s</b>", _("Options"));
	priv->options = parrillada_utils_pack_properties_list (title, list);
	g_free (title);

	g_slist_free (list);

	gtk_widget_show_all (priv->options);
	gtk_box_pack_start (GTK_BOX (priv->upper_box),
			    priv->options,
			    FALSE,
			    FALSE,
			    0);
}

void
parrillada_tool_dialog_set_button (ParrilladaToolDialog *self,
				const gchar *text,
				const gchar *image,
				const gchar *theme)
{
	GtkWidget *button;
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);

	if (priv->button)
		g_object_unref (priv->button);

	button = parrillada_utils_make_button (text,
					    image,
					    theme,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (self),
				      button,
				      GTK_RESPONSE_OK);

	priv->button = button;
}

void
parrillada_tool_dialog_set_valid (ParrilladaToolDialog *self,
			       gboolean valid)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	gtk_widget_set_sensitive (priv->button, valid);
}

void
parrillada_tool_dialog_set_medium_type_shown (ParrilladaToolDialog *self,
					   ParrilladaMediaType media_type)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (priv->selector),
						  media_type);
}

/**
 * parrillada_tool_dialog_get_medium:
 * @dialog: a #ParrilladaToolDialog
 *
 * This function returns the currently selected medium.
 *
 * Return value: a #ParrilladaMedium or NULL if none is set.
 **/

ParrilladaMedium *
parrillada_tool_dialog_get_medium (ParrilladaToolDialog *self)
{
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	return parrillada_medium_selection_get_active (PARRILLADA_MEDIUM_SELECTION (priv->selector));
}

/**
 * parrillada_tool_dialog_set_medium:
 * @dialog: a #ParrilladaToolDialog
 * @medium: a #ParrilladaMedium
 *
 * Selects the medium that should be currently selected.
 *
 * Return value: a #gboolean. TRUE if it was successful.
 **/

gboolean
parrillada_tool_dialog_set_medium (ParrilladaToolDialog *self,
				ParrilladaMedium *medium)
{
	ParrilladaToolDialogPrivate *priv;

	if (!medium)
		return FALSE;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);

	return parrillada_medium_selection_set_active (PARRILLADA_MEDIUM_SELECTION (priv->selector), medium);
}

static void
parrillada_tool_dialog_drive_changed_cb (ParrilladaMediumSelection *selection,
				      ParrilladaMedium *medium,
				      ParrilladaToolDialog *self)
{
	ParrilladaToolDialogClass *klass;

	klass = PARRILLADA_TOOL_DIALOG_GET_CLASS (self);
	if (klass->medium_changed)
		klass->medium_changed (self, medium);
}

static gboolean
parrillada_tool_dialog_cancel_dialog (GtkWidget *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_window_set_icon_name (GTK_WINDOW (message),
	                          gtk_window_get_icon_name (GTK_WINDOW (toplevel)));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Interrupting the process may make disc unusable."));
	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = parrillada_utils_make_button (_("_Continue"),
					    GTK_STOCK_OK,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_OK);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (result != GTK_RESPONSE_OK)
		return TRUE;

	return FALSE;
}

/**
 * parrillada_tool_dialog_cancel:
 * @dialog: a #ParrilladaToolDialog
 *
 * Cancels any ongoing operation.
 *
 * Return value: a #gboolean. TRUE when cancellation was successful. FALSE otherwise.
 **/

gboolean
parrillada_tool_dialog_cancel (ParrilladaToolDialog *self)
{
	ParrilladaToolDialogClass *klass;
	ParrilladaToolDialogPrivate *priv;

	klass = PARRILLADA_TOOL_DIALOG_GET_CLASS (self);
	if (klass->cancel) {
		gboolean res;

		res = klass->cancel (self);
		if (!res)
			return FALSE;
	}

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (self);
	if (!priv->burn)
		return TRUE;

	if (parrillada_burn_cancel (priv->burn, TRUE) == PARRILLADA_BURN_DANGEROUS) {
		if (!parrillada_tool_dialog_cancel_dialog (GTK_WIDGET (self)))
			return FALSE;

		parrillada_burn_cancel (priv->burn, FALSE);
	}

	return TRUE;
}

static gboolean
parrillada_tool_dialog_delete (GtkWidget *widget, GdkEventAny *event)
{
	ParrilladaToolDialog *self;

	self = PARRILLADA_TOOL_DIALOG (widget);

	return (parrillada_tool_dialog_cancel (self) != TRUE);
}

static void
parrillada_tool_dialog_response (GtkDialog *dialog,
			      GtkResponseType response,
			      gpointer NULL_data)
{
	if (response == GTK_RESPONSE_CANCEL) {
		if (!parrillada_tool_dialog_cancel (PARRILLADA_TOOL_DIALOG (dialog)))
			g_signal_stop_emission_by_name (dialog, "response");
	}
	else if (response == GTK_RESPONSE_OK) {
		if (!parrillada_tool_dialog_run (PARRILLADA_TOOL_DIALOG (dialog)))
			g_signal_stop_emission_by_name (dialog, "response");
	}
}

static void
parrillada_tool_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_tool_dialog_constructed (GObject *object)
{
	ParrilladaToolDialogPrivate *priv;

	G_OBJECT_CLASS (parrillada_tool_dialog_parent_class)->constructed (object);

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (object);

	parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (priv->selector),
						  PARRILLADA_MEDIA_TYPE_REWRITABLE |
						  PARRILLADA_MEDIA_TYPE_WRITABLE |
						  PARRILLADA_MEDIA_TYPE_AUDIO |
						  PARRILLADA_MEDIA_TYPE_DATA);
}

static void
parrillada_tool_dialog_class_init (ParrilladaToolDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaToolDialogPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = parrillada_tool_dialog_finalize;
	object_class->constructed = parrillada_tool_dialog_constructed;

	widget_class->delete_event = parrillada_tool_dialog_delete;
}

static void
parrillada_tool_dialog_init (ParrilladaToolDialog *obj)
{
	GtkWidget *title;
	GtkWidget *content_area;
	gchar *title_str;
	ParrilladaToolDialogPrivate *priv;

	priv = PARRILLADA_TOOL_DIALOG_PRIVATE (obj);

	/* upper part */
	priv->upper_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (GTK_WIDGET (priv->upper_box));

	priv->selector = parrillada_medium_selection_new ();
	gtk_widget_show (GTK_WIDGET (priv->selector));

	title_str = g_strdup_printf ("<b>%s</b>", _("Select a disc"));
	gtk_box_pack_start (GTK_BOX (priv->upper_box),
			    parrillada_utils_pack_properties (title_str,
							   priv->selector,
							   NULL),
			    FALSE, FALSE, 0);
	g_free (title_str);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (obj));
	gtk_box_pack_start (GTK_BOX (content_area),
			    priv->upper_box,
			    FALSE,
			    FALSE,
			    0);

	/* lower part */
	priv->lower_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (priv->lower_box), 12);
	gtk_widget_set_sensitive (priv->lower_box, FALSE);
	gtk_widget_show (priv->lower_box);

	gtk_box_pack_start (GTK_BOX (content_area),
			    priv->lower_box,
			    FALSE,
			    FALSE,
			    0);

	title_str = g_strdup_printf ("<b>%s</b>", _("Progress"));
	title = gtk_label_new (title_str);
	g_free (title_str);

	gtk_label_set_use_markup (GTK_LABEL (title), TRUE);
	gtk_misc_set_alignment (GTK_MISC (title), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC (title), 0, 6);
	gtk_widget_show (title);
	gtk_box_pack_start (GTK_BOX (priv->lower_box),
			    title,
			    FALSE,
			    FALSE,
			    0);

	priv->progress = parrillada_burn_progress_new ();
	gtk_widget_show (priv->progress);
	g_object_set (G_OBJECT (priv->progress),
		      "show-info", FALSE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (content_area),
			    priv->progress,
			    FALSE,
			    FALSE,
			    0);

	/* buttons */
	priv->cancel = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (priv->cancel);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      priv->cancel,
				      GTK_RESPONSE_CANCEL);

	g_signal_connect (G_OBJECT (priv->selector),
			  "medium-changed",
			  G_CALLBACK (parrillada_tool_dialog_drive_changed_cb),
			  obj);

	g_signal_connect (obj,
			  "response",
			  G_CALLBACK (parrillada_tool_dialog_response),
			  NULL);

	gtk_window_resize (GTK_WINDOW (obj), 10, 10);
}

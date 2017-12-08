/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            project.c
 *
 *  mar nov 29 09:32:17 2005
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gio/gio.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <gst/gst.h>

#include <libxml/xmlerror.h>

#include "parrillada-units.h"

#include "parrillada-misc.h"
#include "parrillada-jacket-edit.h"
#include "parrillada-pk.h"

#include "parrillada-tags.h"
#include "parrillada-session.h"

#include "parrillada-setting.h"

#ifdef BUILD_PREVIEW
#include "parrillada-player.h"
#endif

#include "parrillada-track-data.h"
#include "parrillada-track-data-cfg.h"
#include "parrillada-track-stream-cfg.h"
#include "parrillada-session-cfg.h"

/* These includes are not in the exported *.h files by 
 * libparrillada-burn. */
#include "parrillada-medium-selection-priv.h"
#include "parrillada-session-helper.h"
#include "parrillada-dest-selection.h"
#include "parrillada-cover.h"
#include "parrillada-status-dialog.h"
#include "parrillada-video-options.h"
#include "parrillada-drive-properties.h"
#include "parrillada-image-properties.h"
#include "burn-plugin-manager.h"

#include "parrillada-project-type-chooser.h"
#include "parrillada-app.h"
#include "parrillada-project.h"
#include "parrillada-disc.h"
#include "parrillada-data-disc.h"
#include "parrillada-audio-disc.h"
#include "parrillada-video-disc.h"
#include "parrillada-uri-container.h"
#include "parrillada-layout-object.h"
#include "parrillada-disc-message.h"
#include "parrillada-file-chooser.h"
#include "parrillada-notify.h"
#include "parrillada-project-parse.h"
#include "parrillada-project-name.h"
#include "parrillada-drive-settings.h"

static void parrillada_project_class_init (ParrilladaProjectClass *klass);
static void parrillada_project_init (ParrilladaProject *sp);
static void parrillada_project_iface_uri_container_init (ParrilladaURIContainerIFace *iface);
static void parrillada_project_iface_layout_object_init (ParrilladaLayoutObjectIFace *iface);
static void parrillada_project_finalize (GObject *object);

static void
parrillada_project_save_cb (GtkAction *action, ParrilladaProject *project);
static void
parrillada_project_save_as_cb (GtkAction *action, ParrilladaProject *project);

static void
parrillada_project_add_uris_cb (GtkAction *action, ParrilladaProject *project);
static void
parrillada_project_remove_selected_uris_cb (GtkAction *action, ParrilladaProject *project);
static void
parrillada_project_empty_cb (GtkAction *action, ParrilladaProject *project);

static void
parrillada_project_burn_cb (GtkAction *action, ParrilladaProject *project);

static void
parrillada_project_burn_clicked_cb (GtkButton *button, ParrilladaProject *project);

static void
parrillada_project_selection_changed_cb (ParrilladaDisc *disc,
				      ParrilladaProject *project);

static gchar *
parrillada_project_get_selected_uri (ParrilladaURIContainer *container);
static gboolean
parrillada_project_get_boundaries (ParrilladaURIContainer *container,
				gint64 *start,
				gint64 *end);

static void
parrillada_project_get_proportion (ParrilladaLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer);

static void
parrillada_project_get_proportion (ParrilladaLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer);

struct ParrilladaProjectPrivate {
	ParrilladaSessionCfg *session;

	GtkWidget *help;
	GtkWidget *selection;
	GtkWidget *name_display;
	GtkWidget *discs;
	GtkWidget *audio;
	GtkWidget *data;
	GtkWidget *video;

	GtkWidget *message;

	GtkUIManager *manager;

	guint status_ctx;

	GtkWidget *project_status;

	/* header */
	GtkWidget *burn;

	GtkActionGroup *project_group;
	guint merge_id;

	gchar *project;

	gchar *cover;

	gint64 sectors;
	ParrilladaDisc *current;

	ParrilladaURIContainer *current_source;

	GCancellable *cancel;

	GtkWidget *chooser;
	gulong selected_id;
	gulong activated_id;

	guint is_burning:1;

    	guint burnt:1;

	guint empty:1;
	guint modified:1;
	guint has_focus:1;
	guint oversized:1;
	guint selected_uris:1;
};

static GtkActionEntry entries [] = {
	{"Save", GTK_STOCK_SAVE, NULL, NULL,
	 N_("Save current project"), G_CALLBACK (parrillada_project_save_cb)},
	{"SaveAs", GTK_STOCK_SAVE_AS, N_("Save _As…"), NULL,
	 N_("Save current project to a different location"), G_CALLBACK (parrillada_project_save_as_cb)},
	{"Add", GTK_STOCK_ADD, N_("_Add Files"), NULL,
	 N_("Add files to the project"), G_CALLBACK (parrillada_project_add_uris_cb)},
	{"DeleteProject", GTK_STOCK_REMOVE, N_("_Remove Files"), NULL,
	 N_("Remove the selected files from the project"), G_CALLBACK (parrillada_project_remove_selected_uris_cb)},
	/* Translators: "empty" is a verb here */
	{"DeleteAll", GTK_STOCK_CLEAR, N_("E_mpty Project"), NULL,
	 N_("Remove all files from the project"), G_CALLBACK (parrillada_project_empty_cb)},
	{"Burn", "media-optical-burn", N_("_Burn…"), NULL,
	 N_("Burn the disc"), G_CALLBACK (parrillada_project_burn_cb)},
};

static const gchar *description = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ProjectPlaceholder'>"
			    "<menuitem action='Save'/>"
			    "<menuitem action='SaveAs'/>"
			    "<separator/>"
			"<menuitem action='Burn'/>"
			"</placeholder>"
		"</menu>"
		
		"<menu action='EditMenu'>"
			"<placeholder name='EditPlaceholder'>"
			    "<menuitem action='Add'/>"
			    "<menuitem action='DeleteProject'/>"
			    "<menuitem action='DeleteAll'/>"
			    "<separator/>"
			"</placeholder>"
		"</menu>"

		"<menu action='ViewMenu'>"
		"</menu>"

		"<menu action='ToolMenu'>"
			"<placeholder name='DiscPlaceholder'/>"
		"</menu>"
	    "</menubar>"
	    "<toolbar name='Toolbar'>"
		"<separator/>"
		"<toolitem action='Add'/>"
		"<toolitem action='DeleteProject'/>"
		"<toolitem action='DeleteAll'/>"
		"<placeholder name='DiscButtonPlaceholder'/>"
	     "</toolbar>"
	"</ui>"
};

static GObjectClass *parent_class = NULL;

#define PARRILLADA_PROJECT_SIZE_WIDGET_BORDER	1

#define PARRILLADA_PROJECT_VERSION "0.2"

#define PARRILLADA_RESPONSE_ADD			1976

enum {
	TREE_MODEL_ROW = 150,
	FILE_NODES_ROW,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd[] = {
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, FILE_NODES_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd [0]);

GType
parrillada_project_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (ParrilladaProjectClass),
			NULL,
			NULL,
			(GClassInitFunc)parrillada_project_class_init,
			NULL,
			NULL,
			sizeof (ParrilladaProject),
			0,
			(GInstanceInitFunc) parrillada_project_init,
		};

		static const GInterfaceInfo uri_container_info =
		{
			(GInterfaceInitFunc) parrillada_project_iface_uri_container_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo layout_object =
		{
			(GInterfaceInitFunc) parrillada_project_iface_layout_object_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_BOX, 
					       "ParrilladaProject",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     PARRILLADA_TYPE_URI_CONTAINER,
					     &uri_container_info);
		g_type_add_interface_static (type,
					     PARRILLADA_TYPE_LAYOUT_OBJECT,
					     &layout_object);
	}

	return type;
}

static void
parrillada_project_class_init (ParrilladaProjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_project_finalize;
}

static void
parrillada_project_iface_uri_container_init (ParrilladaURIContainerIFace *iface)
{
	iface->get_selected_uri = parrillada_project_get_selected_uri;
	iface->get_boundaries = parrillada_project_get_boundaries;
}

static void
parrillada_project_iface_layout_object_init (ParrilladaLayoutObjectIFace *iface)
{
	iface->get_proportion = parrillada_project_get_proportion;
}

static void
parrillada_project_get_proportion (ParrilladaLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer)
{
	GtkAllocation allocation;

	if (!PARRILLADA_PROJECT (object)->priv->name_display)
		return;

	gtk_widget_get_allocation (gtk_widget_get_parent (PARRILLADA_PROJECT (object)->priv->name_display),
				   &allocation);
	*footer = allocation.height;
}

static void
parrillada_project_set_remove_button_state (ParrilladaProject *project)
{
	GtkAction *action;
	gboolean sensitive;

	sensitive = (project->priv->has_focus &&
	             project->priv->selected_uris);

	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_sensitive (action, sensitive);
}

static void
parrillada_project_set_add_button_state (ParrilladaProject *project)
{
	GtkAction *action;
	GtkWidget *widget;
	gboolean sensitive;
	GtkWidget *toplevel;

	sensitive = ((!project->priv->current_source || !project->priv->has_focus) &&
		      !project->priv->oversized && !project->priv->chooser);

	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, sensitive);

	/* set the Add button to be the default for the whole window. That fixes 
	 * #465175 – Location field not working. GtkFileChooser needs a default
	 * widget to be activated. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	if (!sensitive) {
		gtk_window_set_default (GTK_WINDOW (toplevel), NULL);
		return;
	}

	widget = gtk_ui_manager_get_widget (project->priv->manager, "/Toolbar/Add");
	if (!widget)
		return;

	widget = gtk_bin_get_child (GTK_BIN (widget));
	gtk_widget_set_can_default (widget, TRUE);

	gtk_window_set_default (GTK_WINDOW (toplevel), widget);
}

static void
parrillada_project_focus_changed_cb (GtkContainer *container,
				  GtkWidget *widget,
				  gpointer NULL_data)
{
	ParrilladaProject *project;

	project = PARRILLADA_PROJECT (container);
	project->priv->has_focus = (widget != NULL);

	parrillada_project_set_remove_button_state (project);
	parrillada_project_set_add_button_state (project);
}

static void
parrillada_project_name_changed_cb (ParrilladaProjectName *name,
				 ParrilladaProject *project)
{
	GtkAction *action;

	project->priv->modified = TRUE;

	/* the state of the following depends on the existence of an opened project */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	if (project->priv->modified)
		gtk_action_set_sensitive (action, TRUE);
	else
		gtk_action_set_sensitive (action, FALSE);
}

/********************************** help ***************************************/
static GtkWidget *
parrillada_utils_disc_find_tree_view_in_container (GtkContainer *container)
{
	GList *children;
	GList *iter;

	children = gtk_container_get_children (container);
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		if (GTK_IS_TREE_VIEW (widget)) {
			g_list_free (children);
			return widget;
		}

		if (GTK_IS_CONTAINER (widget)) {
			widget = parrillada_utils_disc_find_tree_view_in_container (GTK_CONTAINER (widget));
			if (widget) {
				g_list_free (children);
				return widget;
			}
		}
	}
	g_list_free (children);

	return NULL;
}

static GtkWidget *
parrillada_utils_disc_find_tree_view (ParrilladaDisc *widget)
{
	return parrillada_utils_disc_find_tree_view_in_container (GTK_CONTAINER (widget));
}

static void
parrillada_utils_disc_hide_use_info_leave_cb (GtkWidget *widget,
					   GdkDragContext *drag_context,
					   guint time,
					   ParrilladaProject *project)
{
	GtkWidget *other_widget;

	other_widget = parrillada_utils_disc_find_tree_view (project->priv->current);
	if (!other_widget)
		return;

	g_signal_emit_by_name (other_widget,
			       "drag-leave",
			       drag_context,
			       time);
}

static gboolean
parrillada_utils_disc_hide_use_info_drop_cb (GtkWidget *widget,
					  GdkDragContext *drag_context,
					  gint x,
					  gint y,
					  guint time,
					  ParrilladaProject *project)
{
	GdkAtom target = GDK_NONE;
	GtkWidget *other_widget;

	/* here the treeview is not realized so we'll have a warning message
	 * if we ever try to forward the event */
	other_widget = parrillada_utils_disc_find_tree_view (project->priv->current);
	if (!other_widget)
		return FALSE;

	target = gtk_drag_dest_find_target (other_widget,
					    drag_context,
					    gtk_drag_dest_get_target_list (GTK_WIDGET (other_widget)));

	if (target != GDK_NONE) {
		gboolean return_value = FALSE;

		/* It's necessary to make sure the treeview
		 * is realized already before sending the
		 * signal */
		gtk_widget_realize (other_widget);

		/* The widget must be realized to receive such events. */
		g_signal_emit_by_name (other_widget,
				       "drag-drop",
				       drag_context,
				       x,
				       y,
				       time,
				       &return_value);
		return return_value;
	}

	return FALSE;
}

static void
parrillada_utils_disc_hide_use_info_data_received_cb (GtkWidget *widget,
						   GdkDragContext *drag_context,
						   gint x,
						   gint y,
						   GtkSelectionData *data,
						   guint info,
						   guint time,
						   ParrilladaProject *project)
{
	GtkWidget *other_widget;

	g_return_if_fail(PARRILLADA_IS_PROJECT(project));

	other_widget = parrillada_utils_disc_find_tree_view (project->priv->current);
	if (!other_widget)
		return;

	g_signal_emit_by_name (other_widget,
			       "drag-data-received",
			       drag_context,
			       x,
			       y,
			       data,
			       info,
			       time);
}

static gboolean
parrillada_utils_disc_hide_use_info_motion_cb (GtkWidget *widget,
					    GdkDragContext *drag_context,
					    gint x,
					    gint y,
					    guint time,
					    GtkNotebook *notebook)
{
	return TRUE;
}

static gboolean
parrillada_utils_disc_hide_use_info_button_cb (GtkWidget *widget,
					    GdkEventButton *event,
					    ParrilladaProject *project)
{
	GtkWidget *other_widget;
	gboolean result;

	if (event->button != 3)
		return TRUE;

	other_widget = parrillada_utils_disc_find_tree_view (project->priv->current);
	if (!other_widget)
		return TRUE;

	g_signal_emit_by_name (other_widget,
			       "button-press-event",
			       event,
			       &result);

	return result;
}

static void
parrillada_utils_disc_style_changed_cb (GtkWidget *widget,
				     GtkStyle *previous,
				     GtkWidget *event_box)
{
	GdkRGBA color;

	/* The widget (a treeview here) needs to be realized to get proper style */
	gtk_widget_realize (widget);
	gdk_rgba_parse (&color, "white");
	gtk_widget_override_background_color (event_box, GTK_STATE_NORMAL, &color);
}

static void
parrillada_utils_disc_realized_cb (GtkWidget *event_box,
				GtkWidget *textview)
{
	GdkRGBA color;

	/* The widget (a treeview here) needs to be realized to get proper style */
	gtk_widget_realize (textview);
	gdk_rgba_parse (&color, "white");
	gtk_widget_override_background_color (event_box, GTK_STATE_NORMAL, &color);

	g_signal_handlers_disconnect_by_func (textview,
					      parrillada_utils_disc_style_changed_cb,
					      event_box);
	g_signal_connect (textview,
			  "style-set",
			  G_CALLBACK (parrillada_utils_disc_style_changed_cb),
			  event_box);
}

static GtkWidget *
parrillada_disc_get_use_info_notebook (ParrilladaProject *project)
{
	GList *chain;
	GtkTextIter iter;
	GtkWidget *frame;
	GtkWidget *textview;
	GtkWidget *notebook;
	GtkWidget *alignment;
	GtkTextBuffer *buffer;
	GtkWidget *event_box;

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  frame,
				  NULL);

	/* Now this event box must be 'transparent' to have the same background 
	 * color as a treeview */
	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), TRUE);
	gtk_event_box_set_above_child (GTK_EVENT_BOX (event_box), TRUE);
	gtk_drag_dest_set (event_box, 
			   GTK_DEST_DEFAULT_MOTION,
			   ntables_cd,
			   nb_targets_cd,
			   GDK_ACTION_COPY|
			   GDK_ACTION_MOVE);

	/* the following signals need to be forwarded to the widget underneath */
	g_signal_connect (event_box,
			  "drag-motion",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_motion_cb),
			  project);
	g_signal_connect (event_box,
			  "drag-leave",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_leave_cb),
			  project);
	g_signal_connect (event_box,
			  "drag-drop",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_drop_cb),
			  project);
	g_signal_connect (event_box,
			  "button-press-event",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_button_cb),
			  project);
	g_signal_connect (event_box,
			  "drag-data-received",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_data_received_cb),
			  project);

	gtk_container_add (GTK_CONTAINER (frame), event_box);

	/* The alignment to set properly the position of the GtkTextView */
	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 0.0);
	gtk_container_set_border_width (GTK_CONTAINER (alignment), 10);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (event_box), alignment);

	/* The TreeView for the message */
	buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_create_tag (buffer, "Title",
	                            "scale", 1.1,
	                            "justification", GTK_JUSTIFY_CENTER,
	                            "foreground", "grey50",
	                            "wrap-mode", GTK_WRAP_WORD,
	                            NULL);

	gtk_text_buffer_get_start_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, _("To add files to this project click the \"Add\" button or drag files to this area"), -1, "Title", NULL);
	gtk_text_buffer_insert (buffer, &iter, "\n\n\n", -1);
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, _("To remove files select them then click on the \"Remove\" button or press \"Delete\" key"), -1, "Title", NULL);

	textview = gtk_text_view_new_with_buffer (buffer);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);

	gtk_drag_dest_set (textview, 
			   GTK_DEST_DEFAULT_MOTION,
			   ntables_cd,
			   nb_targets_cd,
			   GDK_ACTION_COPY|
			   GDK_ACTION_MOVE);

	/* the following signals need to be forwarded to the widget underneath */
	g_signal_connect (textview,
			  "drag-motion",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_motion_cb),
			  project);
	g_signal_connect (textview,
			  "drag-leave",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_leave_cb),
			  project);
	g_signal_connect (textview,
			  "drag-drop",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_drop_cb),
			  project);
	g_signal_connect (textview,
			  "button-press-event",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_button_cb),
			  project);
	g_signal_connect (textview,
			  "drag-data-received",
			  G_CALLBACK (parrillada_utils_disc_hide_use_info_data_received_cb),
			  project);

	gtk_container_add (GTK_CONTAINER (alignment), textview);

	g_signal_connect (event_box,
			  "realize",
			  G_CALLBACK (parrillada_utils_disc_realized_cb),
			  project);

	chain = g_list_prepend (NULL, event_box);
	gtk_container_set_focus_chain (GTK_CONTAINER (frame), chain);
	g_list_free (chain);

	chain = g_list_prepend (NULL, alignment);
	gtk_container_set_focus_chain (GTK_CONTAINER (event_box), chain);
	g_list_free (chain);

	chain = g_list_prepend (NULL, textview);
	gtk_container_set_focus_chain (GTK_CONTAINER (alignment), chain);
	g_list_free (chain);

	gtk_widget_show_all (notebook);
	return notebook;
}

/********************************** size ***************************************/
static gchar *
parrillada_project_get_sectors_string (gint64 sectors,
				    ParrilladaTrackType *type)
{
	gint64 size_bytes;

	if (parrillada_track_type_get_has_stream (type)) {
		if (PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (type)))
			/* This is an embarassing problem: this is an approximation
			 * based on the fact that 2 hours = 4.3GiB */
			size_bytes = sectors * 2048LL * 72000LL / 47LL;
		else
			size_bytes = sectors * GST_SECOND / 75LL;
		return parrillada_units_get_time_string (size_bytes, TRUE, FALSE);
	}
	else {
		size_bytes = sectors * 2048LL;
		return g_format_size (size_bytes);
	}
}

static void
parrillada_project_update_project_size (ParrilladaProject *project)
{
	ParrilladaTrackType *session_type;
	goffset sectors = 0;
	GtkWidget *status;
	gchar *size_str;
	gchar *string;

	status = parrillada_app_get_statusbar2 (parrillada_app_get_default ());

	if (!project->priv->status_ctx)
		project->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
									  "size_project");

	gtk_statusbar_pop (GTK_STATUSBAR (status), project->priv->status_ctx);

	parrillada_burn_session_get_size (PARRILLADA_BURN_SESSION (project->priv->session),
				       &sectors,
				       NULL);

	session_type = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (project->priv->session), session_type);

	string = parrillada_project_get_sectors_string (sectors, session_type);
	parrillada_track_type_free (session_type);

	size_str = g_strdup_printf (_("Estimated project size: %s"), string);
	g_free (string);

	gtk_statusbar_push (GTK_STATUSBAR (status), project->priv->status_ctx, size_str);
	g_free (size_str);
}

static void
parrillada_project_update_controls (ParrilladaProject *project)
{
	GtkAction *action;

	parrillada_project_set_remove_button_state (project);
	parrillada_project_set_add_button_state (project);

	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_sensitive (action, (parrillada_disc_is_empty (PARRILLADA_DISC (project->priv->current))));
}

static void
parrillada_project_modified (ParrilladaProject *project)
{
	GtkAction *action;

	parrillada_project_update_controls (project);
	parrillada_project_update_project_size (project);

	/* the state of the following depends on the existence of an opened project */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, TRUE);
	project->priv->modified = TRUE;
}

static void
parrillada_project_track_removed (ParrilladaBurnSession *session,
			       ParrilladaTrack *track,
			       guint former_position,
			       ParrilladaProject *project)
{
	parrillada_project_modified (project);
}

static void
parrillada_project_track_changed (ParrilladaBurnSession *session,
			       ParrilladaTrack *track,
			       ParrilladaProject *project)
{
	parrillada_project_modified (project);
}

static void
parrillada_project_track_added (ParrilladaBurnSession *session,
			     ParrilladaTrack *track,
			     ParrilladaProject *project)
{
	parrillada_project_modified (project);
}

static void
parrillada_project_message_response_span_cb (ParrilladaDiscMessage *message,
					  GtkResponseType response,
					  ParrilladaProject *project)
{
	if (response == GTK_RESPONSE_OK)
		parrillada_session_span_start (PARRILLADA_SESSION_SPAN (project->priv->session));
}

static void
parrillada_project_message_response_overburn_cb (ParrilladaDiscMessage *message,
					      GtkResponseType response,
					      ParrilladaProject *project)
{
	if (response == GTK_RESPONSE_OK)
		parrillada_session_cfg_add_flags (project->priv->session, PARRILLADA_BURN_FLAG_OVERBURN);
}

static void
parrillada_project_is_valid (ParrilladaSessionCfg *session,
			  ParrilladaProject *project)
{
	ParrilladaSessionError valid;
	GdkWindow *window;
	GdkCursor *cursor;
	GtkAction *action;

	/* Update the cursor */
	window = gtk_widget_get_window (GTK_WIDGET (project));
	if (window) {
		ParrilladaStatus *status;

		status = parrillada_status_new ();
		parrillada_burn_session_get_status (PARRILLADA_BURN_SESSION (session), status);
		if (parrillada_status_get_result (status) == PARRILLADA_BURN_NOT_READY
		||  parrillada_status_get_result (status) == PARRILLADA_BURN_RUNNING) {
			cursor = gdk_cursor_new (GDK_WATCH);
			gdk_window_set_cursor (window, cursor);
			g_object_unref (cursor);
		}
		else
			gdk_window_set_cursor (window, NULL);

		g_object_unref (status);
	}

	valid = parrillada_session_cfg_get_error (project->priv->session);

	/* Update burn button state */
	action = gtk_action_group_get_action (project->priv->project_group, "Burn");
	gtk_action_set_sensitive (action, PARRILLADA_SESSION_IS_VALID (valid));
	gtk_widget_set_sensitive (project->priv->burn, PARRILLADA_SESSION_IS_VALID (valid));

	/* FIXME: update option button state as well */

	/* NOTE: empty error is the first checked error; so if another errors comes up
	 * that means that file selection is not empty */

	/* Clean any message */
	parrillada_notify_message_remove (project->priv->message,
				       PARRILLADA_NOTIFY_CONTEXT_SIZE);

	if (valid == PARRILLADA_SESSION_EMPTY) {
		project->priv->empty = TRUE;
		project->priv->oversized = FALSE;
	}
	else if (valid == PARRILLADA_SESSION_INSUFFICIENT_SPACE) {
		goffset min_disc_size;
		goffset available_space;

		project->priv->oversized = TRUE;
		project->priv->empty = FALSE;

		min_disc_size = parrillada_session_span_get_max_space (PARRILLADA_SESSION_SPAN (session));

		/* One rule should be that the maximum batch size should not exceed the disc size
		 * FIXME! we could change it into a dialog telling the user what is the maximum
		 * size required. */
		available_space = parrillada_burn_session_get_available_medium_space (PARRILLADA_BURN_SESSION (session));

		/* Here there is an alternative: we may be able to span the data
		 * across multiple media. So try that. */
		if (available_space > min_disc_size
		&& parrillada_session_span_possible (PARRILLADA_SESSION_SPAN (session)) == PARRILLADA_BURN_RETRY) {
			GtkWidget *message;

			message = parrillada_notify_message_add (project->priv->message,
							      _("Would you like to burn the selection of files across several media?"),
							      _("The project is too large for the disc even with the overburn option."),
							      -1,
							      PARRILLADA_NOTIFY_CONTEXT_SIZE);
			gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
									      _("_Burn Several Discs"),
								    	      GTK_RESPONSE_OK),
						     _("Burn the selection of files across several media"));

			g_signal_connect (message,
					  "response",
					  G_CALLBACK (parrillada_project_message_response_span_cb),
					  project);
		}
		else
			parrillada_notify_message_add (project->priv->message,
						    _("Please choose another CD or DVD or insert a new one."),
						    _("The project is too large for the disc even with the overburn option."),
						    -1,
						    PARRILLADA_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == PARRILLADA_SESSION_OVERBURN_NECESSARY) {
		GtkWidget *message;

		project->priv->empty = FALSE;
		project->priv->oversized = TRUE;
		message = parrillada_notify_message_add (project->priv->message,
						      _("Would you like to burn beyond the disc's reported capacity?"),
						      _("The project is too large for the disc and you must remove files from it."
							"\nYou may want to use this option if you're using 90 or 100 min CD-R(W) which cannot be properly recognized and therefore needs the overburn option."
							"\nNote: This option might cause failure."),
						      -1,
						      PARRILLADA_NOTIFY_CONTEXT_SIZE);
		gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
								      _("_Overburn"),
							    	      GTK_RESPONSE_OK),
					     _("Burn beyond the disc's reported capacity"));

		g_signal_connect (message,
				  "response",
				  G_CALLBACK (parrillada_project_message_response_overburn_cb),
				  project);
	}
	else if (valid == PARRILLADA_SESSION_NO_OUTPUT) {
		project->priv->empty = FALSE;
		parrillada_notify_message_add (project->priv->message,
					    _("Please insert a writable CD or DVD."),
					    NULL,
					    -1,
					    PARRILLADA_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == PARRILLADA_SESSION_NOT_SUPPORTED) {
		project->priv->empty = FALSE;
		parrillada_notify_message_add (project->priv->message,
					    _("Please replace the disc with a supported CD or DVD."),
					    NULL,
					    -1,
					    PARRILLADA_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == PARRILLADA_SESSION_NO_CD_TEXT) {
		parrillada_notify_message_add (project->priv->message,
					    _("No track information (artist, title, ...) will be written to the disc."),
					    _("This is not supported by the current active burning backend."),
					    -1,
					    PARRILLADA_NOTIFY_CONTEXT_SIZE);
	}
	else if (parrillada_burn_session_is_dest_file (PARRILLADA_BURN_SESSION (project->priv->session))
	     &&  parrillada_medium_selection_get_media_num (PARRILLADA_MEDIUM_SELECTION (project->priv->selection)) == 1) {
		/* The user may have forgotten to insert a disc so remind him of that if
		 * there aren't any other possibility in the selection */
		parrillada_notify_message_add (project->priv->message,
					    _("Please insert a writable CD or DVD if you don't want to write to an image file."),
					    NULL,
					    10000,
					    PARRILLADA_NOTIFY_CONTEXT_SIZE);
	}

	if (PARRILLADA_SESSION_IS_VALID (valid) || valid == PARRILLADA_SESSION_NOT_READY)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->help), 0);

	if (PARRILLADA_SESSION_IS_VALID (valid)) {
		project->priv->empty = FALSE;
		project->priv->oversized = FALSE;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->help),
	                               parrillada_disc_is_empty (PARRILLADA_DISC (project->priv->current))? 0:1);
}

static void
parrillada_project_init (ParrilladaProject *obj)
{
	GtkSizeGroup *name_size_group;
	GtkSizeGroup *size_group;
	GtkWidget *alignment;
	GtkWidget *selector;
	GtkWidget *table;

	obj->priv = g_new0 (ParrilladaProjectPrivate, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (obj), GTK_ORIENTATION_VERTICAL);

	g_signal_connect (G_OBJECT (obj),
			  "set-focus-child",
			  G_CALLBACK (parrillada_project_focus_changed_cb),
			  NULL);

	obj->priv->message = parrillada_notify_new ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->message, FALSE, TRUE, 0);
	gtk_widget_show (obj->priv->message);

	/* bottom */
	table = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_end (GTK_BOX (obj), table, FALSE, TRUE, 0);

	/* Media selection widget */
	name_size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
	selector = parrillada_dest_selection_new (NULL);
	gtk_size_group_add_widget (GTK_SIZE_GROUP (name_size_group), selector);
	g_object_unref (name_size_group);

	gtk_widget_show (selector);
	obj->priv->selection = selector;

	gtk_table_attach (GTK_TABLE (table), selector,
			  0, 2,
			  1, 2,
			  GTK_FILL|GTK_EXPAND,
			  GTK_FILL|GTK_EXPAND,
			  0, 0);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	obj->priv->burn = parrillada_utils_make_button (_("_Burn…"),
						     NULL,
						     "media-optical-burn",
						     GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (obj->priv->burn);
	gtk_widget_set_sensitive (obj->priv->burn, FALSE);
	gtk_button_set_focus_on_click (GTK_BUTTON (obj->priv->burn), FALSE);
	g_signal_connect (obj->priv->burn,
			  "clicked",
			  G_CALLBACK (parrillada_project_burn_clicked_cb),
			  obj);
	gtk_widget_set_tooltip_text (obj->priv->burn,
				     _("Start to burn the contents of the selection"));
	gtk_size_group_add_widget (GTK_SIZE_GROUP (size_group), obj->priv->burn);

	alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->burn);
	gtk_table_attach (GTK_TABLE (table), alignment,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);

	/* Name widget */
	obj->priv->name_display = parrillada_project_name_new (PARRILLADA_BURN_SESSION (obj->priv->session));
	gtk_size_group_add_widget (GTK_SIZE_GROUP (name_size_group), obj->priv->name_display);
	gtk_widget_show (obj->priv->name_display);
	gtk_table_attach (GTK_TABLE (table), obj->priv->name_display,
			  0, 2,
			  0, 1,
			  GTK_EXPAND|GTK_FILL,
			  GTK_EXPAND|GTK_FILL,
			  0, 0);
	obj->priv->empty = 1;

	g_signal_connect (obj->priv->name_display,
			  "name-changed",
			  G_CALLBACK (parrillada_project_name_changed_cb),
			  obj);

	/* The three panes to put into the notebook */
	obj->priv->audio = parrillada_audio_disc_new ();
	gtk_widget_show (obj->priv->audio);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "selection-changed",
			  G_CALLBACK (parrillada_project_selection_changed_cb),
			  obj);

	obj->priv->data = parrillada_data_disc_new ();
	gtk_widget_show (obj->priv->data);
	parrillada_data_disc_set_right_button_group (PARRILLADA_DATA_DISC (obj->priv->data), size_group);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "selection-changed",
			  G_CALLBACK (parrillada_project_selection_changed_cb),
			  obj);

	obj->priv->video = parrillada_video_disc_new ();
	gtk_widget_show (obj->priv->video);
	g_signal_connect (G_OBJECT (obj->priv->video),
			  "selection-changed",
			  G_CALLBACK (parrillada_project_selection_changed_cb),
			  obj);

	obj->priv->help = parrillada_disc_get_use_info_notebook (obj);
	gtk_widget_show (obj->priv->help);

	obj->priv->discs = gtk_notebook_new ();
	gtk_widget_show (obj->priv->discs);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->discs), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->discs), FALSE);

	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->video, NULL);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->data, NULL);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->audio, NULL);

	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->help),
				   obj->priv->discs, NULL);

	gtk_box_pack_start (GTK_BOX (obj),
			    obj->priv->help,
			    TRUE,
			    TRUE,
			    0);

	g_object_unref (size_group);
}

static void
parrillada_project_finalize (GObject *object)
{
	ParrilladaProject *cobj;
	cobj = PARRILLADA_PROJECT(object);

	if (cobj->priv->cancel) {
		g_cancellable_cancel (cobj->priv->cancel);
		cobj->priv->cancel = NULL;
	}

	if (cobj->priv->session) {
		g_object_unref (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	if (cobj->priv->project)
		g_free (cobj->priv->project);

	if (cobj->priv->cover)
		g_free (cobj->priv->cover);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
parrillada_project_new ()
{
	ParrilladaProject *obj;
	
	obj = PARRILLADA_PROJECT (g_object_new (PARRILLADA_TYPE_PROJECT, NULL));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (obj->priv->discs), 0);

	return GTK_WIDGET (obj);
}

/***************************** URIContainer ************************************/
static void
parrillada_project_selection_changed_cb (ParrilladaDisc *disc,
				      ParrilladaProject *project)
{
	project->priv->selected_uris = parrillada_disc_get_selected_uri (project->priv->current, NULL);
	parrillada_project_set_remove_button_state (project);
	parrillada_uri_container_uri_selected (PARRILLADA_URI_CONTAINER (project));
}

static gchar *
parrillada_project_get_selected_uri (ParrilladaURIContainer *container)
{
	ParrilladaProject *project;
	gchar *uri = NULL;

	project = PARRILLADA_PROJECT (container);

	/* if we are burning we better not return anything so as to stop 
	 * preview widget from carrying on to play */
	if (project->priv->is_burning)
		return NULL;

	if (parrillada_disc_get_selected_uri (project->priv->current, &uri))
		return uri;

	return NULL;
}

static gboolean
parrillada_project_get_boundaries (ParrilladaURIContainer *container,
				gint64 *start,
				gint64 *end)
{
	ParrilladaProject *project;

	project = PARRILLADA_PROJECT (container);

	/* if we are burning we better not return anything so as to stop 
	 * preview widget from carrying on to play */
	if (project->priv->is_burning)
		return FALSE;

	return parrillada_disc_get_boundaries (project->priv->current,
					    start,
					    end);
}

static void
parrillada_project_no_song_dialog (ParrilladaProject *project)
{
	parrillada_app_alert (parrillada_app_get_default (),
			   _("Please add songs to the project."),
			   _("There are no songs to write to disc"),
			   GTK_MESSAGE_WARNING);
}

static void
parrillada_project_no_file_dialog (ParrilladaProject *project)
{
	parrillada_app_alert (parrillada_app_get_default (),
			   _("Please add files to the project."),
			   _("There are no files to write to disc"),
			   GTK_MESSAGE_WARNING);
}

static ParrilladaBurnResult
parrillada_project_check_status (ParrilladaProject *project)
{
        GtkWidget *dialog;
        ParrilladaStatus *status;
	GtkResponseType response;
	ParrilladaBurnResult result;

        status = parrillada_status_new ();
        parrillada_burn_session_get_status (PARRILLADA_BURN_SESSION (project->priv->session), status);
        result = parrillada_status_get_result (status);
        g_object_unref (status);

        if (result == PARRILLADA_BURN_ERR) {
                /* At the moment the only error possible is an empty project */
		if (PARRILLADA_IS_AUDIO_DISC (project->priv->current))
			parrillada_project_no_song_dialog (project);
		else
			parrillada_project_no_file_dialog (project);

		return PARRILLADA_BURN_ERR;
	}

	if (result == PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_OK;

        dialog = parrillada_status_dialog_new (PARRILLADA_BURN_SESSION (project->priv->session),
                                            gtk_widget_get_toplevel (GTK_WIDGET (project)));

	gtk_widget_show (dialog);
        response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        return (response == GTK_RESPONSE_OK)? PARRILLADA_BURN_OK:PARRILLADA_BURN_CANCEL;
}

static ParrilladaBurnResult
parrillada_project_install_missing (ParrilladaPluginErrorType type,
                                 const gchar *detail,
                                 gpointer user_data)
{
	ParrilladaProject *project = PARRILLADA_PROJECT (user_data);
	GCancellable *cancel;
	ParrilladaPK *package;
	GtkWidget *parent;
	gboolean res;
	int xid = 0;

	/* Get the xid */
	parent = gtk_widget_get_toplevel (GTK_WIDGET (project));
	xid = gdk_x11_window_get_xid (gtk_widget_get_window (parent));

	package = parrillada_pk_new ();
	cancel = g_cancellable_new ();
	project->priv->cancel = cancel;
	switch (type) {
		case PARRILLADA_PLUGIN_ERROR_MISSING_APP:
			res = parrillada_pk_install_missing_app (package, detail, xid, cancel);
			break;

		case PARRILLADA_PLUGIN_ERROR_MISSING_LIBRARY:
			res = parrillada_pk_install_missing_library (package, detail, xid, cancel);
			break;

		case PARRILLADA_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN:
			res = parrillada_pk_install_gstreamer_plugin (package, detail, xid, cancel);
			break;

		default:
			res = FALSE;
			break;
	}

	if (package) {
		g_object_unref (package);
		package = NULL;
	}

	if (g_cancellable_is_cancelled (cancel)) {
		g_object_unref (cancel);
		return PARRILLADA_BURN_CANCEL;
	}

	project->priv->cancel = NULL;
	g_object_unref (cancel);

	if (!res)
		return PARRILLADA_BURN_ERR;

	return PARRILLADA_BURN_RETRY;
}

static ParrilladaBurnResult
parrillada_project_list_missing (ParrilladaPluginErrorType type,
                              const gchar *detail,
                              gpointer user_data)
{
	GString *string = user_data;

	if (type == PARRILLADA_PLUGIN_ERROR_MISSING_APP ||
	    type == PARRILLADA_PLUGIN_ERROR_SYMBOLIC_LINK_APP ||
	    type == PARRILLADA_PLUGIN_ERROR_WRONG_APP_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing application */
		g_string_append_printf (string, _("%s (application)"), detail);
	}
	else if (type == PARRILLADA_PLUGIN_ERROR_MISSING_LIBRARY ||
	         type == PARRILLADA_PLUGIN_ERROR_LIBRARY_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing library */
		g_string_append_printf (string, _("%s (library)"), detail);
	}
	else if (type == PARRILLADA_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing GStreamer plugin */
		g_string_append_printf (string, _("%s (GStreamer plugin)"), detail);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_project_check_plugins_not_ready (ParrilladaProject *project,
                                         ParrilladaBurnSession *session)
{
	ParrilladaBurnResult result;
	GtkWidget *parent;
	GString *string;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (project));
	gtk_widget_set_sensitive (parent, FALSE);

	parrillada_burn_session_set_strict_support (PARRILLADA_BURN_SESSION (session), TRUE);
	result = parrillada_burn_session_can_burn (session, FALSE);
	parrillada_burn_session_set_strict_support (PARRILLADA_BURN_SESSION (session), FALSE);

	if (result == PARRILLADA_BURN_OK) {
		gtk_widget_set_sensitive (parent, TRUE);
		return result;
	}

	result = parrillada_burn_session_can_burn (session, FALSE);
	if (result != PARRILLADA_BURN_OK) {
		gtk_widget_set_sensitive (parent, TRUE);
		return result;
	}

	result = parrillada_session_foreach_plugin_error (session,
	                                               parrillada_project_install_missing,
	                                               project);
	gtk_widget_set_sensitive (parent, TRUE);

	if (result == PARRILLADA_BURN_CANCEL)
		return result;

	if (result == PARRILLADA_BURN_OK)
		return result;

	string = g_string_new (_("Please install the following manually and try again:"));
	parrillada_session_foreach_plugin_error (session,
	                                      parrillada_project_list_missing,
	                                      string);

	parrillada_utils_message_dialog (parent,
	                              _("All required applications and libraries are not installed."),
	                              string->str,
	                              GTK_MESSAGE_ERROR);
	g_string_free (string, TRUE);

	return PARRILLADA_BURN_ERR;
}

/******************************** burning **************************************/
static void
parrillada_project_setup_session (ParrilladaProject *project,
			       ParrilladaBurnSession *session)
{
	const gchar *label;

	label = gtk_entry_get_text (GTK_ENTRY (project->priv->name_display));
	parrillada_burn_session_set_label (session, label);

	if (project->priv->cover) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, project->priv->cover);
		parrillada_burn_session_tag_add (session,
					      PARRILLADA_COVER_URI,
					      value);
	}
}

static void
parrillada_project_output_changed (ParrilladaBurnSession *session,
                                ParrilladaMedium *former_medium,
                                GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

static ParrilladaBurnResult
parrillada_project_drive_properties (ParrilladaProject *project)
{
	ParrilladaTrackType *track_type;
	GtkWidget *medium_prop;
	GtkResponseType answer;
	ParrilladaDrive *drive;
	gchar *display_name;
	GtkWidget *options;
	GtkWidget *button;
	GtkWidget *dialog;
	glong cancel_sig;
	GtkWidget *box;
	gchar *header;
	gchar *string;

	/* Build dialog */
	drive = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (project->priv->session));

	display_name = parrillada_drive_get_display_name (drive);
	header = g_strdup_printf (_("Properties of %s"), display_name);
	g_free (display_name);

	dialog = gtk_dialog_new_with_buttons (header,
					      NULL,
					      GTK_DIALOG_MODAL|
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      NULL);
	g_free (header);

	/* This is in case the medium gets ejected instead of our locking it */
	cancel_sig = g_signal_connect (project->priv->session,
	                               "output-changed",
	                               G_CALLBACK (parrillada_project_output_changed),
	                               dialog);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Burn _Several Copies"),
			       GTK_RESPONSE_ACCEPT);

	button = parrillada_utils_make_button (_("_Burn"),
					    NULL,
					    "media-optical-burn",
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_OK);

	box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	track_type = parrillada_track_type_new ();

	parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (project->priv->session), track_type);
	if (parrillada_track_type_get_has_stream (track_type)
	&& PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (track_type))) {
		/* Special case for video project */
		options = parrillada_video_options_new (PARRILLADA_BURN_SESSION (project->priv->session));
		gtk_widget_show (options);

		string = g_strdup_printf ("<b>%s</b>", _("Video Options"));
		options = parrillada_utils_pack_properties (string,
							 options,
							 NULL);
		g_free (string);
		gtk_box_pack_start (GTK_BOX (box), options, FALSE, TRUE, 0);
	}

	parrillada_track_type_free (track_type);

	medium_prop = parrillada_drive_properties_new (project->priv->session);
	gtk_box_pack_start (GTK_BOX (box), medium_prop, TRUE, TRUE, 0);
	gtk_widget_show (medium_prop);

	parrillada_app_set_toplevel (parrillada_app_get_default (), GTK_WINDOW (dialog));

	/* launch the dialog */
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_signal_handler_disconnect (project->priv->session, cancel_sig);

	if (answer == GTK_RESPONSE_OK)
		return PARRILLADA_BURN_OK;

	if (answer == GTK_RESPONSE_ACCEPT)
		return PARRILLADA_BURN_RETRY;

	return PARRILLADA_BURN_CANCEL;
}

static gboolean
parrillada_project_image_properties (ParrilladaProject *project)
{
	ParrilladaTrackType *track_type;
	GtkResponseType answer;
	GtkWidget *button;
	GtkWidget *dialog;

	/* Build dialog */
	dialog = parrillada_image_properties_new ();

	parrillada_app_set_toplevel (parrillada_app_get_default (), GTK_WINDOW (dialog));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);

	button = gtk_dialog_add_button (GTK_DIALOG (dialog),
					_("Create _Image"),
				       GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("iso-image-new", GTK_ICON_SIZE_BUTTON));

	parrillada_image_properties_set_session (PARRILLADA_IMAGE_PROPERTIES (dialog), project->priv->session);

	track_type = parrillada_track_type_new ();

	parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (project->priv->session), track_type);
	if (parrillada_track_type_get_has_stream (track_type)
	&& PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (track_type))) {
		GtkWidget *box;
		GtkWidget *options;

		/* create video widget */
		options = parrillada_video_options_new (PARRILLADA_BURN_SESSION (project->priv->session));
		gtk_widget_show (options);

		box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		gtk_box_pack_end (GTK_BOX (box), options, FALSE, TRUE, 0);
	}

	parrillada_track_type_free (track_type);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	/* launch the dialog */
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer == GTK_RESPONSE_OK) ? PARRILLADA_BURN_OK:PARRILLADA_BURN_ERR;
}

static void
parrillada_project_burn (ParrilladaProject *project)
{
	ParrilladaBurnResult res;
	ParrilladaDisc *current_disc;
	ParrilladaDriveSettings *settings;

	/* Check that we are ready */
	if (parrillada_project_check_status (project) != PARRILLADA_BURN_OK)
		return;

	/* Check that we are not missing any plugin */
	if (parrillada_project_check_plugins_not_ready (project, PARRILLADA_BURN_SESSION (project->priv->session)) != PARRILLADA_BURN_OK)
		return;

	/* Set saved parameters for the session */
	settings = parrillada_drive_settings_new ();
	parrillada_drive_settings_set_session (settings, PARRILLADA_BURN_SESSION (project->priv->session));

	if (!parrillada_burn_session_is_dest_file (PARRILLADA_BURN_SESSION (project->priv->session)))
		res = parrillada_project_drive_properties (project);
	else
		res = parrillada_project_image_properties (project);

	if (res != PARRILLADA_BURN_OK
	&&  res != PARRILLADA_BURN_RETRY) {
		g_object_unref (settings);
		return;
	}

	project->priv->is_burning = 1;

	/* This is to avoid having the settings being wrongly reflected or changed */
	current_disc = project->priv->current;
	parrillada_disc_set_session_contents (current_disc, NULL);
	project->priv->current = NULL;

	parrillada_dest_selection_set_session (PARRILLADA_DEST_SELECTION (project->priv->selection), NULL);
	parrillada_project_setup_session (project, PARRILLADA_BURN_SESSION (project->priv->session));

	/* This is to stop the preview widget from playing */
	parrillada_uri_container_uri_selected (PARRILLADA_URI_CONTAINER (project));

	/* now setup the burn dialog */
	project->priv->burnt = parrillada_app_burn (parrillada_app_get_default (),
						 PARRILLADA_BURN_SESSION (project->priv->session),
						 res == PARRILLADA_BURN_RETRY);

	g_object_unref (settings);

	/* empty the stack of temporary tracks */
	while (parrillada_burn_session_pop_tracks (PARRILLADA_BURN_SESSION (project->priv->session)) == PARRILLADA_BURN_RETRY);

	project->priv->current = current_disc;
	parrillada_disc_set_session_contents (current_disc, PARRILLADA_BURN_SESSION (project->priv->session));
	parrillada_dest_selection_set_session (PARRILLADA_DEST_SELECTION (project->priv->selection),
					    PARRILLADA_BURN_SESSION (project->priv->session));

	project->priv->is_burning = 0;

	parrillada_project_update_controls (project);
}

/******************************** cover ****************************************/
void
parrillada_project_create_audio_cover (ParrilladaProject *project)
{
	GtkWidget *window;

	if (!PARRILLADA_IS_AUDIO_DISC (project->priv->current))
		return;

	parrillada_project_setup_session (project, PARRILLADA_BURN_SESSION (project->priv->session));
	window = parrillada_session_edit_cover (PARRILLADA_BURN_SESSION (project->priv->session),
					     gtk_widget_get_toplevel (GTK_WIDGET (project)));

	/* This strange hack is a way to workaround #568358.
	 * At one point we'll need to hide the dialog which means it
	 * will anwer with a GTK_RESPONSE_NONE */
	while (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_NONE)
		gtk_widget_show (GTK_WIDGET (window));

	gtk_widget_destroy (window);
}

/********************************     ******************************************/
static void
parrillada_project_reset (ParrilladaProject *project)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->help), 1);

	if (project->priv->project_status) {
		gtk_widget_hide (project->priv->project_status);
		gtk_dialog_response (GTK_DIALOG (project->priv->project_status), GTK_RESPONSE_CANCEL);
		project->priv->project_status = NULL;
	}

	if (project->priv->current) {
		parrillada_disc_set_session_contents (project->priv->current, NULL);
		project->priv->current = NULL;
	}

	if (project->priv->chooser) {
		gtk_widget_destroy (project->priv->chooser);
		project->priv->chooser = NULL;
	}

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
	}

	if (project->priv->cover) {
		g_free (project->priv->cover);
		project->priv->cover = NULL;
	}

	/* remove the buttons from the "toolbar" */
	if (project->priv->merge_id > 0)
		gtk_ui_manager_remove_ui (project->priv->manager,
					  project->priv->merge_id);

	project->priv->empty = 1;
    	project->priv->burnt = 0;
	project->priv->modified = 0;

	if (project->priv->session) {
		g_signal_handlers_disconnect_by_func (project->priv->session,
						      parrillada_project_is_valid,
						      project);
		g_signal_handlers_disconnect_by_func (project->priv->session,
						      parrillada_project_track_added,
						      project);
		g_signal_handlers_disconnect_by_func (project->priv->session,
						      parrillada_project_track_changed,
						      project);
		g_signal_handlers_disconnect_by_func (project->priv->session,
						      parrillada_project_track_removed,
						      project);

		/* unref session to force it to remove all temporary files */
		g_object_unref (project->priv->session);
		project->priv->session = NULL;
	}

	parrillada_notify_message_remove (project->priv->message, PARRILLADA_NOTIFY_CONTEXT_SIZE);
	parrillada_notify_message_remove (project->priv->message, PARRILLADA_NOTIFY_CONTEXT_LOADING);
	parrillada_notify_message_remove (project->priv->message, PARRILLADA_NOTIFY_CONTEXT_MULTISESSION);
}

static void
parrillada_project_new_session (ParrilladaProject *project,
                             ParrilladaSessionCfg *session)
{
	if (project->priv->session)
		parrillada_project_reset (project);

	if (session)
		project->priv->session = g_object_ref (session);
	else
		project->priv->session = parrillada_session_cfg_new ();

	parrillada_burn_session_set_strict_support (PARRILLADA_BURN_SESSION (project->priv->session), FALSE);

	/* NOTE: "is-valid" is emitted whenever there is a change in the
	 * contents of the session. So no need to connect to track-added, ... */
	g_signal_connect (project->priv->session,
			  "is-valid",
			  G_CALLBACK (parrillada_project_is_valid),
			  project);
	g_signal_connect (project->priv->session,
			  "track-added",
			  G_CALLBACK (parrillada_project_track_added),
			  project);
	g_signal_connect (project->priv->session,
			  "track-changed",
			  G_CALLBACK (parrillada_project_track_changed),
			  project);
	g_signal_connect (project->priv->session,
			  "track-removed",
			  G_CALLBACK (parrillada_project_track_removed),
			  project);

	parrillada_dest_selection_set_session (PARRILLADA_DEST_SELECTION (project->priv->selection),
					    PARRILLADA_BURN_SESSION (project->priv->session));
	parrillada_project_name_set_session (PARRILLADA_PROJECT_NAME (project->priv->name_display),
					  PARRILLADA_BURN_SESSION (project->priv->session));
}

static void
parrillada_project_switch (ParrilladaProject *project, ParrilladaProjectType type)
{
	GtkAction *action;

	if (type == PARRILLADA_PROJECT_TYPE_AUDIO) {
		project->priv->current = PARRILLADA_DISC (project->priv->audio);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 0);
		parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (project->priv->selection),
							  PARRILLADA_MEDIA_TYPE_WRITABLE|
							  PARRILLADA_MEDIA_TYPE_FILE|
		                                          PARRILLADA_MEDIA_TYPE_CD);
	}
	else if (type == PARRILLADA_PROJECT_TYPE_DATA) {
		project->priv->current = PARRILLADA_DISC (project->priv->data);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 1);
		parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (project->priv->selection),
							  PARRILLADA_MEDIA_TYPE_WRITABLE|
							  PARRILLADA_MEDIA_TYPE_FILE);
	}
	else if (type == PARRILLADA_PROJECT_TYPE_VIDEO) {
		project->priv->current = PARRILLADA_DISC (project->priv->video);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 2);
		parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (project->priv->selection),
							  PARRILLADA_MEDIA_TYPE_WRITABLE|
							  PARRILLADA_MEDIA_TYPE_FILE);
	}

	if (project->priv->current) {
		project->priv->merge_id = parrillada_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);
		parrillada_disc_set_session_contents (project->priv->current,
						   PARRILLADA_BURN_SESSION (project->priv->session));
	}

	parrillada_notify_message_remove (project->priv->message, PARRILLADA_NOTIFY_CONTEXT_SIZE);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
}

void
parrillada_project_set_audio (ParrilladaProject *project)
{
	parrillada_project_new_session (project, NULL);
	parrillada_project_switch (project, PARRILLADA_PROJECT_TYPE_AUDIO);
}

void
parrillada_project_set_data (ParrilladaProject *project)
{
	parrillada_project_new_session (project, NULL);
	parrillada_project_switch (project, PARRILLADA_PROJECT_TYPE_DATA);
}

void
parrillada_project_set_video (ParrilladaProject *project)
{
	parrillada_project_new_session (project, NULL);
	parrillada_project_switch (project, PARRILLADA_PROJECT_TYPE_VIDEO);
}

ParrilladaBurnResult
parrillada_project_confirm_switch (ParrilladaProject *project,
				gboolean keep_files)
{
	GtkWidget *dialog;
	GtkResponseType answer;

	if (project->priv->project) {
		if (!project->priv->modified)
			return TRUE;

		dialog = parrillada_app_dialog (parrillada_app_get_default (),
					     _("Do you really want to create a new project and discard the current one?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new empty project, all changes will be lost."));

		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard Changes"),
				       GTK_RESPONSE_OK);
	}
	else if (keep_files) {
		if (project->priv->empty)
			return TRUE;

		dialog = parrillada_app_dialog (parrillada_app_get_default (),
					     _("Do you want to discard the file selection or add it to the new project?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new empty project, the file selection will be discarded."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard File Selection"),
				       GTK_RESPONSE_OK);

		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Keep File Selection"),
				       GTK_RESPONSE_ACCEPT);
	}
	else {
		if (project->priv->empty)
			return TRUE;

		dialog = parrillada_app_dialog (parrillada_app_get_default (),
					     _("Do you really want to create a new project and discard the current one?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new empty project, the file selection will be discarded."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard Project"),
				       GTK_RESPONSE_OK);
	}

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer == GTK_RESPONSE_ACCEPT)
		return PARRILLADA_BURN_RETRY;

	if (answer != GTK_RESPONSE_OK)
		return PARRILLADA_BURN_CANCEL;

	return PARRILLADA_BURN_OK;
}

void
parrillada_project_set_none (ParrilladaProject *project)
{
	GtkAction *action;
	GtkWidget *status;

	parrillada_project_reset (project);

	/* update buttons/menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);

	if (project->priv->merge_id)
		gtk_ui_manager_remove_ui (project->priv->manager,
					  project->priv->merge_id);

	status = parrillada_app_get_statusbar2 (parrillada_app_get_default ());

	if (project->priv->status_ctx)
		gtk_statusbar_pop (GTK_STATUSBAR (status), project->priv->status_ctx);
}

/**************************** manage the relations with the sources ************/
static void
parrillada_project_transfer_uris_from_src (ParrilladaProject *project)
{
	gchar **uris;
	gchar **uri;

	if (!project->priv->current_source)
		return;

	uris = parrillada_uri_container_get_selected_uris (project->priv->current_source);
	if (!uris)
		return;

	uri = uris;
	while (*uri) {
		parrillada_disc_add_uri (project->priv->current, *uri);
		uri ++;
	}

	g_strfreev (uris);
}

static void
parrillada_project_source_uri_activated_cb (ParrilladaURIContainer *container,
					 ParrilladaProject *project)
{
	parrillada_project_transfer_uris_from_src (project);
}

static void
parrillada_project_source_uri_selected_cb (ParrilladaURIContainer *container,
					ParrilladaProject *project)
{
	parrillada_project_set_add_button_state (project);
}

void
parrillada_project_set_source (ParrilladaProject *project,
			    ParrilladaURIContainer *source)
{
	if (project->priv->chooser)
		gtk_dialog_response (GTK_DIALOG (project->priv->chooser), GTK_RESPONSE_CANCEL);

	if (project->priv->activated_id) {
		g_signal_handler_disconnect (project->priv->current_source,
					     project->priv->activated_id);
		project->priv->activated_id = 0;
	}

	if (project->priv->selected_id) {
		g_signal_handler_disconnect (project->priv->current_source,
					     project->priv->selected_id);
		project->priv->selected_id = 0;
	}

	project->priv->current_source = source;

	if (source) {
		project->priv->activated_id = g_signal_connect (source,
							        "uri-activated",
							        G_CALLBACK (parrillada_project_source_uri_activated_cb),
							        project);
		project->priv->selected_id = g_signal_connect (source,
							       "uri-selected",
							       G_CALLBACK (parrillada_project_source_uri_selected_cb),
							       project);
	}

	parrillada_project_set_add_button_state (project);
}

/******************************* menus/buttons *********************************/
static void
parrillada_project_save_cb (GtkAction *action, ParrilladaProject *project)
{
	parrillada_project_save_project (project);
}

static void
parrillada_project_save_as_cb (GtkAction *action, ParrilladaProject *project)
{
	parrillada_project_save_project_as (project);
}

static void
parrillada_project_file_chooser_activated_cb (GtkWidget *chooser,
					   ParrilladaProject *project)
{
	gboolean sensitive;
	GtkAction *action;
	GSList *uris;
	GSList *iter;

	if (!project->priv->chooser)
		return;

	project->priv->chooser = NULL;
	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (GTK_WIDGET (chooser));

	sensitive = ((!project->priv->current_source || !project->priv->has_focus) &&
		      !project->priv->oversized);

	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, sensitive);

	for (iter = uris; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		parrillada_disc_add_uri (project->priv->current, uri);
	}
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

static void
parrillada_project_file_chooser_response_cb (GtkWidget *chooser,
					  GtkResponseType response,
					  ParrilladaProject *project)
{
	gboolean sensitive;
	GtkAction *action;
	GSList *uris;
	GSList *iter;

	if (!project->priv->chooser)
		return;

	sensitive = ((!project->priv->current_source || !project->priv->has_focus) &&
		      !project->priv->oversized);

	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, sensitive);

	if (response != PARRILLADA_RESPONSE_ADD) {
		gtk_widget_destroy (chooser);
		project->priv->chooser = NULL;
		return;
	}

	project->priv->chooser = NULL;
	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (GTK_WIDGET (chooser));

	for (iter = uris; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		parrillada_disc_add_uri (project->priv->current, uri);
	}
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

#ifdef BUILD_PREVIEW
static void
parrillada_project_preview_ready (ParrilladaPlayer *player,
			       GtkFileChooser *chooser)
{
	gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
parrillada_project_update_preview (GtkFileChooser *chooser,
				ParrilladaPlayer *player)
{
	gchar *uri;

	gtk_file_chooser_set_preview_widget_active (chooser, TRUE);

	uri = gtk_file_chooser_get_preview_uri (chooser);
	parrillada_player_set_uri (player, uri);
	g_free (uri);
}
#endif

static void
parrillada_project_add_uris_cb (GtkAction *action,
			     ParrilladaProject *project)
{
	GtkWidget *toplevel;
	GtkFileFilter *filter;

	if (project->priv->current_source) {
		parrillada_project_transfer_uris_from_src (project);
		return;
	}

	/* Set the Add button grey as we don't want
	 * the user to be able to click again until the
	 * dialog has been closed */
	gtk_action_set_sensitive (action, FALSE);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	project->priv->chooser = gtk_file_chooser_dialog_new (_("Select Files"),
							      GTK_WINDOW (toplevel),
							      GTK_FILE_CHOOSER_ACTION_OPEN,
							      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							      NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (project->priv->chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (project->priv->chooser), FALSE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (project->priv->chooser), g_get_home_dir ());
	parrillada_file_chooser_customize (project->priv->chooser, NULL);
	gtk_widget_show (project->priv->chooser);

	/* This is to work around a bug in GTK+ which doesn't want to add "Add"
	 * button or anything that is not "Open" or "Cancel" buttons */
	/* Just for the record, file chooser creation uses all GtkResponseType
	 * that are already defined for internal use like GTK_RESPONSE_OK,
	 * *_APPLY and so on (usually to open directories, not add them). So we
	 * have to define on custom here. */
	gtk_dialog_add_button (GTK_DIALOG (project->priv->chooser),
			       GTK_STOCK_ADD,
			       PARRILLADA_RESPONSE_ADD);
	gtk_dialog_set_default_response (GTK_DIALOG (project->priv->chooser),
					 PARRILLADA_RESPONSE_ADD);

	g_signal_connect (project->priv->chooser,
			  "file-activated",
			  G_CALLBACK (parrillada_project_file_chooser_activated_cb),
			  project);
	g_signal_connect (project->priv->chooser,
			  "response",
			  G_CALLBACK (parrillada_project_file_chooser_response_cb),
			  project);
	g_signal_connect (project->priv->chooser,
			  "close",
			  G_CALLBACK (parrillada_project_file_chooser_activated_cb),
			  project);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Audio files"));
	gtk_file_filter_add_mime_type (filter, "audio/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	if (PARRILLADA_IS_AUDIO_DISC (project->priv->current))
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Movies"));
	gtk_file_filter_add_mime_type (filter, "video/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_filter_add_mime_type (filter, "application/x-flash-video");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	if (PARRILLADA_IS_VIDEO_DISC (project->priv->current))
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_file_filter_set_name (filter, C_("picture", "Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

#ifdef BUILD_PREVIEW

	GtkWidget *player;
	gpointer value;

	parrillada_setting_get_value (parrillada_setting_get_default (),
	                           PARRILLADA_SETTING_SHOW_PREVIEW,
	                           &value);

	if (!GPOINTER_TO_INT (value))
		return;

	/* if preview is activated add it */
	player = parrillada_player_new ();

	gtk_widget_show (player);
	gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (project->priv->chooser), TRUE);
	gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (project->priv->chooser), FALSE);
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (project->priv->chooser), player);

	g_signal_connect (project->priv->chooser,
			  "update-preview",
			  G_CALLBACK (parrillada_project_update_preview),
			  player);

	g_signal_connect (player,
			  "ready",
			  G_CALLBACK (parrillada_project_preview_ready),
			  project->priv->chooser);
#endif

}

static void
parrillada_project_remove_selected_uris_cb (GtkAction *action, ParrilladaProject *project)
{
	parrillada_disc_delete_selected (PARRILLADA_DISC (project->priv->current));
}

static void
parrillada_project_empty_cb (GtkAction *action, ParrilladaProject *project)
{
	if (!project->priv->empty) {
		GtkWidget *dialog;
		GtkResponseType answer;

		dialog = parrillada_app_dialog (parrillada_app_get_default (),
					      _("Do you really want to empty the current project?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Emptying a project will remove all files already added. "
							    "All the work will be lost. "
							    "Note that files will not be deleted from their own location, "
							    "just no longer listed here."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
					/* Translators: "empty" is a verb here */
				       _("E_mpty Project"),
				       GTK_RESPONSE_OK);

		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (answer != GTK_RESPONSE_OK)
			return;
	}

	if (!parrillada_disc_clear (PARRILLADA_DISC (project->priv->current)))
		parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (project->priv->session), NULL, NULL);
}

static void
parrillada_project_burn_cb (GtkAction *action, ParrilladaProject *project)
{
	parrillada_project_burn (project);
}

static void
parrillada_project_burn_clicked_cb (GtkButton *button, ParrilladaProject *project)
{
	parrillada_project_burn (project);
}

void
parrillada_project_register_ui (ParrilladaProject *project, GtkUIManager *manager)
{
	GError *error = NULL;
	GtkAction *action;
	GtkWidget *toolbar;

	/* menus */
	project->priv->project_group = gtk_action_group_new ("ProjectActions1");
	gtk_action_group_set_translation_domain (project->priv->project_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (project->priv->project_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      project);

	gtk_ui_manager_insert_action_group (manager, project->priv->project_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (manager,
						description,
						-1,
						&error)) {
		g_message ("building menus/toolbar failed: %s", error->message);
		g_error_free (error);
	}

	toolbar = gtk_ui_manager_get_widget (manager, "/Toolbar");
	gtk_style_context_add_class (gtk_widget_get_style_context (toolbar),
				     GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	g_object_set (action,
		      "short-label", _("_Save"), /* for toolbar buttons */
		      NULL);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);

	action = gtk_action_group_get_action (project->priv->project_group, "Burn");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("_Add"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("_Remove"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_sensitive (action, FALSE);

	project->priv->manager = manager;

	if (gtk_widget_get_realized (project->priv->name_display))
		gtk_widget_grab_focus (project->priv->name_display);
}

/******************************* common to save/open ***************************/
static void
parrillada_project_add_to_recents (ParrilladaProject *project,
				const gchar *uri,
				gboolean is_project)
{
   	GtkRecentManager *recent;
	gchar *groups [] = { "parrillada", NULL };
	gchar *open_playlist = "parrillada -l %u";
	GtkRecentData recent_data = { NULL,
				      NULL,
				      "application/x-parrillada",
				      "parrillada",
				      "parrillada -p %u",
				      groups,
				      FALSE };

    	recent = gtk_recent_manager_get_default ();

	if (is_project)
		recent_data.app_exec = open_playlist;

    	gtk_recent_manager_add_full (recent, uri, &recent_data);
}

static void
parrillada_project_set_uri (ParrilladaProject *project,
			 const gchar *uri,
			 ParrilladaProjectType type)
{
     	gchar *name;
	gchar *title;
	GtkAction *action;
	GtkWidget *toplevel;

	/* possibly reset the name of the project */
	if (uri) {
		if (project->priv->project)
			g_free (project->priv->project);

		project->priv->project = g_strdup (uri);
	}

	uri = uri ? uri : project->priv->project;

	/* add it to recent manager */
	if (parrillada_app_is_running (parrillada_app_get_default ()))
		parrillada_project_add_to_recents (project, uri, TRUE);

	/* update the name of the main window */
    	PARRILLADA_GET_BASENAME_FOR_DISPLAY (uri, name);
	if (type == PARRILLADA_PROJECT_TYPE_DATA)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Parrillada — %s (Data Disc)"), name);
	else if (type == PARRILLADA_PROJECT_TYPE_AUDIO)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Parrillada — %s (Audio Disc)"), name);
	else if (type == PARRILLADA_PROJECT_TYPE_VIDEO)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Parrillada — %s (Video Disc)"), name);
	else
		title = NULL;
 
	g_free (name);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	if (toplevel)
		gtk_window_set_title (GTK_WINDOW (toplevel), title);
	g_free (title);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
}

static ParrilladaProjectType
parrillada_project_get_session_type (ParrilladaProject *project)
{
	ParrilladaTrackType *session_type;
	ParrilladaProjectType type;

        session_type = parrillada_track_type_new ();
        parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (project->priv->session), session_type);

	if (parrillada_track_type_get_has_stream (session_type)) {
                if (PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (session_type)))
		        type = PARRILLADA_PROJECT_TYPE_VIDEO;
                else
		        type = PARRILLADA_PROJECT_TYPE_AUDIO;
        }
	else if (parrillada_track_type_get_has_data (session_type))
		type = PARRILLADA_PROJECT_TYPE_DATA;
	else
		type = PARRILLADA_PROJECT_TYPE_INVALID;

    	parrillada_track_type_free (session_type);

        return type;
}

/******************************* Projects **************************************/
ParrilladaProjectType
parrillada_project_open_session (ParrilladaProject *project,
                              ParrilladaSessionCfg *session)
{
	GValue *value;
	ParrilladaProjectType type;

	parrillada_project_new_session (project, session);

	type = parrillada_project_get_session_type (project);
        if (type == PARRILLADA_PROJECT_TYPE_INVALID)
                return type;

	parrillada_project_switch (project, type);

	if (parrillada_burn_session_get_label (PARRILLADA_BURN_SESSION (project->priv->session))) {
		g_signal_handlers_block_by_func (project->priv->name_display,
						 parrillada_project_name_changed_cb,
						 project);
		gtk_entry_set_text (GTK_ENTRY (project->priv->name_display),
					       parrillada_burn_session_get_label (PARRILLADA_BURN_SESSION (project->priv->session)));
		g_signal_handlers_unblock_by_func (project->priv->name_display,
						   parrillada_project_name_changed_cb,
						   project);
	}

	value = NULL;
	parrillada_burn_session_tag_lookup (PARRILLADA_BURN_SESSION (project->priv->session),
					 PARRILLADA_COVER_URI,
					 &value);
	if (value) {
		if (project->priv->cover)
			g_free (project->priv->cover);

		project->priv->cover = g_strdup (g_value_get_string (value));
	}

	project->priv->modified = 0;

	return type;
}

ParrilladaProjectType
parrillada_project_convert_to_data (ParrilladaProject *project)
{
	GSList *tracks;
	ParrilladaProjectType type;
	ParrilladaSessionCfg *newsession;
	ParrilladaTrackDataCfg *data_track;

	newsession = parrillada_session_cfg_new ();
	data_track = parrillada_track_data_cfg_new ();
	parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (newsession),
					PARRILLADA_TRACK (data_track),
					NULL);
	g_object_unref (data_track);

	tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (project->priv->session));
	for (; tracks; tracks = tracks->next) {
		ParrilladaTrackStream *track;
		gchar *uri;

		track = tracks->data;
		uri = parrillada_track_stream_get_source (track, TRUE);
		parrillada_track_data_cfg_add (data_track, uri, NULL);
		g_free (uri);
	}

	type = parrillada_project_open_session (project, newsession);
	g_object_unref (newsession);

	return type;
}

ParrilladaProjectType
parrillada_project_convert_to_stream (ParrilladaProject *project,
				   gboolean is_video)
{
	GSList *tracks;
	GtkTreeIter iter;
	ParrilladaProjectType type;
	ParrilladaSessionCfg *newsession;
	ParrilladaTrackDataCfg *data_track;

	tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (project->priv->session));
	if (!tracks)
		return PARRILLADA_PROJECT_TYPE_INVALID;

	data_track = tracks->data;
	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (data_track), &iter))
		return PARRILLADA_PROJECT_TYPE_INVALID;

	newsession = parrillada_session_cfg_new ();
	do {
		gchar *uri;
		ParrilladaTrackStreamCfg *track;

		gtk_tree_model_get (GTK_TREE_MODEL (data_track), &iter,
				    PARRILLADA_DATA_TREE_MODEL_URI, &uri,
				    -1);

		track = parrillada_track_stream_cfg_new ();
		parrillada_track_stream_set_source (PARRILLADA_TRACK_STREAM (track), uri);
		parrillada_track_stream_set_format (PARRILLADA_TRACK_STREAM (track),
						 is_video ? PARRILLADA_VIDEO_FORMAT_UNDEFINED:PARRILLADA_AUDIO_FORMAT_UNDEFINED);
		g_free (uri);

		parrillada_burn_session_add_track (PARRILLADA_BURN_SESSION (newsession),
						PARRILLADA_TRACK (track),
						NULL);
		g_object_unref (track);

	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (data_track), &iter));

	type = parrillada_project_open_session (project, newsession);
	g_object_unref (newsession);

	return type;
}

/******************************** save project *********************************/
static void
parrillada_project_not_saved_dialog (ParrilladaProject *project)
{
	xmlError *error;

	error = xmlGetLastError ();
	parrillada_app_alert (parrillada_app_get_default (),
			   _("Your project has not been saved."),
			   error? error->message:_("An unknown error occurred"),
			   GTK_MESSAGE_ERROR);
	xmlResetLastError ();
}

static GtkResponseType
parrillada_project_save_project_dialog (ParrilladaProject *project,
				     gboolean show_cancel)
{
	GtkWidget *dialog;
	GtkResponseType result;

	dialog = parrillada_app_dialog (parrillada_app_get_default (),
				     _("Save the changes of current project before closing?"),
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you don't save, changes will be permanently lost."));

	if (show_cancel)
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose Without Saving"), GTK_RESPONSE_NO,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_YES,
					NULL);
	else
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose Without Saving"), GTK_RESPONSE_NO,
					GTK_STOCK_SAVE, GTK_RESPONSE_YES,
					NULL);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result == GTK_RESPONSE_CANCEL)
		return GTK_RESPONSE_CANCEL;

	if (show_cancel && result == GTK_RESPONSE_DELETE_EVENT)
		return GTK_RESPONSE_CANCEL;

	if (result != GTK_RESPONSE_YES)
		return GTK_RESPONSE_NO;

	return GTK_RESPONSE_YES;
}

static gboolean
parrillada_project_save_project_real (ParrilladaProject *project,
				   const gchar *uri,
				   ParrilladaProjectSave save_type)
{
	ParrilladaBurnResult result;
	ParrilladaProjectType type;

	g_return_val_if_fail (uri != NULL || project->priv->project != NULL, FALSE);

	result = parrillada_project_check_status (project);
	if (result != PARRILLADA_BURN_OK)
		return FALSE;

	parrillada_project_setup_session (project, PARRILLADA_BURN_SESSION (project->priv->session));
        type = parrillada_project_get_session_type (project);

	if (save_type == PARRILLADA_PROJECT_SAVE_XML
	||  type == PARRILLADA_PROJECT_TYPE_DATA) {
		parrillada_project_set_uri (project, uri, type);
		if (!parrillada_project_save_project_xml (PARRILLADA_BURN_SESSION (project->priv->session),
						       uri ? uri : project->priv->project)) {
			parrillada_project_not_saved_dialog (project);
			return FALSE;
		}

		project->priv->modified = 0;
	}
	else if (save_type == PARRILLADA_PROJECT_SAVE_PLAIN) {
		if (!parrillada_project_save_audio_project_plain_text (PARRILLADA_BURN_SESSION (project->priv->session),
								    uri)) {
			parrillada_project_not_saved_dialog (project);
			return FALSE;
		}
	}

#ifdef BUILD_PLAYLIST

	else {
		if (!parrillada_project_save_audio_project_playlist (PARRILLADA_BURN_SESSION (project->priv->session),
								  uri,
								  save_type)) {
			parrillada_project_not_saved_dialog (project);
			return FALSE;
		}
	}

#endif

	return TRUE;
}

static gchar *
parrillada_project_save_project_ask_for_path (ParrilladaProject *project,
					   ParrilladaProjectSave *type)
{
	GtkWidget *combo = NULL;
	GtkWidget *toplevel;
	GtkWidget *chooser;
	gchar *uri = NULL;
	gint answer;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	chooser = gtk_file_chooser_dialog_new (_("Save Current Project"),
					       GTK_WINDOW (toplevel),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());

	/* if the file chooser is an audio project offer the possibility to save
	 * in plain text a list of the current displayed songs (only in save as
	 * mode) */
	if (type && PARRILLADA_IS_AUDIO_DISC (project->priv->current)) {
		combo = gtk_combo_box_text_new ();
		gtk_widget_show (combo);

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as a Parrillada audio project"));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as a plain text list"));

#ifdef BUILD_PLAYLIST

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as a PLS playlist"));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as an M3U playlist"));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as an XSPF playlist"));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Save project as an iriver playlist"));

#endif

		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (chooser), combo);
	}

	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer == GTK_RESPONSE_OK) {
		if (combo)
			*type = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
		if (*uri == '\0') {
			g_free (uri);
			uri = NULL;
		}
	}

	gtk_widget_destroy (chooser);
	return uri;
}

gboolean
parrillada_project_save_project (ParrilladaProject *project)
{
	gchar *uri = NULL;
	gboolean result;

	if (!project->priv->session)
		return FALSE;

	if (!project->priv->project && !(uri = parrillada_project_save_project_ask_for_path (project, NULL)))
		return FALSE;

	result = parrillada_project_save_project_real (project, uri, PARRILLADA_PROJECT_SAVE_XML);
	g_free (uri);

	return result;
}

gboolean
parrillada_project_save_project_as (ParrilladaProject *project)
{
	ParrilladaProjectSave type = PARRILLADA_PROJECT_SAVE_XML;
	gboolean result;
	gchar *uri;

	if (!project->priv->session)
		return FALSE;

	uri = parrillada_project_save_project_ask_for_path (project, &type);
	if (!uri)
		return FALSE;

	result = parrillada_project_save_project_real (project, uri, type);
	g_free (uri);

	return result;
}

/**
 * NOTE: this function returns FALSE if it succeeds and TRUE otherwise.
 * this value is mainly used by the session object to cancel or not the app
 * closing
 */

gboolean
parrillada_project_save_session (ParrilladaProject *project,
			      const gchar *uri,
			      gchar **saved_uri,
			      gboolean show_cancel)
{
	if (!project->priv->session)
		return FALSE;

	if (!project->priv->current) {
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

	if (project->priv->empty) {
		/* the project is empty anyway. No need to ask anything.
		 * return FALSE since this is not a tmp project */
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

	if (project->priv->project) {
		GtkResponseType answer;

		if (!project->priv->modified) {
			/* there is a saved project but unmodified.
			 * No need to ask anything */
			if (saved_uri)
				*saved_uri = g_strdup (project->priv->project);

			return FALSE;
		}

		/* ask the user if he wants to save the changes */
		answer = parrillada_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES) {
			if (saved_uri)
				*saved_uri = NULL;

			return FALSE;
		}

		if (!parrillada_project_save_project_real (project, NULL, PARRILLADA_PROJECT_SAVE_XML))
			return TRUE;

		if (saved_uri)
			*saved_uri = g_strdup (project->priv->project);

		return FALSE;
	}

    	if (project->priv->burnt) {
		GtkResponseType answer;

		/* the project wasn't saved but burnt ask if the user wants to
		 * keep it for another time by saving it */
		answer = parrillada_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES) {
			if (saved_uri)
				*saved_uri = NULL;

			return FALSE;
		}

		if (!parrillada_project_save_project_as (project))
			return TRUE;

		if (saved_uri)
			*saved_uri = g_strdup (project->priv->project);

		return FALSE;
	}

    	if (!uri) {
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

	parrillada_project_setup_session (project, PARRILLADA_BURN_SESSION (project->priv->session));
	if (!parrillada_project_save_project_xml (PARRILLADA_BURN_SESSION (project->priv->session), uri)) {
		GtkResponseType response;
		GtkWidget *dialog;

		/* If the automatic saving failed, let the user decide */
		dialog = parrillada_app_dialog (parrillada_app_get_default (),
					      _("Your project has not been saved."),
					     GTK_BUTTONS_NONE,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you don't save, changes will be permanently lost."));

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
		                        _("Cl_ose Without Saving"), GTK_RESPONSE_NO,
		                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		                        NULL);

		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (saved_uri)
			*saved_uri = NULL;

		return (response == GTK_RESPONSE_CANCEL);
	}

	if (saved_uri)
		*saved_uri = g_strdup (uri);

    	return FALSE;
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-media is distributed in the hope that it will be useful,
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
#include <glib-object.h>
#include <glib/gi18n-lib.h>
  
#include <gtk/gtk.h>
  
#include "parrillada-drive-selection.h"
#include "parrillada-medium-monitor.h"
#include "parrillada-drive.h"
#include "parrillada-units.h"
  
typedef struct _ParrilladaDriveSelectionPrivate ParrilladaDriveSelectionPrivate;
struct _ParrilladaDriveSelectionPrivate
{
	ParrilladaDrive *active;

	ParrilladaDriveType type;
	gulong added_sig;
	gulong removed_sig;
};
  
#define PARRILLADA_DRIVE_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DRIVE_SELECTION, ParrilladaDriveSelectionPrivate))

typedef enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
} ParrilladaDriveSelectionSignalType;

/* GtkBuildable */
static GtkBuildableIface *parent_buildable_iface;

static guint parrillada_drive_selection_signals [LAST_SIGNAL] = { 0 };

enum {
	PROP_0,
	PROP_DRIVE,
	PROP_DRIVE_TYPE
};
  
enum {
	DRIVE_COL,
	NAME_COL,
	ICON_COL,
	NUM_COL
};

  
static void
parrillada_drive_selection_buildable_init (GtkBuildableIface *iface)
{
	parent_buildable_iface = g_type_interface_peek_parent (iface);
}  

G_DEFINE_TYPE_WITH_CODE (ParrilladaDriveSelection, parrillada_drive_selection, GTK_TYPE_COMBO_BOX, G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, parrillada_drive_selection_buildable_init));

static void
parrillada_drive_selection_set_current_drive (ParrilladaDriveSelection *self,
					   GtkTreeIter *iter)
{
	ParrilladaDriveSelectionPrivate *priv;
	ParrilladaDrive *drive;
	GtkTreeModel *model;

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	gtk_tree_model_get (model, iter,
			    DRIVE_COL, &drive,
			    -1);

	if (drive)
		gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

	if (priv->active == drive)
		return;
  
	if (priv->active)
		g_object_unref (priv->active);
  
	priv->active = drive;
  
	if (priv->active)
		g_object_ref (priv->active);
  
	g_signal_emit (self,
		       parrillada_drive_selection_signals [CHANGED_SIGNAL],
		       0,
		       priv->active);
}
  
static void
parrillada_drive_selection_changed (GtkComboBox *combo)
{
	GtkTreeIter iter;
  
	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;
  
	parrillada_drive_selection_set_current_drive (PARRILLADA_DRIVE_SELECTION (combo), &iter);
}
  
/**
 * parrillada_drive_selection_set_active:
 * @selector: a #ParrilladaDriveSelection
 * @drive: a #ParrilladaDrive to set as the active one in the selector
 *
 * Sets the active drive. Emits the ::drive-changed signal.
 *
 * Return value: a #gboolean. TRUE if it succeeded, FALSE otherwise.
 **/
gboolean
parrillada_drive_selection_set_active (ParrilladaDriveSelection *selector,
				     ParrilladaDrive *drive)
{
	ParrilladaDriveSelectionPrivate *priv;
	gboolean result = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (selector != NULL, FALSE);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE_SELECTION (selector), FALSE);

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (selector);
  
	if (priv->active == drive)
		return TRUE;
  
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return FALSE;
  
	do {
		ParrilladaDrive *iter_drive;
  
		gtk_tree_model_get (model, &iter,
				    DRIVE_COL, &iter_drive,
				    -1);
  
		if (drive == iter_drive) {
			if (iter_drive)
				g_object_unref (iter_drive);

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &iter);
			parrillada_drive_selection_set_current_drive (selector, &iter);
			result = TRUE;
			break;
		}

		g_object_unref (iter_drive);
	} while (gtk_tree_model_iter_next (model, &iter));

	return result;
}
  
/**
 * parrillada_drive_selection_get_active:
 * @selector: a #ParrilladaDriveSelection
 *
 * Gets the active drive.
 *
 * Return value: a #ParrilladaDrive or NULL. Unref when it is not needed anymore.
 **/
ParrilladaDrive *
parrillada_drive_selection_get_active (ParrilladaDriveSelection *selector)
{
	ParrilladaDriveSelectionPrivate *priv;

	g_return_val_if_fail (selector != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_DRIVE_SELECTION (selector), NULL);

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (selector);
	if (!priv->active)
		return NULL;

	return g_object_ref (priv->active);
}

static void
parrillada_drive_selection_update_no_disc_entry (ParrilladaDriveSelection *self,
					      GtkTreeModel *model,
					      GtkTreeIter *iter)
{
	GIcon *icon;

	icon = g_themed_icon_new_with_default_fallbacks ("drive-optical");

	/* FIXME: that needs a string */
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    NAME_COL, NULL,
			    ICON_COL, NULL,
			    -1);

	g_object_unref (icon);

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), iter);
	parrillada_drive_selection_set_current_drive (self, iter);
}
  
static void
parrillada_drive_selection_add_no_disc_entry (ParrilladaDriveSelection *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* Nothing's available. Say it. Two cases here, either we're
	 * still probing drives or there isn't actually any available
	 * drive. */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	parrillada_drive_selection_update_no_disc_entry (self, model, &iter);
}
  
/**
 * parrillada_drive_selection_show_type:
 * @selector: a #ParrilladaDriveSelection
 * @type: a #ParrilladaDriveType
 *
 * Filters and displays drive corresponding to @type.
 *
 **/
void
parrillada_drive_selection_show_type (ParrilladaDriveSelection *selector,
				   ParrilladaDriveType type)
{
	ParrilladaDriveSelectionPrivate *priv;
	ParrilladaMediumMonitor *monitor;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list;
	GSList *item;

	g_return_if_fail (selector != NULL);
	g_return_if_fail (PARRILLADA_IS_DRIVE_SELECTION (selector));

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (selector);

	priv->type = type;

	monitor = parrillada_medium_monitor_get_default ();
	list = parrillada_medium_monitor_get_drives (monitor, type);
	g_object_unref (monitor);
  
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		/* First filter */
		do {
			GSList *node;
			ParrilladaDrive *drive;
  
			gtk_tree_model_get (model, &iter,
					    DRIVE_COL, &drive,
					    -1);
  
			if (!drive) {
				/* That's the dummy line saying there isn't any
				 * available drive for whatever action it is */
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				break;
			}
  
			node = g_slist_find (list, drive);
			g_object_unref (drive);
  
			if (!node) {
				if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
					continue;
  
				/* no more iter in the tree get out */
				break;
			}
  
			g_object_unref (node->data);
			list = g_slist_delete_link (list, node);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
  
	if (list) {
		/* add remaining drive */
		for (item = list; item; item = item->next) {
			gchar *drive_name;
			ParrilladaDrive *drive;
  			GIcon *drive_icon = NULL;

			drive = item->data;

			drive_name = parrillada_drive_get_display_name (drive);

			if (!parrillada_drive_is_fake (drive)) {
				GDrive *gdrive;

				gdrive = parrillada_drive_get_gdrive (drive);
				if (gdrive) {
					drive_icon = g_drive_get_icon (gdrive);
					g_object_unref (gdrive);
				}
				else
					drive_icon = g_themed_icon_new_with_default_fallbacks ("drive-optical");
			}
			else
				drive_icon = g_themed_icon_new_with_default_fallbacks ("iso-image-new");
  
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    DRIVE_COL, drive,
					    NAME_COL, drive_name?drive_name:_("Unnamed CD/DVD Drive"),
					    ICON_COL, drive_icon,
					    -1);
			g_free (drive_name);
			g_object_unref (drive_icon);
		}
		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}
  
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		parrillada_drive_selection_add_no_disc_entry (selector);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (selector)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &iter);
		parrillada_drive_selection_set_current_drive (selector, &iter);
	}
}

static void
parrillada_drive_selection_drive_added_cb (ParrilladaMediumMonitor *monitor,
					ParrilladaDrive *drive,
					ParrilladaDriveSelection *self)
{
	ParrilladaDriveSelectionPrivate *priv;
	gchar *drive_name = NULL;
	gboolean add = FALSE;
	GtkTreeModel *model;
	GIcon *drive_icon;
	GtkTreeIter iter;

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (self);

	if ((priv->type & PARRILLADA_DRIVE_TYPE_WRITER)
	&&  (parrillada_drive_can_write (drive)))
		add = TRUE;
	else if (priv->type & PARRILLADA_DRIVE_TYPE_READER)
		add = TRUE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));

	if (!add) {
		/* Try to get the first iter (it shouldn't fail) */
		if (!gtk_tree_model_get_iter_first (model, &iter)) {
			parrillada_drive_selection_add_no_disc_entry (self);
			return;
		}
  
		/* See if that's a real drive or not; if so, return. */
		drive = NULL;
		gtk_tree_model_get (model, &iter,
				    DRIVE_COL, &drive,
				    -1);
		if (drive)
			return;
  
		parrillada_drive_selection_update_no_disc_entry (self, model, &iter);
		return;
	}
  
	/* remove warning message */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		ParrilladaDrive *tmp;
  
		gtk_tree_model_get (model, &iter,
				    DRIVE_COL, &tmp,
				    -1);
		if (!tmp)
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		else
			g_object_unref (tmp);
	}

	if (!parrillada_drive_is_fake (drive)) {
		GDrive *gdrive;

		gdrive = parrillada_drive_get_gdrive (drive);
		if (gdrive) {
			drive_icon = g_drive_get_icon (gdrive);
			g_object_unref (gdrive);
		}
		else
			drive_icon = g_themed_icon_new_with_default_fallbacks ("drive-optical");
	}
	else
		drive_icon = g_themed_icon_new_with_default_fallbacks ("iso-image-new");

	drive_name = parrillada_drive_get_display_name (drive);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    DRIVE_COL, drive,
			    NAME_COL, drive_name?drive_name:_("Unnamed CD/DVD Drive"),
			    ICON_COL, drive_icon,
			    -1);
	g_free (drive_name);
	g_object_unref (drive_icon);

	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		parrillada_drive_selection_set_current_drive (self, &iter);
	}
}
  
static void
parrillada_drive_selection_drive_removed_cb (ParrilladaMediumMonitor *monitor,
					    ParrilladaDrive *drive,
					    ParrilladaDriveSelection *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
  
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;
  
	do {
		ParrilladaDrive *iter_drive;
  
		gtk_tree_model_get (model, &iter,
				    DRIVE_COL, &iter_drive,
				    -1);
  
		if (drive == iter_drive) {
			g_object_unref (iter_drive);
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}
  
		/* Could be NULL if a message "there is no drive ..." is on */
		if (iter_drive)
			g_object_unref (iter_drive);
  
	} while (gtk_tree_model_iter_next (model, &iter));

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		parrillada_drive_selection_add_no_disc_entry (self);
		return;
	}

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		parrillada_drive_selection_set_current_drive (self, &iter);
	}
}
  
static void
parrillada_drive_selection_init (ParrilladaDriveSelection *object)
{
	GtkListStore *model;
	GtkCellRenderer *renderer;
	ParrilladaMediumMonitor *monitor;
	ParrilladaDriveSelectionPrivate *priv;

	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (object);

	monitor = parrillada_medium_monitor_get_default ();
	priv->added_sig = g_signal_connect (monitor,
					    "drive-added",
					    G_CALLBACK (parrillada_drive_selection_drive_added_cb),
					    object);
	priv->removed_sig = g_signal_connect (monitor,
					      "drive-removed",
					      G_CALLBACK (parrillada_drive_selection_drive_removed_cb),
					      object);

	g_object_unref (monitor);

	/* get the list and fill the model */
	model = gtk_list_store_new (NUM_COL,
				    G_TYPE_OBJECT,
				    G_TYPE_STRING,
				    G_TYPE_ICON);

	gtk_combo_box_set_model (GTK_COMBO_BOX (object), GTK_TREE_MODEL (model));
	g_object_unref (model);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
					"gicon", ICON_COL,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xpad", 8, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
					"markup", NAME_COL,
					NULL);
}

static void
parrillada_drive_selection_constructed (GObject *object)
{
	G_OBJECT_CLASS (parrillada_drive_selection_parent_class)->constructed (object);

	parrillada_drive_selection_show_type (PARRILLADA_DRIVE_SELECTION (object),
					   PARRILLADA_DRIVE_TYPE_ALL_BUT_FILE);
}

static void
parrillada_drive_selection_finalize (GObject *object)
{
	ParrilladaDriveSelectionPrivate *priv;
	ParrilladaMediumMonitor *monitor;
  
	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (object);
  
	monitor = parrillada_medium_monitor_get_default ();
  
	g_signal_handler_disconnect (monitor, priv->added_sig);
	g_signal_handler_disconnect (monitor, priv->removed_sig);
	priv->removed_sig = 0;
	priv->added_sig = 0;

	g_object_unref (monitor);

	G_OBJECT_CLASS (parrillada_drive_selection_parent_class)->finalize (object);
  }
  
static void
parrillada_drive_selection_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	g_return_if_fail (PARRILLADA_IS_DRIVE_SELECTION (object));
  
	switch (prop_id)
	{
	case PROP_DRIVE_TYPE:
		parrillada_drive_selection_show_type (PARRILLADA_DRIVE_SELECTION (object),
						   g_value_get_uint (value));
		break;
	case PROP_DRIVE:
		parrillada_drive_selection_set_active (PARRILLADA_DRIVE_SELECTION (object),
						     PARRILLADA_DRIVE (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
  }
  
static void
parrillada_drive_selection_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	ParrilladaDriveSelectionPrivate *priv;
  
	g_return_if_fail (PARRILLADA_IS_DRIVE_SELECTION (object));
  
	priv = PARRILLADA_DRIVE_SELECTION_PRIVATE (object);
  
	switch (prop_id)
	{
	case PROP_DRIVE_TYPE:
		g_value_set_uint (value, priv->type);
		break;
	case PROP_DRIVE:
		g_value_set_object (value, parrillada_drive_selection_get_active (PARRILLADA_DRIVE_SELECTION (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
  
static void
parrillada_drive_selection_class_init (ParrilladaDriveSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (klass);
  
	g_type_class_add_private (klass, sizeof (ParrilladaDriveSelectionPrivate));
  
	object_class->finalize = parrillada_drive_selection_finalize;
	object_class->set_property = parrillada_drive_selection_set_property;
	object_class->get_property = parrillada_drive_selection_get_property;
	object_class->constructed = parrillada_drive_selection_constructed;
  
	combo_class->changed = parrillada_drive_selection_changed;

	g_object_class_install_property (object_class, PROP_DRIVE,
					 g_param_spec_object ("Drive",
							      "Selected drive",
							      "The currently selected drive",
							      PARRILLADA_TYPE_DRIVE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_DRIVE_TYPE,
					 g_param_spec_uint ("drive-type",
							    "The type of drives",
							    "The type of drives displayed",
							    0, PARRILLADA_DRIVE_TYPE_ALL,
							    PARRILLADA_DRIVE_TYPE_ALL_BUT_FILE,
							    G_PARAM_READWRITE));
	/**
 	* ParrilladaDriveSelection::drive-changed:
 	* @selector: the object which received the signal
  	* @drive: the drive which is now selected
	*
 	* This signal gets emitted when the selected medium has changed
 	*
 	*/
	parrillada_drive_selection_signals [CHANGED_SIGNAL] =
	    g_signal_new ("drive_changed",
			  PARRILLADA_TYPE_DRIVE_SELECTION,
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (ParrilladaDriveSelectionClass, drive_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  PARRILLADA_TYPE_DRIVE);
}

/**
 * parrillada_drive_selection_new:
 *
 * Creates a new #ParrilladaDriveSelection object
 *
 * Return value: a #GtkWidget. Unref when it is not needed anymore.
 **/

GtkWidget *
parrillada_drive_selection_new (void)
{
	return g_object_new (PARRILLADA_TYPE_DRIVE_SELECTION, NULL);
}

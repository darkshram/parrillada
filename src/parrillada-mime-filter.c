/***************************************************************************
*            mime_filter.c
*
*  dim mai 22 18:39:03 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include "parrillada-mime-filter.h"
#include "parrillada-utils.h"

static void parrillada_mime_filter_class_init (ParrilladaMimeFilterClass *
					    klass);
static void parrillada_mime_filter_init (ParrilladaMimeFilter * sp);
static void parrillada_mime_filter_finalize (GObject * object);

enum {
	PARRILLADA_MIME_FILTER_ICON_COL,
	PARRILLADA_MIME_FILTER_DISPLAY_COL,
	PARRILLADA_MIME_FILTER_ROW_SPAN_COL,
	PARRILLADA_MIME_FILTER_FILTER_COL,
	PARRILLADA_MIME_FILTER_NB_COL
};

struct ParrilladaMimeFilterPrivate {
	GtkWidget *label;
	GHashTable *table;
	GSList *custom_filters;
};

static GObjectClass *parent_class = NULL;

GType
parrillada_mime_filter_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (ParrilladaMimeFilterClass),
			NULL,
			NULL,
			(GClassInitFunc) parrillada_mime_filter_class_init,
			NULL,
			NULL,
			sizeof (ParrilladaMimeFilter),
			0,
			(GInstanceInitFunc) parrillada_mime_filter_init,
		};

		type = g_type_register_static (GTK_TYPE_HBOX,
					       "ParrilladaMimeFilter",
					       &our_info, 0);
	}

	return type;
}

static void
parrillada_mime_filter_class_init (ParrilladaMimeFilterClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_mime_filter_finalize;
}

static void
parrillada_mime_filter_init (ParrilladaMimeFilter * obj)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;

	gtk_box_set_spacing (GTK_BOX (obj), 6);

	obj->priv = g_new0 (ParrilladaMimeFilterPrivate, 1);

	store = gtk_list_store_new (PARRILLADA_MIME_FILTER_NB_COL,
				    G_TYPE_ICON,
				    G_TYPE_STRING,
				    G_TYPE_INT,
				    G_TYPE_POINTER);

	obj->combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->combo),
				       renderer, "gicon",
				       PARRILLADA_MIME_FILTER_ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->combo),
				       renderer, "text",
				       PARRILLADA_MIME_FILTER_DISPLAY_COL);

	gtk_box_pack_end (GTK_BOX (obj), obj->combo, FALSE, FALSE, 0);

	obj->priv->table = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  (GDestroyNotify) g_free,
						  NULL);
}

static void
parrillada_mime_filter_finalize (GObject * object)
{
	ParrilladaMimeFilter *cobj;

	cobj = PARRILLADA_MIME_FILTER (object);

	if (cobj->priv->custom_filters) {
		g_slist_foreach (cobj->priv->custom_filters,
				 (GFunc) g_object_unref,
				 NULL);
		g_slist_free (cobj->priv->custom_filters);
		cobj->priv->custom_filters = NULL;
	}

	g_hash_table_destroy (cobj->priv->table);

	g_free (cobj->priv);
	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
parrillada_mime_filter_new ()
{
	return g_object_new (PARRILLADA_TYPE_MIME_FILTER, NULL);
}

void
parrillada_mime_filter_unref_mime (ParrilladaMimeFilter * filter, const char *mime)
{
	GtkFileFilter *item;

	item = g_hash_table_lookup (filter->priv->table, mime);
	if (item)
		g_object_unref (item);
}

static void
parrillada_mime_filter_destroy_item_cb (ParrilladaMimeFilter *filter,
                                     GtkFileFilter *item,
                                     gboolean is_last_ref)
{
	GtkTreeModel *model;
	GtkFileFilter *item2;
	GtkTreeIter row;

	g_hash_table_remove (filter->priv->table,
			     gtk_file_filter_get_name (item));

	/* Now we must remove the item from the combo as well */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (filter->combo));
	if (!gtk_tree_model_get_iter_first (model, &row))
		return;

	do {
		gtk_tree_model_get (model, &row,
				    PARRILLADA_MIME_FILTER_FILTER_COL, &item2,
				    -1);

		if (item == item2) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &row);
			break;
		}
	} while (gtk_tree_model_iter_next (model, &row));

	/* we check that the first entry at least is visible */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (filter->combo)) == -1
	&&  gtk_tree_model_get_iter_first (model, &row) == TRUE)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (filter->combo),
					       &row);
}

void
parrillada_mime_filter_add_mime (ParrilladaMimeFilter *filter,
                              const gchar *mime)
{
	GtkFileFilter *item;

	item = g_hash_table_lookup (filter->priv->table, mime);
	if (!item) {
		GIcon *icon;
		GtkTreeIter row;
		gchar *description;
		GtkTreeModel *model;

		description = g_content_type_get_description (mime);
		icon = g_content_type_get_icon (mime);

		/* create the GtkFileFilter */
		item = gtk_file_filter_new ();
		gtk_file_filter_set_name (item, mime);
		gtk_file_filter_add_mime_type (item, mime);
		g_hash_table_insert (filter->priv->table,
				     g_strdup (mime),
				     item);

		g_object_add_toggle_ref (G_OBJECT (item),
		                         (GToggleNotify) parrillada_mime_filter_destroy_item_cb,
		                         filter);

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (filter->combo));
		gtk_list_store_append (GTK_LIST_STORE (model), &row);

		g_object_ref_sink (item);
		gtk_list_store_set (GTK_LIST_STORE (model), &row,
				    PARRILLADA_MIME_FILTER_DISPLAY_COL, description,
				    PARRILLADA_MIME_FILTER_ICON_COL, icon,
				    PARRILLADA_MIME_FILTER_FILTER_COL, item,
				    -1);
		g_object_unref (icon);
		g_free (description);

		/* we check that the first entry at least is visible */
		if (gtk_combo_box_get_active (GTK_COMBO_BOX (filter->combo)) == -1
		&&  gtk_tree_model_get_iter_first (model, &row) == TRUE)
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (filter->combo),
						       &row);
	}
	else
		g_object_ref (item);
}

void
parrillada_mime_filter_add_filter (ParrilladaMimeFilter *filter,
				GtkFileFilter *item)
{
	GtkTreeModel *model;
	GtkTreeIter row;
	const char *name;

	name = gtk_file_filter_get_name (item);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (filter->combo));

	gtk_list_store_append (GTK_LIST_STORE (model), &row);

	g_object_ref (item);
	gtk_list_store_set (GTK_LIST_STORE (model), &row,
			    PARRILLADA_MIME_FILTER_DISPLAY_COL, name,
			    PARRILLADA_MIME_FILTER_FILTER_COL, item,
			    -1);
	g_object_ref_sink (GTK_OBJECT (item));

	g_hash_table_insert (filter->priv->table,
			     g_strdup (name),
			     item);

	filter->priv->custom_filters = g_slist_prepend (filter->priv->custom_filters, item);

	/* we check that the first entry at least is visible */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (filter->combo)) == -1
	&&  gtk_tree_model_get_iter_first (model, &row) == TRUE)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (filter->combo), &row);
}

gboolean
parrillada_mime_filter_filter (ParrilladaMimeFilter * filter, const char *filename,
			    const char *uri, const char *display_name, const char *mime_type)
{
	GtkTreeModel *model;
	GtkFileFilterInfo info;
	GtkFileFilter *item;
	GtkTreeIter row;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (filter->combo));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (filter->combo), &row) == FALSE)
		return TRUE;

	gtk_tree_model_get (model, &row,
			    PARRILLADA_MIME_FILTER_FILTER_COL, &item,
			    -1);

	info.contains = gtk_file_filter_get_needed (item);
	if (info.contains & GTK_FILE_FILTER_FILENAME)
		info.filename = (gchar *) filename;

	if (info.contains & GTK_FILE_FILTER_URI)
		info.uri = (gchar *) uri;

	if (info.contains & GTK_FILE_FILTER_DISPLAY_NAME)
		info.display_name = (gchar *) display_name;

	if (info.contains & GTK_FILE_FILTER_MIME_TYPE)
		info.mime_type =  (gchar *) mime_type;

	return gtk_file_filter_filter (item, &info);
}

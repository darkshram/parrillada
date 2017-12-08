/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "parrillada-pref.h"
#include "parrillada-plugin-manager-ui.h"


typedef struct _ParrilladaPrefPrivate ParrilladaPrefPrivate;
struct _ParrilladaPrefPrivate
{
	GtkWidget *notebook;
};

#define PARRILLADA_PREF_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_PREF, ParrilladaPrefPrivate))


G_DEFINE_TYPE (ParrilladaPref, parrillada_pref, GTK_TYPE_DIALOG);

GtkWidget *
parrillada_pref_new (void)
{
	return g_object_new (PARRILLADA_TYPE_PREF, NULL);
}

static void
parrillada_pref_init (ParrilladaPref *object)
{
	GtkWidget *notebook;
	GtkWidget *plugins;

	gtk_dialog_add_button (GTK_DIALOG (object), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	gtk_window_set_default_size (GTK_WINDOW (object), 600, 400);
	gtk_window_set_title (GTK_WINDOW (object), _("Parrillada Plugins"));

	notebook = gtk_notebook_new ();

	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

	gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (object))),
			  notebook,
			  TRUE,
			  TRUE,
			  0);

	plugins = parrillada_plugin_manager_ui_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  plugins,
				  NULL);
}

static void
parrillada_pref_finalize (GObject *object)
{
	G_OBJECT_CLASS (parrillada_pref_parent_class)->finalize (object);
}

static void
parrillada_pref_class_init (ParrilladaPrefClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaPrefPrivate));

	object_class->finalize = parrillada_pref_finalize;
}

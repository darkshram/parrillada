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

#ifndef _PARRILLADA_PLUGIN_OPTION_H_
#define _PARRILLADA_PLUGIN_OPTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PLUGIN_OPTION             (parrillada_plugin_option_get_type ())
#define PARRILLADA_PLUGIN_OPTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_PLUGIN_OPTION, ParrilladaPluginOption))
#define PARRILLADA_PLUGIN_OPTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_PLUGIN_OPTION, ParrilladaPluginOptionClass))
#define PARRILLADA_IS_PLUGIN_OPTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_PLUGIN_OPTION))
#define PARRILLADA_IS_PLUGIN_OPTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_PLUGIN_OPTION))
#define PARRILLADA_PLUGIN_OPTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_PLUGIN_OPTION, ParrilladaPluginOptionClass))

typedef struct _ParrilladaPluginOptionClass ParrilladaPluginOptionClass;
typedef struct _ParrilladaPluginOption ParrilladaPluginOption;

struct _ParrilladaPluginOptionClass
{
	GtkDialogClass parent_class;
};

struct _ParrilladaPluginOption
{
	GtkDialog parent_instance;
};

GType parrillada_plugin_option_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_plugin_option_new (void);

void
parrillada_plugin_option_set_plugin (ParrilladaPluginOption *dialog,
				  ParrilladaPlugin *plugin);

G_END_DECLS

#endif /* _PARRILLADA_PLUGIN_OPTION_H_ */

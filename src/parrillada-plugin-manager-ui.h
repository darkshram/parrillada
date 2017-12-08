/*
 * parrillada-plugin-manager.h
 * This file is part of parrillada
 *
 * Copyright (C) 2007 Philippe Rouquier
 *
 * Based on parrillada code (parrillada/parrillada-plugin-manager.c) by: 
 * 	- Paolo Maggi <paolo@gnome.org>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
fy
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Parrillada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef __PARRILLADA_PLUGIN_MANAGER_UI_H__
#define __PARRILLADA_PLUGIN_MANAGER_UI_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define PARRILLADA_TYPE_PLUGIN_MANAGER_UI              (parrillada_plugin_manager_ui_get_type())
#define PARRILLADA_PLUGIN_MANAGER_UI(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), PARRILLADA_TYPE_PLUGIN_MANAGER_UI, ParrilladaPluginManagerUI))
#define PARRILLADA_PLUGIN_MANAGER_UI_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), PARRILLADA_TYPE_PLUGIN_MANAGER_UI, ParrilladaPluginManagerUIClass))
#define PARRILLADA_IS_PLUGIN_MANAGER_UI(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), PARRILLADA_TYPE_PLUGIN_MANAGER_UI))
#define PARRILLADA_IS_PLUGIN_MANAGER_UI_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_PLUGIN_MANAGER_UI))
#define PARRILLADA_PLUGIN_MANAGER_UI_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), PARRILLADA_TYPE_PLUGIN_MANAGER_UI, ParrilladaPluginManagerUIClass))

/* Private structure type */
typedef struct _ParrilladaPluginManagerUIPrivate ParrilladaPluginManagerUIPrivate;

/*
 * Main object structure
 */
typedef struct _ParrilladaPluginManagerUI ParrilladaPluginManagerUI;

struct _ParrilladaPluginManagerUI 
{
	GtkBox vbox;
};

/*
 * Class definition
 */
typedef struct _ParrilladaPluginManagerUIClass ParrilladaPluginManagerUIClass;

struct _ParrilladaPluginManagerUIClass 
{
	GtkBoxClass parent_class;
};

/*
 * Public methods
 */
GType		 parrillada_plugin_manager_ui_get_type		(void) G_GNUC_CONST;

GtkWidget	*parrillada_plugin_manager_ui_new		(void);
   
G_END_DECLS

#endif  /* __PARRILLADA_PLUGIN_MANAGER_UI_H__  */

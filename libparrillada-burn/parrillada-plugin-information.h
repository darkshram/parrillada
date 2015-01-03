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
 
#ifndef _BURN_PLUGIN_REGISTRATION_H
#define _BURN_PLUGIN_REGISTRATION_H

#include <glib.h>
#include <glib-object.h>

#include "parrillada-session.h"
#include "parrillada-plugin.h"

G_BEGIN_DECLS

void
parrillada_plugin_set_active (ParrilladaPlugin *plugin, gboolean active);

gboolean
parrillada_plugin_get_active (ParrilladaPlugin *plugin,
                           gboolean ignore_errors);

const gchar *
parrillada_plugin_get_name (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_display_name (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_author (ParrilladaPlugin *plugin);

guint
parrillada_plugin_get_group (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_copyright (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_website (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_description (ParrilladaPlugin *plugin);

const gchar *
parrillada_plugin_get_icon_name (ParrilladaPlugin *plugin);

typedef struct _ParrilladaPluginError ParrilladaPluginError;
struct _ParrilladaPluginError {
	ParrilladaPluginErrorType type;
	gchar *detail;
};

GSList *
parrillada_plugin_get_errors (ParrilladaPlugin *plugin);

gchar *
parrillada_plugin_get_error_string (ParrilladaPlugin *plugin);

gboolean
parrillada_plugin_get_compulsory (ParrilladaPlugin *plugin);

guint
parrillada_plugin_get_priority (ParrilladaPlugin *plugin);

/** 
 * This is to find out what are the capacities of a plugin 
 */

ParrilladaBurnResult
parrillada_plugin_can_burn (ParrilladaPlugin *plugin);

ParrilladaBurnResult
parrillada_plugin_can_image (ParrilladaPlugin *plugin);

ParrilladaBurnResult
parrillada_plugin_can_convert (ParrilladaPlugin *plugin);


/**
 * Plugin configuration options
 */

ParrilladaPluginConfOption *
parrillada_plugin_get_next_conf_option (ParrilladaPlugin *plugin,
				     ParrilladaPluginConfOption *current);

ParrilladaBurnResult
parrillada_plugin_conf_option_get_info (ParrilladaPluginConfOption *option,
				     gchar **key,
				     gchar **description,
				     ParrilladaPluginConfOptionType *type);

GSList *
parrillada_plugin_conf_option_bool_get_suboptions (ParrilladaPluginConfOption *option);

gint
parrillada_plugin_conf_option_int_get_min (ParrilladaPluginConfOption *option);
gint
parrillada_plugin_conf_option_int_get_max (ParrilladaPluginConfOption *option);


struct _ParrilladaPluginChoicePair {
	gchar *string;
	guint value;
};
typedef struct _ParrilladaPluginChoicePair ParrilladaPluginChoicePair;

GSList *
parrillada_plugin_conf_option_choice_get (ParrilladaPluginConfOption *option);

G_END_DECLS

#endif
 

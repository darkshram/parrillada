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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include "parrillada-media-private.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "parrillada-drive.h"
#include "parrillada-medium.h"
#include "parrillada-session.h"
#include "parrillada-plugin.h"
#include "parrillada-plugin-information.h"
#include "burn-task.h"
#include "burn-caps.h"
#include "parrillada-track-type-private.h"

#define PARRILLADA_ENGINE_GROUP_KEY	"engine-group"
#define PARRILLADA_SCHEMA_CONFIG		"org.mate.parrillada.config"

G_DEFINE_TYPE (ParrilladaBurnCaps, parrillada_burn_caps, G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;


static void
parrillada_caps_link_free (ParrilladaCapsLink *link)
{
	g_slist_free (link->plugins);
	g_free (link);
}

ParrilladaBurnResult
parrillada_caps_link_check_recorder_flags_for_input (ParrilladaCapsLink *link,
                                                  ParrilladaBurnFlag session_flags)
{
	if (parrillada_track_type_get_has_image (&link->caps->type)) {
		ParrilladaImageFormat format;

		format = parrillada_track_type_get_image_format (&link->caps->type);
		if (format == PARRILLADA_IMAGE_FORMAT_CUE
		||  format == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
			if ((session_flags & PARRILLADA_BURN_FLAG_DAO) == 0)
				return PARRILLADA_BURN_NOT_SUPPORTED;
		}
		else if (format == PARRILLADA_IMAGE_FORMAT_CLONE) {
			/* RAW write mode should (must) only be used in this case */
			if ((session_flags & PARRILLADA_BURN_FLAG_RAW) == 0)
				return PARRILLADA_BURN_NOT_SUPPORTED;
		}
	}

	return PARRILLADA_BURN_OK;
}

gboolean
parrillada_caps_link_active (ParrilladaCapsLink *link,
                          gboolean ignore_plugin_errors)
{
	GSList *iter;

	/* See if link is active by going through all plugins. There must be at
	 * least one. */
	for (iter = link->plugins; iter; iter = iter->next) {
		ParrilladaPlugin *plugin;

		plugin = iter->data;
		if (parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			return TRUE;
	}

	return FALSE;
}

ParrilladaPlugin *
parrillada_caps_link_need_download (ParrilladaCapsLink *link)
{
	GSList *iter;
	ParrilladaPlugin *plugin_ret = NULL;

	/* See if for link to be active, we need to 
	 * download additional apps/libs/.... */
	for (iter = link->plugins; iter; iter = iter->next) {
		ParrilladaPlugin *plugin;

		plugin = iter->data;

		/* If a plugin can be used without any
		 * error then that means that the link
		 * can be followed without additional
		 * download. */
		if (parrillada_plugin_get_active (plugin, FALSE))
			return NULL;

		if (parrillada_plugin_get_active (plugin, TRUE)) {
			if (!plugin_ret)
				plugin_ret = plugin;
			else if (parrillada_plugin_get_priority (plugin) > parrillada_plugin_get_priority (plugin_ret))
				plugin_ret = plugin;
		}
	}

	return plugin_ret;
}

static void
parrillada_caps_test_free (ParrilladaCapsTest *caps)
{
	g_slist_foreach (caps->links, (GFunc) parrillada_caps_link_free, NULL);
	g_slist_free (caps->links);
	g_free (caps);
}

static void
parrillada_caps_free (ParrilladaCaps *caps)
{
	g_slist_free (caps->modifiers);
	g_slist_foreach (caps->links, (GFunc) parrillada_caps_link_free, NULL);
	g_slist_free (caps->links);
	g_free (caps);
}

static gboolean
parrillada_caps_has_active_input (ParrilladaCaps *caps,
			       ParrilladaCaps *input)
{
	GSList *links;

	for (links = caps->links; links; links = links->next) {
		ParrilladaCapsLink *link;

		link = links->data;
		if (link->caps != input)
			continue;

		/* Ignore plugin errors */
		if (parrillada_caps_link_active (link, TRUE))
			return TRUE;
	}

	return FALSE;
}

gboolean
parrillada_burn_caps_is_input (ParrilladaBurnCaps *self,
			    ParrilladaCaps *input)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *tmp;

		tmp = iter->data;
		if (tmp == input)
			continue;

		if (parrillada_caps_has_active_input (tmp, input))
			return TRUE;
	}

	return FALSE;
}

gboolean
parrillada_caps_is_compatible_type (const ParrilladaCaps *caps,
				 const ParrilladaTrackType *type)
{
	if (caps->type.type != type->type)
		return FALSE;

	switch (type->type) {
	case PARRILLADA_TRACK_TYPE_DATA:
		if ((caps->type.subtype.fs_type & type->subtype.fs_type) != type->subtype.fs_type)
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_DISC:
		if (type->subtype.media == PARRILLADA_MEDIUM_NONE)
			return FALSE;

		/* Reminder: we now create every possible types */
		if (caps->type.subtype.media != type->subtype.media)
			return FALSE;
		break;
	
	case PARRILLADA_TRACK_TYPE_IMAGE:
		if (type->subtype.img_format == PARRILLADA_IMAGE_FORMAT_NONE)
			return FALSE;

		if ((caps->type.subtype.img_format & type->subtype.img_format) != type->subtype.img_format)
			return FALSE;
		break;

	case PARRILLADA_TRACK_TYPE_STREAM:
		/* There is one small special case here with video. */
		if ((caps->type.subtype.stream_format & (PARRILLADA_VIDEO_FORMAT_UNDEFINED|
							 PARRILLADA_VIDEO_FORMAT_VCD|
							 PARRILLADA_VIDEO_FORMAT_VIDEO_DVD))
		&& !(type->subtype.stream_format & (PARRILLADA_VIDEO_FORMAT_UNDEFINED|
						   PARRILLADA_VIDEO_FORMAT_VCD|
						   PARRILLADA_VIDEO_FORMAT_VIDEO_DVD)))
			return FALSE;

		if ((caps->type.subtype.stream_format & type->subtype.stream_format) != type->subtype.stream_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

ParrilladaCaps *
parrillada_burn_caps_find_start_caps (ParrilladaBurnCaps *self,
				   ParrilladaTrackType *output)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;

		caps = iter->data;

		if (!parrillada_caps_is_compatible_type (caps, output))
			continue;

		if (parrillada_track_type_get_has_medium (&caps->type)
		|| (caps->flags & PARRILLADA_PLUGIN_IO_ACCEPT_FILE))
			return caps;
	}

	return NULL;
}

static void
parrillada_burn_caps_finalize (GObject *object)
{
	ParrilladaBurnCaps *cobj;

	cobj = PARRILLADA_BURNCAPS (object);
	
	if (cobj->priv->groups) {
		g_hash_table_destroy (cobj->priv->groups);
		cobj->priv->groups = NULL;
	}

	g_slist_foreach (cobj->priv->caps_list, (GFunc) parrillada_caps_free, NULL);
	g_slist_free (cobj->priv->caps_list);

	if (cobj->priv->group_str) {
		g_free (cobj->priv->group_str);
		cobj->priv->group_str = NULL;
	}

	if (cobj->priv->tests) {
		g_slist_foreach (cobj->priv->tests, (GFunc) parrillada_caps_test_free, NULL);
		g_slist_free (cobj->priv->tests);
		cobj->priv->tests = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_burn_caps_class_init (ParrilladaBurnCapsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_burn_caps_finalize;
}

static void
parrillada_burn_caps_init (ParrilladaBurnCaps *obj)
{
	GSettings *settings;

	obj->priv = g_new0 (ParrilladaBurnCapsPrivate, 1);

	settings = g_settings_new (PARRILLADA_SCHEMA_CONFIG);
	obj->priv->group_str = g_settings_get_string (settings, PARRILLADA_ENGINE_GROUP_KEY);
	g_object_unref (settings);
}

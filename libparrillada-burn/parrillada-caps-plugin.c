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

#include "parrillada-media.h"
#include "parrillada-media-private.h"

#include "burn-caps.h"
#include "burn-debug.h"

#include "parrillada-plugin-private.h"
#include "parrillada-plugin-information.h"

#define SUBSTRACT(a, b)		((a) &= ~((b)&(a)))

/**
 * the following functions are used to register new caps
 */

static ParrilladaCapsLink *
parrillada_caps_find_link_for_input (ParrilladaCaps *caps,
				  ParrilladaCaps *input)
{
	GSList *links;

	for (links = caps->links; links; links = links->next) {
		ParrilladaCapsLink *link;

		link = links->data;
		if (link->caps == input)
			return link;
	}

	return NULL;
}

static gint
parrillada_burn_caps_sort (gconstpointer a, gconstpointer b)
{
	const ParrilladaCaps *caps_a = a;
	const ParrilladaCaps *caps_b = b;
	gint result;

	/* First put DISC (the most used caps) then AUDIO then IMAGE type; these
	 * two types are the ones that most often searched. At the end of the
	 * list we put DATA.
	 * Another (sub)rule is that for DATA, DISC, AUDIO we put a caps that is
	 * encompassed by another before.
	 */

	result = caps_b->type.type - caps_a->type.type;
	if (result)
		return result;

	switch (caps_a->type.type) {
	case PARRILLADA_TRACK_TYPE_DISC:
		if (PARRILLADA_MEDIUM_TYPE (caps_a->type.subtype.media) !=
		    PARRILLADA_MEDIUM_TYPE (caps_b->type.subtype.media))
			return ((gint32) PARRILLADA_MEDIUM_TYPE (caps_a->type.subtype.media) -
			        (gint32) PARRILLADA_MEDIUM_TYPE (caps_b->type.subtype.media));

		if ((caps_a->type.subtype.media & PARRILLADA_MEDIUM_DVD)
		&&  PARRILLADA_MEDIUM_SUBTYPE (caps_a->type.subtype.media) !=
		    PARRILLADA_MEDIUM_SUBTYPE (caps_b->type.subtype.media))			
			return ((gint32) PARRILLADA_MEDIUM_SUBTYPE (caps_a->type.subtype.media) -
			        (gint32) PARRILLADA_MEDIUM_SUBTYPE (caps_b->type.subtype.media));

		if (PARRILLADA_MEDIUM_ATTR (caps_a->type.subtype.media) !=
		    PARRILLADA_MEDIUM_ATTR (caps_b->type.subtype.media))
			return PARRILLADA_MEDIUM_ATTR (caps_a->type.subtype.media) -
			       PARRILLADA_MEDIUM_ATTR (caps_b->type.subtype.media);

		if (PARRILLADA_MEDIUM_STATUS (caps_a->type.subtype.media) !=
		    PARRILLADA_MEDIUM_STATUS (caps_b->type.subtype.media))
			return PARRILLADA_MEDIUM_STATUS (caps_a->type.subtype.media) -
			       PARRILLADA_MEDIUM_STATUS (caps_b->type.subtype.media);

		return (PARRILLADA_MEDIUM_INFO (caps_a->type.subtype.media) -
			PARRILLADA_MEDIUM_INFO (caps_b->type.subtype.media));

	case PARRILLADA_TRACK_TYPE_IMAGE:
		/* This way BIN subtype is always sorted at the end */
		return caps_a->type.subtype.img_format - caps_b->type.subtype.img_format;

	case PARRILLADA_TRACK_TYPE_STREAM:
		if (caps_a->type.subtype.stream_format != caps_b->type.subtype.stream_format) {
			result = (caps_a->type.subtype.stream_format & caps_b->type.subtype.stream_format);
			if (result == caps_a->type.subtype.stream_format)
				return -1;
			else if (result == caps_b->type.subtype.stream_format)
				return 1;

			return  (gint32) caps_a->type.subtype.stream_format -
				(gint32) caps_b->type.subtype.stream_format;
		}
		break;

	case PARRILLADA_TRACK_TYPE_DATA:
		result = (caps_a->type.subtype.fs_type & caps_b->type.subtype.fs_type);
		if (result == caps_a->type.subtype.fs_type)
			return -1;
		else if (result == caps_b->type.subtype.fs_type)
			return 1;

		return (caps_a->type.subtype.fs_type - caps_b->type.subtype.fs_type);

	default:
		break;
	}

	return 0;
}

static ParrilladaCapsLink *
parrillada_caps_link_copy (ParrilladaCapsLink *link)
{
	ParrilladaCapsLink *retval;

	retval = g_new0 (ParrilladaCapsLink, 1);
	retval->plugins = g_slist_copy (link->plugins);
	retval->caps = link->caps;

	return retval;
}

static void
parrillada_caps_link_list_duplicate (ParrilladaCaps *dest, ParrilladaCaps *src)
{
	GSList *iter;

	for (iter = src->links; iter; iter = iter->next) {
		ParrilladaCapsLink *link;

		link = iter->data;
		dest->links = g_slist_prepend (dest->links, parrillada_caps_link_copy (link));
	}
}

static ParrilladaCaps *
parrillada_caps_duplicate (ParrilladaCaps *caps)
{
	ParrilladaCaps *retval;

	retval = g_new0 (ParrilladaCaps, 1);
	retval->flags = caps->flags;
	memcpy (&retval->type, &caps->type, sizeof (ParrilladaTrackType));
	retval->modifiers = g_slist_copy (caps->modifiers);

	return retval;
}

static void
parrillada_caps_replicate_modifiers (ParrilladaCaps *dest, ParrilladaCaps *src)
{
	GSList *iter;

	for (iter = src->modifiers; iter; iter = iter->next) {
		ParrilladaPlugin *plugin;

		plugin = iter->data;

		if (g_slist_find (dest->modifiers, plugin))
			continue;

		dest->modifiers = g_slist_prepend (dest->modifiers, plugin);
	}
}

static void
parrillada_caps_replicate_links (ParrilladaBurnCaps *self,
			      ParrilladaCaps *dest,
			      ParrilladaCaps *src)
{
	GSList *iter;

	parrillada_caps_link_list_duplicate (dest, src);

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *iter_caps;
		GSList *links;

		iter_caps = iter->data;
		if (iter_caps == src)
			continue;

		for (links = iter_caps->links; links; links = links->next) {
			ParrilladaCapsLink *link;

			link = links->data;
			if (link->caps == src) {
				ParrilladaCapsLink *copy;

				copy = parrillada_caps_link_copy (link);
				copy->caps = dest;
				iter_caps->links = g_slist_prepend (iter_caps->links, copy);
			}
		}
	}
}

static void
parrillada_caps_replicate_tests (ParrilladaBurnCaps *self,
			      ParrilladaCaps *dest,
			      ParrilladaCaps *src)
{
	GSList *iter;

	for (iter = self->priv->tests; iter; iter = iter->next) {
		ParrilladaCapsTest *test;
		GSList *links;

		test = iter->data;
		for (links = test->links; links; links = links->next) {
			ParrilladaCapsLink *link;

			link = links->data;
			if (link->caps == src) {
				ParrilladaCapsLink *copy;

				copy = parrillada_caps_link_copy (link);
				copy->caps = dest;
				test->links = g_slist_prepend (test->links, copy);
			}
		}
	}
}

static void
parrillada_caps_copy_deep (ParrilladaBurnCaps *self,
			ParrilladaCaps *dest,
			ParrilladaCaps *src)
{
	parrillada_caps_replicate_links (self, dest, src);
	parrillada_caps_replicate_tests (self, dest, src);
	parrillada_caps_replicate_modifiers (dest,src);
}

static ParrilladaCaps *
parrillada_caps_duplicate_deep (ParrilladaBurnCaps *self,
			     ParrilladaCaps *caps)
{
	ParrilladaCaps *retval;

	retval = parrillada_caps_duplicate (caps);
	parrillada_caps_copy_deep (self, retval, caps);
	return retval;
}

static GSList *
parrillada_caps_list_check_io (ParrilladaBurnCaps *self,
			    GSList *list,
			    ParrilladaPluginIOFlag flags)
{
	GSList *iter;

	/* in this function we create the caps with the missing IO. All in the
	 * list have something in common with flags. */
	for (iter = list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		ParrilladaPluginIOFlag common;

		caps = iter->data;
		common = caps->flags & flags;
		if (common != caps->flags) {
			ParrilladaCaps *new_caps;

			/* (common == flags) && common != caps->flags
			 * caps->flags encompasses flags: Split the caps in two
			 * and only keep the interesting part */
			caps->flags &= ~common;

			/* this caps has changed and needs to be sorted again */
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      parrillada_burn_caps_sort);

			new_caps = parrillada_caps_duplicate_deep (self, caps);
			new_caps->flags = common;

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       new_caps,
								       parrillada_burn_caps_sort);

			list = g_slist_prepend (list, new_caps);
		}
		else if (common != flags) {
			GSList *node, *next;
			ParrilladaPluginIOFlag complement = flags;

			complement &= ~common;
			for (node = list; node; node = next) {
				ParrilladaCaps *tmp;

				tmp = node->data;
				next = node->next;

				if (node == iter)
					continue;

				if (caps->type.type != tmp->type.type
				||  caps->type.subtype.media != tmp->type.subtype.media)
					continue;

				/* substract the flags and relocate them at the
				 * head of the list since we don't need to look
				 * them up again */
				complement &= ~(tmp->flags);
				list = g_slist_remove (list, tmp);
				list = g_slist_prepend (list, tmp);
			}

			if (complement != PARRILLADA_PLUGIN_IO_NONE) {
				ParrilladaCaps *new_caps;

				/* common == caps->flags && common != flags.
				 * Flags encompasses caps->flags. So we need to
				 * create a new caps for this type with the
				 * substraction of flags if the other part isn't
				 * in the list */
				new_caps = parrillada_caps_duplicate (caps);
				new_caps->flags = flags & (~common);
				self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
									       new_caps,
									       parrillada_burn_caps_sort);

				list = g_slist_prepend (list, new_caps);
			}
		}
	}

	return list;
}

GSList *
parrillada_caps_image_new (ParrilladaPluginIOFlag flags,
			ParrilladaImageFormat format)
{
	ParrilladaImageFormat remaining_format;
	ParrilladaBurnCaps *self;
	GSList *retval = NULL;
	GSList *iter;

	PARRILLADA_BURN_LOG_WITH_FULL_TYPE (PARRILLADA_TRACK_TYPE_IMAGE,
					 format,
					 flags,
					 "New caps required");

	self = parrillada_burn_caps_get_default ();

	remaining_format = format;

	/* We have to search all caps with or related to the format */
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		ParrilladaImageFormat common;
		ParrilladaPluginIOFlag common_io;

		caps = iter->data;
		if (caps->type.type != PARRILLADA_TRACK_TYPE_IMAGE)
			continue;

		common_io = caps->flags & flags;
		if (common_io == PARRILLADA_PLUGIN_IO_NONE)
			continue;

		common = (caps->type.subtype.img_format & format);
		if (common == PARRILLADA_IMAGE_FORMAT_NONE)
			continue;

		if (common != caps->type.subtype.img_format) {
			/* img_format encompasses format. Split it in two and
			 * keep caps with common format */
			SUBSTRACT (caps->type.subtype.img_format, common);
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      parrillada_burn_caps_sort);

			caps = parrillada_caps_duplicate_deep (self, caps);
			caps->type.subtype.img_format = common;

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       caps,
								       parrillada_burn_caps_sort);
		}

		retval = g_slist_prepend (retval, caps);
		remaining_format &= ~common;
	}

	/* Now we make sure that all these new or already 
	 * existing caps have the proper IO Flags */
	retval = parrillada_caps_list_check_io (self, retval, flags);

	if (remaining_format != PARRILLADA_IMAGE_FORMAT_NONE) {
		ParrilladaCaps *caps;

		caps = g_new0 (ParrilladaCaps, 1);
		caps->flags = flags;
		caps->type.subtype.img_format = remaining_format;
		caps->type.type = PARRILLADA_TRACK_TYPE_IMAGE;

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       parrillada_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);

		PARRILLADA_BURN_LOG_TYPE (&caps->type, "Created new caps");
	}

	g_object_unref (self);
	return retval;
}

GSList *
parrillada_caps_audio_new (ParrilladaPluginIOFlag flags,
			ParrilladaStreamFormat format)
{
	GSList *iter;
	GSList *retval = NULL;
	ParrilladaBurnCaps *self;
	GSList *encompassing = NULL;
	gboolean have_the_one = FALSE;

	PARRILLADA_BURN_LOG_WITH_FULL_TYPE (PARRILLADA_TRACK_TYPE_STREAM,
					 format,
					 flags,
					 "New caps required");

	self = parrillada_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		ParrilladaStreamFormat common;
		ParrilladaPluginIOFlag common_io;
		ParrilladaStreamFormat common_audio;
		ParrilladaStreamFormat common_video;

		caps = iter->data;

		if (caps->type.type != PARRILLADA_TRACK_TYPE_STREAM)
			continue;

		common_io = (flags & caps->flags);
		if (common_io == PARRILLADA_PLUGIN_IO_NONE)
			continue;

		if (caps->type.subtype.stream_format == format) {
			/* that's the perfect fit */
			have_the_one = TRUE;
			retval = g_slist_prepend (retval, caps);
			continue;
		}

		/* Search caps strictly encompassed or encompassing our format
		 * NOTE: make sure that if there is a VIDEO stream in one of
		 * them, the other does have a VIDEO stream too. */
		common_audio = PARRILLADA_STREAM_FORMAT_AUDIO (caps->type.subtype.stream_format) & 
			       PARRILLADA_STREAM_FORMAT_AUDIO (format);
		if (common_audio == PARRILLADA_AUDIO_FORMAT_NONE
		&& (PARRILLADA_STREAM_FORMAT_AUDIO (caps->type.subtype.stream_format)
		||  PARRILLADA_STREAM_FORMAT_AUDIO (format)))
			continue;

		common_video = PARRILLADA_STREAM_FORMAT_VIDEO (caps->type.subtype.stream_format) & 
			       PARRILLADA_STREAM_FORMAT_VIDEO (format);

		if (common_video == PARRILLADA_AUDIO_FORMAT_NONE
		&& (PARRILLADA_STREAM_FORMAT_VIDEO (caps->type.subtype.stream_format)
		||  PARRILLADA_STREAM_FORMAT_VIDEO (format)))
			continue;

		/* Likewise... that must be common */
		if ((caps->type.subtype.stream_format & PARRILLADA_METADATA_INFO) != (format & PARRILLADA_METADATA_INFO))
			continue;

		common = common_audio|common_video|(format & PARRILLADA_METADATA_INFO);

		/* encompassed caps just add it to retval */
		if (caps->type.subtype.stream_format == common)
			retval = g_slist_prepend (retval, caps);

		/* encompassing caps keep it if we need to create perfect fit */
		if (format == common)
			encompassing = g_slist_prepend (encompassing, caps);
	}

	/* Now we make sure that all these new or already 
	 * existing caps have the proper IO Flags */
	retval = parrillada_caps_list_check_io (self, retval, flags);

	if (!have_the_one) {
		ParrilladaCaps *caps;

		caps = g_new0 (ParrilladaCaps, 1);
		caps->flags = flags;
		caps->type.subtype.stream_format = format;
		caps->type.type = PARRILLADA_TRACK_TYPE_STREAM;

		if (encompassing) {
			for (iter = encompassing; iter; iter = iter->next) {
				ParrilladaCaps *iter_caps;

				iter_caps = iter->data;
				parrillada_caps_copy_deep (self, caps, iter_caps);
			}
		}

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       parrillada_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);

		PARRILLADA_BURN_LOG_TYPE (&caps->type, "Created new caps");
	}

	g_slist_free (encompassing);

	g_object_unref (self);

	return retval;
}

GSList *
parrillada_caps_data_new (ParrilladaImageFS fs_type)
{
	GSList *iter;
	GSList *retval = NULL;
	ParrilladaBurnCaps *self;
	GSList *encompassing = NULL;
	gboolean have_the_one = FALSE;

	PARRILLADA_BURN_LOG_WITH_FULL_TYPE (PARRILLADA_TRACK_TYPE_DATA,
					 fs_type,
					 PARRILLADA_PLUGIN_IO_NONE,
					 "New caps required");

	self = parrillada_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		ParrilladaImageFS common;

		caps = iter->data;

		if (caps->type.type != PARRILLADA_TRACK_TYPE_DATA)
			continue;

		if (caps->type.subtype.fs_type == fs_type) {
			/* that's the perfect fit */
			have_the_one = TRUE;
			retval = g_slist_prepend (retval, caps);
			continue;
		}

		/* search caps strictly encompassing our format ... */
		common = caps->type.subtype.fs_type & fs_type;
		if (common == PARRILLADA_IMAGE_FS_NONE)
			continue;

		/* encompassed caps just add it to retval */
		if (caps->type.subtype.fs_type == common)
			retval = g_slist_prepend (retval, caps);

		/* encompassing caps keep it if we need to create perfect fit */
		if (fs_type == common)
			encompassing = g_slist_prepend (encompassing, caps);
	}

	if (!have_the_one) {
		ParrilladaCaps *caps;

		caps = g_new0 (ParrilladaCaps, 1);
		caps->flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE;
		caps->type.type = PARRILLADA_TRACK_TYPE_DATA;
		caps->type.subtype.fs_type = fs_type;

		if (encompassing) {
			for (iter = encompassing; iter; iter = iter->next) {
				ParrilladaCaps *iter_caps;

				iter_caps = iter->data;
				parrillada_caps_copy_deep (self, caps, iter_caps);
			}
		}

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       parrillada_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);
	}

	g_slist_free (encompassing);

	g_object_unref (self);

	return retval;
}

static GSList *
parrillada_caps_disc_lookup_or_create (ParrilladaBurnCaps *self,
				    GSList *retval,
				    ParrilladaMedia media)
{
	GSList *iter;
	ParrilladaCaps *caps;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		caps = iter->data;

		if (caps->type.type != PARRILLADA_TRACK_TYPE_DISC)
			continue;

		if (caps->type.subtype.media == media) {
			PARRILLADA_BURN_LOG_WITH_TYPE (&caps->type,
						    caps->flags,
						    "Retrieved");
			return g_slist_prepend (retval, caps);
		}
	}

	caps = g_new0 (ParrilladaCaps, 1);
	caps->flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE;
	caps->type.type = PARRILLADA_TRACK_TYPE_DISC;
	caps->type.subtype.media = media;

	PARRILLADA_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "Created");

	self->priv->caps_list = g_slist_prepend (self->priv->caps_list, caps);

	return g_slist_prepend (retval, caps);
}

GSList *
parrillada_caps_disc_new (ParrilladaMedia type)
{
	ParrilladaBurnCaps *self;
	GSList *retval = NULL;
	GSList *list;
	GSList *iter;

	self = parrillada_burn_caps_get_default ();

	list = parrillada_media_get_all_list (type);
	for (iter = list; iter; iter = iter->next) {
		ParrilladaMedia medium;

		medium = GPOINTER_TO_INT (iter->data);
		retval = parrillada_caps_disc_lookup_or_create (self, retval, medium);
	}
	g_slist_free (list);

	g_object_unref (self);
	return retval;
}

/**
 * these functions are to create links
 */

static void
parrillada_caps_create_links (ParrilladaCaps *output,
	 		   GSList *inputs,
	 		   ParrilladaPlugin *plugin)
{
	for (; inputs; inputs = inputs->next) {
		ParrilladaCaps *input;
		ParrilladaCapsLink *link;

		input = inputs->data;

		if (output == input) {
			PARRILLADA_BURN_LOG ("Same input and output for link. Dropping");
			continue;
		}

		if (input->flags == output->flags
		&&  input->type.type == output->type.type  
		&&  input->type.subtype.media == output->type.subtype.media)
			PARRILLADA_BURN_LOG ("Recursive link");

		link = parrillada_caps_find_link_for_input (output, input);

#if 0

		/* Mainly for extra debugging */
		PARRILLADA_BURN_LOG_TYPE (&output->type, "Linking");
		PARRILLADA_BURN_LOG_TYPE (&input->type, "to");
		PARRILLADA_BURN_LOG ("with %s", parrillada_plugin_get_name (plugin));

#endif

		if (!link) {
			link = g_new0 (ParrilladaCapsLink, 1);
			link->caps = input;
			link->plugins = g_slist_prepend (NULL, plugin);

			output->links = g_slist_prepend (output->links, link);
		}
		else
			link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

void
parrillada_plugin_link_caps (ParrilladaPlugin *plugin,
			  GSList *outputs,
			  GSList *inputs)
{
	/* we make sure the caps exists and if not we create them */
	for (; outputs; outputs = outputs->next) {
		ParrilladaCaps *output;

		output = outputs->data;
		parrillada_caps_create_links (output, inputs, plugin);
	}
}

void
parrillada_plugin_blank_caps (ParrilladaPlugin *plugin,
			   GSList *caps_list)
{
	for (; caps_list; caps_list = caps_list->next) {
		ParrilladaCaps *caps;
		ParrilladaCapsLink *link;

		caps = caps_list->data;

		if (caps->type.type != PARRILLADA_TRACK_TYPE_DISC)
			continue;
	
		PARRILLADA_BURN_LOG_WITH_TYPE (&caps->type,
					    caps->flags,
					    "Adding blank caps for");

		/* we need to find the link whose caps is NULL */
		link = parrillada_caps_find_link_for_input (caps, NULL);
		if (!link) {
			link = g_new0 (ParrilladaCapsLink, 1);
			link->caps = NULL;
			link->plugins = g_slist_prepend (NULL, plugin);

			caps->links = g_slist_prepend (caps->links, link);
		}
		else
			link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

void
parrillada_plugin_process_caps (ParrilladaPlugin *plugin,
			     GSList *caps_list)
{
	for (; caps_list; caps_list = caps_list->next) {
		ParrilladaCaps *caps;

		caps = caps_list->data;
		caps->modifiers = g_slist_prepend (caps->modifiers, plugin);
	}
}

void
parrillada_plugin_check_caps (ParrilladaPlugin *plugin,
			   ParrilladaChecksumType type,
			   GSList *caps_list)
{
	ParrilladaCapsTest *test = NULL;
	ParrilladaBurnCaps *self;
	GSList *iter;

	/* Find the the ParrilladaCapsTest for this type; if none create it */
	self = parrillada_burn_caps_get_default ();

	for (iter = self->priv->tests; iter; iter = iter->next) {
		ParrilladaCapsTest *tmp;

		tmp = iter->data;
		if (tmp->type == type) {
			test = tmp;
			break;
		}
	}

	if (!test) {
		test = g_new0 (ParrilladaCapsTest, 1);
		test->type = type;
		self->priv->tests = g_slist_prepend (self->priv->tests, test);
	}

	g_object_unref (self);

	for (; caps_list; caps_list = caps_list->next) {
		GSList *links;
		ParrilladaCaps *caps;
		ParrilladaCapsLink *link;

		caps = caps_list->data;

		/* try to find a link for the above caps, if none create one */
		link = NULL;
		for (links = test->links; links; links = links->next) {
			ParrilladaCapsLink *tmp;

			tmp = links->data;
			if (tmp->caps == caps) {
				link = tmp;
				break;
			}
		}

		if (!link) {
			link = g_new0 (ParrilladaCapsLink, 1);
			link->caps = caps;
			test->links = g_slist_prepend (test->links, link);
		}

		link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

void
parrillada_plugin_register_group (ParrilladaPlugin *plugin,
			       const gchar *name)
{
	guint retval;
	ParrilladaBurnCaps *self;

	if (!name) {
		parrillada_plugin_set_group (plugin, 0);
		return;
	}

	self = parrillada_burn_caps_get_default ();

	if (!self->priv->groups)
		self->priv->groups = g_hash_table_new_full (g_str_hash,
							    g_str_equal,
							    g_free,
							    NULL);

	retval = GPOINTER_TO_INT (g_hash_table_lookup (self->priv->groups, name));
	if (retval) {
		parrillada_plugin_set_group (plugin, retval);
		g_object_unref (self);
		return;
	}

	g_hash_table_insert (self->priv->groups,
			     g_strdup (name),
			     GINT_TO_POINTER (g_hash_table_size (self->priv->groups) + 1));

	/* see if we have a group id now */
	if (!self->priv->group_id
	&&   self->priv->group_str
	&&  !strcmp (name, self->priv->group_str))
		self->priv->group_id = g_hash_table_size (self->priv->groups) + 1;

	parrillada_plugin_set_group (plugin, g_hash_table_size (self->priv->groups) + 1);

	g_object_unref (self);
}

/** 
 * This is to find out what are the capacities of a plugin
 * Declared in parrillada-plugin-private.h
 */

ParrilladaBurnResult
parrillada_plugin_can_burn (ParrilladaPlugin *plugin)
{
	GSList *iter;
	ParrilladaBurnCaps *self;

	self = parrillada_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != PARRILLADA_TRACK_TYPE_DISC)
			continue;

		for (links = caps->links; links; links = links->next) {
			ParrilladaCapsLink *link;
			GSList *plugins;

			link = links->data;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				ParrilladaPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin) {
					g_object_unref (self);
					return PARRILLADA_BURN_OK;
				}
			}
		}
	}

	g_object_unref (self);
	return PARRILLADA_BURN_NOT_SUPPORTED;
}

ParrilladaBurnResult
parrillada_plugin_can_image (ParrilladaPlugin *plugin)
{
	GSList *iter;
	ParrilladaBurnCaps *self;

	self = parrillada_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaTrackDataType destination;
		ParrilladaCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != PARRILLADA_TRACK_TYPE_IMAGE
		&&  caps->type.type != PARRILLADA_TRACK_TYPE_STREAM
		&&  caps->type.type != PARRILLADA_TRACK_TYPE_DATA)
			continue;

		destination = caps->type.type;
		for (links = caps->links; links; links = links->next) {
			ParrilladaCapsLink *link;
			GSList *plugins;

			link = links->data;
			if (!link->caps
			||   link->caps->type.type == destination)
				continue;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				ParrilladaPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin) {
					g_object_unref (self);
					return PARRILLADA_BURN_OK;
				}
			}
		}
	}

	g_object_unref (self);
	return PARRILLADA_BURN_NOT_SUPPORTED;
}

ParrilladaBurnResult
parrillada_plugin_can_convert (ParrilladaPlugin *plugin)
{
	GSList *iter;
	ParrilladaBurnCaps *self;

	self = parrillada_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaTrackDataType destination;
		ParrilladaCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != PARRILLADA_TRACK_TYPE_IMAGE
		&&  caps->type.type != PARRILLADA_TRACK_TYPE_STREAM)
			continue;

		destination = caps->type.type;
		for (links = caps->links; links; links = links->next) {
			ParrilladaCapsLink *link;
			GSList *plugins;

			link = links->data;
			if (!link->caps
			||   link->caps->type.type != destination)
				continue;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				ParrilladaPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin) {
					g_object_unref (self);
					return PARRILLADA_BURN_OK;
				}
			}
		}
	}

	g_object_unref (self);

	return PARRILLADA_BURN_NOT_SUPPORTED;
}


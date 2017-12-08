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
#include <glib/gi18n.h>

#include "burn-caps.h"
#include "burn-debug.h"
#include "parrillada-plugin.h"
#include "parrillada-plugin-private.h"
#include "parrillada-plugin-information.h"
#include "parrillada-session-helper.h"

#define PARRILLADA_BURN_CAPS_SHOULD_BLANK(media_MACRO, flags_MACRO)		\
	((media_MACRO & PARRILLADA_MEDIUM_UNFORMATTED) ||				\
	((media_MACRO & (PARRILLADA_MEDIUM_HAS_AUDIO|PARRILLADA_MEDIUM_HAS_DATA)) &&	\
	 (flags_MACRO & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND)) == FALSE))

#define PARRILLADA_BURN_CAPS_NOT_SUPPORTED_LOG_RES(session)			\
{										\
	parrillada_burn_session_log (session, "Unsupported type of task operation"); \
	PARRILLADA_BURN_LOG ("Unsupported type of task operation");		\
	return PARRILLADA_BURN_NOT_SUPPORTED;					\
}

static ParrilladaBurnResult
parrillada_burn_caps_get_blanking_flags_real (ParrilladaBurnCaps *caps,
                                           gboolean ignore_errors,
					   ParrilladaMedia media,
					   ParrilladaBurnFlag session_flags,
					   ParrilladaBurnFlag *supported,
					   ParrilladaBurnFlag *compulsory)
{
	GSList *iter;
	gboolean supported_media;
	ParrilladaBurnFlag supported_flags = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag compulsory_flags = PARRILLADA_BURN_FLAG_ALL;

	PARRILLADA_BURN_LOG_DISC_TYPE (media, "Getting blanking flags for");
	if (media == PARRILLADA_MEDIUM_NONE) {
		PARRILLADA_BURN_LOG ("Blanking not possible: no media");
		if (supported)
			*supported = PARRILLADA_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = PARRILLADA_BURN_FLAG_NONE;
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	supported_media = FALSE;
	for (iter = caps->priv->caps_list; iter; iter = iter->next) {
		ParrilladaMedia caps_media;
		ParrilladaCaps *caps;
		GSList *links;

		caps = iter->data;
		if (!parrillada_track_type_get_has_medium (&caps->type))
			continue;

		caps_media = parrillada_track_type_get_medium_type (&caps->type);
		if ((media & caps_media) != media)
			continue;

		for (links = caps->links; links; links = links->next) {
			GSList *plugins;
			ParrilladaCapsLink *link;

			link = links->data;

			if (link->caps != NULL)
				continue;

			supported_media = TRUE;
			/* don't need the plugins to be sorted since we go
			 * through all the plugin list to get all blanking flags
			 * available. */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				ParrilladaPlugin *plugin;
				ParrilladaBurnFlag supported_plugin;
				ParrilladaBurnFlag compulsory_plugin;

				plugin = plugins->data;
				if (!parrillada_plugin_get_active (plugin, ignore_errors))
					continue;

				if (!parrillada_plugin_get_blank_flags (plugin,
								     media,
								     session_flags,
								     &supported_plugin,
								     &compulsory_plugin))
					continue;

				supported_flags |= supported_plugin;
				compulsory_flags &= compulsory_plugin;
			}
		}
	}

	if (!supported_media) {
		PARRILLADA_BURN_LOG ("media blanking not supported");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	/* This is a special case that is in MMC specs:
	 * DVD-RW sequential must be fully blanked
	 * if we really want multisession support. */
	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW)
	&& (session_flags & PARRILLADA_BURN_FLAG_MULTI)) {
		if (compulsory_flags & PARRILLADA_BURN_FLAG_FAST_BLANK) {
			PARRILLADA_BURN_LOG ("fast media blanking only supported but multisession required for DVDRW");
			return PARRILLADA_BURN_NOT_SUPPORTED;
		}

		supported_flags &= ~PARRILLADA_BURN_FLAG_FAST_BLANK;

		PARRILLADA_BURN_LOG ("removed fast blank for a DVDRW with multisession");
	}

	if (supported)
		*supported = supported_flags;
	if (compulsory)
		*compulsory = compulsory_flags;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_blank_flags:
 * @session: a #ParrilladaBurnSession
 * @supported: a #ParrilladaBurnFlag
 * @compulsory: a #ParrilladaBurnFlag
 *
 * Given the various parameters stored in @session,
 * stores in @supported and @compulsory, the flags
 * that can be used (@supported) and must be used
 * (@compulsory) when blanking the medium in the
 * #ParrilladaDrive (set with parrillada_burn_session_set_burner ()).
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if the retrieval was successful.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_get_blank_flags (ParrilladaBurnSession *session,
				      ParrilladaBurnFlag *supported,
				      ParrilladaBurnFlag *compulsory)
{
	ParrilladaMedia media;
	ParrilladaBurnCaps *self;
	ParrilladaBurnResult result;
	ParrilladaBurnFlag session_flags;

	media = parrillada_burn_session_get_dest_media (session);
	PARRILLADA_BURN_LOG_DISC_TYPE (media, "Getting blanking flags for");

	if (media == PARRILLADA_MEDIUM_NONE) {
		PARRILLADA_BURN_LOG ("Blanking not possible: no media");
		if (supported)
			*supported = PARRILLADA_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = PARRILLADA_BURN_FLAG_NONE;
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	session_flags = parrillada_burn_session_get_flags (session);

	self = parrillada_burn_caps_get_default ();
	result = parrillada_burn_caps_get_blanking_flags_real (self,
	                                                    parrillada_burn_session_get_strict_support (session) == FALSE,
							    media,
							    session_flags,
							    supported,
							    compulsory);
	g_object_unref (self);

	return result;
}

static ParrilladaBurnResult
parrillada_burn_caps_can_blank_real (ParrilladaBurnCaps *self,
                                  gboolean ignore_plugin_errors,
                                  ParrilladaMedia media,
				  ParrilladaBurnFlag flags)
{
	GSList *iter;

	PARRILLADA_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");
	if (media == PARRILLADA_MEDIUM_NONE) {
		PARRILLADA_BURN_LOG ("no media => no blanking possible");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	/* This is a special case from MMC: DVD-RW sequential
	 * can only be multisession is they were fully blanked
	 * so if there are the two flags, abort. */
	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW)
	&&  (flags & PARRILLADA_BURN_FLAG_MULTI)
	&&  (flags & PARRILLADA_BURN_FLAG_FAST_BLANK)) {
		PARRILLADA_BURN_LOG ("fast media blanking only supported but multisession required for DVD-RW");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != PARRILLADA_TRACK_TYPE_DISC)
			continue;

		if ((media & parrillada_track_type_get_medium_type (&caps->type)) != media)
			continue;

		PARRILLADA_BURN_LOG_TYPE (&caps->type, "Searching links for caps");

		for (links = caps->links; links; links = links->next) {
			GSList *plugins;
			ParrilladaCapsLink *link;

			link = links->data;

			if (link->caps != NULL)
				continue;

			PARRILLADA_BURN_LOG ("Searching plugins");

			/* Go through all plugins for the link and stop if we 
			 * find at least one active plugin that accepts the
			 * flags. No need for plugins to be sorted */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				ParrilladaPlugin *plugin;

				plugin = plugins->data;
				if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
					continue;

				if (parrillada_plugin_check_blank_flags (plugin, media, flags)) {
					PARRILLADA_BURN_LOG_DISC_TYPE (media, "Can blank");
					return PARRILLADA_BURN_OK;
				}
			}
		}
	}

	PARRILLADA_BURN_LOG_DISC_TYPE (media, "No blanking capabilities for");

	return PARRILLADA_BURN_NOT_SUPPORTED;
}

/**
 * parrillada_burn_session_can_blank:
 * @session: a #ParrilladaBurnSession
 *
 * Given the various parameters stored in @session, this
 * function checks whether the medium in the
 * #ParrilladaDrive (set with parrillada_burn_session_set_burner ())
 * can be blanked.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it is possible.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_can_blank (ParrilladaBurnSession *session)
{
	ParrilladaMedia media;
	ParrilladaBurnFlag flags;
	ParrilladaBurnCaps *self;
	ParrilladaBurnResult result;

	self = parrillada_burn_caps_get_default ();

	media = parrillada_burn_session_get_dest_media (session);
	PARRILLADA_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");

	if (media == PARRILLADA_MEDIUM_NONE) {
		PARRILLADA_BURN_LOG ("no media => no blanking possible");
		g_object_unref (self);
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	flags = parrillada_burn_session_get_flags (session);
	result = parrillada_burn_caps_can_blank_real (self,
	                                           parrillada_burn_session_get_strict_support (session) == FALSE,
	                                           media,
	                                           flags);
	g_object_unref (self);

	return result;
}

static void
parrillada_caps_link_get_record_flags (ParrilladaCapsLink *link,
                                    gboolean ignore_plugin_errors,
				    ParrilladaMedia media,
				    ParrilladaBurnFlag session_flags,
				    ParrilladaBurnFlag *supported,
				    ParrilladaBurnFlag *compulsory_retval)
{
	GSList *iter;
	ParrilladaBurnFlag compulsory;

	compulsory = PARRILLADA_BURN_FLAG_ALL;

	/* Go through all plugins to get the supported/... record flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		ParrilladaPlugin *plugin;
		ParrilladaBurnFlag plugin_supported;
		ParrilladaBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = parrillada_plugin_get_record_flags (plugin,
							  media,
							  session_flags,
							  &plugin_supported,
							  &plugin_compulsory);
		if (!result)
			continue;

		*supported |= plugin_supported;
		compulsory &= plugin_compulsory;
	}

	*compulsory_retval = compulsory;
}

static void
parrillada_caps_link_get_data_flags (ParrilladaCapsLink *link,
                                  gboolean ignore_plugin_errors,
				  ParrilladaMedia media,
				  ParrilladaBurnFlag session_flags,
				  ParrilladaBurnFlag *supported)
{
	GSList *iter;

	/* Go through all plugins the get the supported/... data flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		ParrilladaPlugin *plugin;
		ParrilladaBurnFlag plugin_supported;
		ParrilladaBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		parrillada_plugin_get_image_flags (plugin,
		                                media,
		                                session_flags,
		                                &plugin_supported,
		                                &plugin_compulsory);
		*supported |= plugin_supported;
	}
}

static gboolean
parrillada_caps_link_check_data_flags (ParrilladaCapsLink *link,
                                    gboolean ignore_plugin_errors,
				    ParrilladaBurnFlag session_flags,
				    ParrilladaMedia media)
{
	GSList *iter;
	ParrilladaBurnFlag flags;

	/* here we just make sure that at least one of the plugins in the link
	 * can comply with the flags (APPEND/MERGE) */
	flags = session_flags & (PARRILLADA_BURN_FLAG_APPEND|PARRILLADA_BURN_FLAG_MERGE);

	/* If there are no image flags forget it */
	if (flags == PARRILLADA_BURN_FLAG_NONE)
		return TRUE;

	/* Go through all plugins; at least one must support image flags */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		ParrilladaPlugin *plugin;

		plugin = iter->data;
		if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = parrillada_plugin_check_image_flags (plugin,
							   media,
							   session_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
parrillada_caps_link_check_record_flags (ParrilladaCapsLink *link,
                                      gboolean ignore_plugin_errors,
				      ParrilladaBurnFlag session_flags,
				      ParrilladaMedia media)
{
	GSList *iter;
	ParrilladaBurnFlag flags;

	flags = session_flags & PARRILLADA_PLUGIN_BURN_FLAG_MASK;

	/* If there are no record flags forget it */
	if (flags == PARRILLADA_BURN_FLAG_NONE)
		return TRUE;
	
	/* Go through all plugins: at least one must support record flags */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		ParrilladaPlugin *plugin;

		plugin = iter->data;
		if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = parrillada_plugin_check_record_flags (plugin,
							    media,
							    session_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
parrillada_caps_link_check_media_restrictions (ParrilladaCapsLink *link,
                                            gboolean ignore_plugin_errors,
					    ParrilladaMedia media)
{
	GSList *iter;

	/* Go through all plugins: at least one must support media */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		ParrilladaPlugin *plugin;

		plugin = iter->data;
		if (!parrillada_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = parrillada_plugin_check_media_restrictions (plugin, media);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static ParrilladaBurnResult
parrillada_caps_report_plugin_error (ParrilladaPlugin *plugin,
                                  ParrilladaForeachPluginErrorCb callback,
                                  gpointer user_data)
{
	GSList *errors;

	errors = parrillada_plugin_get_errors (plugin);
	if (!errors)
		return PARRILLADA_BURN_ERR;

	do {
		ParrilladaPluginError *error;
		ParrilladaBurnResult result;

		error = errors->data;
		result = callback (error->type, error->detail, user_data);
		if (result == PARRILLADA_BURN_RETRY) {
			/* Something has been done
			 * to fix the error like an install
			 * so reload the errors */
			parrillada_plugin_check_plugin_ready (plugin);
			errors = parrillada_plugin_get_errors (plugin);
			continue;
		}

		if (result != PARRILLADA_BURN_OK)
			return result;

		errors = errors->next;
	} while (errors);

	return PARRILLADA_BURN_OK;
}

struct _ParrilladaFindLinkCtx {
	ParrilladaMedia media;
	ParrilladaTrackType *input;
	ParrilladaPluginIOFlag io_flags;
	ParrilladaBurnFlag session_flags;

	ParrilladaForeachPluginErrorCb callback;
	gpointer user_data;

	guint ignore_plugin_errors:1;
	guint check_session_flags:1;
};
typedef struct _ParrilladaFindLinkCtx ParrilladaFindLinkCtx;

static void
parrillada_caps_find_link_set_ctx (ParrilladaBurnSession *session,
                                ParrilladaFindLinkCtx *ctx,
                                ParrilladaTrackType *input)
{
	ctx->input = input;

	if (ctx->check_session_flags) {
		ctx->session_flags = parrillada_burn_session_get_flags (session);

		if (PARRILLADA_BURN_SESSION_NO_TMP_FILE (session))
			ctx->io_flags = PARRILLADA_PLUGIN_IO_ACCEPT_PIPE;
		else
			ctx->io_flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE;
	}
	else
		ctx->io_flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					PARRILLADA_PLUGIN_IO_ACCEPT_PIPE;

	if (!ctx->callback)
		ctx->ignore_plugin_errors = (parrillada_burn_session_get_strict_support (session) == FALSE);
	else
		ctx->ignore_plugin_errors = TRUE;
}

static ParrilladaBurnResult
parrillada_caps_find_link (ParrilladaCaps *caps,
                        ParrilladaFindLinkCtx *ctx)
{
	GSList *iter;

	PARRILLADA_BURN_LOG_WITH_TYPE (&caps->type, PARRILLADA_PLUGIN_IO_NONE, "Found link (with %i links):", g_slist_length (caps->links));

	/* Here we only make sure we have at least one link working. For a link
	 * to be followed it must first:
	 * - link to a caps with correct io flags
	 * - have at least a plugin accepting the record flags if caps type is
	 *   a disc (that means that the link is the recording part)
	 *
	 * and either:
	 * - link to a caps equal to the input
	 * - link to a caps (linking itself to another caps, ...) accepting the
	 *   input
	 */

	for (iter = caps->links; iter; iter = iter->next) {
		ParrilladaCapsLink *link;
		ParrilladaBurnResult result;

		link = iter->data;

		if (!link->caps)
			continue;
		
		/* check that the link has some active plugin */
		if (!parrillada_caps_link_active (link, ctx->ignore_plugin_errors))
			continue;

		/* since this link contains recorders, check that at least one
		 * of them can handle the record flags */
		if (ctx->check_session_flags && parrillada_track_type_get_has_medium (&caps->type)) {
			if (!parrillada_caps_link_check_record_flags (link, ctx->ignore_plugin_errors, ctx->session_flags, ctx->media))
				continue;

			if (parrillada_caps_link_check_recorder_flags_for_input (link, ctx->session_flags) != PARRILLADA_BURN_OK)
				continue;
		}

		/* first see if that's the perfect fit:
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO */
		if (parrillada_track_type_get_has_data (&link->caps->type)) {
			if (ctx->check_session_flags
			&& !parrillada_caps_link_check_data_flags (link, ctx->ignore_plugin_errors, ctx->session_flags, ctx->media))
				continue;
		}
		else if (!parrillada_caps_link_check_media_restrictions (link, ctx->ignore_plugin_errors, ctx->media))
			continue;

		if ((link->caps->flags & PARRILLADA_PLUGIN_IO_ACCEPT_FILE)
		&&   parrillada_caps_is_compatible_type (link->caps, ctx->input)) {
			if (ctx->callback) {
				ParrilladaPlugin *plugin;

				/* If we are supposed to report/signal that the plugin
				 * could be used but only if some more elements are 
				 * installed */
				plugin = parrillada_caps_link_need_download (link);
				if (plugin)
					return parrillada_caps_report_plugin_error (plugin, ctx->callback, ctx->user_data);
			}
			return PARRILLADA_BURN_OK;
		}

		/* we can't go further than a DISC type */
		if (parrillada_track_type_get_has_medium (&link->caps->type))
			continue;

		if ((link->caps->flags & ctx->io_flags) == PARRILLADA_PLUGIN_IO_NONE)
			continue;

		/* try to see where the inputs of this caps leads to */
		result = parrillada_caps_find_link (link->caps, ctx);
		if (result == PARRILLADA_BURN_CANCEL)
			return result;

		if (result == PARRILLADA_BURN_OK) {
			if (ctx->callback) {
				ParrilladaPlugin *plugin;

				/* If we are supposed to report/signal that the plugin
				 * could be used but only if some more elements are 
				 * installed */
				plugin = parrillada_caps_link_need_download (link);
				if (plugin)
					return parrillada_caps_report_plugin_error (plugin, ctx->callback, ctx->user_data);
			}
			return PARRILLADA_BURN_OK;
		}
	}

	return PARRILLADA_BURN_NOT_SUPPORTED;
}

static ParrilladaBurnResult
parrillada_caps_try_output (ParrilladaBurnCaps *self,
                         ParrilladaFindLinkCtx *ctx,
                         ParrilladaTrackType *output)
{
	ParrilladaCaps *caps;

	/* here we search the start caps */
	caps = parrillada_burn_caps_find_start_caps (self, output);
	if (!caps) {
		PARRILLADA_BURN_LOG ("No caps available");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	/* Here flags don't matter as we don't record anything.
	 * Even the IOFlags since that can be checked later with
	 * parrillada_burn_caps_get_flags. */
	if (parrillada_track_type_get_has_medium (output))
		ctx->media = parrillada_track_type_get_medium_type (output);
	else
		ctx->media = PARRILLADA_MEDIUM_FILE;

	return parrillada_caps_find_link (caps, ctx);
}

static ParrilladaBurnResult
parrillada_caps_try_output_with_blanking (ParrilladaBurnCaps *self,
                                       ParrilladaBurnSession *session,
                                       ParrilladaFindLinkCtx *ctx,
                                       ParrilladaTrackType *output)
{
	gboolean result;
	ParrilladaCaps *last_caps;

	result = parrillada_caps_try_output (self, ctx, output);
	if (result == PARRILLADA_BURN_OK
	||  result == PARRILLADA_BURN_CANCEL)
		return result;

	/* we reached this point in two cases:
	 * - if the disc cannot be handled
	 * - if some flags are not handled
	 * It is helpful only if:
	 * - the disc was closed and no plugin can handle this type of 
	 * disc once closed (CD-R(W))
	 * - there was the flag BLANK_BEFORE_WRITE set and no plugin can
	 * handle this flag (means that the plugin should erase and
	 * then write on its own. Basically that works only with
	 * overwrite formatted discs, DVD+RW, ...) */
	if (!parrillada_track_type_get_has_medium (output))
		return PARRILLADA_BURN_NOT_SUPPORTED;

	/* output is a disc try with initial blanking */
	PARRILLADA_BURN_LOG ("Support for input/output failed.");

	/* apparently nothing can be done to reach our goal. Maybe that
	 * is because we first have to blank the disc. If so add a blank 
	 * task to the others as a first step */
	if ((ctx->check_session_flags
	&& !(ctx->session_flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE))
	||   parrillada_burn_session_can_blank (session) != PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	PARRILLADA_BURN_LOG ("Trying with initial blanking");

	/* retry with the same disc type but blank this time */
	ctx->media = parrillada_track_type_get_medium_type (output);
	ctx->media &= ~(PARRILLADA_MEDIUM_CLOSED|
	                PARRILLADA_MEDIUM_APPENDABLE|
	                PARRILLADA_MEDIUM_UNFORMATTED|
	                PARRILLADA_MEDIUM_HAS_DATA|
	                PARRILLADA_MEDIUM_HAS_AUDIO);
	ctx->media |= PARRILLADA_MEDIUM_BLANK;
	parrillada_track_type_set_medium_type (output, ctx->media);

	last_caps = parrillada_burn_caps_find_start_caps (self, output);
	if (!last_caps)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	return parrillada_caps_find_link (last_caps, ctx);
}

/**
 * parrillada_burn_session_input_supported:
 * @session: a #ParrilladaBurnSession
 * @input: a #ParrilladaTrackType
 * @check_flags: a #gboolean
 *
 * Given the various parameters stored in @session, this
 * function checks whether a session with the data type
 * @type could be burnt to the medium in the #ParrilladaDrive (set 
 * through parrillada_burn_session_set_burner ()).
 * If @check_flags is %TRUE, then flags are taken into account
 * and are not if it is %FALSE.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it is possible.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_input_supported (ParrilladaBurnSession *session,
				      ParrilladaTrackType *input,
                                      gboolean check_flags)
{
	ParrilladaBurnCaps *self;
	ParrilladaBurnResult result;
	ParrilladaTrackType output;
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };

	result = parrillada_burn_session_get_output_type (session, &output);
	if (result != PARRILLADA_BURN_OK)
		return result;

	PARRILLADA_BURN_LOG_TYPE (input, "Checking support for input");
	PARRILLADA_BURN_LOG_TYPE (&output, "and output");

	ctx.check_session_flags = check_flags;
	parrillada_caps_find_link_set_ctx (session, &ctx, input);

	if (check_flags) {
		result = parrillada_check_flags_for_drive (parrillada_burn_session_get_burner (session),
							ctx.session_flags);

		if (!result)
			PARRILLADA_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

		PARRILLADA_BURN_LOG_FLAGS (ctx.session_flags, "with flags");
	}

	self = parrillada_burn_caps_get_default ();
	result = parrillada_caps_try_output_with_blanking (self,
							session,
	                                                &ctx,
							&output);
	g_object_unref (self);

	if (result != PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG_TYPE (input, "Input not supported");
		return result;
	}

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_output_supported:
 * @session: a #ParrilladaBurnSession *
 * @output: a #ParrilladaTrackType *
 *
 * Make sure that the image type or medium type defined in @output can be
 * created/burnt given the parameters and the current data set in @session.
 *
 * Return value: PARRILLADA_BURN_OK if the medium type or the image type can be used as an output.
 **/
ParrilladaBurnResult
parrillada_burn_session_output_supported (ParrilladaBurnSession *session,
				       ParrilladaTrackType *output)
{
	ParrilladaBurnCaps *self;
	ParrilladaTrackType input;
	ParrilladaBurnResult result;
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };

	/* Here, we can't check if the drive supports the flags since the output
	 * is hypothetical. There is no real medium. So forget the following :
	 * if (!parrillada_burn_caps_flags_check_for_drive (session))
	 *	PARRILLADA_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);
	 * The only thing we could do would be to check some known forbidden 
	 * flags for some media provided the output type is DISC. */

	parrillada_burn_session_get_input_type (session, &input);
	parrillada_caps_find_link_set_ctx (session, &ctx, &input);

	PARRILLADA_BURN_LOG_TYPE (output, "Checking support for output");
	PARRILLADA_BURN_LOG_TYPE (&input, "and input");
	PARRILLADA_BURN_LOG_FLAGS (parrillada_burn_session_get_flags (session), "with flags");
	
	self = parrillada_burn_caps_get_default ();
	result = parrillada_caps_try_output_with_blanking (self,
							session,
	                                                &ctx,
							output);
	g_object_unref (self);

	if (result != PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG_TYPE (output, "Output not supported");
		return result;
	}

	return PARRILLADA_BURN_OK;
}

/**
 * This is only to be used in case one wants to copy using the same drive.
 * It determines the possible middle image type.
 */

static ParrilladaBurnResult
parrillada_burn_caps_is_session_supported_same_src_dest (ParrilladaBurnCaps *self,
						      ParrilladaBurnSession *session,
                                                      ParrilladaFindLinkCtx *ctx,
                                                      ParrilladaTrackType *tmp_type)
{
	GSList *iter;
	ParrilladaDrive *burner;
	ParrilladaTrackType input;
	ParrilladaBurnResult result;
	ParrilladaTrackType output;
	ParrilladaImageFormat format;

	PARRILLADA_BURN_LOG ("Checking disc copy support with same source and destination");

	/* To determine if a CD/DVD can be copied using the same source/dest,
	 * we first determine if can be imaged and then if this image can be 
	 * burnt to whatever medium type. */
	parrillada_caps_find_link_set_ctx (session, ctx, &input);
	ctx->io_flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE;

	memset (&input, 0, sizeof (ParrilladaTrackType));
	parrillada_burn_session_get_input_type (session, &input);
	PARRILLADA_BURN_LOG_TYPE (&input, "input");

	if (ctx->check_session_flags) {
		/* NOTE: DAO can be a problem. So just in case remove it. It is
		 * not really useful in this context. What we want here is to
		 * know whether a medium can be used given the input; only 1
		 * flag is important here (MERGE) and can have consequences. */
		ctx->session_flags &= ~PARRILLADA_BURN_FLAG_DAO;
		PARRILLADA_BURN_LOG_FLAGS (ctx->session_flags, "flags");
	}

	burner = parrillada_burn_session_get_burner (session);

	/* First see if it works with a stream type */
	parrillada_track_type_set_has_stream (&output);

	/* FIXME! */
	parrillada_track_type_set_stream_format (&output,
	                                      PARRILLADA_AUDIO_FORMAT_RAW|
	                                      PARRILLADA_METADATA_INFO);

	PARRILLADA_BURN_LOG_TYPE (&output, "Testing stream type");
	result = parrillada_caps_try_output (self, ctx, &output);
	if (result == PARRILLADA_BURN_CANCEL)
		return result;

	if (result == PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG ("Stream type seems to be supported as output");

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			ParrilladaBurnResult result;
			ParrilladaMedia media;
			ParrilladaCaps *caps;

			caps = iter->data;

			if (!parrillada_track_type_get_has_medium (&caps->type))
				continue;

			media = parrillada_track_type_get_medium_type (&caps->type);
			/* Audio is only supported by CDs */
			if ((media & PARRILLADA_MEDIUM_CD) == 0)
				continue;

			/* This type of disc cannot be burnt; skip them */
			if (media & PARRILLADA_MEDIUM_ROM)
				continue;

			/* Make sure this is supported by the drive */
			if (!parrillada_drive_can_write_media (burner, media))
				continue;

			ctx->media = media;
			result = parrillada_caps_find_link (caps, ctx);
			PARRILLADA_BURN_LOG_DISC_TYPE (media,
						    "Tested medium (%s)",
						    result == PARRILLADA_BURN_OK ? "working":"not working");

			if (result == PARRILLADA_BURN_OK) {
				if (tmp_type) {
					parrillada_track_type_set_has_stream (tmp_type);
					parrillada_track_type_set_stream_format (tmp_type, parrillada_track_type_get_stream_format (&output));
				}
					
				return PARRILLADA_BURN_OK;
			}

			if (result == PARRILLADA_BURN_CANCEL)
				return result;
		}
	}
	else
		PARRILLADA_BURN_LOG ("Stream format not supported as output");

	/* Find one available output format */
	format = PARRILLADA_IMAGE_FORMAT_CDRDAO;
	parrillada_track_type_set_has_image (&output);

	for (; format > PARRILLADA_IMAGE_FORMAT_NONE; format >>= 1) {
		parrillada_track_type_set_image_format (&output, format);

		PARRILLADA_BURN_LOG_TYPE (&output, "Testing temporary image format");

		/* Don't need to try blanking here (saves
		 * a few lines of debug) since that is an 
		 * image */
		result = parrillada_caps_try_output (self, ctx, &output);
		if (result == PARRILLADA_BURN_CANCEL)
			return result;

		if (result != PARRILLADA_BURN_OK)
			continue;

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			ParrilladaBurnResult result;
			ParrilladaMedia media;
			ParrilladaCaps *caps;

			caps = iter->data;

			if (!parrillada_track_type_get_has_medium (&caps->type))
				continue;

			media = parrillada_track_type_get_medium_type (&caps->type);

			/* This type of disc cannot be burnt; skip them */
			if (media & PARRILLADA_MEDIUM_ROM)
				continue;

			/* These three types only work with CDs. Skip the rest. */
			if ((format == PARRILLADA_IMAGE_FORMAT_CDRDAO
			||   format == PARRILLADA_IMAGE_FORMAT_CLONE
			||   format == PARRILLADA_IMAGE_FORMAT_CUE)
			&& (media & PARRILLADA_MEDIUM_CD) == 0)
				continue;

			/* Make sure this is supported by the drive */
			if (!parrillada_drive_can_write_media (burner, media))
				continue;

			ctx->media = media;
			result = parrillada_caps_find_link (caps, ctx);
			PARRILLADA_BURN_LOG_DISC_TYPE (media,
						    "Tested medium (%s)",
						    result == PARRILLADA_BURN_OK ? "working":"not working");

			if (result == PARRILLADA_BURN_OK) {
				if (tmp_type) {
					parrillada_track_type_set_has_image (tmp_type);
					parrillada_track_type_set_image_format (tmp_type, parrillada_track_type_get_image_format (&output));
				}
					
				return PARRILLADA_BURN_OK;
			}

			if (result == PARRILLADA_BURN_CANCEL)
				return result;
		}
	}

	return PARRILLADA_BURN_NOT_SUPPORTED;
}

ParrilladaBurnResult
parrillada_burn_session_get_tmp_image_type_same_src_dest (ParrilladaBurnSession *session,
                                                       ParrilladaTrackType *image_type)
{
	ParrilladaBurnCaps *self;
	ParrilladaBurnResult result;
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	self = parrillada_burn_caps_get_default ();
	result = parrillada_burn_caps_is_session_supported_same_src_dest (self,
	                                                               session,
	                                                               &ctx,
	                                                               image_type);
	g_object_unref (self);
	return result;
}

static ParrilladaBurnResult
parrillada_burn_session_supported (ParrilladaBurnSession *session,
                                ParrilladaFindLinkCtx *ctx)
{
	gboolean result;
	ParrilladaBurnCaps *self;
	ParrilladaTrackType input;
	ParrilladaTrackType output;

	/* Special case */
	if (parrillada_burn_session_same_src_dest_drive (session)) {
		ParrilladaBurnResult res;

		self = parrillada_burn_caps_get_default ();
		res = parrillada_burn_caps_is_session_supported_same_src_dest (self, session, ctx, NULL);
		g_object_unref (self);

		return res;
	}

	result = parrillada_burn_session_get_output_type (session, &output);
	if (result != PARRILLADA_BURN_OK)
		PARRILLADA_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

	parrillada_burn_session_get_input_type (session, &input);
	parrillada_caps_find_link_set_ctx (session, ctx, &input);

	PARRILLADA_BURN_LOG_TYPE (&output, "Checking support for session. Ouput is ");
	PARRILLADA_BURN_LOG_TYPE (&input, "and input is");

	if (ctx->check_session_flags) {
		result = parrillada_check_flags_for_drive (parrillada_burn_session_get_burner (session), ctx->session_flags);
		if (!result)
			PARRILLADA_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

		PARRILLADA_BURN_LOG_FLAGS (ctx->session_flags, "with flags");
	}

	self = parrillada_burn_caps_get_default ();
	result = parrillada_caps_try_output_with_blanking (self,
							session,
	                                                ctx,
							&output);
	g_object_unref (self);

	if (result != PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG_TYPE (&output, "Session not supported");
		return result;
	}

	PARRILLADA_BURN_LOG_TYPE (&output, "Session supported");
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_can_burn:
 * @session: a #ParrilladaBurnSession
 * @check_flags: a #gboolean
 *
 * Given the various parameters stored in @session, this
 * function checks whether the data contained in @session
 * can be burnt to the medium in the #ParrilladaDrive (set 
 * through parrillada_burn_session_set_burner ()).
 * If @check_flags determine the behavior of this function.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it is possible.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_can_burn (ParrilladaBurnSession *session,
			       gboolean check_flags)
{
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	ctx.check_session_flags = check_flags;

	return parrillada_burn_session_supported (session, &ctx);
}

/**
 * parrillada_session_foreach_plugin_error:
 * @session: a #ParrilladaBurnSession.
 * @callback: a #ParrilladaSessionPluginErrorCb.
 * @user_data: a #gpointer. The data passed to @callback.
 *
 * Call @callback for each error in plugins.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it is possible.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_session_foreach_plugin_error (ParrilladaBurnSession *session,
                                      ParrilladaForeachPluginErrorCb callback,
                                      gpointer user_data)
{
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	ctx.callback = callback;
	ctx.user_data = user_data;
	
	return parrillada_burn_session_supported (session, &ctx);
}

/**
 * parrillada_burn_session_get_required_media_type:
 * @session: a #ParrilladaBurnSession
 *
 * Return the medium types that could be used to burn
 * @session.
 *
 * Return value: a #ParrilladaMedia
 **/

ParrilladaMedia
parrillada_burn_session_get_required_media_type (ParrilladaBurnSession *session)
{
	ParrilladaMedia required_media = PARRILLADA_MEDIUM_NONE;
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };
	ParrilladaTrackType input;
	ParrilladaBurnCaps *self;
	GSList *iter;

	if (parrillada_burn_session_is_dest_file (session))
		return PARRILLADA_MEDIUM_FILE;

	/* we try to determine here what type of medium is allowed to be burnt
	 * to whether a CD or a DVD. Appendable, blank are not properties being
	 * determined here. We just want it to be writable in a broad sense. */
	ctx.check_session_flags = TRUE;
	parrillada_burn_session_get_input_type (session, &input);
	parrillada_caps_find_link_set_ctx (session, &ctx, &input);
	PARRILLADA_BURN_LOG_TYPE (&input, "Determining required media type for input");

	/* NOTE: PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE is a problem here since it
	 * is only used if needed. Likewise DAO can be a problem. So just in
	 * case remove them. They are not really useful in this context. What we
	 * want here is to know which media can be used given the input; only 1
	 * flag is important here (MERGE) and can have consequences. */
	ctx.session_flags &= ~PARRILLADA_BURN_FLAG_DAO;
	PARRILLADA_BURN_LOG_FLAGS (ctx.session_flags, "and flags");

	self = parrillada_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaCaps *caps;
		gboolean result;

		caps = iter->data;

		if (!parrillada_track_type_get_has_medium (&caps->type))
			continue;

		/* Put PARRILLADA_MEDIUM_NONE so we can always succeed */
		result = parrillada_caps_find_link (caps, &ctx);
		PARRILLADA_BURN_LOG_DISC_TYPE (caps->type.subtype.media,
					    "Tested (%s)",
					    result == PARRILLADA_BURN_OK ? "working":"not working");

		if (result == PARRILLADA_BURN_CANCEL) {
			g_object_unref (self);
			return result;
		}

		if (result != PARRILLADA_BURN_OK)
			continue;

		/* This caps work, add its subtype */
		required_media |= parrillada_track_type_get_medium_type (&caps->type);
	}

	/* filter as we are only interested in these */
	required_media &= PARRILLADA_MEDIUM_WRITABLE|
			  PARRILLADA_MEDIUM_CD|
			  PARRILLADA_MEDIUM_DVD;

	g_object_unref (self);
	return required_media;
}

/**
 * parrillada_burn_session_get_possible_output_formats:
 * @session: a #ParrilladaBurnSession
 * @formats: a #ParrilladaImageFormat
 *
 * Returns the disc image types that could be set to create
 * an image given the current state of @session.
 *
 * Return value: a #guint. The number of formats available.
 **/

guint
parrillada_burn_session_get_possible_output_formats (ParrilladaBurnSession *session,
						  ParrilladaImageFormat *formats)
{
	guint num = 0;
	ParrilladaImageFormat format;
	ParrilladaTrackType *output = NULL;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), 0);

	/* see how many output format are available */
	format = PARRILLADA_IMAGE_FORMAT_CDRDAO;
	(*formats) = PARRILLADA_IMAGE_FORMAT_NONE;

	output = parrillada_track_type_new ();
	parrillada_track_type_set_has_image (output);

	for (; format > PARRILLADA_IMAGE_FORMAT_NONE; format >>= 1) {
		ParrilladaBurnResult result;

		parrillada_track_type_set_image_format (output, format);
		result = parrillada_burn_session_output_supported (session, output);
		if (result == PARRILLADA_BURN_OK) {
			(*formats) |= format;
			num ++;
		}
	}

	parrillada_track_type_free (output);

	return num;
}

/**
 * parrillada_burn_session_get_default_output_format:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the default disc image type that should be set to create
 * an image given the current state of @session.
 *
 * Return value: a #ParrilladaImageFormat
 **/

ParrilladaImageFormat
parrillada_burn_session_get_default_output_format (ParrilladaBurnSession *session)
{
	ParrilladaBurnCaps *self;
	ParrilladaBurnResult result;
	ParrilladaTrackType source = { PARRILLADA_TRACK_TYPE_NONE, { 0, }};
	ParrilladaTrackType output = { PARRILLADA_TRACK_TYPE_NONE, { 0, }};

	self = parrillada_burn_caps_get_default ();

	if (!parrillada_burn_session_is_dest_file (session)) {
		g_object_unref (self);
		return PARRILLADA_IMAGE_FORMAT_NONE;
	}

	parrillada_burn_session_get_input_type (session, &source);
	if (parrillada_track_type_is_empty (&source)) {
		g_object_unref (self);
		return PARRILLADA_IMAGE_FORMAT_NONE;
	}

	if (parrillada_track_type_get_has_image (&source)) {
		g_object_unref (self);
		return parrillada_track_type_get_image_format (&source);
	}

	parrillada_track_type_set_has_image (&output);
	parrillada_track_type_set_image_format (&output, PARRILLADA_IMAGE_FORMAT_NONE);

	if (parrillada_track_type_get_has_stream (&source)) {
		/* Otherwise try all possible image types */
		output.subtype.img_format = PARRILLADA_IMAGE_FORMAT_CDRDAO;
		for (; output.subtype.img_format != PARRILLADA_IMAGE_FORMAT_NONE;
		       output.subtype.img_format >>= 1) {
		
			result = parrillada_burn_session_output_supported (session,
									&output);
			if (result == PARRILLADA_BURN_OK) {
				g_object_unref (self);
				return parrillada_track_type_get_image_format (&output);
			}
		}

		g_object_unref (self);
		return PARRILLADA_IMAGE_FORMAT_NONE;
	}

	if (parrillada_track_type_get_has_data (&source)
	|| (parrillada_track_type_get_has_medium (&source)
	&& (parrillada_track_type_get_medium_type (&source) & PARRILLADA_MEDIUM_DVD))) {
		parrillada_track_type_set_image_format (&output, PARRILLADA_IMAGE_FORMAT_BIN);
		result = parrillada_burn_session_output_supported (session, &output);
		g_object_unref (self);

		if (result != PARRILLADA_BURN_OK)
			return PARRILLADA_IMAGE_FORMAT_NONE;

		return PARRILLADA_IMAGE_FORMAT_BIN;
	}

	/* for the input which are CDs there are lots of possible formats */
	output.subtype.img_format = PARRILLADA_IMAGE_FORMAT_CDRDAO;
	for (; output.subtype.img_format != PARRILLADA_IMAGE_FORMAT_NONE;
	       output.subtype.img_format >>= 1) {
	
		result = parrillada_burn_session_output_supported (session, &output);
		if (result == PARRILLADA_BURN_OK) {
			g_object_unref (self);
			return parrillada_track_type_get_image_format (&output);
		}
	}

	g_object_unref (self);
	return PARRILLADA_IMAGE_FORMAT_NONE;
}

static ParrilladaBurnResult
parrillada_caps_set_flags_from_recorder_input (ParrilladaTrackType *input,
                                            ParrilladaBurnFlag *supported,
                                            ParrilladaBurnFlag *compulsory)
{
	if (parrillada_track_type_get_has_image (input)) {
		ParrilladaImageFormat format;

		format = parrillada_track_type_get_image_format (input);
		if (format == PARRILLADA_IMAGE_FORMAT_CUE
		||  format == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
			if ((*supported) & PARRILLADA_BURN_FLAG_DAO)
				(*compulsory) |= PARRILLADA_BURN_FLAG_DAO;
			else
				return PARRILLADA_BURN_NOT_SUPPORTED;
		}
		else if (format == PARRILLADA_IMAGE_FORMAT_CLONE) {
			/* RAW write mode should (must) only be used in this case */
			if ((*supported) & PARRILLADA_BURN_FLAG_RAW) {
				(*supported) &= ~PARRILLADA_BURN_FLAG_DAO;
				(*compulsory) &= ~PARRILLADA_BURN_FLAG_DAO;
				(*compulsory) |= PARRILLADA_BURN_FLAG_RAW;
			}
			else
				return PARRILLADA_BURN_NOT_SUPPORTED;
		}
		else
			(*supported) &= ~PARRILLADA_BURN_FLAG_RAW;
	}
	
	return PARRILLADA_BURN_OK;
}

static ParrilladaPluginIOFlag
parrillada_caps_get_flags (ParrilladaCaps *caps,
                        gboolean ignore_plugin_errors,
			ParrilladaBurnFlag session_flags,
			ParrilladaMedia media,
			ParrilladaTrackType *input,
			ParrilladaPluginIOFlag flags,
			ParrilladaBurnFlag *supported,
			ParrilladaBurnFlag *compulsory)
{
	GSList *iter;
	ParrilladaPluginIOFlag retval = PARRILLADA_PLUGIN_IO_NONE;

	/* First we must know if this link leads somewhere. It must 
	 * accept the already existing flags. If it does, see if it 
	 * accepts the input and if not, if one of its ancestors does */
	for (iter = caps->links; iter; iter = iter->next) {
		ParrilladaBurnFlag data_supported = PARRILLADA_BURN_FLAG_NONE;
		ParrilladaBurnFlag rec_compulsory = PARRILLADA_BURN_FLAG_ALL;
		ParrilladaBurnFlag rec_supported = PARRILLADA_BURN_FLAG_NONE;
		ParrilladaPluginIOFlag io_flags;
		ParrilladaCapsLink *link;

		link = iter->data;

		if (!link->caps)
			continue;

		/* check that the link has some active plugin */
		if (!parrillada_caps_link_active (link, ignore_plugin_errors))
			continue;

		if (parrillada_track_type_get_has_medium (&caps->type)) {
			ParrilladaBurnFlag tmp;
			ParrilladaBurnResult result;

			parrillada_caps_link_get_record_flags (link,
			                                    ignore_plugin_errors,
							    media,
							    session_flags,
							    &rec_supported,
							    &rec_compulsory);

			/* see if that link can handle the record flags.
			 * NOTE: compulsory are not a failure in this case. */
			tmp = session_flags & PARRILLADA_PLUGIN_BURN_FLAG_MASK;
			if ((tmp & rec_supported) != tmp)
				continue;

			/* This is the recording plugin, check its input as
			 * some flags depend on it. */
			result = parrillada_caps_set_flags_from_recorder_input (&link->caps->type,
			                                                     &rec_supported,
			                                                     &rec_compulsory);
			if (result != PARRILLADA_BURN_OK)
				continue;
		}

		if (parrillada_track_type_get_has_data (&link->caps->type)) {
			ParrilladaBurnFlag tmp;

			parrillada_caps_link_get_data_flags (link,
			                                  ignore_plugin_errors,
							  media,
							  session_flags,
						    	  &data_supported);

			/* see if that link can handle the data flags. 
			 * NOTE: compulsory are not a failure in this case. */
			tmp = session_flags & (PARRILLADA_BURN_FLAG_APPEND|
					       PARRILLADA_BURN_FLAG_MERGE);

			if ((tmp & data_supported) != tmp)
				continue;
		}
		else if (!parrillada_caps_link_check_media_restrictions (link, ignore_plugin_errors, media))
			continue;

		/* see if that's the perfect fit */
		if ((link->caps->flags & PARRILLADA_PLUGIN_IO_ACCEPT_FILE)
		&&   parrillada_caps_is_compatible_type (link->caps, input)) {
			/* special case for input that handle output/input */
			if (caps->type.type == PARRILLADA_TRACK_TYPE_DISC)
				retval |= PARRILLADA_PLUGIN_IO_ACCEPT_PIPE;
			else
				retval |= caps->flags;

			(*compulsory) &= rec_compulsory;
			(*supported) |= data_supported|rec_supported;
			continue;
		}

		if ((link->caps->flags & flags) == PARRILLADA_PLUGIN_IO_NONE)
			continue;

		/* we can't go further than a DISC type */
		if (link->caps->type.type == PARRILLADA_TRACK_TYPE_DISC)
			continue;

		/* try to see where the inputs of this caps leads to */
		io_flags = parrillada_caps_get_flags (link->caps,
		                                   ignore_plugin_errors,
						   session_flags,
						   media,
						   input,
						   flags,
						   supported,
						   compulsory);
		if (io_flags == PARRILLADA_PLUGIN_IO_NONE)
			continue;

		(*compulsory) &= rec_compulsory;
		retval |= (io_flags & flags);
		(*supported) |= data_supported|rec_supported;
	}

	return retval;
}

static void
parrillada_medium_supported_flags (ParrilladaMedium *medium,
				ParrilladaBurnFlag *supported_flags,
                                ParrilladaBurnFlag *compulsory_flags)
{
	ParrilladaMedia media;

	media = parrillada_medium_get_status (medium);

	/* This is always FALSE */
	if (media & PARRILLADA_MEDIUM_PLUS)
		(*supported_flags) &= ~PARRILLADA_BURN_FLAG_DUMMY;

	/* Simulation is only possible according to write modes. This mode is
	 * mostly used by cdrecord/wodim for CLONE images. */
	else if (media & PARRILLADA_MEDIUM_DVD) {
		if (!parrillada_medium_can_use_dummy_for_sao (medium))
			(*supported_flags) &= ~PARRILLADA_BURN_FLAG_DUMMY;
	}
	else if ((*supported_flags) & PARRILLADA_BURN_FLAG_DAO) {
		if (!parrillada_medium_can_use_dummy_for_sao (medium))
			(*supported_flags) &= ~PARRILLADA_BURN_FLAG_DUMMY;
	}
	else if (!parrillada_medium_can_use_dummy_for_tao (medium))
		(*supported_flags) &= ~PARRILLADA_BURN_FLAG_DUMMY;

	/* The following is only true if we won't _have_ to blank
	 * the disc since a CLOSED disc is not able for tao/sao.
	 * so if BLANK_BEFORE_RIGHT is TRUE then we leave 
	 * the benefit of the doubt, but flags should be rechecked
	 * after the drive was blanked. */
	if (((*compulsory_flags) & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE) == 0
	&&  !PARRILLADA_MEDIUM_RANDOM_WRITABLE (media)
	&&  !parrillada_medium_can_use_tao (medium)) {
		(*supported_flags) &= ~PARRILLADA_BURN_FLAG_MULTI;

		if (parrillada_medium_can_use_sao (medium))
			(*compulsory_flags) |= PARRILLADA_BURN_FLAG_DAO;
		else
			(*supported_flags) &= ~PARRILLADA_BURN_FLAG_DAO;
	}

	if (!parrillada_medium_can_use_burnfree (medium))
		(*supported_flags) &= ~PARRILLADA_BURN_FLAG_BURNPROOF;
}

static void
parrillada_burn_caps_flags_update_for_drive (ParrilladaBurnSession *session,
                                          ParrilladaBurnFlag *supported_flags,
                                          ParrilladaBurnFlag *compulsory_flags)
{
	ParrilladaDrive *drive;
	ParrilladaMedium *medium;

	drive = parrillada_burn_session_get_burner (session);
	if (!drive)
		return;

	medium = parrillada_drive_get_medium (drive);
	if (!medium)
		return;

	parrillada_medium_supported_flags (medium,
	                                supported_flags,
	                                compulsory_flags);
}

static ParrilladaBurnResult
parrillada_caps_get_flags_for_disc (ParrilladaBurnCaps *self,
                                 gboolean ignore_plugin_errors,
				 ParrilladaBurnFlag session_flags,
				 ParrilladaMedia media,
				 ParrilladaTrackType *input,
				 ParrilladaBurnFlag *supported,
				 ParrilladaBurnFlag *compulsory)
{
	ParrilladaBurnFlag supported_flags = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag compulsory_flags = PARRILLADA_BURN_FLAG_ALL;
	ParrilladaPluginIOFlag io_flags;
	ParrilladaTrackType output;
	ParrilladaCaps *caps;

	/* create the output to find first caps */
	parrillada_track_type_set_has_medium (&output);
	parrillada_track_type_set_medium_type (&output, media);

	caps = parrillada_burn_caps_find_start_caps (self, &output);
	if (!caps) {
		PARRILLADA_BURN_LOG_DISC_TYPE (media, "FLAGS: no caps could be found for");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	PARRILLADA_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "FLAGS: trying caps");

	io_flags = parrillada_caps_get_flags (caps,
	                                   ignore_plugin_errors,
					   session_flags,
					   media,
					   input,
					   PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					   PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					   &supported_flags,
					   &compulsory_flags);

	if (io_flags == PARRILLADA_PLUGIN_IO_NONE) {
		PARRILLADA_BURN_LOG ("FLAGS: not supported");
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	/* NOTE: DO NOT TEST the input image here. What should be tested is the
	 * type of the input right before the burner plugin. See:
	 * parrillada_burn_caps_set_flags_from_recorder_input())
	 * For example in the following situation: AUDIO => CUE => BURNER the
	 * DAO flag would not be set otherwise. */

	if (parrillada_track_type_get_has_stream (input)) {
		ParrilladaStreamFormat format;

		format = parrillada_track_type_get_stream_format (input);
		if (format & PARRILLADA_METADATA_INFO) {
			/* In this case, DAO is compulsory if we want to write CD-TEXT */
			if (supported_flags & PARRILLADA_BURN_FLAG_DAO)
				compulsory_flags |= PARRILLADA_BURN_FLAG_DAO;
			else
				return PARRILLADA_BURN_NOT_SUPPORTED;
		}
	}

	if (compulsory_flags & PARRILLADA_BURN_FLAG_DAO) {
		/* unlikely */
		compulsory_flags &= ~PARRILLADA_BURN_FLAG_RAW;
		supported_flags &= ~PARRILLADA_BURN_FLAG_RAW;
	}
	
	if (io_flags & PARRILLADA_PLUGIN_IO_ACCEPT_PIPE) {
		supported_flags |= PARRILLADA_BURN_FLAG_NO_TMP_FILES;

		if ((io_flags & PARRILLADA_PLUGIN_IO_ACCEPT_FILE) == 0)
			compulsory_flags |= PARRILLADA_BURN_FLAG_NO_TMP_FILES;
	}

	*supported |= supported_flags;
	*compulsory |= compulsory_flags;

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_caps_get_flags_for_medium (ParrilladaBurnCaps *self,
                                        ParrilladaBurnSession *session,
					ParrilladaMedia media,
					ParrilladaBurnFlag session_flags,
					ParrilladaTrackType *input,
					ParrilladaBurnFlag *supported_flags,
					ParrilladaBurnFlag *compulsory_flags)
{
	ParrilladaBurnResult result;
	gboolean can_blank = FALSE;

	/* See if medium is supported out of the box */
	result = parrillada_caps_get_flags_for_disc (self,
	                                          parrillada_burn_session_get_strict_support (session) == FALSE,
						  session_flags,
						  media,
						  input,
						  supported_flags,
						  compulsory_flags);

	/* see if we can add PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE. Add it when:
	 * - media can be blanked, it has audio or data and we're not merging
	 * - media is not formatted and it can be blanked/formatted */
	if (parrillada_burn_caps_can_blank_real (self, parrillada_burn_session_get_strict_support (session) == FALSE, media, session_flags) == PARRILLADA_BURN_OK)
		can_blank = TRUE;
	else if (session_flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	if (can_blank) {
		gboolean first_success;
		ParrilladaBurnFlag blank_compulsory = PARRILLADA_BURN_FLAG_NONE;
		ParrilladaBurnFlag blank_supported = PARRILLADA_BURN_FLAG_NONE;

		/* we reached this point in two cases:
		 * - if the disc cannot be handled
		 * - if some flags are not handled
		 * It is helpful only if:
		 * - the disc was closed and no plugin can handle this type of 
		 * disc once closed (CD-R(W))
		 * - there was the flag BLANK_BEFORE_WRITE set and no plugin can
		 * handle this flag (means that the plugin should erase and
		 * then write on its own. Basically that works only with
		 * overwrite formatted discs, DVD+RW, ...) */

		/* What's above is not entirely true. In fact we always need to
		 * check even if we first succeeded. There are some cases like
		 * CDRW where it's useful.
		 * Ex: a CDRW with data appendable can be either appended (then
		 * no DAO possible) or blanked and written (DAO possible). */

		/* result here is the result of the first operation, so if it
		 * failed, BLANK before becomes compulsory. */
		first_success = (result == PARRILLADA_BURN_OK);

		/* pretends it is blank and formatted to see if it would work.
		 * If it works then that means that the BLANK_BEFORE_WRITE flag
		 * is compulsory. */
		media &= ~(PARRILLADA_MEDIUM_CLOSED|
			   PARRILLADA_MEDIUM_APPENDABLE|
			   PARRILLADA_MEDIUM_UNFORMATTED|
			   PARRILLADA_MEDIUM_HAS_DATA|
			   PARRILLADA_MEDIUM_HAS_AUDIO);
		media |= PARRILLADA_MEDIUM_BLANK;
		result = parrillada_caps_get_flags_for_disc (self,
		                                          parrillada_burn_session_get_strict_support (session) == FALSE,
							  session_flags,
							  media,
							  input,
							  supported_flags,
							  compulsory_flags);

		/* if both attempts failed, drop it */
		if (result != PARRILLADA_BURN_OK) {
			/* See if we entirely failed */
			if (!first_success)
				return result;

			/* we tried with a blank medium but did not 
			 * succeed. So that means the flags BLANK.
			 * is not supported */
		}
		else {
			(*supported_flags) |= PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE;

			if (!first_success)
				(*compulsory_flags) |= PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE;

			/* If BLANK flag is supported then MERGE/APPEND can't be compulsory */
			(*compulsory_flags) &= ~(PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND);

			/* need to add blanking flags */
			parrillada_burn_caps_get_blanking_flags_real (self,
			                                           parrillada_burn_session_get_strict_support (session) == FALSE,
								   media,
								   session_flags,
								   &blank_supported,
								   &blank_compulsory);
			(*supported_flags) |= blank_supported;
			(*compulsory_flags) |= blank_compulsory;
		}
		
	}
	else if (result != PARRILLADA_BURN_OK)
		return result;

	/* These are a special case for DVDRW sequential */
	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW)) {
		/* That's a way to give priority to MULTI over FAST
		 * and leave the possibility to always use MULTI. */
		if (session_flags & PARRILLADA_BURN_FLAG_MULTI)
			(*supported_flags) &= ~PARRILLADA_BURN_FLAG_FAST_BLANK;
		else if ((session_flags & PARRILLADA_BURN_FLAG_FAST_BLANK)
		         &&  (session_flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE)) {
			/* We should be able to handle this case differently but unfortunately
			 * there are buggy firmwares that won't report properly the supported
			 * mode writes */
			if (!((*supported_flags) & PARRILLADA_BURN_FLAG_DAO))
					 return PARRILLADA_BURN_NOT_SUPPORTED;

			(*compulsory_flags) |= PARRILLADA_BURN_FLAG_DAO;
		}
	}

	if (session_flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE) {
		/* make sure we remove MERGE/APPEND from supported and
		 * compulsory since that's not possible anymore */
		(*supported_flags) &= ~(PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND);
		(*compulsory_flags) &= ~(PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND);
	}

	/* FIXME! we should restart the whole process if
	 * ((session_flags & compulsory_flags) != compulsory_flags) since that
	 * means that some supported files could be excluded but were not */

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_caps_get_flags_same_src_dest_for_types (ParrilladaBurnCaps *self,
                                                     ParrilladaBurnSession *session,
                                                     ParrilladaTrackType *input,
                                                     ParrilladaTrackType *output,
                                                     ParrilladaBurnFlag *supported_ret,
                                                     ParrilladaBurnFlag *compulsory_ret)
{
	GSList *iter;
	gboolean type_supported;
	ParrilladaBurnResult result;
	ParrilladaBurnFlag session_flags;
	ParrilladaFindLinkCtx ctx = { 0, NULL, 0, };
	ParrilladaBurnFlag supported_final = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag compulsory_final = PARRILLADA_BURN_FLAG_ALL;

	/* NOTE: there is no need to get the flags here since there are
	 * no specific DISC => IMAGE flags. We just want to know if that
	 * is possible. */
	PARRILLADA_BURN_LOG_TYPE (output, "Testing temporary image format");

	parrillada_caps_find_link_set_ctx (session, &ctx, input);
	ctx.io_flags = PARRILLADA_PLUGIN_IO_ACCEPT_FILE;

	/* Here there is no need to try blanking as there
	 * is no disc (saves a few debug lines) */
	result = parrillada_caps_try_output (self, &ctx, output);
	if (result != PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG_TYPE (output, "Format not supported");
		return result;
	}

	session_flags = parrillada_burn_session_get_flags (session);

	/* This format can be used to create an image. Check if can be
	 * burnt now. Just find at least one medium. */
	type_supported = FALSE;
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		ParrilladaBurnFlag compulsory;
		ParrilladaBurnFlag supported;
		ParrilladaBurnResult result;
		ParrilladaMedia media;
		ParrilladaCaps *caps;

		caps = iter->data;
		if (!parrillada_track_type_get_has_medium (&caps->type))
			continue;

		media = parrillada_track_type_get_medium_type (&caps->type);

		/* This type of disc cannot be burnt; skip them */
		if (media & PARRILLADA_MEDIUM_ROM)
			continue;

		if ((media & PARRILLADA_MEDIUM_CD) == 0) {
			if (parrillada_track_type_get_has_image (output)) {
				ParrilladaImageFormat format;

				format = parrillada_track_type_get_image_format (output);
				/* These three types only work with CDs. */
				if (format == PARRILLADA_IMAGE_FORMAT_CDRDAO
				||   format == PARRILLADA_IMAGE_FORMAT_CLONE
				||   format == PARRILLADA_IMAGE_FORMAT_CUE)
					continue;
			}
			else if (parrillada_track_type_get_has_stream (output))
				continue;
		}

		/* Merge all available flags for each possible medium type */
		supported = PARRILLADA_BURN_FLAG_NONE;
		compulsory = PARRILLADA_BURN_FLAG_NONE;

		result = parrillada_caps_get_flags_for_disc (self,
		                                          parrillada_burn_session_get_strict_support (session) == FALSE,
		                                          session_flags,
		                                          media,
							  output,
							  &supported,
							  &compulsory);

		if (result != PARRILLADA_BURN_OK)
			continue;

		type_supported = TRUE;
		supported_final |= supported;
		compulsory_final &= compulsory;
	}

	PARRILLADA_BURN_LOG_TYPE (output, "Format supported %i", type_supported);
	if (!type_supported)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	*supported_ret = supported_final;
	*compulsory_ret = compulsory_final;
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_caps_get_flags_same_src_dest (ParrilladaBurnCaps *self,
					   ParrilladaBurnSession *session,
					   ParrilladaBurnFlag *supported_ret,
					   ParrilladaBurnFlag *compulsory_ret)
{
	ParrilladaTrackType input;
	ParrilladaBurnResult result;
	gboolean copy_supported;
	ParrilladaTrackType output;
	ParrilladaImageFormat format;
	ParrilladaBurnFlag session_flags;
	ParrilladaBurnFlag supported_final = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag compulsory_final = PARRILLADA_BURN_FLAG_ALL;

	PARRILLADA_BURN_LOG ("Retrieving disc copy flags with same source and destination");

	/* To determine if a CD/DVD can be copied using the same source/dest,
	 * we first determine if can be imaged and then what are the flags when
	 * we can burn it to a particular medium type. */
	memset (&input, 0, sizeof (ParrilladaTrackType));
	parrillada_burn_session_get_input_type (session, &input);
	PARRILLADA_BURN_LOG_TYPE (&input, "input");

	session_flags = parrillada_burn_session_get_flags (session);
	PARRILLADA_BURN_LOG_FLAGS (session_flags, "(FLAGS) Session flags");

	/* Check the current flags are possible */
	if (session_flags & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_NO_TMP_FILES))
		return PARRILLADA_BURN_NOT_SUPPORTED;

	/* Check for stream type */
	parrillada_track_type_set_has_stream (&output);
	/* FIXME! */
	parrillada_track_type_set_stream_format (&output,
	                                      PARRILLADA_AUDIO_FORMAT_RAW|
	                                      PARRILLADA_METADATA_INFO);

	result = parrillada_burn_caps_get_flags_same_src_dest_for_types (self,
	                                                              session,
	                                                              &input,
	                                                              &output,
	                                                              &supported_final,
	                                                              &compulsory_final);
	if (result == PARRILLADA_BURN_CANCEL)
		return result;

	copy_supported = (result == PARRILLADA_BURN_OK);

	/* Check flags for all available format */
	format = PARRILLADA_IMAGE_FORMAT_CDRDAO;
	parrillada_track_type_set_has_image (&output);
	for (; format > PARRILLADA_IMAGE_FORMAT_NONE; format >>= 1) {
		ParrilladaBurnFlag supported;
		ParrilladaBurnFlag compulsory;

		/* check if this image type is possible given the current flags */
		if (format != PARRILLADA_IMAGE_FORMAT_CLONE
		&& (session_flags & PARRILLADA_BURN_FLAG_RAW))
			continue;

		parrillada_track_type_set_image_format (&output, format);

		supported = PARRILLADA_BURN_FLAG_NONE;
		compulsory = PARRILLADA_BURN_FLAG_NONE;
		result = parrillada_burn_caps_get_flags_same_src_dest_for_types (self,
		                                                              session,
		                                                              &input,
		                                                              &output,
		                                                              &supported,
		                                                              &compulsory);
		if (result == PARRILLADA_BURN_CANCEL)
			return result;

		if (result != PARRILLADA_BURN_OK)
			continue;

		copy_supported = TRUE;
		supported_final |= supported;
		compulsory_final &= compulsory;
	}

	if (!copy_supported)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	*supported_ret |= supported_final;
	*compulsory_ret |= compulsory_final;

	/* we also add these two flags as being supported
	 * since they could be useful and can't be tested
	 * until the disc is inserted which it is not at the
	 * moment */
	(*supported_ret) |= (PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE|
			     PARRILLADA_BURN_FLAG_FAST_BLANK);

	if (parrillada_track_type_get_medium_type (&input) & PARRILLADA_MEDIUM_HAS_AUDIO) {
		/* This is a special case for audio discs.
		 * Since they may contain CD-TEXT and
		 * since CD-TEXT can only be written with
		 * DAO then we must make sure the user
		 * can't use MULTI since then DAO is
		 * impossible. */
		(*compulsory_ret) |= PARRILLADA_BURN_FLAG_DAO;

		/* This is just to make sure */
		(*supported_ret) &= (~PARRILLADA_BURN_FLAG_MULTI);
		(*compulsory_ret) &= (~PARRILLADA_BURN_FLAG_MULTI);
	}

	return PARRILLADA_BURN_OK;
}

/**
 * This is meant to use as internal API
 */
ParrilladaBurnResult
parrillada_caps_session_get_image_flags (ParrilladaTrackType *input,
                                     ParrilladaTrackType *output,
                                     ParrilladaBurnFlag *supported,
                                     ParrilladaBurnFlag *compulsory)
{
	ParrilladaBurnFlag compulsory_flags = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag supported_flags = PARRILLADA_BURN_FLAG_CHECK_SIZE|PARRILLADA_BURN_FLAG_NOGRACE;

	PARRILLADA_BURN_LOG ("FLAGS: image required");

	/* In this case no APPEND/MERGE is possible */
	if (parrillada_track_type_get_has_medium (input))
		supported_flags |= PARRILLADA_BURN_FLAG_EJECT;

	*supported = supported_flags;
	*compulsory = compulsory_flags;

	PARRILLADA_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	PARRILLADA_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_burn_flags:
 * @session: a #ParrilladaBurnSession
 * @supported: a #ParrilladaBurnFlag or NULL
 * @compulsory: a #ParrilladaBurnFlag or NULL
 *
 * Given the various parameters stored in @session, this function
 * stores:
 * - the flags that can be used (@supported)
 * - the flags that must be used (@compulsory)
 * when writing @session to a disc.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if the retrieval was successful.
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_get_burn_flags (ParrilladaBurnSession *session,
				     ParrilladaBurnFlag *supported,
				     ParrilladaBurnFlag *compulsory)
{
	gboolean res;
	ParrilladaMedia media;
	ParrilladaBurnCaps *self;
	ParrilladaTrackType *input;
	ParrilladaBurnResult result;

	ParrilladaBurnFlag session_flags;
	/* FIXME: what's the meaning of NOGRACE when outputting ? */
	ParrilladaBurnFlag compulsory_flags = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag supported_flags = PARRILLADA_BURN_FLAG_CHECK_SIZE|
					  PARRILLADA_BURN_FLAG_NOGRACE;

	self = parrillada_burn_caps_get_default ();

	input = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (session, input);
	PARRILLADA_BURN_LOG_WITH_TYPE (input,
				    PARRILLADA_PLUGIN_IO_NONE,
				    "FLAGS: searching available flags for input");

	if (parrillada_burn_session_is_dest_file (session)) {
		ParrilladaTrackType *output;

		PARRILLADA_BURN_LOG ("FLAGS: image required");

		output = parrillada_track_type_new ();
		parrillada_burn_session_get_output_type (session, output);
		parrillada_caps_session_get_image_flags (input, output, supported, compulsory);
		parrillada_track_type_free (output);

		parrillada_track_type_free (input);
		g_object_unref (self);
		return PARRILLADA_BURN_OK;
	}

	supported_flags |= PARRILLADA_BURN_FLAG_EJECT;

	/* special case */
	if (parrillada_burn_session_same_src_dest_drive (session)) {
		PARRILLADA_BURN_LOG ("Same source and destination");
		result = parrillada_burn_caps_get_flags_same_src_dest (self,
								    session,
								    &supported_flags,
								    &compulsory_flags);

		/* These flags are of course never possible */
		supported_flags &= ~(PARRILLADA_BURN_FLAG_NO_TMP_FILES|
				     PARRILLADA_BURN_FLAG_MERGE);
		compulsory_flags &= ~(PARRILLADA_BURN_FLAG_NO_TMP_FILES|
				      PARRILLADA_BURN_FLAG_MERGE);

		if (result == PARRILLADA_BURN_OK) {
			PARRILLADA_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
			PARRILLADA_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");

			*supported = supported_flags;
			*compulsory = compulsory_flags;
		}
		else
			PARRILLADA_BURN_LOG ("No available flags for copy");

		parrillada_track_type_free (input);
		g_object_unref (self);
		return result;
	}

	session_flags = parrillada_burn_session_get_flags (session);
	PARRILLADA_BURN_LOG_FLAGS (session_flags, "FLAGS (session):");

	/* sanity check:
	 * - drive must support flags
	 * - MERGE and BLANK are not possible together.
	 * - APPEND and MERGE are compatible. MERGE wins
	 * - APPEND and BLANK are incompatible */
	res = parrillada_check_flags_for_drive (parrillada_burn_session_get_burner (session), session_flags);
	if (!res) {
		PARRILLADA_BURN_LOG ("Session flags not supported by drive");
		parrillada_track_type_free (input);
		g_object_unref (self);
		return PARRILLADA_BURN_ERR;
	}

	if ((session_flags & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND))
	&&  (session_flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE)) {
		parrillada_track_type_free (input);
		g_object_unref (self);
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}
	
	/* Let's get flags for recording */
	media = parrillada_burn_session_get_dest_media (session);
	result = parrillada_burn_caps_get_flags_for_medium (self,
	                                                 session,
							 media,
							 session_flags,
							 input,
							 &supported_flags,
							 &compulsory_flags);

	parrillada_track_type_free (input);
	g_object_unref (self);

	if (result != PARRILLADA_BURN_OK)
		return result;

	parrillada_burn_caps_flags_update_for_drive (session,
	                                          &supported_flags,
	                                          &compulsory_flags);

	if (supported)
		*supported = supported_flags;

	if (compulsory)
		*compulsory = compulsory_flags;

	PARRILLADA_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	PARRILLADA_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");
	return PARRILLADA_BURN_OK;
}

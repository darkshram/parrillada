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

#ifndef _BURN_PLUGIN_H_REGISTRATION_
#define _BURN_PLUGIN_H_REGISTRATION_

#include <glib.h>

#include "parrillada-medium.h"

#include "parrillada-enums.h"
#include "parrillada-track.h"
#include "parrillada-track-stream.h"
#include "parrillada-track-data.h"
#include "parrillada-plugin.h"

G_BEGIN_DECLS

#define PARRILLADA_PLUGIN_BURN_FLAG_MASK	(PARRILLADA_BURN_FLAG_DUMMY|		\
					 PARRILLADA_BURN_FLAG_MULTI|		\
					 PARRILLADA_BURN_FLAG_DAO|			\
					 PARRILLADA_BURN_FLAG_RAW|			\
					 PARRILLADA_BURN_FLAG_BURNPROOF|		\
					 PARRILLADA_BURN_FLAG_OVERBURN|		\
					 PARRILLADA_BURN_FLAG_NOGRACE|		\
					 PARRILLADA_BURN_FLAG_APPEND|		\
					 PARRILLADA_BURN_FLAG_MERGE)

#define PARRILLADA_PLUGIN_IMAGE_FLAG_MASK	(PARRILLADA_BURN_FLAG_APPEND|		\
					 PARRILLADA_BURN_FLAG_MERGE)

#define PARRILLADA_PLUGIN_BLANK_FLAG_MASK	(PARRILLADA_BURN_FLAG_NOGRACE|		\
					 PARRILLADA_BURN_FLAG_FAST_BLANK)

/**
 * These are the functions a plugin must implement
 */

GType parrillada_plugin_register_caps (ParrilladaPlugin *plugin, gchar **error);

void
parrillada_plugin_define (ParrilladaPlugin *plugin,
		       const gchar *name,
                       const gchar *display_name,
		       const gchar *description,
		       const gchar *author,
		       guint priority);
void
parrillada_plugin_set_compulsory (ParrilladaPlugin *self,
			       gboolean compulsory);

void
parrillada_plugin_register_group (ParrilladaPlugin *plugin,
			       const gchar *name);

typedef enum {
	PARRILLADA_PLUGIN_IO_NONE			= 0,
	PARRILLADA_PLUGIN_IO_ACCEPT_PIPE		= 1,
	PARRILLADA_PLUGIN_IO_ACCEPT_FILE		= 1 << 1,
} ParrilladaPluginIOFlag;

GSList *
parrillada_caps_image_new (ParrilladaPluginIOFlag flags,
			ParrilladaImageFormat format);

GSList *
parrillada_caps_audio_new (ParrilladaPluginIOFlag flags,
			ParrilladaStreamFormat format);

GSList *
parrillada_caps_data_new (ParrilladaImageFS fs_type);

GSList *
parrillada_caps_disc_new (ParrilladaMedia media);

GSList *
parrillada_caps_checksum_new (ParrilladaChecksumType checksum);

void
parrillada_plugin_link_caps (ParrilladaPlugin *plugin,
			  GSList *outputs,
			  GSList *inputs);

void
parrillada_plugin_blank_caps (ParrilladaPlugin *plugin,
			   GSList *caps);

/**
 * This function is important since not only does it set the flags but it also 
 * tells parrillada which types of media are supported. So even if a plugin doesn't
 * support any flag, use it to tell parrillada which media are supported.
 * That's only needed if the plugin supports burn/blank operations.
 */
void
parrillada_plugin_set_flags (ParrilladaPlugin *plugin,
			  ParrilladaMedia media,
			  ParrilladaBurnFlag supported,
			  ParrilladaBurnFlag compulsory);
void
parrillada_plugin_set_blank_flags (ParrilladaPlugin *self,
				ParrilladaMedia media,
				ParrilladaBurnFlag supported,
				ParrilladaBurnFlag compulsory);

void
parrillada_plugin_process_caps (ParrilladaPlugin *plugin,
			     GSList *caps);

void
parrillada_plugin_set_process_flags (ParrilladaPlugin *plugin,
				  ParrilladaPluginProcessFlag flags);

void
parrillada_plugin_check_caps (ParrilladaPlugin *plugin,
			   ParrilladaChecksumType type,
			   GSList *caps);

/**
 * Plugin configure options
 */

ParrilladaPluginConfOption *
parrillada_plugin_conf_option_new (const gchar *key,
				const gchar *description,
				ParrilladaPluginConfOptionType type);

ParrilladaBurnResult
parrillada_plugin_add_conf_option (ParrilladaPlugin *plugin,
				ParrilladaPluginConfOption *option);

ParrilladaBurnResult
parrillada_plugin_conf_option_bool_add_suboption (ParrilladaPluginConfOption *option,
					       ParrilladaPluginConfOption *suboption);

ParrilladaBurnResult
parrillada_plugin_conf_option_int_set_range (ParrilladaPluginConfOption *option,
					  gint min,
					  gint max);

ParrilladaBurnResult
parrillada_plugin_conf_option_choice_add (ParrilladaPluginConfOption *option,
				       const gchar *string,
				       gint value);

void
parrillada_plugin_add_error (ParrilladaPlugin *plugin,
                          ParrilladaPluginErrorType type,
                          const gchar *detail);

void
parrillada_plugin_test_gstreamer_plugin (ParrilladaPlugin *plugin,
                                      const gchar *name);

void
parrillada_plugin_test_app (ParrilladaPlugin *plugin,
                         const gchar *name,
                         const gchar *version_arg,
                         const gchar *version_format,
                         gint version [3]);

/**
 * Boiler plate for plugin definition to save the hassle of definition.
 * To be put at the beginning of the .c file.
 */
typedef GType	(* ParrilladaPluginRegisterType)	(ParrilladaPlugin *plugin);

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin);

#define PARRILLADA_PLUGIN_BOILERPLATE(PluginName, plugin_name, PARENT_NAME, ParentName) \
typedef struct {								\
	ParentName parent;							\
} PluginName;									\
										\
typedef struct {								\
	ParentName##Class parent_class;						\
} PluginName##Class;								\
										\
static GType plugin_name##_type = 0;						\
										\
static GType									\
plugin_name##_get_type (void)							\
{										\
	return plugin_name##_type;						\
}										\
										\
static void plugin_name##_class_init (PluginName##Class *klass);		\
static void plugin_name##_init (PluginName *sp);				\
static void plugin_name##_finalize (GObject *object);			\
static void plugin_name##_export_caps (ParrilladaPlugin *plugin);	\
G_MODULE_EXPORT GType								\
parrillada_plugin_register (ParrilladaPlugin *plugin);				\
G_MODULE_EXPORT GType								\
parrillada_plugin_register (ParrilladaPlugin *plugin)				\
{														\
	if (parrillada_plugin_get_gtype (plugin) == G_TYPE_NONE)	\
		plugin_name##_export_caps (plugin);					\
	static const GTypeInfo our_info = {					\
		sizeof (PluginName##Class),					\
		NULL,										\
		NULL,										\
		(GClassInitFunc)plugin_name##_class_init,			\
		NULL,										\
		NULL,										\
		sizeof (PluginName),							\
		0,											\
		(GInstanceInitFunc)plugin_name##_init,			\
	};												\
	plugin_name##_type = g_type_module_register_type (G_TYPE_MODULE (plugin),		\
							  PARENT_NAME,			\
							  G_STRINGIFY (PluginName),		\
							  &our_info,				\
							  0);						\
	return plugin_name##_type;						\
}

#define PARRILLADA_PLUGIN_ADD_STANDARD_CDR_FLAGS(plugin_MACRO, unsupported_MACRO)	\
	/* Use DAO for first session since AUDIO need it to write CD-TEXT */	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_CD|				\
				  PARRILLADA_MEDIUM_WRITABLE|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_OVERBURN|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* This is a CDR with data data can be merged or at least appended */	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_CD|				\
				  PARRILLADA_MEDIUM_WRITABLE|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_AUDIO|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_OVERBURN|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_APPEND);

#define PARRILLADA_PLUGIN_ADD_STANDARD_CDRW_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* Use DAO for first session since AUDIO needs it to write CD-TEXT */	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_CD|				\
				  PARRILLADA_MEDIUM_REWRITABLE|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_OVERBURN|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* It is a CDRW we want the CD to be either blanked before or appended	\
	 * that's why we set MERGE as compulsory. That way if the CD is not 	\
	 * MERGED we force the blank before writing to avoid appending sessions	\
	 * endlessly until there is no free space. */				\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_CD|				\
				  PARRILLADA_MEDIUM_REWRITABLE|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_AUDIO|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_OVERBURN|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MERGE);

#define PARRILLADA_PLUGIN_ADD_STANDARD_DVDR_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* DAO and MULTI are exclusive */					\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR|				\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_JUMP|				\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR|				\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_JUMP|				\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* This is a DVDR with data; data can be merged or at least appended */	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR|				\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_JUMP|				\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_APPEND);

#define PARRILLADA_PLUGIN_ADD_STANDARD_DVDR_PLUS_FLAGS(plugin_MACRO, unsupported_MACRO)		\
	/* DVD+R don't have a DUMMY mode */					\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR_PLUS|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR_PLUS|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* DVD+R with data: data can be merged or at least appended */		\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDR_PLUS|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_APPEND);

#define PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDRW|				\
				  PARRILLADA_MEDIUM_UNFORMATTED|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDRW|				\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* This is a DVDRW we want the DVD to be either blanked before or	\
	 * appended that's why we set MERGE as compulsory. That way if the DVD	\
	 * is not MERGED we force the blank before writing to avoid appending	\
	 * sessions endlessly until there is no free space. */			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDRW|				\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_BURNPROOF|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MERGE);

/**
 * These kind of media don't support:
 * - BURNPROOF
 * - DAO
 * - APPEND
 * since they don't behave and are not written in the same way.
 * They also can't be closed so MULTI is compulsory.
 */
#define PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_PLUS_FLAGS(plugin_MACRO, unsupported_MACRO)		\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDRW_PLUS|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_UNFORMATTED|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVDRW_PLUS|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_CLOSED|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE|			\
				  PARRILLADA_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);

/**
 * The above statement apply to these as well. There is no longer dummy mode
 * NOTE: there is no such thing as a DVD-RW DL.
 */
#define PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_RESTRICTED_FLAGS(plugin_MACRO, unsupported_MACRO)	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVD|				\
				  PARRILLADA_MEDIUM_RESTRICTED|			\
				  PARRILLADA_MEDIUM_REWRITABLE|			\
				  PARRILLADA_MEDIUM_UNFORMATTED|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_DVD|				\
				  PARRILLADA_MEDIUM_RESTRICTED|			\
				  PARRILLADA_MEDIUM_REWRITABLE|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_CLOSED|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE|			\
				  PARRILLADA_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);

#define PARRILLADA_PLUGIN_ADD_STANDARD_BD_R_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* DAO and MULTI are exclusive */					\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_BDR_RANDOM|			\
				  PARRILLADA_MEDIUM_BDR_SRM|			\
				  PARRILLADA_MEDIUM_BDR_SRM_POW|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_DAO|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_BDR_RANDOM|			\
				  PARRILLADA_MEDIUM_BDR_SRM|			\
				  PARRILLADA_MEDIUM_BDR_SRM_POW|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_NONE);			\
	/* This is a DVDR with data data can be merged or at least appended */	\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_BDR_RANDOM|			\
				  PARRILLADA_MEDIUM_BDR_SRM|			\
				  PARRILLADA_MEDIUM_BDR_SRM_POW|			\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_APPEND|			\
				  PARRILLADA_BURN_FLAG_MERGE|			\
				  PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_DUMMY|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_APPEND);

/**
 * These kind of media don't support:
 * - BURNPROOF
 * - DAO
 * - APPEND
 * since they don't behave and are not written in the same way.
 * They also can't be closed so MULTI is compulsory.
 */
#define PARRILLADA_PLUGIN_ADD_STANDARD_BD_RE_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_BDRE|				\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_UNFORMATTED|			\
				  PARRILLADA_MEDIUM_BLANK,				\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);			\
	parrillada_plugin_set_flags (plugin_MACRO,					\
				  PARRILLADA_MEDIUM_BDRE|				\
				  PARRILLADA_MEDIUM_DUAL_L|			\
				  PARRILLADA_MEDIUM_APPENDABLE|			\
				  PARRILLADA_MEDIUM_CLOSED|			\
				  PARRILLADA_MEDIUM_HAS_DATA,			\
				  (PARRILLADA_BURN_FLAG_MULTI|			\
				  PARRILLADA_BURN_FLAG_NOGRACE|			\
				  PARRILLADA_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  PARRILLADA_BURN_FLAG_MULTI);

G_END_DECLS

#endif

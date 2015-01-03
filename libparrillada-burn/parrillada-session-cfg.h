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

#ifndef _PARRILLADA_SESSION_CFG_H_
#define _PARRILLADA_SESSION_CFG_H_

#include <glib-object.h>

#include <parrillada-session.h>
#include <parrillada-session-span.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SESSION_CFG             (parrillada_session_cfg_get_type ())
#define PARRILLADA_SESSION_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SESSION_CFG, ParrilladaSessionCfg))
#define PARRILLADA_SESSION_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SESSION_CFG, ParrilladaSessionCfgClass))
#define PARRILLADA_IS_SESSION_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SESSION_CFG))
#define PARRILLADA_IS_SESSION_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SESSION_CFG))
#define PARRILLADA_SESSION_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SESSION_CFG, ParrilladaSessionCfgClass))

typedef struct _ParrilladaSessionCfgClass ParrilladaSessionCfgClass;
typedef struct _ParrilladaSessionCfg ParrilladaSessionCfg;

struct _ParrilladaSessionCfgClass
{
	ParrilladaSessionSpanClass parent_class;
};

struct _ParrilladaSessionCfg
{
	ParrilladaSessionSpan parent_instance;
};

GType parrillada_session_cfg_get_type (void) G_GNUC_CONST;

/**
 * This is for the signal sent to tell whether or not session is valid
 */

typedef enum {
	PARRILLADA_SESSION_VALID				= 0,
	PARRILLADA_SESSION_NO_CD_TEXT			= 1,
	PARRILLADA_SESSION_NOT_READY,
	PARRILLADA_SESSION_EMPTY,
	PARRILLADA_SESSION_NO_INPUT_IMAGE,
	PARRILLADA_SESSION_UNKNOWN_IMAGE,
	PARRILLADA_SESSION_NO_INPUT_MEDIUM,
	PARRILLADA_SESSION_NO_OUTPUT,
	PARRILLADA_SESSION_INSUFFICIENT_SPACE,
	PARRILLADA_SESSION_OVERBURN_NECESSARY,
	PARRILLADA_SESSION_NOT_SUPPORTED,
	PARRILLADA_SESSION_DISC_PROTECTED
} ParrilladaSessionError;

#define PARRILLADA_SESSION_IS_VALID(result_MACRO)					\
	((result_MACRO) == PARRILLADA_SESSION_VALID ||				\
	 (result_MACRO) == PARRILLADA_SESSION_NO_CD_TEXT)

ParrilladaSessionCfg *
parrillada_session_cfg_new (void);

ParrilladaSessionError
parrillada_session_cfg_get_error (ParrilladaSessionCfg *cfg);

void
parrillada_session_cfg_add_flags (ParrilladaSessionCfg *cfg,
			       ParrilladaBurnFlag flags);
void
parrillada_session_cfg_remove_flags (ParrilladaSessionCfg *cfg,
				  ParrilladaBurnFlag flags);
gboolean
parrillada_session_cfg_is_supported (ParrilladaSessionCfg *cfg,
				  ParrilladaBurnFlag flag);
gboolean
parrillada_session_cfg_is_compulsory (ParrilladaSessionCfg *cfg,
				   ParrilladaBurnFlag flag);

gboolean
parrillada_session_cfg_has_default_output_path (ParrilladaSessionCfg *cfg);

void
parrillada_session_cfg_enable (ParrilladaSessionCfg *cfg);

void
parrillada_session_cfg_disable (ParrilladaSessionCfg *cfg);

G_END_DECLS

#endif /* _PARRILLADA_SESSION_CFG_H_ */

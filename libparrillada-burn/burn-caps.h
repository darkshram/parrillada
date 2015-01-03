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

#ifndef BURN_CAPS_H
#define BURN_CAPS_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "parrillada-track-type.h"
#include "parrillada-track-type-private.h"
#include "parrillada-plugin.h"
#include "parrillada-plugin-information.h"
#include "parrillada-plugin-registration.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_BURNCAPS         (parrillada_burn_caps_get_type ())
#define PARRILLADA_BURNCAPS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_BURNCAPS, ParrilladaBurnCaps))
#define PARRILLADA_BURNCAPS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_BURNCAPS, ParrilladaBurnCapsClass))
#define PARRILLADA_IS_BURNCAPS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_BURNCAPS))
#define PARRILLADA_IS_BURNCAPS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_BURNCAPS))
#define PARRILLADA_BURNCAPS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_BURNCAPS, ParrilladaBurnCapsClass))

struct _ParrilladaCaps {
	GSList *links;			/* ParrilladaCapsLink */
	GSList *modifiers;		/* ParrilladaPlugin */
	ParrilladaTrackType type;
	ParrilladaPluginIOFlag flags;
};
typedef struct _ParrilladaCaps ParrilladaCaps;

struct _ParrilladaCapsLink {
	GSList *plugins;
	ParrilladaCaps *caps;
};
typedef struct _ParrilladaCapsLink ParrilladaCapsLink;

struct _ParrilladaCapsTest {
	GSList *links;
	ParrilladaChecksumType type;
};
typedef struct _ParrilladaCapsTest ParrilladaCapsTest;

typedef struct ParrilladaBurnCapsPrivate ParrilladaBurnCapsPrivate;
struct ParrilladaBurnCapsPrivate {
	GSList *caps_list;		/* ParrilladaCaps */
	GSList *tests;			/* ParrilladaCapsTest */

	GHashTable *groups;

	gchar *group_str;
	guint group_id;
};

typedef struct {
	GObject parent;
	ParrilladaBurnCapsPrivate *priv;
} ParrilladaBurnCaps;

typedef struct {
	GObjectClass parent_class;
} ParrilladaBurnCapsClass;

GType parrillada_burn_caps_get_type (void);

ParrilladaBurnCaps *parrillada_burn_caps_get_default (void);

ParrilladaPlugin *
parrillada_caps_link_need_download (ParrilladaCapsLink *link);

gboolean
parrillada_caps_link_active (ParrilladaCapsLink *link,
                          gboolean ignore_plugin_errors);

gboolean
parrillada_burn_caps_is_input (ParrilladaBurnCaps *self,
			    ParrilladaCaps *input);

ParrilladaCaps *
parrillada_burn_caps_find_start_caps (ParrilladaBurnCaps *self,
				   ParrilladaTrackType *output);

gboolean
parrillada_caps_is_compatible_type (const ParrilladaCaps *caps,
				 const ParrilladaTrackType *type);

ParrilladaBurnResult
parrillada_caps_link_check_recorder_flags_for_input (ParrilladaCapsLink *link,
                                                  ParrilladaBurnFlag session_flags);

G_END_DECLS

#endif /* BURN_CAPS_H */

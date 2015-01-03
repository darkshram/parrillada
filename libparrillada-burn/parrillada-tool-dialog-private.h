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

#ifndef PARRILLADA_TOOL_DIALOG_PRIVATE_H
#define PARRILLADA_TOOL_DIALOG_PRIVATE_H

#include "parrillada-tool-dialog.h"


G_BEGIN_DECLS

void
parrillada_tool_dialog_pack_options (ParrilladaToolDialog *dialog, ...);

void
parrillada_tool_dialog_set_button (ParrilladaToolDialog *dialog,
				const gchar *text,
				const gchar *image,
				const gchar *theme);
void
parrillada_tool_dialog_set_valid (ParrilladaToolDialog *dialog,
			       gboolean valid);
void
parrillada_tool_dialog_set_medium_type_shown (ParrilladaToolDialog *dialog,
					   ParrilladaMediaType media_type);
void
parrillada_tool_dialog_set_progress (ParrilladaToolDialog *dialog,
				  gdouble overall_progress,
				  gdouble task_progress,
				  glong remaining,
				  gint size_mb,
				  gint written_mb);
void
parrillada_tool_dialog_set_action (ParrilladaToolDialog *dialog,
				ParrilladaBurnAction action,
				const gchar *string);

ParrilladaBurn *
parrillada_tool_dialog_get_burn (ParrilladaToolDialog *dialog);

ParrilladaMedium *
parrillada_tool_dialog_get_medium (ParrilladaToolDialog *dialog);

G_END_DECLS

#endif

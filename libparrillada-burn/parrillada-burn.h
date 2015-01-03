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

#ifndef BURN_H
#define BURN_H

#include <glib.h>
#include <glib-object.h>

#include <parrillada-error.h>
#include <parrillada-track.h>
#include <parrillada-session.h>

#include <parrillada-medium.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_BURN         (parrillada_burn_get_type ())
#define PARRILLADA_BURN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_BURN, ParrilladaBurn))
#define PARRILLADA_BURN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_BURN, ParrilladaBurnClass))
#define PARRILLADA_IS_BURN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_BURN))
#define PARRILLADA_IS_BURN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_BURN))
#define PARRILLADA_BURN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_BURN, ParrilladaBurnClass))

typedef struct {
	GObject parent;
} ParrilladaBurn;

typedef struct {
	GObjectClass parent_class;

	/* signals */
	ParrilladaBurnResult		(*insert_media_request)		(ParrilladaBurn *obj,
									 ParrilladaDrive *drive,
									 ParrilladaBurnError error,
									 ParrilladaMedia required_media);

	ParrilladaBurnResult		(*eject_failure)		(ParrilladaBurn *obj,
							                 ParrilladaDrive *drive);

	ParrilladaBurnResult		(*blank_failure)		(ParrilladaBurn *obj);

	ParrilladaBurnResult		(*location_request)		(ParrilladaBurn *obj,
									 GError *error,
									 gboolean is_temporary);

	ParrilladaBurnResult		(*ask_disable_joliet)		(ParrilladaBurn *obj);

	ParrilladaBurnResult		(*warn_data_loss)		(ParrilladaBurn *obj);
	ParrilladaBurnResult		(*warn_previous_session_loss)	(ParrilladaBurn *obj);
	ParrilladaBurnResult		(*warn_audio_to_appendable)	(ParrilladaBurn *obj);
	ParrilladaBurnResult		(*warn_rewritable)		(ParrilladaBurn *obj);

	ParrilladaBurnResult		(*dummy_success)		(ParrilladaBurn *obj);

	void				(*progress_changed)		(ParrilladaBurn *obj,
									 gdouble overall_progress,
									 gdouble action_progress,
									 glong time_remaining);
	void				(*action_changed)		(ParrilladaBurn *obj,
									 ParrilladaBurnAction action);

	ParrilladaBurnResult		(*install_missing)		(ParrilladaBurn *obj,
									 ParrilladaPluginErrorType error,
									 const gchar *detail);
} ParrilladaBurnClass;

GType parrillada_burn_get_type (void);
ParrilladaBurn *parrillada_burn_new (void);

ParrilladaBurnResult 
parrillada_burn_record (ParrilladaBurn *burn,
		     ParrilladaBurnSession *session,
		     GError **error);

ParrilladaBurnResult
parrillada_burn_check (ParrilladaBurn *burn,
		    ParrilladaBurnSession *session,
		    GError **error);

ParrilladaBurnResult
parrillada_burn_blank (ParrilladaBurn *burn,
		    ParrilladaBurnSession *session,
		    GError **error);

ParrilladaBurnResult
parrillada_burn_cancel (ParrilladaBurn *burn,
		     gboolean protect);

ParrilladaBurnResult
parrillada_burn_status (ParrilladaBurn *burn,
		     ParrilladaMedia *media,
		     goffset *isosize,
		     goffset *written,
		     guint64 *rate);

void
parrillada_burn_get_action_string (ParrilladaBurn *burn,
				ParrilladaBurnAction action,
				gchar **string);

G_END_DECLS

#endif /* BURN_H */

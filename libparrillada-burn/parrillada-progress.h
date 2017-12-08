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

#ifndef PROGRESS_H
#define PROGRESS_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "parrillada-media.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_BURN_PROGRESS         (parrillada_burn_progress_get_type ())
#define PARRILLADA_BURN_PROGRESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_BURN_PROGRESS, ParrilladaBurnProgress))
#define PARRILLADA_BURN_PROGRESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_BURN_PROGRESS, ParrilladaBurnProgressClass))
#define PARRILLADA_IS_BURN_PROGRESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_BURN_PROGRESS))
#define PARRILLADA_IS_BURN_PROGRESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_BURN_PROGRESS))
#define PARRILLADA_BURN_PROGRESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_BURN_PROGRESS, ParrilladaBurnProgressClass))

typedef struct ParrilladaBurnProgressPrivate ParrilladaBurnProgressPrivate;

typedef struct {
	GtkBox parent;
	ParrilladaBurnProgressPrivate *priv;
} ParrilladaBurnProgress;

typedef struct {
	GtkBoxClass parent_class;
} ParrilladaBurnProgressClass;

GType parrillada_burn_progress_get_type (void);

GtkWidget *parrillada_burn_progress_new (void);

void
parrillada_burn_progress_reset (ParrilladaBurnProgress *progress);

void
parrillada_burn_progress_set_status (ParrilladaBurnProgress *progress,
				  ParrilladaMedia media,
				  gdouble overall_progress,
				  gdouble action_progress,
				  glong remaining,
				  gint mb_isosize,
				  gint mb_written,
				  gint64 rate);
void
parrillada_burn_progress_display_session_info (ParrilladaBurnProgress *progress,
					    glong time,
					    gint64 rate,
					    ParrilladaMedia media,
					    gint mb_written);

void
parrillada_burn_progress_set_action (ParrilladaBurnProgress *progress,
				  ParrilladaBurnAction action,
				  const gchar *string);

G_END_DECLS

#endif /* PROGRESS_H */

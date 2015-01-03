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

#ifndef _PARRILLADA_STATUS_H_
#define _PARRILLADA_STATUS_H_

#include <glib.h>
#include <glib-object.h>

#include <parrillada-enums.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_STATUS             (parrillada_status_get_type ())
#define PARRILLADA_STATUS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_STATUS, ParrilladaStatus))
#define PARRILLADA_STATUS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_STATUS, ParrilladaStatusClass))
#define PARRILLADA_IS_STATUS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_STATUS))
#define PARRILLADA_IS_STATUS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_STATUS))
#define PARRILLADA_STATUS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_STATUS, ParrilladaStatusClass))

typedef struct _ParrilladaStatusClass ParrilladaStatusClass;
typedef struct _ParrilladaStatus ParrilladaStatus;

struct _ParrilladaStatusClass
{
	GObjectClass parent_class;
};

struct _ParrilladaStatus
{
	GObject parent_instance;
};

GType parrillada_status_get_type (void) G_GNUC_CONST;

ParrilladaStatus *
parrillada_status_new (void);

typedef enum {
	PARRILLADA_STATUS_OK			= 0,
	PARRILLADA_STATUS_ERROR,
	PARRILLADA_STATUS_QUESTION,
	PARRILLADA_STATUS_INFORMATION
} ParrilladaStatusType;

G_GNUC_DEPRECATED void
parrillada_status_free (ParrilladaStatus *status);

ParrilladaBurnResult
parrillada_status_get_result (ParrilladaStatus *status);

gdouble
parrillada_status_get_progress (ParrilladaStatus *status);

GError *
parrillada_status_get_error (ParrilladaStatus *status);

gchar *
parrillada_status_get_current_action (ParrilladaStatus *status);

void
parrillada_status_set_completed (ParrilladaStatus *status);

void
parrillada_status_set_not_ready (ParrilladaStatus *status,
			      gdouble progress,
			      const gchar *current_action);

void
parrillada_status_set_running (ParrilladaStatus *status,
			    gdouble progress,
			    const gchar *current_action);

void
parrillada_status_set_error (ParrilladaStatus *status,
			  GError *error);

G_END_DECLS

#endif

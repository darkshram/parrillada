/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-misc is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_NOTIFY_H_
#define _PARRILLADA_NOTIFY_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-disc-message.h"

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_NOTIFY_CONTEXT_NONE		= 0,
	PARRILLADA_NOTIFY_CONTEXT_SIZE		= 1,
	PARRILLADA_NOTIFY_CONTEXT_LOADING		= 2,
	PARRILLADA_NOTIFY_CONTEXT_MULTISESSION	= 3,
} ParrilladaNotifyContextId;

GType parrillada_notify_get_type (void) G_GNUC_CONST;

GtkWidget *parrillada_notify_new (void);

GtkWidget *
parrillada_notify_message_add (GtkWidget *notify,
			    const gchar *primary,
			    const gchar *secondary,
			    gint timeout,
			    guint context_id);

void
parrillada_notify_message_remove (GtkWidget *notify,
			       guint context_id);

GtkWidget *
parrillada_notify_get_message_by_context_id (GtkWidget *notify,
					  guint context_id);

G_END_DECLS

#endif /* _PARRILLADA_NOTIFY_H_ */

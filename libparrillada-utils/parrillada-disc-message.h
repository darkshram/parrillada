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

#ifndef _PARRILLADA_DISC_MESSAGE_H_
#define _PARRILLADA_DISC_MESSAGE_H_

#include <glib-object.h>

#include <gtk/gtk.h>
G_BEGIN_DECLS

#define PARRILLADA_TYPE_DISC_MESSAGE             (parrillada_disc_message_get_type ())
#define PARRILLADA_DISC_MESSAGE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DISC_MESSAGE, ParrilladaDiscMessage))
#define PARRILLADA_DISC_MESSAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DISC_MESSAGE, ParrilladaDiscMessageClass))
#define PARRILLADA_IS_DISC_MESSAGE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DISC_MESSAGE))
#define PARRILLADA_IS_DISC_MESSAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DISC_MESSAGE))
#define PARRILLADA_DISC_MESSAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DISC_MESSAGE, ParrilladaDiscMessageClass))

typedef struct _ParrilladaDiscMessageClass ParrilladaDiscMessageClass;
typedef struct _ParrilladaDiscMessage ParrilladaDiscMessage;

struct _ParrilladaDiscMessageClass
{
	GtkInfoBarClass parent_class;
};

struct _ParrilladaDiscMessage
{
	GtkInfoBar parent_instance;
};

GType parrillada_disc_message_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_disc_message_new (void);

void
parrillada_disc_message_destroy (ParrilladaDiscMessage *message);

void
parrillada_disc_message_set_primary (ParrilladaDiscMessage *message,
				  const gchar *text);
void
parrillada_disc_message_set_secondary (ParrilladaDiscMessage *message,
				    const gchar *text);

void
parrillada_disc_message_set_progress_active (ParrilladaDiscMessage *message,
					  gboolean active);
void
parrillada_disc_message_set_progress (ParrilladaDiscMessage *self,
				   gdouble progress);

void
parrillada_disc_message_remove_buttons (ParrilladaDiscMessage *message);

void
parrillada_disc_message_add_errors (ParrilladaDiscMessage *message,
				 GSList *errors);
void
parrillada_disc_message_remove_errors (ParrilladaDiscMessage *message);

void
parrillada_disc_message_set_timeout (ParrilladaDiscMessage *message,
				  guint mseconds);

void
parrillada_disc_message_set_context (ParrilladaDiscMessage *message,
				  guint context_id);

guint
parrillada_disc_message_get_context (ParrilladaDiscMessage *message);

G_END_DECLS

#endif /* _PARRILLADA_DISC_MESSAGE_H_ */

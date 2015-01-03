/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with parrillada.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARRILLADA_TIME_BUTTON_H_
#define _PARRILLADA_TIME_BUTTON_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TIME_BUTTON             (parrillada_time_button_get_type ())
#define PARRILLADA_TIME_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_TIME_BUTTON, ParrilladaTimeButton))
#define PARRILLADA_TIME_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_TIME_BUTTON, ParrilladaTimeButtonClass))
#define PARRILLADA_IS_TIME_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_TIME_BUTTON))
#define PARRILLADA_IS_TIME_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_TIME_BUTTON))
#define PARRILLADA_TIME_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_TIME_BUTTON, ParrilladaTimeButtonClass))

typedef struct _ParrilladaTimeButtonClass ParrilladaTimeButtonClass;
typedef struct _ParrilladaTimeButton ParrilladaTimeButton;

struct _ParrilladaTimeButtonClass
{
	GtkHBoxClass parent_class;

	void		(*value_changed)	(ParrilladaTimeButton *self);
};

struct _ParrilladaTimeButton
{
	GtkHBox parent_instance;
};

GType parrillada_time_button_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_time_button_new (void);

gint64
parrillada_time_button_get_value (ParrilladaTimeButton *time);

void
parrillada_time_button_set_value (ParrilladaTimeButton *time,
			       gint64 value);
void
parrillada_time_button_set_max (ParrilladaTimeButton *time,
			     gint64 max);

void
parrillada_time_button_set_show_frames (ParrilladaTimeButton *time,
				     gboolean show);

G_END_DECLS

#endif /* _PARRILLADA_TIME_BUTTON_H_ */

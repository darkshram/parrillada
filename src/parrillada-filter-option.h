/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_FILTER_OPTION_H_
#define _PARRILLADA_FILTER_OPTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_FILTER_OPTION             (parrillada_filter_option_get_type ())
#define PARRILLADA_FILTER_OPTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_FILTER_OPTION, ParrilladaFilterOption))
#define PARRILLADA_FILTER_OPTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_FILTER_OPTION, ParrilladaFilterOptionClass))
#define PARRILLADA_IS_FILTER_OPTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_FILTER_OPTION))
#define PARRILLADA_IS_FILTER_OPTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_FILTER_OPTION))
#define PARRILLADA_FILTER_OPTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_FILTER_OPTION, ParrilladaFilterOptionClass))

typedef struct _ParrilladaFilterOptionClass ParrilladaFilterOptionClass;
typedef struct _ParrilladaFilterOption ParrilladaFilterOption;

struct _ParrilladaFilterOptionClass
{
	GtkVBoxClass parent_class;
};

struct _ParrilladaFilterOption
{
	GtkVBox parent_instance;
};

GType parrillada_filter_option_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_filter_option_new (void);

G_END_DECLS

#endif /* _PARRILLADA_FILTER_OPTION_H_ */

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

#ifndef _PARRILLADA_FILE_FILTERED_H_
#define _PARRILLADA_FILE_FILTERED_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "parrillada-track-data-cfg.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_FILE_FILTERED             (parrillada_file_filtered_get_type ())
#define PARRILLADA_FILE_FILTERED(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_FILE_FILTERED, ParrilladaFileFiltered))
#define PARRILLADA_FILE_FILTERED_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_FILE_FILTERED, ParrilladaFileFilteredClass))
#define PARRILLADA_IS_FILE_FILTERED(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_FILE_FILTERED))
#define PARRILLADA_IS_FILE_FILTERED_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_FILE_FILTERED))
#define PARRILLADA_FILE_FILTERED_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_FILE_FILTERED, ParrilladaFileFilteredClass))

typedef struct _ParrilladaFileFilteredClass ParrilladaFileFilteredClass;
typedef struct _ParrilladaFileFiltered ParrilladaFileFiltered;

struct _ParrilladaFileFilteredClass
{
	GtkExpanderClass parent_class;
};

struct _ParrilladaFileFiltered
{
	GtkExpander parent_instance;
};

GType parrillada_file_filtered_get_type (void) G_GNUC_CONST;

GtkWidget*
parrillada_file_filtered_new (ParrilladaTrackDataCfg *track);

void
parrillada_file_filtered_set_right_button_group (ParrilladaFileFiltered *self,
					      GtkSizeGroup *group);

G_END_DECLS

#endif /* _PARRILLADA_FILE_FILTERED_H_ */

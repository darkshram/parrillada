/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_PROJECT_NAME_H_
#define _PARRILLADA_PROJECT_NAME_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-session.h"

#include "parrillada-project-type-chooser.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PROJECT_NAME             (parrillada_project_name_get_type ())
#define PARRILLADA_PROJECT_NAME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_PROJECT_NAME, ParrilladaProjectName))
#define PARRILLADA_PROJECT_NAME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_PROJECT_NAME, ParrilladaProjectNameClass))
#define PARRILLADA_IS_PROJECT_NAME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_PROJECT_NAME))
#define PARRILLADA_IS_PROJECT_NAME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_PROJECT_NAME))
#define PARRILLADA_PROJECT_NAME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_PROJECT_NAME, ParrilladaProjectNameClass))

typedef struct _ParrilladaProjectNameClass ParrilladaProjectNameClass;
typedef struct _ParrilladaProjectName ParrilladaProjectName;

struct _ParrilladaProjectNameClass
{
	GtkEntryClass parent_class;
};

struct _ParrilladaProjectName
{
	GtkEntry parent_instance;
};

GType parrillada_project_name_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_project_name_new (ParrilladaBurnSession *session);

void
parrillada_project_name_set_session (ParrilladaProjectName *project,
				  ParrilladaBurnSession *session);

G_END_DECLS

#endif /* _PARRILLADA_PROJECT_NAME_H_ */

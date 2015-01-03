/***************************************************************************
*            cd-type-chooser.h
*
*  ven mai 27 17:33:12 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

/*
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Parrillada is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef CD_TYPE_CHOOSER_H
#define CD_TYPE_CHOOSER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-project-parse.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER         (parrillada_project_type_chooser_get_type ())
#define PARRILLADA_PROJECT_TYPE_CHOOSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER, ParrilladaProjectTypeChooser))
#define PARRILLADA_PROJECT_TYPE_CHOOSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER, ParrilladaProjectTypeChooserClass))
#define PARRILLADA_IS_PROJECT_TYPE_CHOOSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER))
#define PARRILLADA_IS_PROJECT_TYPE_CHOOSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER))
#define PARRILLADA_PROJECT_TYPE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_PROJECT_TYPE_CHOOSER, ParrilladaProjectTypeChooserClass))

typedef struct ParrilladaProjectTypeChooserPrivate ParrilladaProjectTypeChooserPrivate;

typedef struct {
	GtkEventBox parent;
	ParrilladaProjectTypeChooserPrivate *priv;
} ParrilladaProjectTypeChooser;

typedef struct {
	GtkEventBoxClass parent_class;

	void	(*last_saved_clicked)	(ParrilladaProjectTypeChooser *chooser,
					 const gchar *path);
	void	(*recent_clicked)	(ParrilladaProjectTypeChooser *chooser,
					 const gchar *uri);
	void	(*chosen)		(ParrilladaProjectTypeChooser *chooser,
					 ParrilladaProjectType type);
} ParrilladaProjectTypeChooserClass;

GType parrillada_project_type_chooser_get_type (void);
GtkWidget *parrillada_project_type_chooser_new (void);

G_END_DECLS

G_END_DECLS

#endif				/* CD_TYPE_CHOOSER_H */

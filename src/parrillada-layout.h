/***************************************************************************
 *            parrillada-layout.h
 *
 *  mer mai 24 15:14:42 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

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

#ifndef PARRILLADA_LAYOUT_H
#define PARRILLADA_LAYOUT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_LAYOUT         (parrillada_layout_get_type ())
#define PARRILLADA_LAYOUT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_LAYOUT, ParrilladaLayout))
#define PARRILLADA_LAYOUT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_LAYOUT, ParrilladaLayoutClass))
#define PARRILLADA_IS_LAYOUT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_LAYOUT))
#define PARRILLADA_IS_LAYOUT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_LAYOUT))
#define PARRILLADA_LAYOUT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_LAYOUT, ParrilladaLayoutClass))

typedef struct ParrilladaLayoutPrivate ParrilladaLayoutPrivate;

typedef enum {
	PARRILLADA_LAYOUT_NONE		= 0,
	PARRILLADA_LAYOUT_AUDIO		= 1,
	PARRILLADA_LAYOUT_DATA		= 1 << 1,
	PARRILLADA_LAYOUT_VIDEO		= 1 << 2
} ParrilladaLayoutType;

typedef struct {
	GtkPaned parent;
	ParrilladaLayoutPrivate *priv;
} ParrilladaLayout;

typedef struct {
	GtkPanedClass parent_class;
} ParrilladaLayoutClass;

GType parrillada_layout_get_type (void);
GtkWidget *parrillada_layout_new (void);

void
parrillada_layout_add_project (ParrilladaLayout *layout,
			    GtkWidget *project);
void
parrillada_layout_add_preview (ParrilladaLayout*layout,
			    GtkWidget *preview);

void
parrillada_layout_add_source (ParrilladaLayout *layout,
			   GtkWidget *child,
			   const gchar *id,
			   const gchar *subtitle,
			   const gchar *icon_name,
			   ParrilladaLayoutType types);
void
parrillada_layout_load (ParrilladaLayout *layout,
		     ParrilladaLayoutType type);

void
parrillada_layout_register_ui (ParrilladaLayout *layout,
			    GtkUIManager *manager);

G_END_DECLS

#endif /* PARRILLADA_LAYOUT_H */

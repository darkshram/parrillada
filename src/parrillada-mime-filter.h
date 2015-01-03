/***************************************************************************
*            mime_filter.h
*
*  dim mai 22 18:39:03 2005
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

#ifndef MIME_FILTER_H
#define MIME_FILTER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_MIME_FILTER         (parrillada_mime_filter_get_type ())
#define PARRILLADA_MIME_FILTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_MIME_FILTER, ParrilladaMimeFilter))
#define PARRILLADA_MIME_FILTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_MIME_FILTER, ParrilladaMimeFilterClass))
#define PARRILLADA_IS_MIME_FILTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_MIME_FILTER))
#define PARRILLADA_IS_MIME_FILTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_MIME_FILTER))
#define PARRILLADA_MIME_FILTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_MIME_FILTER, ParrilladaMimeFilterClass))
typedef struct ParrilladaMimeFilterPrivate ParrilladaMimeFilterPrivate;

typedef struct {
	GtkHBox parent;

	/* Public */
	GtkWidget *combo;

	/* Private */
	ParrilladaMimeFilterPrivate *priv;
} ParrilladaMimeFilter;


typedef struct {
	GtkHBoxClass parent_class;
	/* Signal Functions */
	void (*changed) (ParrilladaMimeFilter * filter);
} ParrilladaMimeFilterClass;

GType parrillada_mime_filter_get_type (void);
GtkWidget *parrillada_mime_filter_new (void);

void parrillada_mime_filter_add_filter (ParrilladaMimeFilter * filter,
				     GtkFileFilter * item);
void parrillada_mime_filter_add_mime (ParrilladaMimeFilter * filter,
				   const char *mime);
void parrillada_mime_filter_unref_mime (ParrilladaMimeFilter * filter,
				     const char *mime);
gboolean parrillada_mime_filter_filter (ParrilladaMimeFilter * filter,
				     const char *filename, const char *uri,
				     const char *display_name, const char *mime_type);

G_END_DECLS

G_END_DECLS

#endif				/* MIME_FILTER_H */

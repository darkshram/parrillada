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

#ifndef _PARRILLADA_FILTERED_URI_H_
#define _PARRILLADA_FILTERED_URI_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_FILTER_NONE			= 0,
	PARRILLADA_FILTER_HIDDEN			= 1,
	PARRILLADA_FILTER_UNREADABLE,
	PARRILLADA_FILTER_BROKEN_SYM,
	PARRILLADA_FILTER_RECURSIVE_SYM,
	PARRILLADA_FILTER_UNKNOWN,
} ParrilladaFilterStatus;

#define PARRILLADA_TYPE_FILTERED_URI             (parrillada_filtered_uri_get_type ())
#define PARRILLADA_FILTERED_URI(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_FILTERED_URI, ParrilladaFilteredUri))
#define PARRILLADA_FILTERED_URI_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_FILTERED_URI, ParrilladaFilteredUriClass))
#define PARRILLADA_IS_FILTERED_URI(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_FILTERED_URI))
#define PARRILLADA_IS_FILTERED_URI_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_FILTERED_URI))
#define PARRILLADA_FILTERED_URI_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_FILTERED_URI, ParrilladaFilteredUriClass))

typedef struct _ParrilladaFilteredUriClass ParrilladaFilteredUriClass;
typedef struct _ParrilladaFilteredUri ParrilladaFilteredUri;

struct _ParrilladaFilteredUriClass
{
	GtkListStoreClass parent_class;
};

struct _ParrilladaFilteredUri
{
	GtkListStore parent_instance;
};

GType parrillada_filtered_uri_get_type (void) G_GNUC_CONST;

ParrilladaFilteredUri *
parrillada_filtered_uri_new (void);

gchar *
parrillada_filtered_uri_restore (ParrilladaFilteredUri *filtered,
			      GtkTreePath *treepath);

ParrilladaFilterStatus
parrillada_filtered_uri_lookup_restored (ParrilladaFilteredUri *filtered,
				      const gchar *uri);

GSList *
parrillada_filtered_uri_get_restored_list (ParrilladaFilteredUri *filtered);

void
parrillada_filtered_uri_filter (ParrilladaFilteredUri *filtered,
			     const gchar *uri,
			     ParrilladaFilterStatus status);
void
parrillada_filtered_uri_dont_filter (ParrilladaFilteredUri *filtered,
				  const gchar *uri);

void
parrillada_filtered_uri_clear (ParrilladaFilteredUri *filtered);

void
parrillada_filtered_uri_remove_with_children (ParrilladaFilteredUri *filtered,
					   const gchar *uri);

G_END_DECLS

#endif /* _PARRILLADA_FILTERED_URI_H_ */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Rouquier Philippe 2009 <bonfire-app@wanadoo.fr>
 * 
 * parrillada is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PARRILLADA_SEARCH_TRACKER_H_
#define _PARRILLADA_SEARCH_TRACKER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SEARCH_TRACKER             (parrillada_search_tracker_get_type ())
#define PARRILLADA_SEARCH_TRACKER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SEARCH_TRACKER, ParrilladaSearchTracker))
#define PARRILLADA_SEARCH_TRACKER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SEARCH_TRACKER, ParrilladaSearchTrackerClass))
#define PARRILLADA_IS_SEARCH_TRACKER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SEARCH_TRACKER))
#define PARRILLADA_IS_SEARCH_TRACKER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SEARCH_TRACKER))
#define PARRILLADA_SEARCH_TRACKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SEARCH_TRACKER, ParrilladaSearchTrackerClass))

typedef struct _ParrilladaSearchTrackerClass ParrilladaSearchTrackerClass;
typedef struct _ParrilladaSearchTracker ParrilladaSearchTracker;

struct _ParrilladaSearchTrackerClass
{
	GObjectClass parent_class;
};

struct _ParrilladaSearchTracker
{
	GObject parent_instance;
};

GType parrillada_search_tracker_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _PARRILLADA_SEARCH_TRACKER_H_ */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-media is distributed in the hope that it will be useful,
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

#ifndef _PARRILLADA_MEDIUM_SELECTION_H_
#define _PARRILLADA_MEDIUM_SELECTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include <parrillada-medium-monitor.h>
#include <parrillada-medium.h>
#include <parrillada-drive.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_MEDIUM_SELECTION             (parrillada_medium_selection_get_type ())
#define PARRILLADA_MEDIUM_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_MEDIUM_SELECTION, ParrilladaMediumSelection))
#define PARRILLADA_MEDIUM_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_MEDIUM_SELECTION, ParrilladaMediumSelectionClass))
#define PARRILLADA_IS_MEDIUM_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_MEDIUM_SELECTION))
#define PARRILLADA_IS_MEDIUM_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_MEDIUM_SELECTION))
#define PARRILLADA_MEDIUM_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_MEDIUM_SELECTION, ParrilladaMediumSelectionClass))

typedef struct _ParrilladaMediumSelectionClass ParrilladaMediumSelectionClass;
typedef struct _ParrilladaMediumSelection ParrilladaMediumSelection;

struct _ParrilladaMediumSelectionClass
{
	GtkComboBoxClass parent_class;

	/* Signals */
	void		(* medium_changed)		(ParrilladaMediumSelection *selection,
							 ParrilladaMedium *medium);

	/* virtual function */
	gchar *		(*format_medium_string)		(ParrilladaMediumSelection *selection,
							 ParrilladaMedium *medium);
};

/**
 * ParrilladaMediumSelection:
 *
 * Rename to: MediumSelection
 */

struct _ParrilladaMediumSelection
{
	GtkComboBox parent_instance;
};

G_MODULE_EXPORT GType parrillada_medium_selection_get_type (void) G_GNUC_CONST;

GtkWidget* parrillada_medium_selection_new (void);

ParrilladaMedium *
parrillada_medium_selection_get_active (ParrilladaMediumSelection *selector);

gboolean
parrillada_medium_selection_set_active (ParrilladaMediumSelection *selector,
				     ParrilladaMedium *medium);

void
parrillada_medium_selection_show_media_type (ParrilladaMediumSelection *selector,
					  ParrilladaMediaType type);

G_END_DECLS

#endif /* _PARRILLADA_MEDIUM_SELECTION_H_ */

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

#ifndef _PARRILLADA_DRIVE_SELECTION_H_
#define _PARRILLADA_DRIVE_SELECTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include <parrillada-medium-monitor.h>
#include <parrillada-drive.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_DRIVE_SELECTION             (parrillada_drive_selection_get_type ())
#define PARRILLADA_DRIVE_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DRIVE_SELECTION, ParrilladaDriveSelection))
#define PARRILLADA_DRIVE_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DRIVE_SELECTION, ParrilladaDriveSelectionClass))
#define PARRILLADA_IS_DRIVE_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DRIVE_SELECTION))
#define PARRILLADA_IS_DRIVE_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DRIVE_SELECTION))
#define PARRILLADA_DRIVE_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DRIVE_SELECTION, ParrilladaDriveSelectionClass))

typedef struct _ParrilladaDriveSelectionClass ParrilladaDriveSelectionClass;
typedef struct _ParrilladaDriveSelection ParrilladaDriveSelection;

struct _ParrilladaDriveSelectionClass
{
	GtkComboBoxClass parent_class;

	/* Signals */
	void		(* drive_changed)		(ParrilladaDriveSelection *selector,
							 ParrilladaDrive *drive);
};

struct _ParrilladaDriveSelection
{
	GtkComboBox parent_instance;
};

G_MODULE_EXPORT GType parrillada_drive_selection_get_type (void) G_GNUC_CONST;

GtkWidget* parrillada_drive_selection_new (void);

ParrilladaDrive *
parrillada_drive_selection_get_active (ParrilladaDriveSelection *selector);

gboolean
parrillada_drive_selection_set_active (ParrilladaDriveSelection *selector,
				    ParrilladaDrive *drive);

void
parrillada_drive_selection_show_type (ParrilladaDriveSelection *selector,
				   ParrilladaDriveType type);

G_END_DECLS

#endif /* _PARRILLADA_DRIVE_SELECTION_H_ */

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

#ifndef PARRILLADA_SUM_DIALOG_H
#define PARRILLADA_SUM_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include <parrillada-tool-dialog.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_SUM_DIALOG         (parrillada_sum_dialog_get_type ())
#define PARRILLADA_SUM_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_SUM_DIALOG, ParrilladaSumDialog))
#define PARRILLADA_SUM_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_SUM_DIALOG, ParrilladaSumDialogClass))
#define PARRILLADA_IS_SUM_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_SUM_DIALOG))
#define PARRILLADA_IS_SUM_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_SUM_DIALOG))
#define PARRILLADA_SUM_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_SUM_DIALOG, ParrilladaSumDialogClass))

typedef struct _ParrilladaSumDialog ParrilladaSumDialog;
typedef struct _ParrilladaSumDialogClass ParrilladaSumDialogClass;

struct _ParrilladaSumDialog {
	ParrilladaToolDialog parent;
};

struct _ParrilladaSumDialogClass {
	ParrilladaToolDialogClass parent_class;
};

GType parrillada_sum_dialog_get_type (void);

ParrilladaSumDialog *parrillada_sum_dialog_new (void);

G_END_DECLS

#endif /* PARRILLADA_SUM_DIALOG_H */

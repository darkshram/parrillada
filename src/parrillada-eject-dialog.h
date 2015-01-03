/***************************************************************************
 *            
 *
 *  Copyright  2008  Philippe Rouquier <brasero-app@wanadoo.fr>
 *  Copyright  2008  Luis Medinas <lmedinas@gmail.com>
 *
 *
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
 *
 */

#ifndef PARRILLADA_EJECT_DIALOG_H
#define PARRILLADA_EJECT_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PARRILLADA_TYPE_EJECT_DIALOG         (parrillada_eject_dialog_get_type ())
#define PARRILLADA_EJECT_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_EJECT_DIALOG, ParrilladaEjectDialog))
#define PARRILLADA_EJECT_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_EJECT_DIALOG, ParrilladaEjectDialogClass))
#define PARRILLADA_IS_EJECT_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_EJECT_DIALOG))
#define PARRILLADA_IS_EJECT_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_EJECT_DIALOG))
#define PARRILLADA_EJECT_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_EJECT_DIALOG, ParrilladaEjectDialogClass))

typedef struct _ParrilladaEjectDialog ParrilladaEjectDialog;
typedef struct _ParrilladaEjectDialogClass ParrilladaEjectDialogClass;

struct _ParrilladaEjectDialog {
	GtkDialog parent;
};

struct _ParrilladaEjectDialogClass {
	GtkDialogClass parent_class;
};

GType parrillada_eject_dialog_get_type (void);
GtkWidget *parrillada_eject_dialog_new (void);

gboolean
parrillada_eject_dialog_cancel (ParrilladaEjectDialog *dialog);

G_END_DECLS

#endif /* PARRILLADA_Eject_DIALOG_H */

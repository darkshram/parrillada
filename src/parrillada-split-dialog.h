/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_SPLIT_DIALOG_H_
#define _PARRILLADA_SPLIT_DIALOG_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _ParrilladaAudioSlice {
	gint64 start;
	gint64 end;
};
typedef struct _ParrilladaAudioSlice ParrilladaAudioSlice;

#define PARRILLADA_TYPE_SPLIT_DIALOG             (parrillada_split_dialog_get_type ())
#define PARRILLADA_SPLIT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SPLIT_DIALOG, ParrilladaSplitDialog))
#define PARRILLADA_SPLIT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SPLIT_DIALOG, ParrilladaSplitDialogClass))
#define PARRILLADA_IS_SPLIT_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SPLIT_DIALOG))
#define PARRILLADA_IS_SPLIT_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SPLIT_DIALOG))
#define PARRILLADA_SPLIT_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SPLIT_DIALOG, ParrilladaSplitDialogClass))

typedef struct _ParrilladaSplitDialogClass ParrilladaSplitDialogClass;
typedef struct _ParrilladaSplitDialog ParrilladaSplitDialog;

struct _ParrilladaSplitDialogClass
{
	GtkDialogClass parent_class;
};

struct _ParrilladaSplitDialog
{
	GtkDialog parent_instance;
};

GType parrillada_split_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *
parrillada_split_dialog_new (void);

void
parrillada_split_dialog_set_uri (ParrilladaSplitDialog *dialog,
			      const gchar *uri,
                              const gchar *title,
                              const gchar *artist);
void
parrillada_split_dialog_set_boundaries (ParrilladaSplitDialog *dialog,
				     gint64 start,
				     gint64 end);

GSList *
parrillada_split_dialog_get_slices (ParrilladaSplitDialog *self);

G_END_DECLS

#endif /* _PARRILLADA_SPLIT_DIALOG_H_ */

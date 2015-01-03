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

#ifndef TRAY_H
#define TRAY_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-basics.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_TRAYICON         (parrillada_tray_icon_get_type ())
#define PARRILLADA_TRAYICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_TRAYICON, ParrilladaTrayIcon))
#define PARRILLADA_TRAYICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_TRAYICON, ParrilladaTrayIconClass))
#define PARRILLADA_IS_TRAYICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_TRAYICON))
#define PARRILLADA_IS_TRAYICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_TRAYICON))
#define PARRILLADA_TRAYICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_TRAYICON, ParrilladaTrayIconClass))

typedef struct ParrilladaTrayIconPrivate ParrilladaTrayIconPrivate;

typedef struct {
	GtkStatusIcon parent;
	ParrilladaTrayIconPrivate *priv;
} ParrilladaTrayIcon;

typedef struct {
	GtkStatusIconClass parent_class;

	void		(*show_dialog)		(ParrilladaTrayIcon *tray, gboolean show);
	void		(*close_after)		(ParrilladaTrayIcon *tray, gboolean close);
	void		(*cancel)		(ParrilladaTrayIcon *tray);

} ParrilladaTrayIconClass;

GType parrillada_tray_icon_get_type (void);
ParrilladaTrayIcon *parrillada_tray_icon_new (void);

void
parrillada_tray_icon_set_progress (ParrilladaTrayIcon *tray,
				gdouble fraction,
				long remaining);

void
parrillada_tray_icon_set_action (ParrilladaTrayIcon *tray,
			      ParrilladaBurnAction action,
			      const gchar *string);

void
parrillada_tray_icon_set_show_dialog (ParrilladaTrayIcon *tray,
				   gboolean show);

G_END_DECLS

#endif /* TRAY_H */

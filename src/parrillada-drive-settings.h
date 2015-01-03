/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Parrillada
 * Copyright (C) Philippe Rouquier 2005-2010 <bonfire-app@wanadoo.fr>
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

#ifndef _PARRILLADA_DRIVE_SETTINGS_H_
#define _PARRILLADA_DRIVE_SETTINGS_H_

#include <glib-object.h>

#include "parrillada-session.h"

G_BEGIN_DECLS

#define PARRILLADA_TYPE_DRIVE_SETTINGS             (parrillada_drive_settings_get_type ())
#define PARRILLADA_DRIVE_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DRIVE_SETTINGS, ParrilladaDriveSettings))
#define PARRILLADA_DRIVE_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DRIVE_SETTINGS, ParrilladaDriveSettingsClass))
#define PARRILLADA_IS_DRIVE_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DRIVE_SETTINGS))
#define PARRILLADA_IS_DRIVE_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DRIVE_SETTINGS))
#define PARRILLADA_DRIVE_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DRIVE_SETTINGS, ParrilladaDriveSettingsClass))

typedef struct _ParrilladaDriveSettingsClass ParrilladaDriveSettingsClass;
typedef struct _ParrilladaDriveSettings ParrilladaDriveSettings;

struct _ParrilladaDriveSettingsClass
{
	GObjectClass parent_class;
};

struct _ParrilladaDriveSettings
{
	GObject parent_instance;
};

GType parrillada_drive_settings_get_type (void) G_GNUC_CONST;

ParrilladaDriveSettings *
parrillada_drive_settings_new (void);

void
parrillada_drive_settings_set_session (ParrilladaDriveSettings *self,
                                    ParrilladaBurnSession *session);

G_END_DECLS

#endif /* _PARRILLADA_DRIVE_SETTINGS_H_ */

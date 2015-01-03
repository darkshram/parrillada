/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * parrillada
 * Copyright (C) Philippe Rouquier 2009 <bonfire-app@wanadoo.fr>
 * 
 * parrillada is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * parrillada is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PARRILLADA_SETTING_H_
#define _PARRILLADA_SETTING_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	PARRILLADA_SETTING_VALUE_NONE,

	/** gint value **/
	PARRILLADA_SETTING_WIN_WIDTH,
	PARRILLADA_SETTING_WIN_HEIGHT,
	PARRILLADA_SETTING_STOCK_FILE_CHOOSER_PERCENT,
	PARRILLADA_SETTING_PARRILLADA_FILE_CHOOSER_PERCENT,
	PARRILLADA_SETTING_PLAYER_VOLUME,
	PARRILLADA_SETTING_DISPLAY_PROPORTION,
	PARRILLADA_SETTING_DISPLAY_LAYOUT,
	PARRILLADA_SETTING_DATA_DISC_COLUMN,
	PARRILLADA_SETTING_DATA_DISC_COLUMN_ORDER,
	PARRILLADA_SETTING_IMAGE_SIZE_WIDTH,
	PARRILLADA_SETTING_IMAGE_SIZE_HEIGHT,
	PARRILLADA_SETTING_VIDEO_SIZE_HEIGHT,
	PARRILLADA_SETTING_VIDEO_SIZE_WIDTH,

	/** gboolean **/
	PARRILLADA_SETTING_WIN_MAXIMIZED,
	PARRILLADA_SETTING_SHOW_SIDEPANE,
	PARRILLADA_SETTING_SHOW_PREVIEW,

	/** gchar * **/
	PARRILLADA_SETTING_DISPLAY_LAYOUT_AUDIO,
	PARRILLADA_SETTING_DISPLAY_LAYOUT_DATA,
	PARRILLADA_SETTING_DISPLAY_LAYOUT_VIDEO,

	/** gchar ** **/
	PARRILLADA_SETTING_SEARCH_ENTRY_HISTORY,

} ParrilladaSettingValue;

#define PARRILLADA_TYPE_SETTING             (parrillada_setting_get_type ())
#define PARRILLADA_SETTING(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_SETTING, ParrilladaSetting))
#define PARRILLADA_SETTING_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_SETTING, ParrilladaSettingClass))
#define PARRILLADA_IS_SETTING(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_SETTING))
#define PARRILLADA_IS_SETTING_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_SETTING))
#define PARRILLADA_SETTING_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_SETTING, ParrilladaSettingClass))

typedef struct _ParrilladaSettingClass ParrilladaSettingClass;
typedef struct _ParrilladaSetting ParrilladaSetting;

struct _ParrilladaSettingClass
{
	GObjectClass parent_class;

	/* Signals */
	void(* value_changed) (ParrilladaSetting *self, gint value);
};

struct _ParrilladaSetting
{
	GObject parent_instance;
};

GType parrillada_setting_get_type (void) G_GNUC_CONST;

ParrilladaSetting *
parrillada_setting_get_default (void);

gboolean
parrillada_setting_get_value (ParrilladaSetting *setting,
                           ParrilladaSettingValue setting_value,
                           gpointer *value);

gboolean
parrillada_setting_set_value (ParrilladaSetting *setting,
                           ParrilladaSettingValue setting_value,
                           gconstpointer value);

gboolean
parrillada_setting_load (ParrilladaSetting *setting);

gboolean
parrillada_setting_save (ParrilladaSetting *setting);

G_END_DECLS

#endif /* _PARRILLADA_SETTING_H_ */

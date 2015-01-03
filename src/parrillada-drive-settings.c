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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "parrillada-drive-settings.h"
#include "parrillada-session.h"
#include "parrillada-session-helper.h"
#include "parrillada-drive-properties.h"

typedef struct _ParrilladaDriveSettingsPrivate ParrilladaDriveSettingsPrivate;
struct _ParrilladaDriveSettingsPrivate
{
	ParrilladaMedia dest_media;
	ParrilladaDrive *dest_drive;
	ParrilladaTrackType *src_type;

	GSettings *settings;
	GSettings *config_settings;
	ParrilladaBurnSession *session;
};

#define PARRILLADA_DRIVE_SETTINGS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DRIVE_SETTINGS, ParrilladaDriveSettingsPrivate))

#define PARRILLADA_SCHEMA_DRIVES		"org.mate.parrillada.drives"
#define PARRILLADA_DRIVE_PROPERTIES_PATH	"/org/mate/parrillada/drives/"
#define PARRILLADA_PROPS_FLAGS			"flags"
#define PARRILLADA_PROPS_SPEED			"speed"

#define PARRILLADA_SCHEMA_CONFIG		"org.mate.parrillada.config"
#define PARRILLADA_PROPS_TMP_DIR		"tmpdir"

#define PARRILLADA_DEST_SAVED_FLAGS		(PARRILLADA_DRIVE_PROPERTIES_FLAGS|PARRILLADA_BURN_FLAG_MULTI)

G_DEFINE_TYPE (ParrilladaDriveSettings, parrillada_drive_settings, G_TYPE_OBJECT);

static GVariant *
parrillada_drive_settings_set_mapping_speed (const GValue *value,
                                          const GVariantType *variant_type,
                                          gpointer user_data)
{
	return g_variant_new_int32 (g_value_get_int64 (value) / 1000);
}

static gboolean
parrillada_drive_settings_get_mapping_speed (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
	if (!g_variant_get_int32 (variant)) {
		ParrilladaDriveSettingsPrivate *priv;
		ParrilladaMedium *medium;
		ParrilladaDrive *drive;

		priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (user_data);
		drive = parrillada_burn_session_get_burner (priv->session);
		medium = parrillada_drive_get_medium (drive);

		/* Must not be NULL since we only bind when a medium is available */
		g_assert (medium != NULL);

		g_value_set_int64 (value, parrillada_medium_get_max_write_speed (medium));
	}
	else
		g_value_set_int64 (value, g_variant_get_int32 (variant) * 1000);

	return TRUE;
}

static GVariant *
parrillada_drive_settings_set_mapping_flags (const GValue *value,
                                          const GVariantType *variant_type,
                                          gpointer user_data)
{
	return g_variant_new_int32 (g_value_get_int (value) & PARRILLADA_DEST_SAVED_FLAGS);
}

static gboolean
parrillada_drive_settings_get_mapping_flags (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
	ParrilladaBurnFlag flags;
	ParrilladaBurnFlag current_flags;
	ParrilladaDriveSettingsPrivate *priv;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (user_data);

	flags = g_variant_get_int32 (variant);
	if (parrillada_burn_session_same_src_dest_drive (priv->session)) {
		/* Special case */
		if (flags == 1) {
			flags = PARRILLADA_BURN_FLAG_EJECT|
				PARRILLADA_BURN_FLAG_BURNPROOF;
		}
		else
			flags &= PARRILLADA_DEST_SAVED_FLAGS;

		flags |= PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE|
			 PARRILLADA_BURN_FLAG_FAST_BLANK;
	}
	/* This is for the default value when the user has never used it */
	else if (flags == 1) {
		ParrilladaTrackType *source;

		flags = PARRILLADA_BURN_FLAG_EJECT|
			PARRILLADA_BURN_FLAG_BURNPROOF;

		source = parrillada_track_type_new ();
		parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (priv->session), source);

		if (!parrillada_track_type_get_has_medium (source))
			flags |= PARRILLADA_BURN_FLAG_NO_TMP_FILES;

		parrillada_track_type_free (source);
	}
	else
		flags &= PARRILLADA_DEST_SAVED_FLAGS;

	current_flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (priv->session));
	current_flags &= (~PARRILLADA_DEST_SAVED_FLAGS);

	g_value_set_int (value, flags|current_flags);
	return TRUE;
}

static void
parrillada_drive_settings_bind_session (ParrilladaDriveSettings *self)
{
	ParrilladaDriveSettingsPrivate *priv;
	gchar *display_name;
	gchar *path;
	gchar *tmp;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (self);

	/* Get the drive name: it's done this way to avoid escaping */
	tmp = parrillada_drive_get_display_name (priv->dest_drive);
	display_name = g_strdup_printf ("drive-%u", g_str_hash (tmp));
	g_free (tmp);

	if (parrillada_track_type_get_has_medium (priv->src_type))
		path = g_strdup_printf ("%s%s/disc-%i/",
		                        PARRILLADA_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (parrillada_track_type_get_has_data (priv->src_type))
		path = g_strdup_printf ("%s%s/data-%i/",
		                        PARRILLADA_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (parrillada_track_type_get_has_image (priv->src_type))
		path = g_strdup_printf ("%s%s/image-%i/",
		                        PARRILLADA_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (parrillada_track_type_get_has_stream (priv->src_type))
		path = g_strdup_printf ("%s%s/audio-%i/",
		                        PARRILLADA_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else {
		g_free (display_name);
		return;
	}
	g_free (display_name);

	priv->settings = g_settings_new_with_path (PARRILLADA_SCHEMA_DRIVES, path);
	g_free (path);

	g_settings_bind_with_mapping (priv->settings, PARRILLADA_PROPS_SPEED,
	                              priv->session, "speed", G_SETTINGS_BIND_DEFAULT,
	                              parrillada_drive_settings_get_mapping_speed,
	                              parrillada_drive_settings_set_mapping_speed,
	                              self,
	                              NULL);

	g_settings_bind_with_mapping (priv->settings, PARRILLADA_PROPS_FLAGS,
	                              priv->session, "flags", G_SETTINGS_BIND_DEFAULT,
	                              parrillada_drive_settings_get_mapping_flags,
	                              parrillada_drive_settings_set_mapping_flags,
	                              self,
	                              NULL);
}

static void
parrillada_drive_settings_unbind_session (ParrilladaDriveSettings *self)
{
	ParrilladaDriveSettingsPrivate *priv;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (self);

	if (priv->settings) {
		parrillada_track_type_free (priv->src_type);
		priv->src_type = NULL;

		g_object_unref (priv->dest_drive);
		priv->dest_drive = NULL;

		priv->dest_media = PARRILLADA_MEDIUM_NONE;

		g_settings_unbind (priv->settings, "speed");
		g_settings_unbind (priv->settings, "flags");

		g_object_unref (priv->settings);
		priv->settings = NULL;
	}
}

static void
parrillada_drive_settings_rebind_session (ParrilladaDriveSettings *self)
{
	ParrilladaDriveSettingsPrivate *priv;
	ParrilladaTrackType *type;
	ParrilladaMedia new_media;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (self);

	/* See if we really need to do that:
	 * - check the source type has changed 
	 * - check the output type has changed */
	drive = parrillada_burn_session_get_burner (priv->session);
	medium = parrillada_drive_get_medium (drive);
	type = parrillada_track_type_new ();

	if (!drive
	||  !medium
	||   parrillada_drive_is_fake (drive)
	||  !PARRILLADA_MEDIUM_VALID (parrillada_medium_get_status (medium))
	||   parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (priv->session), type) != PARRILLADA_BURN_OK) {
		parrillada_drive_settings_unbind_session (self);
		return;
	}

	new_media = PARRILLADA_MEDIUM_TYPE (parrillada_medium_get_status (medium));

	if (priv->dest_drive == drive
	&&  priv->dest_media == new_media
	&&  priv->src_type && parrillada_track_type_equal (priv->src_type, type)) {
		parrillada_track_type_free (type);
		return;
	}

	parrillada_track_type_free (priv->src_type);
	priv->src_type = type;

	if (priv->dest_drive)
		g_object_unref (priv->dest_drive);

	priv->dest_drive = g_object_ref (drive);

	priv->dest_media = new_media;

	parrillada_drive_settings_bind_session (self);
}

static void
parrillada_drive_settings_output_changed_cb (ParrilladaBurnSession *session,
                                          ParrilladaMedium *former_medium,
                                          ParrilladaDriveSettings *self)
{
	parrillada_drive_settings_rebind_session (self);
}

static void
parrillada_drive_settings_track_added_cb (ParrilladaBurnSession *session,
                                       ParrilladaTrack *track,
                                       ParrilladaDriveSettings *self)
{
	parrillada_drive_settings_rebind_session (self);
}

static void
parrillada_drive_settings_track_removed_cb (ParrilladaBurnSession *session,
                                         ParrilladaTrack *track,
                                         guint former_position,
                                         ParrilladaDriveSettings *self)
{
	parrillada_drive_settings_rebind_session (self);
}

static void
parrillada_drive_settings_unset_session (ParrilladaDriveSettings *self)
{
	ParrilladaDriveSettingsPrivate *priv;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (self);

	parrillada_drive_settings_unbind_session (self);

	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_drive_settings_track_added_cb,
		                                      self);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_drive_settings_track_removed_cb,
		                                      self);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_drive_settings_output_changed_cb,
		                                      self);

		g_settings_unbind (priv->config_settings, "tmpdir");
		g_object_unref (priv->config_settings);

		g_object_unref (priv->session);
		priv->session = NULL;
	}
}

void
parrillada_drive_settings_set_session (ParrilladaDriveSettings *self,
                                    ParrilladaBurnSession *session)
{
	ParrilladaDriveSettingsPrivate *priv;

	priv = PARRILLADA_DRIVE_SETTINGS_PRIVATE (self);

	parrillada_drive_settings_unset_session (self);

	priv->session = g_object_ref (session);
	g_signal_connect (session,
	                  "track-added",
	                  G_CALLBACK (parrillada_drive_settings_track_added_cb),
	                  self);
	g_signal_connect (session,
	                  "track-removed",
	                  G_CALLBACK (parrillada_drive_settings_track_removed_cb),
	                  self);
	g_signal_connect (session,
	                  "output-changed",
	                  G_CALLBACK (parrillada_drive_settings_output_changed_cb),
	                  self);
	parrillada_drive_settings_rebind_session (self);

	priv->config_settings = g_settings_new (PARRILLADA_SCHEMA_CONFIG);
	g_settings_bind (priv->config_settings,
	                 PARRILLADA_PROPS_TMP_DIR, session,
	                 "tmpdir", G_SETTINGS_BIND_DEFAULT);
}

static void
parrillada_drive_settings_init (ParrilladaDriveSettings *object)
{ }

static void
parrillada_drive_settings_finalize (GObject *object)
{
	parrillada_drive_settings_unset_session (PARRILLADA_DRIVE_SETTINGS (object));
	G_OBJECT_CLASS (parrillada_drive_settings_parent_class)->finalize (object);
}

static void
parrillada_drive_settings_class_init (ParrilladaDriveSettingsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDriveSettingsPrivate));

	object_class->finalize = parrillada_drive_settings_finalize;
}

ParrilladaDriveSettings *
parrillada_drive_settings_new (void)
{
	return g_object_new (PARRILLADA_TYPE_DRIVE_SETTINGS, NULL);
}


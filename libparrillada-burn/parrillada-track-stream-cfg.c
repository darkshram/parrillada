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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "parrillada-misc.h"

#include "burn-debug.h"

#include "parrillada-track-stream-cfg.h"
#include "parrillada-io.h"
#include "parrillada-tags.h"

static ParrilladaIOJobCallbacks *io_methods = NULL;

typedef struct _ParrilladaTrackStreamCfgPrivate ParrilladaTrackStreamCfgPrivate;
struct _ParrilladaTrackStreamCfgPrivate
{
	ParrilladaIOJobBase *load_uri;

	GFileMonitor *monitor;

	GError *error;

	guint loading:1;
};

#define PARRILLADA_TRACK_STREAM_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_STREAM_CFG, ParrilladaTrackStreamCfgPrivate))

G_DEFINE_TYPE (ParrilladaTrackStreamCfg, parrillada_track_stream_cfg, PARRILLADA_TYPE_TRACK_STREAM);

static void
parrillada_track_stream_cfg_file_changed (GFileMonitor *monitor,
								GFile *file,
								GFile *other_file,
								GFileMonitorEvent event,
								ParrilladaTrackStream *track)
{
	ParrilladaTrackStreamCfgPrivate *priv;
	gchar *name;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (track);

        switch (event) {
 /*               case G_FILE_MONITOR_EVENT_CHANGED:
                        return;
*/
                case G_FILE_MONITOR_EVENT_DELETED:
                        g_object_unref (priv->monitor);
                        priv->monitor = NULL;

			name = g_file_get_basename (file);
			priv->error = g_error_new (PARRILLADA_BURN_ERROR,
								  PARRILLADA_BURN_ERROR_FILE_NOT_FOUND,
								  /* Translators: %s is the name of the file that has just been deleted */
								  _("\"%s\" was removed from the file system."),
								 name);
			g_free (name);
                        break;

                default:
                        return;
        }

        parrillada_track_changed (PARRILLADA_TRACK (track));
}

static void
parrillada_track_stream_cfg_results_cb (GObject *obj,
				     GError *error,
				     const gchar *uri,
				     GFileInfo *info,
				     gpointer user_data)
{
	GFile *file;
	guint64 len;
	GObject *snapshot;
	ParrilladaTrackStreamCfgPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (obj);
	priv->loading = FALSE;

	/* Check the return status for this file */
	if (error) {
		priv->error = g_error_copy (error);
		parrillada_track_changed (PARRILLADA_TRACK (obj));
		return;
	}

	/* FIXME: we don't know whether it's audio or video that is required */
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		/* This error is special as it can be recovered from */
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_FILE_FOLDER,
					   _("Directories cannot be added to video or audio discs"));
		parrillada_track_changed (PARRILLADA_TRACK (obj));
		return;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	&&  (!strcmp (g_file_info_get_content_type (info), "audio/x-scpls")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-ms-asx")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-mp3-playlist")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-mpegurl"))) {
		/* This error is special as it can be recovered from */
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_FILE_PLAYLIST,
					   _("Playlists cannot be added to video or audio discs"));

		parrillada_track_changed (PARRILLADA_TRACK (obj));
		return;
	}

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| (!g_file_info_get_attribute_boolean (info, PARRILLADA_IO_HAS_VIDEO)
	&&  !g_file_info_get_attribute_boolean (info, PARRILLADA_IO_HAS_AUDIO))) {
		gchar *name;

		PARRILLADA_GET_BASENAME_FOR_DISPLAY (uri, name);
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   /* Translators: %s is the name of the file */
					   _("\"%s\" is not suitable for audio or video media"),
					   name);
		g_free (name);

		parrillada_track_changed (PARRILLADA_TRACK (obj));
		return;
	}

	/* Also make sure it's duration is appropriate (!= 0) */
	len = g_file_info_get_attribute_uint64 (info, PARRILLADA_IO_LEN);
	if (len <= 0) {
		gchar *name;

		PARRILLADA_GET_BASENAME_FOR_DISPLAY (uri, name);
		priv->error = g_error_new (PARRILLADA_BURN_ERROR,
					   PARRILLADA_BURN_ERROR_GENERAL,
					   /* Translators: %s is the name of the file */
					   _("\"%s\" is not suitable for audio or video media"),
					   name);
		g_free (name);

		parrillada_track_changed (PARRILLADA_TRACK (obj));
		return;
	}

	if (g_file_info_get_is_symlink (info)) {
		gchar *sym_uri;

		sym_uri = g_strconcat ("file://", g_file_info_get_symlink_target (info), NULL);
		if (PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_source)
			PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_source (PARRILLADA_TRACK_STREAM (obj), sym_uri);

		g_free (sym_uri);
	}

	/* Check whether the stream is wav+dts as it can be burnt as such */
	if (g_file_info_get_attribute_boolean (info, PARRILLADA_IO_HAS_DTS)) {
		PARRILLADA_BURN_LOG ("Track has DTS");
		PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_format (PARRILLADA_TRACK_STREAM (obj), 
		                                                                                PARRILLADA_AUDIO_FORMAT_DTS|
		                                                                                PARRILLADA_METADATA_INFO);
	}
	else if (PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_format)
		PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_format (PARRILLADA_TRACK_STREAM (obj),
		                                                                                (g_file_info_get_attribute_boolean (info, PARRILLADA_IO_HAS_VIDEO)?
		                                                                                 PARRILLADA_VIDEO_FORMAT_UNDEFINED:PARRILLADA_AUDIO_FORMAT_NONE)|
		                                                                                (g_file_info_get_attribute_boolean (info, PARRILLADA_IO_HAS_AUDIO)?
		                                                                                 PARRILLADA_AUDIO_FORMAT_UNDEFINED:PARRILLADA_AUDIO_FORMAT_NONE)|
		                                                                                PARRILLADA_METADATA_INFO);

	/* Size/length. Only set when end value has not been already set.
	 * Fix #607752 -  audio track start and end points are overwritten after
	 * being read from a project file.
	 * We don't want to set a new len if one has been set already. Nevertheless
	 * if the length we detected is smaller than the one that was set we go
	 * for the new one. */
	if (PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_boundaries) {
		gint64 min_start;

		/* Make sure that the start value is coherent */
		min_start = (len - PARRILLADA_MIN_STREAM_LENGTH) >= 0? (len - PARRILLADA_MIN_STREAM_LENGTH):0;
		if (min_start && parrillada_track_stream_get_start (PARRILLADA_TRACK_STREAM (obj)) > min_start) {
			PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_boundaries (PARRILLADA_TRACK_STREAM (obj),
													    min_start,
													    -1,
													    -1);
		}

		if (parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (obj)) > len
		||  parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (obj)) <= 0) {
			/* Don't set either gap or start to make sure we don't remove
			 * values set by project parser or values set from the beginning
			 * Fix #607752 -  audio track start and end points are overwritten
			 * after being read from a project file */
			PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_boundaries (PARRILLADA_TRACK_STREAM (obj),
													    -1,
													    len,
													    -1);
		}
	}

	snapshot = g_file_info_get_attribute_object (info, PARRILLADA_IO_THUMBNAIL);
	if (snapshot) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, g_object_ref (snapshot));
		parrillada_track_tag_add (PARRILLADA_TRACK (obj),
				       PARRILLADA_TRACK_STREAM_THUMBNAIL_TAG,
				       value);
	}

	if (g_file_info_get_content_type (info)) {
		const gchar *icon_string = "text-x-preview";
		GtkIconTheme *theme;
		GIcon *icon;

		theme = gtk_icon_theme_get_default ();

		/* NOTE: implemented in glib 2.15.6 (not for windows though) */
		icon = g_content_type_get_icon (g_file_info_get_content_type (info));
		if (G_IS_THEMED_ICON (icon)) {
			const gchar * const *names = NULL;

			names = g_themed_icon_get_names (G_THEMED_ICON (icon));
			if (names) {
				gint i;

				for (i = 0; names [i]; i++) {
					if (gtk_icon_theme_has_icon (theme, names [i])) {
						icon_string = names [i];
						break;
					}
				}
			}
		}

		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_MIME_TAG,
					      icon_string);
		g_object_unref (icon);
	}

	/* Get the song info */
	if (g_file_info_get_attribute_string (info, PARRILLADA_IO_TITLE)
	&& !parrillada_track_tag_lookup_string (PARRILLADA_TRACK (obj), PARRILLADA_TRACK_STREAM_TITLE_TAG))
		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_TITLE_TAG,
					      g_file_info_get_attribute_string (info, PARRILLADA_IO_TITLE));
	if (g_file_info_get_attribute_string (info, PARRILLADA_IO_ARTIST)
	&& !parrillada_track_tag_lookup_string (PARRILLADA_TRACK (obj), PARRILLADA_TRACK_STREAM_ARTIST_TAG))
		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_ARTIST_TAG,
					      g_file_info_get_attribute_string (info, PARRILLADA_IO_ARTIST));
	if (g_file_info_get_attribute_string (info, PARRILLADA_IO_ALBUM)
	&& !parrillada_track_tag_lookup_string (PARRILLADA_TRACK (obj), PARRILLADA_TRACK_STREAM_ALBUM_TAG))
		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_ALBUM_TAG,
					      g_file_info_get_attribute_string (info, PARRILLADA_IO_ALBUM));
	if (g_file_info_get_attribute_string (info, PARRILLADA_IO_COMPOSER)
	&& !parrillada_track_tag_lookup_string (PARRILLADA_TRACK (obj), PARRILLADA_TRACK_STREAM_COMPOSER_TAG))
		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_COMPOSER_TAG,
					      g_file_info_get_attribute_string (info, PARRILLADA_IO_COMPOSER));
	if (g_file_info_get_attribute_string (info, PARRILLADA_IO_ISRC)
	&& !parrillada_track_tag_lookup_string (PARRILLADA_TRACK (obj), PARRILLADA_TRACK_STREAM_ISRC_TAG))
		parrillada_track_tag_add_string (PARRILLADA_TRACK (obj),
					      PARRILLADA_TRACK_STREAM_ISRC_TAG,
					      g_file_info_get_attribute_string (info, PARRILLADA_IO_ISRC));

	/* Start monitoring it */
	file = g_file_new_for_uri (uri);
	priv->monitor = g_file_monitor_file (file,
	                                     G_FILE_MONITOR_NONE,
	                                     NULL,
	                                     NULL);
	g_object_unref (file);

	g_signal_connect (priv->monitor,
	                  "changed",
	                  G_CALLBACK (parrillada_track_stream_cfg_file_changed),
	                  obj);

	parrillada_track_changed (PARRILLADA_TRACK (obj));
}

static void
parrillada_track_stream_cfg_get_info (ParrilladaTrackStreamCfg *track)
{
	ParrilladaTrackStreamCfgPrivate *priv;
	gchar *uri;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (track);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* get info async for the file */
	if (!priv->load_uri) {
		if (!io_methods)
			io_methods = parrillada_io_register_job_methods (parrillada_track_stream_cfg_results_cb,
			                                              NULL,
			                                              NULL);

		priv->load_uri = parrillada_io_register_with_methods (G_OBJECT (track), io_methods);
	}

	priv->loading = TRUE;
	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), TRUE);
	parrillada_io_get_file_info (uri,
				  priv->load_uri,
				  PARRILLADA_IO_INFO_PERM|
				  PARRILLADA_IO_INFO_MIME|
				  PARRILLADA_IO_INFO_URGENT|
				  PARRILLADA_IO_INFO_METADATA|
				  PARRILLADA_IO_INFO_METADATA_MISSING_CODEC|
				  PARRILLADA_IO_INFO_METADATA_THUMBNAIL,
				  track);
	g_free (uri);
}

static ParrilladaBurnResult
parrillada_track_stream_cfg_set_source (ParrilladaTrackStream *track,
				     const gchar *uri)
{
	ParrilladaTrackStreamCfgPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (track);
	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->load_uri)
		parrillada_io_cancel_by_base (priv->load_uri);

	if (PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_source)
		PARRILLADA_TRACK_STREAM_CLASS (parrillada_track_stream_cfg_parent_class)->set_source (track, uri);

	parrillada_track_stream_cfg_get_info (PARRILLADA_TRACK_STREAM_CFG (track));
	parrillada_track_changed (PARRILLADA_TRACK (track));
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_stream_cfg_get_status (ParrilladaTrack *track,
				     ParrilladaStatus *status)
{
	ParrilladaTrackStreamCfgPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (track);

	if (priv->error) {
		parrillada_status_set_error (status, g_error_copy (priv->error));
		return PARRILLADA_BURN_ERR;
	}

	if (priv->loading) {
		if (status)
			parrillada_status_set_not_ready (status,
						      -1.0,
						      _("Analysing video files"));

		return PARRILLADA_BURN_NOT_READY;
	}

	if (status)
		parrillada_status_set_completed (status);

	return PARRILLADA_TRACK_CLASS (parrillada_track_stream_cfg_parent_class)->get_status (track, status);
}

static void
parrillada_track_stream_cfg_init (ParrilladaTrackStreamCfg *object)
{ }

static void
parrillada_track_stream_cfg_finalize (GObject *object)
{
	ParrilladaTrackStreamCfgPrivate *priv;

	priv = PARRILLADA_TRACK_STREAM_CFG_PRIVATE (object);

	if (priv->load_uri) {
		parrillada_io_cancel_by_base (priv->load_uri);

		if (io_methods->ref == 1)
			io_methods = NULL;

		parrillada_io_job_base_free (priv->load_uri);
		priv->load_uri = NULL;
	}

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	G_OBJECT_CLASS (parrillada_track_stream_cfg_parent_class)->finalize (object);
}

static void
parrillada_track_stream_cfg_class_init (ParrilladaTrackStreamCfgClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass* track_class = PARRILLADA_TRACK_CLASS (klass);
	ParrilladaTrackStreamClass *parent_class = PARRILLADA_TRACK_STREAM_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackStreamCfgPrivate));

	object_class->finalize = parrillada_track_stream_cfg_finalize;

	track_class->get_status = parrillada_track_stream_cfg_get_status;

	parent_class->set_source = parrillada_track_stream_cfg_set_source;
}

/**
 * parrillada_track_stream_cfg_new:
 *
 *  Creates a new #ParrilladaTrackStreamCfg object.
 *
 * Return value: a #ParrilladaTrackStreamCfg object.
 **/

ParrilladaTrackStreamCfg *
parrillada_track_stream_cfg_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_STREAM_CFG, NULL);
}


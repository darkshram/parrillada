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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "burn-plugin-manager.h"
#include "parrillada-medium-selection-priv.h"
#include "parrillada-session-helper.h"

#include "parrillada-dest-selection.h"

#include "parrillada-drive.h"
#include "parrillada-medium.h"
#include "parrillada-volume.h"

#include "parrillada-burn-lib.h"
#include "parrillada-tags.h"
#include "parrillada-track.h"
#include "parrillada-session.h"
#include "parrillada-session-cfg.h"

typedef struct _ParrilladaDestSelectionPrivate ParrilladaDestSelectionPrivate;
struct _ParrilladaDestSelectionPrivate
{
	ParrilladaBurnSession *session;

	ParrilladaDrive *locked_drive;

	guint user_changed:1;
};

#define PARRILLADA_DEST_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DEST_SELECTION, ParrilladaDestSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (ParrilladaDestSelection, parrillada_dest_selection, PARRILLADA_TYPE_MEDIUM_SELECTION);

static void
parrillada_dest_selection_lock (ParrilladaDestSelection *self,
			     gboolean locked)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	if (locked == (priv->locked_drive != NULL))
		return;

	gtk_widget_set_sensitive (GTK_WIDGET (self), (locked != TRUE));
	gtk_widget_queue_draw (GTK_WIDGET (self));

	if (priv->locked_drive) {
		parrillada_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
		priv->locked_drive = NULL;
	}

	if (locked) {
		ParrilladaMedium *medium;

		medium = parrillada_medium_selection_get_active (PARRILLADA_MEDIUM_SELECTION (self));
		priv->locked_drive = parrillada_medium_get_drive (medium);

		if (priv->locked_drive) {
			g_object_ref (priv->locked_drive);
			parrillada_drive_lock (priv->locked_drive,
					    _("Ongoing burning process"),
					    NULL);
		}

		if (medium)
			g_object_unref (medium);
	}
}

static void
parrillada_dest_selection_valid_session (ParrilladaSessionCfg *session,
				      ParrilladaDestSelection *self)
{
	parrillada_medium_selection_update_media_string (PARRILLADA_MEDIUM_SELECTION (self));
}

static void
parrillada_dest_selection_output_changed (ParrilladaSessionCfg *session,
				       ParrilladaMedium *former,
				       ParrilladaDestSelection *self)
{
	ParrilladaDestSelectionPrivate *priv;
	ParrilladaMedium *medium;
	ParrilladaDrive *burner;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	/* make sure the current displayed drive reflects that */
	burner = parrillada_burn_session_get_burner (priv->session);
	medium = parrillada_medium_selection_get_active (PARRILLADA_MEDIUM_SELECTION (self));
	if (burner != parrillada_medium_get_drive (medium))
		parrillada_medium_selection_set_active (PARRILLADA_MEDIUM_SELECTION (self),
						     parrillada_drive_get_medium (burner));

	if (medium)
		g_object_unref (medium);
}

static void
parrillada_dest_selection_flags_changed (ParrilladaBurnSession *session,
                                      GParamSpec *pspec,
				      ParrilladaDestSelection *self)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	parrillada_dest_selection_lock (self, (parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (priv->session)) & PARRILLADA_BURN_FLAG_MERGE) != 0);
}

static void
parrillada_dest_selection_medium_changed (ParrilladaMediumSelection *selection,
				       ParrilladaMedium *medium)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (selection);

	if (!priv->session)
		goto chain;

	if (!medium) {
	    	gtk_widget_set_sensitive (GTK_WIDGET (selection), FALSE);
		goto chain;
	}

	if (parrillada_medium_get_drive (medium) == parrillada_burn_session_get_burner (priv->session))
		goto chain;

	if (priv->locked_drive && priv->locked_drive != parrillada_medium_get_drive (medium)) {
		parrillada_medium_selection_set_active (selection, medium);
		goto chain;
	}

	parrillada_burn_session_set_burner (priv->session, parrillada_medium_get_drive (medium));
	gtk_widget_set_sensitive (GTK_WIDGET (selection), (priv->locked_drive == NULL));

chain:

	if (PARRILLADA_MEDIUM_SELECTION_CLASS (parrillada_dest_selection_parent_class)->medium_changed)
		PARRILLADA_MEDIUM_SELECTION_CLASS (parrillada_dest_selection_parent_class)->medium_changed (selection, medium);
}

static void
parrillada_dest_selection_user_change (ParrilladaDestSelection *selection,
                                    GParamSpec *pspec,
                                    gpointer NULL_data)
{
	gboolean shown = FALSE;
	ParrilladaDestSelectionPrivate *priv;

	/* we are only interested when the menu is shown */
	g_object_get (selection,
	              "popup-shown", &shown,
	              NULL);

	if (!shown)
		return;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (selection);
	priv->user_changed = TRUE;
}

static void
parrillada_dest_selection_medium_removed (GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gpointer user_data)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (user_data);
	if (priv->user_changed)
		return;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)) == -1)
		parrillada_dest_selection_choose_best (PARRILLADA_DEST_SELECTION (user_data));
}

static void
parrillada_dest_selection_medium_added (GtkTreeModel *model,
                                     GtkTreePath *path,
                                     GtkTreeIter *iter,
                                     gpointer user_data)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (user_data);
	if (priv->user_changed)
		return;

	parrillada_dest_selection_choose_best (PARRILLADA_DEST_SELECTION (user_data));
}

static void
parrillada_dest_selection_init (ParrilladaDestSelection *object)
{
	ParrilladaDestSelectionPrivate *priv;
	GtkTreeModel *model;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (object);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (object));
	g_signal_connect (model,
	                  "row-inserted",
	                  G_CALLBACK (parrillada_dest_selection_medium_added),
	                  object);
	g_signal_connect (model,
	                  "row-deleted",
	                  G_CALLBACK (parrillada_dest_selection_medium_removed),
	                  object);

	/* Only show media on which we can write and which are in a burner.
	 * There is one exception though, when we're copying media and when the
	 * burning device is the same as the dest device. */
	parrillada_medium_selection_show_media_type (PARRILLADA_MEDIUM_SELECTION (object),
						  PARRILLADA_MEDIA_TYPE_WRITABLE|
						  PARRILLADA_MEDIA_TYPE_FILE);

	/* This is to know when the user changed it on purpose */
	g_signal_connect (object,
	                  "notify::popup-shown",
	                  G_CALLBACK (parrillada_dest_selection_user_change),
	                  NULL);
}

static void
parrillada_dest_selection_clean (ParrilladaDestSelection *self)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
						      parrillada_dest_selection_valid_session,
						      self);
		g_signal_handlers_disconnect_by_func (priv->session,
						      parrillada_dest_selection_output_changed,
						      self);
		g_signal_handlers_disconnect_by_func (priv->session,
						      parrillada_dest_selection_flags_changed,
						      self);

		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->locked_drive) {
		parrillada_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
		priv->locked_drive = NULL;
	}
}

static void
parrillada_dest_selection_finalize (GObject *object)
{
	parrillada_dest_selection_clean (PARRILLADA_DEST_SELECTION (object));
	G_OBJECT_CLASS (parrillada_dest_selection_parent_class)->finalize (object);
}

static goffset
_get_medium_free_space (ParrilladaMedium *medium,
                        goffset session_blocks)
{
	ParrilladaMedia media;
	goffset blocks = 0;

	media = parrillada_medium_get_status (medium);
	media = parrillada_burn_library_get_media_capabilities (media);

	/* NOTE: we always try to blank a medium when we can */
	parrillada_medium_get_free_space (medium,
				       NULL,
				       &blocks);

	if ((media & PARRILLADA_MEDIUM_REWRITABLE)
	&& blocks < session_blocks)
		parrillada_medium_get_capacity (medium,
		                             NULL,
		                             &blocks);

	return blocks;
}

static gboolean
parrillada_dest_selection_foreach_medium (ParrilladaMedium *medium,
				       gpointer callback_data)
{
	ParrilladaBurnSession *session;
	goffset session_blocks = 0;
	goffset burner_blocks = 0;
	goffset medium_blocks;
	ParrilladaDrive *burner;

	session = callback_data;
	burner = parrillada_burn_session_get_burner (session);
	if (!burner) {
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));
		return TRUE;
	}

	/* no need to deal with this case */
	if (parrillada_drive_get_medium (burner) == medium)
		return TRUE;

	/* The rule is:
	 * - blank media are our favourite since it avoids hiding/blanking data
	 * - take the medium that is closest to the size we need to burn
	 * - try to avoid a medium that is already our source for copying */
	/* NOTE: we could check if medium is bigger */
	if ((parrillada_burn_session_get_dest_media (session) & PARRILLADA_MEDIUM_BLANK)
	&&  (parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_BLANK))
		goto choose_closest_size;

	if (parrillada_burn_session_get_dest_media (session) & PARRILLADA_MEDIUM_BLANK)
		return TRUE;

	if (parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_BLANK) {
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));
		return TRUE;
	}

	/* In case it is the same source/same destination, choose it this new
	 * medium except if the medium is a file. */
	if (parrillada_burn_session_same_src_dest_drive (session)
	&& (parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_FILE) == 0) {
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));
		return TRUE;
	}

	/* Any possible medium is better than file even if it means copying to
	 * the same drive with a new medium later. */
	if (parrillada_drive_is_fake (burner)
	&& (parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_FILE) == 0) {
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));
		return TRUE;
	}


choose_closest_size:

	parrillada_burn_session_get_size (session, &session_blocks, NULL);
	medium_blocks = _get_medium_free_space (medium, session_blocks);

	if (medium_blocks - session_blocks <= 0)
		return TRUE;

	burner_blocks = _get_medium_free_space (parrillada_drive_get_medium (burner), session_blocks);
	if (burner_blocks - session_blocks <= 0)
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));
	else if (burner_blocks - session_blocks > medium_blocks - session_blocks)
		parrillada_burn_session_set_burner (session, parrillada_medium_get_drive (medium));

	return TRUE;
}

void
parrillada_dest_selection_choose_best (ParrilladaDestSelection *self)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	priv->user_changed = FALSE;
	if (!priv->session)
		return;

	if (!(parrillada_burn_session_get_flags (priv->session) & PARRILLADA_BURN_FLAG_MERGE)) {
		ParrilladaDrive *drive;

		/* Select the best fitting media */
		parrillada_medium_selection_foreach (PARRILLADA_MEDIUM_SELECTION (self),
						  parrillada_dest_selection_foreach_medium,
						  priv->session);

		drive = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (priv->session));
		if (drive)
			parrillada_medium_selection_set_active (PARRILLADA_MEDIUM_SELECTION (self),
							     parrillada_drive_get_medium (drive));
	}
}

void
parrillada_dest_selection_set_session (ParrilladaDestSelection *selection,
				    ParrilladaBurnSession *session)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (selection);

	if (priv->session)
		parrillada_dest_selection_clean (selection);

	if (!session)
		return;

	priv->session = g_object_ref (session);
	if (parrillada_burn_session_get_flags (session) & PARRILLADA_BURN_FLAG_MERGE) {
		ParrilladaDrive *drive;

		/* Prevent automatic resetting since a drive was set */
		priv->user_changed = TRUE;

		drive = parrillada_burn_session_get_burner (session);
		parrillada_medium_selection_set_active (PARRILLADA_MEDIUM_SELECTION (selection),
						     parrillada_drive_get_medium (drive));
	}
	else {
		ParrilladaDrive *burner;

		/* Only try to set a better drive if there isn't one already set */
		burner = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (priv->session));
		if (burner) {
			ParrilladaMedium *medium;

			/* Prevent automatic resetting since a drive was set */
			priv->user_changed = TRUE;

			medium = parrillada_drive_get_medium (burner);
			parrillada_medium_selection_set_active (PARRILLADA_MEDIUM_SELECTION (selection), medium);
		}
		else
			parrillada_dest_selection_choose_best (PARRILLADA_DEST_SELECTION (selection));
	}

	g_signal_connect (session,
			  "is-valid",
			  G_CALLBACK (parrillada_dest_selection_valid_session),
			  selection);
	g_signal_connect (session,
			  "output-changed",
			  G_CALLBACK (parrillada_dest_selection_output_changed),
			  selection);
	g_signal_connect (session,
			  "notify::flags",
			  G_CALLBACK (parrillada_dest_selection_flags_changed),
			  selection);

	parrillada_medium_selection_update_media_string (PARRILLADA_MEDIUM_SELECTION (selection));
}

static void
parrillada_dest_selection_set_property (GObject *object,
				     guint property_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	ParrilladaDestSelectionPrivate *priv;
	ParrilladaBurnSession *session;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION: /* Readable and only writable at creation time */
		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		session = g_value_get_object (value);
		parrillada_dest_selection_set_session (PARRILLADA_DEST_SELECTION (object), session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
parrillada_dest_selection_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_object_ref (priv->session);
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gchar *
parrillada_dest_selection_get_output_path (ParrilladaDestSelection *self)
{
	gchar *path = NULL;
	ParrilladaImageFormat format;
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (self);

	format = parrillada_burn_session_get_output_format (priv->session);
	switch (format) {
	case PARRILLADA_IMAGE_FORMAT_BIN:
		parrillada_burn_session_get_output (priv->session,
						 &path,
						 NULL);
		break;

	case PARRILLADA_IMAGE_FORMAT_CLONE:
	case PARRILLADA_IMAGE_FORMAT_CDRDAO:
	case PARRILLADA_IMAGE_FORMAT_CUE:
		parrillada_burn_session_get_output (priv->session,
						 NULL,
						 &path);
		break;

	default:
		break;
	}

	return path;
}

static gchar *
parrillada_dest_selection_format_medium_string (ParrilladaMediumSelection *selection,
					     ParrilladaMedium *medium)
{
	guint used;
	gchar *label;
	goffset blocks = 0;
	gchar *medium_name;
	gchar *size_string;
	ParrilladaMedia media;
	ParrilladaBurnFlag flags;
	goffset size_bytes = 0;
	goffset data_blocks = 0;
	goffset session_bytes = 0;
	ParrilladaTrackType *input = NULL;
	ParrilladaDestSelectionPrivate *priv;

	priv = PARRILLADA_DEST_SELECTION_PRIVATE (selection);

	if (!priv->session)
		return NULL;

	medium_name = parrillada_volume_get_name (PARRILLADA_VOLUME (medium));
	if (parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_FILE) {
		gchar *path;

		input = parrillada_track_type_new ();
		parrillada_burn_session_get_input_type (priv->session, input);

		/* There should be a special name for image in video context */
		if (parrillada_track_type_get_has_stream (input)
		&&  PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (input))) {
			ParrilladaImageFormat format;

			format = parrillada_burn_session_get_output_format (priv->session);
			if (format == PARRILLADA_IMAGE_FORMAT_CUE) {
				g_free (medium_name);
				if (parrillada_burn_session_tag_lookup_int (priv->session, PARRILLADA_VCD_TYPE) == PARRILLADA_SVCD)
					medium_name = g_strdup (_("SVCD image"));
				else
					medium_name = g_strdup (_("VCD image"));
			}
			else if (format == PARRILLADA_IMAGE_FORMAT_BIN) {
				g_free (medium_name);
				medium_name = g_strdup (_("Video DVD image"));
			}
		}
		parrillada_track_type_free (input);

		/* get the set path for the image file */
		path = parrillada_dest_selection_get_output_path (PARRILLADA_DEST_SELECTION (selection));
		if (!path)
			return medium_name;

		/* NOTE for translators: the first %s is medium_name ("File
		 * Image") and the second the path for the image file */
		label = g_strdup_printf (_("%s: \"%s\""),
					 medium_name,
					 path);
		g_free (medium_name);
		g_free (path);

		parrillada_medium_selection_update_used_space (PARRILLADA_MEDIUM_SELECTION (selection),
							    medium,
							    0);
		return label;
	}

	if (!priv->session) {
		g_free (medium_name);
		return NULL;
	}

	input = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (priv->session, input);
	if (parrillada_track_type_get_has_medium (input)) {
		ParrilladaMedium *src_medium;

		src_medium = parrillada_burn_session_get_src_medium (priv->session);
		if (src_medium == medium) {
			parrillada_track_type_free (input);

			/* Translators: this string is only used when the user
			 * wants to copy a disc using the same destination and
			 * source drive. It tells him that parrillada will use as
			 * destination disc a new one (once the source has been
			 * copied) which is to be inserted in the drive currently
			 * holding the source disc */
			label = g_strdup_printf (_("New disc in the burner holding the source disc"));
			g_free (medium_name);

			parrillada_medium_selection_update_used_space (PARRILLADA_MEDIUM_SELECTION (selection),
								    medium,
								    0);
			return label;
		}
	}

	media = parrillada_medium_get_status (medium);
	flags = parrillada_burn_session_get_flags (priv->session);
	parrillada_burn_session_get_size (priv->session,
				       &data_blocks,
				       &session_bytes);

	if (flags & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND))
		parrillada_medium_get_free_space (medium, &size_bytes, &blocks);
	else if (parrillada_burn_library_get_media_capabilities (media) & PARRILLADA_MEDIUM_REWRITABLE)
		parrillada_medium_get_capacity (medium, &size_bytes, &blocks);
	else
		parrillada_medium_get_free_space (medium, &size_bytes, &blocks);

	if (blocks) {
		used = data_blocks * 100 / blocks;
		if (data_blocks && !used)
			used = 1;

		used = MIN (100, used);
	}
	else
		used = 0;

	parrillada_medium_selection_update_used_space (PARRILLADA_MEDIUM_SELECTION (selection),
						    medium,
						    used);
	blocks -= data_blocks;
	if (blocks <= 0) {
		parrillada_track_type_free (input);

		/* NOTE for translators, the first %s is the medium name */
		label = g_strdup_printf (_("%s: not enough free space"), medium_name);
		g_free (medium_name);
		return label;
	}

	/* format the size */
	if (parrillada_track_type_get_has_stream (input)
	&& PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (input))) {
		guint64 free_time;

		/* This is an embarassing problem: this is an approximation
		 * based on the fact that 2 hours = 4.3GiB */
		free_time = size_bytes - session_bytes;
		free_time = free_time * 72000LL / 47LL;
		size_string = parrillada_units_get_time_string (free_time,
							     TRUE,
							     TRUE);
	}
	else if (parrillada_track_type_get_has_stream (input)
	|| (parrillada_track_type_get_has_medium (input)
	&& (parrillada_track_type_get_medium_type (input) & PARRILLADA_MEDIUM_HAS_AUDIO)))
		size_string = parrillada_units_get_time_string (PARRILLADA_SECTORS_TO_DURATION (blocks),
							     TRUE,
							     TRUE);
	else
		size_string = g_format_size_for_display (size_bytes - session_bytes);

	parrillada_track_type_free (input);

	/* NOTE for translators: the first %s is the medium name, the second %s
	 * is its available free space. "Free" here is the free space available. */
	label = g_strdup_printf (_("%s: %s of free space"), medium_name, size_string);
	g_free (medium_name);
	g_free (size_string);

	return label;
}

static void
parrillada_dest_selection_class_init (ParrilladaDestSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaMediumSelectionClass *medium_selection_class = PARRILLADA_MEDIUM_SELECTION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDestSelectionPrivate));

	object_class->finalize = parrillada_dest_selection_finalize;
	object_class->set_property = parrillada_dest_selection_set_property;
	object_class->get_property = parrillada_dest_selection_get_property;

	medium_selection_class->format_medium_string = parrillada_dest_selection_format_medium_string;
	medium_selection_class->medium_changed = parrillada_dest_selection_medium_changed;
	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      PARRILLADA_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE));
}

GtkWidget *
parrillada_dest_selection_new (ParrilladaBurnSession *session)
{
	return g_object_new (PARRILLADA_TYPE_DEST_SELECTION,
			     "session", session,
			     NULL);
}

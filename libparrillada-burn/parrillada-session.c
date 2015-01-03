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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "parrillada-session.h"
#include "parrillada-session-helper.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "libparrillada-marshal.h"
#include "burn-image-format.h"
#include "parrillada-track-type-private.h"

#include "parrillada-medium.h"
#include "parrillada-drive.h"
#include "parrillada-drive-priv.h"
#include "parrillada-medium-monitor.h"

#include "parrillada-tags.h"
#include "parrillada-track.h"
#include "parrillada-track-disc.h"

G_DEFINE_TYPE (ParrilladaBurnSession, parrillada_burn_session, G_TYPE_OBJECT);
#define PARRILLADA_BURN_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_BURN_SESSION, ParrilladaBurnSessionPrivate))

/**
 * SECTION:parrillada-session
 * @short_description: Store parameters when burning and blanking
 * @see_also: #ParrilladaBurn, #ParrilladaSessionCfg
 * @include: parrillada-session.h
 *
 * This object stores all parameters for all operations performed by #ParrilladaBurn such as
 * burning, blanking and checksuming. To have this object configured automatically see
 * #ParrilladaSessionCfg.
 **/

struct _ParrilladaSessionSetting {
	ParrilladaDrive *burner;

	/**
	 * Used when outputting an image instead of burning
	 */
	ParrilladaImageFormat format;
	gchar *image;
	gchar *toc;

	/**
	 * Used when burning
	 */
	gchar *label;
	guint64 rate;

	ParrilladaBurnFlag flags;
};
typedef struct _ParrilladaSessionSetting ParrilladaSessionSetting;

struct _ParrilladaBurnSessionPrivate {
	int session;
	gchar *session_path;

	gchar *tmpdir;
	GSList *tmpfiles;

	ParrilladaSessionSetting settings [1];
	GSList *pile_settings;

	GHashTable *tags;

	guint dest_added_sig;
	guint dest_removed_sig;

	GSList *tracks;
	GSList *pile_tracks;

	guint strict_checks:1;
};
typedef struct _ParrilladaBurnSessionPrivate ParrilladaBurnSessionPrivate;

#define PARRILLADA_BURN_SESSION_WRITE_TO_DISC(priv)	(priv->settings->burner &&			\
							!parrillada_drive_is_fake (priv->settings->burner))
#define PARRILLADA_BURN_SESSION_WRITE_TO_FILE(priv)	(priv->settings->burner &&			\
							 parrillada_drive_is_fake (priv->settings->burner))
#define PARRILLADA_STR_EQUAL(a, b)	((!(a) && !(b)) || ((a) && (b) && !strcmp ((a), (b))))

typedef enum {
	TAG_CHANGED_SIGNAL,
	TRACK_ADDED_SIGNAL,
	TRACK_REMOVED_SIGNAL,
	TRACK_CHANGED_SIGNAL,
	OUTPUT_CHANGED_SIGNAL,
	LAST_SIGNAL
} ParrilladaBurnSessionSignalType;

static guint parrillada_burn_session_signals [LAST_SIGNAL] = { 0 };

enum {
	PROP_0,
	PROP_TMPDIR,
	PROP_RATE,
	PROP_FLAGS
};

static GObjectClass *parent_class = NULL;

static void
parrillada_session_settings_clean (ParrilladaSessionSetting *settings)
{
	if (settings->image)
		g_free (settings->image);

	if (settings->toc)
		g_free (settings->toc);

	if (settings->label)
		g_free (settings->label);

	if (settings->burner)
		g_object_unref (settings->burner);

	memset (settings, 0, sizeof (ParrilladaSessionSetting));
}

static void
parrillada_session_settings_copy (ParrilladaSessionSetting *dest,
			       ParrilladaSessionSetting *original)
{
	parrillada_session_settings_clean (dest);

	memcpy (dest, original, sizeof (ParrilladaSessionSetting));

	g_object_ref (dest->burner);
	dest->image = g_strdup (original->image);
	dest->toc = g_strdup (original->toc);
	dest->label = g_strdup (original->label);
}

static void
parrillada_session_settings_free (ParrilladaSessionSetting *settings)
{
	parrillada_session_settings_clean (settings);
	g_free (settings);
}

static void
parrillada_burn_session_track_changed (ParrilladaTrack *track,
				    ParrilladaBurnSession *self)
{
	g_signal_emit (self,
		       parrillada_burn_session_signals [TRACK_CHANGED_SIGNAL],
		       0,
		       track);
}

static void
parrillada_burn_session_start_track_monitoring (ParrilladaBurnSession *self,
					     ParrilladaTrack *track)
{
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	g_signal_connect (track,
			  "changed",
			  G_CALLBACK (parrillada_burn_session_track_changed),
			  self);
}

static void
parrillada_burn_session_stop_tracks_monitoring (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;
	GSList *iter;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;

		track = iter->data;
		g_signal_handlers_disconnect_by_func (track,
						      parrillada_burn_session_track_changed,
						      self);
	}
}

static void
parrillada_burn_session_free_tracks (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;
	GSList *iter;
	GSList *next;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	parrillada_burn_session_stop_tracks_monitoring (self);

	for (iter = priv->tracks; iter; iter = next) {
		ParrilladaTrack *track;

		track = iter->data;
		next = iter->next;
		priv->tracks = g_slist_remove (priv->tracks, track);
		g_signal_emit (self,
			       parrillada_burn_session_signals [TRACK_REMOVED_SIGNAL],
			       0,
			       track,
			       0);
		g_object_unref (track);
	}
}

/**
 * parrillada_burn_session_set_strict_support:
 * @session: a #ParrilladaBurnSession.
 * @flags: a #ParrilladaSessionCheckFlags
 *
 * For the following functions:
 * parrillada_burn_session_supported ()
 * parrillada_burn_session_input_supported ()
 * parrillada_burn_session_output_supported ()
 * parrillada_burn_session_can_blank ()
 * this function sets whether these functions will
 * ignore the plugins with errors (%TRUE).
 */

void
parrillada_burn_session_set_strict_support (ParrilladaBurnSession *session,
                                         gboolean strict_checks)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (session));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);
	priv->strict_checks = strict_checks;
}

/**
 * parrillada_burn_session_get_strict_support:
 * @session: a #ParrilladaBurnSession.
 *
 * For the following functions:
 * parrillada_burn_session_can_burn ()
 * parrillada_burn_session_input_supported ()
 * parrillada_burn_session_output_supported ()
 * parrillada_burn_session_can_blank ()
 * this function gets whether the checks will 
 * ignore the plugins with errors (return %TRUE).
 *
 * Returns:  #gboolean
 */

gboolean
parrillada_burn_session_get_strict_support (ParrilladaBurnSession *session)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), FALSE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);
	return priv->strict_checks;
}

/**
 * parrillada_burn_session_add_track:
 * @session: a #ParrilladaBurnSession.
 * @new_track: (allow-none): a #ParrilladaTrack or NULL.
 * @sibling: (allow-none): a #ParrilladaTrack or NULL.
 *
 * Inserts a new track after @sibling or appended if @sibling is NULL. If @track is NULL then all tracks
 * already in @session will be removed.
 * NOTE: if there are already #ParrilladaTrack objects inserted in the session and if they are not
 * of the same type as @new_track then they will be removed before the insertion of @new_track.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if the track was successfully inserted or PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_add_track (ParrilladaBurnSession *self,
				ParrilladaTrack *new_track,
				ParrilladaTrack *sibling)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* Prevent adding the same tracks several times */
	if (g_slist_find (priv->tracks, new_track)) {
		PARRILLADA_BURN_LOG ("Tried to add the same track multiple times");
		return PARRILLADA_BURN_OK;
	}

	if (!new_track) {
		if (!priv->tracks)
			return PARRILLADA_BURN_OK;

		parrillada_burn_session_free_tracks (self);
		return PARRILLADA_BURN_OK;
	}

	g_object_ref (new_track);
	if (!priv->tracks) {
		/* we only need to emit the signal here since if there are
		 * multiple tracks they must be exactly of the same type */
		priv->tracks = g_slist_prepend (NULL, new_track);
		parrillada_burn_session_start_track_monitoring (self, new_track);

		/* if (!parrillada_track_type_equal (priv->input, &new_type)) */
		g_signal_emit (self,
			       parrillada_burn_session_signals [TRACK_ADDED_SIGNAL],
			       0,
			       new_track);

		return PARRILLADA_BURN_OK;
	}

	/* if there is already a track, then we replace it on condition that it
	 * is not AUDIO (only one type allowed to have several tracks) */
	if (!PARRILLADA_IS_TRACK_STREAM (new_track)
	||  !PARRILLADA_IS_TRACK_STREAM (priv->tracks->data))
		parrillada_burn_session_free_tracks (self);

	parrillada_burn_session_start_track_monitoring (self, new_track);
	if (sibling) {
		GSList *sibling_node;

		sibling_node = g_slist_find (priv->tracks, sibling);
		priv->tracks = g_slist_insert_before (priv->tracks,
						      sibling_node,
						      new_track);
	}
	else
		priv->tracks = g_slist_append (priv->tracks, new_track);

	g_signal_emit (self,
		       parrillada_burn_session_signals [TRACK_ADDED_SIGNAL],
		       0,
		       new_track);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_move_track:
 * @session: a #ParrilladaBurnSession.
 * @track: a #ParrilladaTrack.
 * @sibling: (allow-none): a #ParrilladaTrack or NULL.
 *
 * Moves @track after @sibling; if @sibling is NULL then it is appended.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if the track was successfully moved or PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_move_track (ParrilladaBurnSession *session,
				 ParrilladaTrack *track,
				 ParrilladaTrack *sibling)
{
	ParrilladaBurnSessionPrivate *priv;
	guint former_position;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);

	/* Find the track, remove it */
	former_position = g_slist_index (priv->tracks, track);
	priv->tracks = g_slist_remove (priv->tracks, track);
	g_signal_emit (session,
		       parrillada_burn_session_signals [TRACK_REMOVED_SIGNAL],
		       0,
		       track,
		       former_position);

	/* Re-add it */
	if (sibling) {
		GSList *sibling_node;

		sibling_node = g_slist_find (priv->tracks, sibling);
		priv->tracks = g_slist_insert_before (priv->tracks,
						      sibling_node,
						      track);
	}
	else
		priv->tracks = g_slist_append (priv->tracks, track);

	g_signal_emit (session,
		       parrillada_burn_session_signals [TRACK_ADDED_SIGNAL],
		       0,
		       track);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_remove_track:
 * @session: a #ParrilladaBurnSession
 * @track: a #ParrilladaTrack
 *
 * Removes @track from @session.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if the track was successfully removed or PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_remove_track (ParrilladaBurnSession *session,
				   ParrilladaTrack *track)
{
	ParrilladaBurnSessionPrivate *priv;
	guint former_position;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);

	/* Find the track, remove it */
	former_position = g_slist_index (priv->tracks, track);
	priv->tracks = g_slist_remove (priv->tracks, track);
	g_signal_handlers_disconnect_by_func (track,
					      parrillada_burn_session_track_changed,
					      session);

	g_signal_emit (session,
		       parrillada_burn_session_signals [TRACK_REMOVED_SIGNAL],
		       0,
		       track,
		       former_position);

	g_object_unref (track);
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_tracks:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the list of #ParrilladaTrack added to @session.
 *
 * Return value: (element-type ParrilladaBurn.Track) (transfer none): a #GSList or #ParrilladaTrack object. Do not unref the objects in the list nor destroy the list.
 *
 **/

GSList *
parrillada_burn_session_get_tracks (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	return priv->tracks;
}

/**
 * parrillada_burn_session_get_status:
 * @session: a #ParrilladaBurnSession
 * @status: a #ParrilladaTrackStatus
 *
 * Sets @status to reflect whether @session is ready to be used.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful
 * PARRILLADA_BURN_NOT_READY if @track needs more time for processing
 * PARRILLADA_BURN_ERR if something is wrong or if it is empty
 **/

ParrilladaBurnResult
parrillada_burn_session_get_status (ParrilladaBurnSession *session,
				 ParrilladaStatus *status)
{
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaStatus *track_status;
	gdouble num_tracks = 0.0;
	guint not_ready = 0;
	gdouble done = -1.0;
	GSList *iter;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_TRACK_TYPE_NONE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);
	if (!priv->tracks)
		return PARRILLADA_BURN_ERR;

	track_status = parrillada_status_new ();

	if (priv->settings->burner && parrillada_drive_probing (priv->settings->burner)) {
		PARRILLADA_BURN_LOG ("Drive not ready yet");
		parrillada_status_set_not_ready (status, -1, NULL);
		return PARRILLADA_BURN_NOT_READY;
	}

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;
		ParrilladaBurnResult result;

		track = iter->data;
		result = parrillada_track_get_status (track, track_status);
		num_tracks ++;

		if (result == PARRILLADA_BURN_NOT_READY || result == PARRILLADA_BURN_RUNNING)
			not_ready ++;
		else if (result != PARRILLADA_BURN_OK) {
			g_object_unref (track_status);
			return parrillada_track_get_status (track, status);
		}

		if (parrillada_status_get_progress (track_status) != -1.0)
			done += parrillada_status_get_progress (track_status);
	}
	g_object_unref (track_status);

	if (not_ready > 0) {
		if (status) {
			if (done != -1.0)
				parrillada_status_set_not_ready (status,
							      (gdouble) ((gdouble) (done) / (gdouble) (num_tracks)),
							      NULL);
			else
				parrillada_status_set_not_ready (status, -1.0, NULL);
		}
		return PARRILLADA_BURN_NOT_READY;
	}

	if (status)
		parrillada_status_set_completed (status);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_size:
 * @session: a #ParrilladaBurnSession
 * @blocks: a #goffset or NULL
 * @bytes: a #goffset or NULL
 *
 * Returns the size of the data contained by @session in bytes or in sectors
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful
 * PARRILLADA_BURN_NOT_READY if @track needs more time for processing the size
 * PARRILLADA_BURN_ERR if something is wrong or if it is empty
 **/

ParrilladaBurnResult
parrillada_burn_session_get_size (ParrilladaBurnSession *session,
			       goffset *blocks,
			       goffset *bytes)
{
	ParrilladaBurnSessionPrivate *priv;
	gsize session_blocks = 0;
	gsize session_bytes = 0;
	GSList *iter;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_TRACK_TYPE_NONE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (session);
	if (!priv->tracks)
		return PARRILLADA_BURN_ERR;

	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaBurnResult res;
		ParrilladaTrack *track;
		goffset track_blocks;
		goffset track_bytes;

		track = iter->data;
		track_blocks = 0;
		track_bytes = 0;

		res = parrillada_track_get_size (track, &track_blocks, &track_bytes);

		/* That way we get the size even if the track has not completed
		 * what's it's doing which allows to show progress */
		if (res != PARRILLADA_BURN_OK && res != PARRILLADA_BURN_NOT_READY)
			continue;

		session_blocks += track_blocks;
		session_bytes += track_bytes;
	}

	if (blocks)
		*blocks = session_blocks;
	if (bytes)
		*bytes = session_bytes;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_input_type:
 * @session: a #ParrilladaBurnSession
 * @type: a #ParrilladaTrackType or NULL
 *
 * Sets @type to reflect the type of data contained in @session
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful
 **/

ParrilladaBurnResult
parrillada_burn_session_get_input_type (ParrilladaBurnSession *self,
				     ParrilladaTrackType *type)
{
	GSList *iter;
	ParrilladaStreamFormat format;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* there can be many tracks (in case of audio) but they must be
	 * all of the same kind for the moment. Yet their subtypes may
	 * be different. */
	format = PARRILLADA_AUDIO_FORMAT_NONE;
	for (iter = priv->tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;

		track = iter->data;
		parrillada_track_get_track_type (track, type);
		if (parrillada_track_type_get_has_stream (type))
			format |= parrillada_track_type_get_stream_format (type);
	}

	if (parrillada_track_type_get_has_stream (type))
		parrillada_track_type_set_image_format (type, format);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_burn_session_dest_media_added (ParrilladaDrive *drive,
				       ParrilladaMedium *medium,
				       ParrilladaBurnSession *self)
{
	/* No medium before */
	g_signal_emit (self,
		       parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0,
		       NULL);
}

static void
parrillada_burn_session_dest_media_removed (ParrilladaDrive *drive,
					 ParrilladaMedium *medium,
					 ParrilladaBurnSession *self)
{
	g_signal_emit (self,
		       parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0,
		       medium);
}

/**
 * parrillada_burn_session_set_burner:
 * @session: a #ParrilladaBurnSession
 * @drive: a #ParrilladaDrive
 *
 * Sets the #ParrilladaDrive that should be used to burn the session contents.
 *
 **/

void
parrillada_burn_session_set_burner (ParrilladaBurnSession *self,
				 ParrilladaDrive *drive)
{
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaMedium *former;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (drive == priv->settings->burner)
		return;

	former = parrillada_drive_get_medium (priv->settings->burner);
	if (former)
		g_object_ref (former);

	/* If there was no drive before no need for a changing signal */
	if (priv->settings->burner) {
		if (priv->dest_added_sig) {
			g_signal_handler_disconnect (priv->settings->burner,
						     priv->dest_added_sig);
			priv->dest_added_sig = 0;
		}

		if (priv->dest_removed_sig) {
			g_signal_handler_disconnect (priv->settings->burner,
						     priv->dest_removed_sig);
			priv->dest_removed_sig = 0;	
		}

		g_object_unref (priv->settings->burner);
	}

	if (drive) {
		priv->dest_added_sig = g_signal_connect (drive,
							 "medium-added",
							 G_CALLBACK (parrillada_burn_session_dest_media_added),
							 self);
		priv->dest_removed_sig = g_signal_connect (drive,
							   "medium-removed",
							   G_CALLBACK (parrillada_burn_session_dest_media_removed),
							   self);
		g_object_ref (drive);
	}

	priv->settings->burner = drive;

	g_signal_emit (self,
		       parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0,
		       former);

	if (former)
		g_object_unref (former);
}

/**
 * parrillada_burn_session_get_burner:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the #ParrilladaDrive that should be used to burn the session contents.
 *
 * Return value: (transfer none) (allow-none): a #ParrilladaDrive or NULL. Do not unref after use.
 **/

ParrilladaDrive *
parrillada_burn_session_get_burner (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->settings->burner;
}

/**
 * parrillada_burn_session_set_rate:
 * @session: a #ParrilladaBurnSession
 * @rate: a #guint64
 *
 * Sets the speed at which the medium should be burnt.
 * NOTE: before using this function a #ParrilladaDrive should have been
 * set with parrillada_burn_session_set_burner ().
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful; PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_set_rate (ParrilladaBurnSession *self,
                               guint64 rate)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!PARRILLADA_BURN_SESSION_WRITE_TO_DISC (priv))
		return PARRILLADA_BURN_ERR;

	priv->settings->rate = rate;
	g_object_notify (G_OBJECT (self), "speed");
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_rate:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the speed at which the medium should be burnt.
 * NOTE: before using this function a #ParrilladaDrive should have been
 * set with parrillada_burn_session_set_burner ().
 *
 * Return value: a #guint64 or 0.
 **/

guint64
parrillada_burn_session_get_rate (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaMedium *medium;
	gint64 max_rate;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), 0);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!PARRILLADA_BURN_SESSION_WRITE_TO_DISC (priv))
		return 0;

	medium = parrillada_drive_get_medium (priv->settings->burner);
	if (!medium)
		return 0;

	max_rate = parrillada_medium_get_max_write_speed (medium);
	if (priv->settings->rate <= 0)
		return max_rate;
	else
		return MIN (max_rate, priv->settings->rate);
}

/**
 * parrillada_burn_session_get_output_type:
 * @session: a #ParrilladaBurnSession
 * @output: a #ParrilladaTrackType or NULL
 *
 * This function returns the type of output set for the session.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful; PARRILLADA_BURN_NOT_READY if no setting has been set; PARRILLADA_BURN_ERR otherwise.
 **/
ParrilladaBurnResult
parrillada_burn_session_get_output_type (ParrilladaBurnSession *self,
                                      ParrilladaTrackType *output)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!priv->settings->burner)
		return PARRILLADA_BURN_NOT_READY;

	if (parrillada_drive_is_fake (priv->settings->burner)) {
		parrillada_track_type_set_has_image (output);
		parrillada_track_type_set_image_format (output, parrillada_burn_session_get_output_format (self));
	}
	else {
		parrillada_track_type_set_has_medium (output);
		parrillada_track_type_set_medium_type (output, parrillada_medium_get_status (parrillada_drive_get_medium (priv->settings->burner)));
	}

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_output:
 * @session: a #ParrilladaBurnSession.
 * @image: (allow-none) (out): a #gchar to store the image path or NULL.
 * @toc: (allow-none) (out): a #gchar to store the toc path or NULL.
 *
 * When the contents of @session should be written to a
 * file then this function returns the image path (and if
 * necessary a toc path).
 * @image and @toc should be freed if not used anymore.
 *
 * NOTE: before using this function a #ParrilladaDrive should have been
 * set with parrillada_burn_session_set_burner () and it should be the
 * fake drive (see parrillada_drive_is_fake ()).
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful; PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_get_output (ParrilladaBurnSession *self,
				 gchar **image,
				 gchar **toc)
{
	ParrilladaBurnSessionClass *klass;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_IMAGE_FORMAT_NONE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv)) {
		PARRILLADA_BURN_LOG ("no file disc");
		return PARRILLADA_BURN_ERR;
	}

	klass = PARRILLADA_BURN_SESSION_GET_CLASS (self);
	return klass->get_output_path (self, image, toc);
}


static ParrilladaBurnResult
parrillada_burn_session_get_output_path_real (ParrilladaBurnSession *self,
					   gchar **image_ret,
					   gchar **toc_ret)
{
	gchar *toc = NULL;
	gchar *image = NULL;
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	image = g_strdup (priv->settings->image);
	toc = g_strdup (priv->settings->toc);
	if (!image && !toc)
		return PARRILLADA_BURN_ERR;

	if (image_ret) {
		/* output paths were set so test them and returns them if OK */
		if (!image && toc) {
			gchar *complement;
			ParrilladaImageFormat format;

			/* get the cuesheet complement */
			format = parrillada_burn_session_get_output_format (self);
			complement = parrillada_image_format_get_complement (format, toc);
			if (!complement) {
				PARRILLADA_BURN_LOG ("no output specified");
				g_free (toc);
				return PARRILLADA_BURN_ERR;
			}

			*image_ret = complement;
		}
		else if (image)
			*image_ret = image;
		else {
			PARRILLADA_BURN_LOG ("no output specified");
			return PARRILLADA_BURN_ERR;
		}
	}
	else
		g_free (image);

	if (toc_ret)
		*toc_ret = toc;
	else
		g_free (toc);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_output_format:
 * @session: a #ParrilladaBurnSession
 *
 * When the contents of @session should be written to a
 * file then this function returns the format of the image to write.
 *
 * NOTE: before using this function a #ParrilladaDrive should have been
 * set with parrillada_burn_session_set_burner () and it should be the
 * fake drive (see parrillada_drive_is_fake ()).
 *
 * Return value: a #ParrilladaImageFormat. The format of the image to be written.
 **/

ParrilladaImageFormat
parrillada_burn_session_get_output_format (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionClass *klass;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_IMAGE_FORMAT_NONE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv))
		return PARRILLADA_IMAGE_FORMAT_NONE;

	klass = PARRILLADA_BURN_SESSION_GET_CLASS (self);
	return klass->get_output_format (self);
}

static ParrilladaImageFormat
parrillada_burn_session_get_output_format_real (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->settings->format;
}

/**
 * This function allows to tell where we should write the image. Depending on
 * the type of image it can be a toc (cue) or the path of the image (all others)
 */

static void
parrillada_burn_session_set_image_output_real (ParrilladaBurnSession *self,
					    ParrilladaImageFormat format,
					    const gchar *image,
					    const gchar *toc)
{
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (priv->settings->image)
		g_free (priv->settings->image);

	if (image)
		priv->settings->image = g_strdup (image);
	else
		priv->settings->image = NULL;

	if (priv->settings->toc)
		g_free (priv->settings->toc);

	if (toc)
		priv->settings->toc = g_strdup (toc);
	else
		priv->settings->toc = NULL;

	priv->settings->format = format;
}

static void
parrillada_burn_session_set_fake_drive (ParrilladaBurnSession *self)
{
	ParrilladaMediumMonitor *monitor;
	ParrilladaDrive *drive;
	GSList *list;

	/* NOTE: changing/changed signals are handled in
	 * set_burner (). */
	monitor = parrillada_medium_monitor_get_default ();
	list = parrillada_medium_monitor_get_media (monitor, PARRILLADA_MEDIA_TYPE_FILE);
	drive = parrillada_medium_get_drive (list->data);
	parrillada_burn_session_set_burner (self, drive);
	g_object_unref (monitor);
	g_slist_free (list);
}

static ParrilladaBurnResult
parrillada_burn_session_set_output_image_real (ParrilladaBurnSession *self,
					    ParrilladaImageFormat format,
					    const gchar *image,
					    const gchar *toc)
{
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (priv->settings->format == format
	&&  PARRILLADA_STR_EQUAL (image, priv->settings->image)
	&&  PARRILLADA_STR_EQUAL (toc, priv->settings->toc)) {
		if (!PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv))
			parrillada_burn_session_set_fake_drive (self);

		return PARRILLADA_BURN_OK;
	}

	parrillada_burn_session_set_image_output_real (self, format, image, toc);
	if (PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv))
		g_signal_emit (self,
			       parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
			       0,
			       parrillada_drive_get_medium (priv->settings->burner));
	else
		parrillada_burn_session_set_fake_drive (self);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_set_image_output_format:
 * @session: a #ParrilladaBurnSession
 * @format: a #ParrilladaImageFormat
 *
 * When the contents of @session should be written to a
 * file, this function sets format of the image that will be
 * created.
 *
 * NOTE: after a call to this function the #ParrilladaDrive for
 * @session will be the fake #ParrilladaDrive.
 *
 * Since 2.29.0
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it was successfully set;
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_set_image_output_format (ParrilladaBurnSession *self,
					    ParrilladaImageFormat format)
{
	ParrilladaBurnSessionClass *klass;
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaBurnResult res;
	gchar *image;
	gchar *toc;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	klass = PARRILLADA_BURN_SESSION_GET_CLASS (self);

	image = g_strdup (priv->settings->image);
	toc = g_strdup (priv->settings->toc);
	res = klass->set_output_image (self, format, image, toc);
	g_free (image);
	g_free (toc);

	return res;
}

/**
 * parrillada_burn_session_set_image_output_full:
 * @session: a #ParrilladaBurnSession.
 * @format: a #ParrilladaImageFormat.
 * @image: (allow-none): a #gchar or NULL.
 * @toc: (allow-none): a #gchar or NULL.
 *
 * When the contents of @session should be written to a
 * file, this function sets the different parameters of this
 * image like its path (and the one of the associated toc if
 * necessary) and its format.
 *
 * NOTE: after a call to this function the #ParrilladaDrive for
 * @session will be the fake #ParrilladaDrive.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it was successfully set;
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_set_image_output_full (ParrilladaBurnSession *self,
					    ParrilladaImageFormat format,
					    const gchar *image,
					    const gchar *toc)
{
	ParrilladaBurnSessionClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	klass = PARRILLADA_BURN_SESSION_GET_CLASS (self);
	return klass->set_output_image (self, format, image, toc);
}

/**
 * parrillada_burn_session_set_tmpdir:
 * @session: a #ParrilladaBurnSession
 * @path: a #gchar or NULL
 *
 * Sets the path of the directory in which to write temporary directories and files.
 * If set to NULL then the result of g_get_tmp_dir () will be used.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it was successfully set;
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_set_tmpdir (ParrilladaBurnSession *self,
				 const gchar *path)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!g_strcmp0 (priv->tmpdir, path))
		return PARRILLADA_BURN_OK;

	if (!path) {
		g_free (priv->tmpdir);
		priv->tmpdir = NULL;
		g_object_notify (G_OBJECT (self), "tmpdir");
		return PARRILLADA_BURN_OK;
	}

	if (!g_str_has_prefix (path, G_DIR_SEPARATOR_S))
		return PARRILLADA_BURN_ERR;

	g_free (priv->tmpdir);
	priv->tmpdir = g_strdup (path);
	g_object_notify (G_OBJECT (self), "tmpdir");

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_tmpdir:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the path of the directory in which to write temporary directories and files.
 *
 * Return value: a #gchar. The path to the temporary directory.
 **/

const gchar *
parrillada_burn_session_get_tmpdir (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->tmpdir? priv->tmpdir:g_get_tmp_dir ();
}

/**
 * parrillada_burn_session_get_tmp_dir:
 * @session: a #ParrilladaBurnSession
 * @path: a #gchar or NULL
 * @error: a #GError
 *
 * Creates then returns (in @path) a temporary directory at the proper location.
 * On error, @error is set appropriately.
 * See parrillada_burn_session_set_tmpdir ().
 * This function is used internally and is not public API.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it was successfully set;
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_get_tmp_dir (ParrilladaBurnSession *self,
				  gchar **path,
				  GError **error)
{
	gchar *tmp;
	const gchar *tmpdir;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* create a working directory in tmp */
	tmpdir = priv->tmpdir ?
		 priv->tmpdir :
		 g_get_tmp_dir ();

	tmp = g_build_path (G_DIR_SEPARATOR_S,
			    tmpdir,
			    PARRILLADA_BURN_TMP_FILE_NAME,
			    NULL);

	*path = mkdtemp (tmp);
	if (*path == NULL) {
                int errsv = errno;

		PARRILLADA_BURN_LOG ("Impossible to create tmp directory");
		g_free (tmp);
		if (errsv != EACCES)
			g_set_error (error, 
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_TMP_DIRECTORY,
				     "%s",
				     g_strerror (errsv));
		else
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_TMP_DIRECTORY,
				     _("You do not have the required permission to write at this location"));
		return PARRILLADA_BURN_ERR;
	}

	/* this must be removed when session is completly unreffed */
	priv->tmpfiles = g_slist_prepend (priv->tmpfiles, g_strdup (tmp));

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_get_tmp_file:
 * @session: a #ParrilladaBurnSession
 * @suffix: a #gchar
 * @path: a #gchar or NULL
 * @error: a #GError
 *
 * Creates then returns (in @path) a temporary file at the proper location. Its name
 * will be appended with suffix.
 * On error, @error is set appropriately.
 * See parrillada_burn_session_set_tmpdir ().
 * This function is used internally and is not public API.
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if it was successfully set;
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_get_tmp_file (ParrilladaBurnSession *self,
				   const gchar *suffix,
				   gchar **path,
				   GError **error)
{
	ParrilladaBurnSessionPrivate *priv;
	const gchar *tmpdir;
	gchar *name;
	gchar *tmp;
	int fd;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!path)
		return PARRILLADA_BURN_OK;

	/* takes care of the output file */
	tmpdir = priv->tmpdir ?
		 priv->tmpdir :
		 g_get_tmp_dir ();

	name = g_strconcat (PARRILLADA_BURN_TMP_FILE_NAME, suffix, NULL);
	tmp = g_build_path (G_DIR_SEPARATOR_S,
			    tmpdir,
			    name,
			    NULL);
	g_free (name);

	fd = g_mkstemp (tmp);
	if (fd == -1) {
                int errsv = errno;

		PARRILLADA_BURN_LOG ("Impossible to create tmp file %s", tmp);

		g_free (tmp);
		if (errsv != EACCES)
			g_set_error (error, 
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_TMP_DIRECTORY,
				     "%s",
				     g_strerror (errsv));
		else
			g_set_error (error, 
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_TMP_DIRECTORY,
				     _("You do not have the required permission to write at this location"));

		return PARRILLADA_BURN_ERR;
	}

	/* this must be removed when session is completly unreffed */
	priv->tmpfiles = g_slist_prepend (priv->tmpfiles,
					  g_strdup (tmp));

	close (fd);
	*path = tmp;
	return PARRILLADA_BURN_OK;
}

static gchar *
parrillada_burn_session_get_image_complement (ParrilladaBurnSession *self,
					   ParrilladaImageFormat format,
					   const gchar *path)
{
	gchar *retval = NULL;
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (format == PARRILLADA_IMAGE_FORMAT_CLONE)
		retval = g_strdup_printf ("%s.toc", path);
	else if (format == PARRILLADA_IMAGE_FORMAT_CUE) {
		if (g_str_has_suffix (path, ".bin"))
			retval = g_strdup_printf ("%.*scue",
						  (int) strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.cue", path);
	}
	else if (format == PARRILLADA_IMAGE_FORMAT_CDRDAO) {
		if (g_str_has_suffix (path, ".bin"))
			retval = g_strdup_printf ("%.*stoc",
						  (int) strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.toc", path);
	}
	else
		retval = NULL;

	return retval;
}

/**
 * This function is used internally and is not public API
 */

ParrilladaBurnResult
parrillada_burn_session_get_tmp_image (ParrilladaBurnSession *self,
				    ParrilladaImageFormat format,
				    gchar **image,
				    gchar **toc,
				    GError **error)
{
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaBurnResult result;
	gchar *complement = NULL;
	gchar *path = NULL;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* Image tmp file */
	result = parrillada_burn_session_get_tmp_file (self,
						    (format == PARRILLADA_IMAGE_FORMAT_CLONE)? NULL:".bin",
						    &path,
						    error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	if (format != PARRILLADA_IMAGE_FORMAT_BIN) {
		/* toc tmp file */
		complement = parrillada_burn_session_get_image_complement (self, format, path);
		if (complement) {
			/* That shouldn't happen ... */
			if (g_file_test (complement, G_FILE_TEST_EXISTS)) {
				g_free (complement);
				return PARRILLADA_BURN_ERR;
			}
		}
	}

	if (complement)
		priv->tmpfiles = g_slist_prepend (priv->tmpfiles,
						  g_strdup (complement));

	if (image)
		*image = path;
	else
		g_free (path);

	if (toc)
		*toc = complement;
	else
		g_free (complement);

	return PARRILLADA_BURN_OK;
}

/**
 * used to modify session flags.
 */

/**
 * parrillada_burn_session_set_flags:
 * @session: a #ParrilladaBurnSession
 * @flags: a #ParrilladaBurnFlag
 *
 * Replaces the current flags set in @session with @flags.
 *
 **/

void
parrillada_burn_session_set_flags (ParrilladaBurnSession *self,
			        ParrilladaBurnFlag flags)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (priv->settings->flags == flags)
		return;

	priv->settings->flags = flags;
	g_object_notify (G_OBJECT (self), "flags");
}

/**
 * parrillada_burn_session_add_flag:
 * @session: a #ParrilladaBurnSession
 * @flags: a #ParrilladaBurnFlag
 *
 * Merges the current flags set in @session with @flags.
 *
 **/

void
parrillada_burn_session_add_flag (ParrilladaBurnSession *self,
			       ParrilladaBurnFlag flag)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if ((priv->settings->flags & flag) == flag)
		return;

	priv->settings->flags |= flag;
	g_object_notify (G_OBJECT (self), "flags");
}

/**
 * parrillada_burn_session_remove_flag:
 * @session: a #ParrilladaBurnSession
 * @flags: a #ParrilladaBurnFlag
 *
 * Removes @flags from the current flags set for @session.
 *
 **/

void
parrillada_burn_session_remove_flag (ParrilladaBurnSession *self,
				  ParrilladaBurnFlag flag)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if ((priv->settings->flags & flag) == 0)
		return;

	priv->settings->flags &= ~flag;
	g_object_notify (G_OBJECT (self), "flags");
}

/**
 * parrillada_burn_session_get_flags:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the current flags set for @session.
 *
 * Return value: a #ParrilladaBurnFlag.
 **/

ParrilladaBurnFlag
parrillada_burn_session_get_flags (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->settings->flags;
}

/**
 * Used to set the label or the title of an album. 
 */

/**
 * parrillada_burn_session_set_label:
 * @session: a #ParrilladaBurnSession
 * @label: (allow-none): a #gchar or %NULL
 *
 * Sets the label for @session.
 *
 **/

void
parrillada_burn_session_set_label (ParrilladaBurnSession *self,
				const gchar *label)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (priv->settings->label)
		g_free (priv->settings->label);

	priv->settings->label = NULL;

	if (label) {
		if (strlen (label) > 32) {
			const gchar *delim;
			gchar *next_char;

			/* find last possible character. We can't just do a tmp 
			 * + 32 since we don't know if we are at the start of a
			 * character */
			delim = label;
			while ((next_char = g_utf8_find_next_char (delim, NULL))) {
				if (next_char - label > 32)
					break;

				delim = next_char;
			}

			priv->settings->label = g_strndup (label, delim - label);
		}
		else
			priv->settings->label = g_strdup (label);
	}
}

/**
 * parrillada_burn_session_get_label:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the label (a string) set for @session.
 *
 * Return value: a #gchar or NULL. Do not free after use.
 **/

const gchar *
parrillada_burn_session_get_label (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->settings->label;
}

static void
parrillada_burn_session_tag_value_free (gpointer user_data)
{
	GValue *value = user_data;

	g_value_reset (value);
	g_free (value);
}

/**
 * parrillada_burn_session_tag_remove:
 * @session: a #ParrilladaBurnSession
 * @tag: a #gchar *
 *
 * Removes a value associated with @session through
 * parrillada_session_tag_add ().
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if the retrieval was successful
 * PARRILLADA_BURN_ERR otherwise
 **/

ParrilladaBurnResult
parrillada_burn_session_tag_remove (ParrilladaBurnSession *self,
				 const gchar *tag)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (tag != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!priv->tags)
		return PARRILLADA_BURN_ERR;

	g_hash_table_remove (priv->tags, tag);

	g_signal_emit (self,
	               parrillada_burn_session_signals [TAG_CHANGED_SIGNAL],
	               0,
	               tag);
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_tag_add:
 * @session: a #ParrilladaBurnSession
 * @tag: a #gchar *
 * @value: a #GValue *
 *
 * Associates a new @tag with @session. This can be used
 * to pass arbitrary information for plugins, like parameters
 * for video discs, ...
 * NOTE: the #ParrilladaBurnSession object takes ownership of @value.
 * See parrillada-tags.h for a list of knowns tags.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_tag_add (ParrilladaBurnSession *self,
			      const gchar *tag,
			      GValue *value)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (tag != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!priv->tags)
		priv->tags = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    parrillada_burn_session_tag_value_free);
	g_hash_table_insert (priv->tags, g_strdup (tag), value);
	g_signal_emit (self,
	               parrillada_burn_session_signals [TAG_CHANGED_SIGNAL],
	               0,
	               tag);

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_tag_add_int:
 * @session: a #ParrilladaBurnSession
 * @tag: a #gchar *
 * @value: a #gint
 *
 * Associates a new @tag with @session. This can be used
 * to pass arbitrary information for plugins, like parameters
 * for video discs, ...
 * See parrillada-tags.h for a list of knowns tags.
 *
 * Since 2.29.0
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_session_tag_add_int (ParrilladaBurnSession *self,
                                  const gchar *tag,
                                  gint value)
{
	GValue *gvalue;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (tag != NULL, PARRILLADA_BURN_ERR);

	gvalue = g_new0 (GValue, 1);
	g_value_init (gvalue, G_TYPE_INT);
	g_value_set_int (gvalue, value);

	return parrillada_burn_session_tag_add (self, tag, gvalue);
}

/**
 * parrillada_burn_session_tag_lookup:
 * @session: a #ParrilladaBurnSession
 * @tag: a #gchar *
 * @value: a #GValue
 *
 * Retrieves a value associated with @session through
 * parrillada_session_tag_add () and stores it in @value. Do
 * not destroy @value afterwards as it is not a copy.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if the retrieval was successful
 * PARRILLADA_BURN_ERR otherwise
 **/

ParrilladaBurnResult
parrillada_burn_session_tag_lookup (ParrilladaBurnSession *self,
				 const gchar *tag,
				 GValue **value)
{
	ParrilladaBurnSessionPrivate *priv;
	gpointer data;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (tag != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (!value)
		return PARRILLADA_BURN_ERR;

	if (!priv->tags)
		return PARRILLADA_BURN_ERR;

	data = g_hash_table_lookup (priv->tags, tag);
	if (!data)
		return PARRILLADA_BURN_ERR;

	/* value can be NULL if the function was just called
	 * to check whether a tag was set. */
	if (value)
		*value = data;
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_session_tag_lookup_int:
 * @session: a #ParrilladaBurnSession
 * @tag: a #gchar
 *
 * Retrieves an int value associated with @session through
 * parrillada_session_tag_add () and returns it.
 *
 * Since 2.29.0
 *
 * Return value: a #gint.
 **/

gint
parrillada_burn_session_tag_lookup_int (ParrilladaBurnSession *self,
                                     const gchar *tag)
{
	GValue *value = NULL;

	parrillada_burn_session_tag_lookup (self, tag, &value);
	if (!value || !G_VALUE_HOLDS_INT (value))
		return 0;

	return g_value_get_int (value);
}

/**
 * Used to save and restore settings/sources
 */

void
parrillada_burn_session_push_settings (ParrilladaBurnSession *self)
{
	ParrilladaSessionSetting *settings;
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* NOTE: don't clean the settings so no need to issue a signal */
	settings = g_new0 (ParrilladaSessionSetting, 1);
	parrillada_session_settings_copy (settings, priv->settings);
	priv->pile_settings = g_slist_prepend (priv->pile_settings, settings);
}

void
parrillada_burn_session_pop_settings (ParrilladaBurnSession *self)
{
	ParrilladaSessionSetting *settings;
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaMedium *former;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!priv->pile_settings)
		return;

	if (priv->dest_added_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_added_sig);
		priv->dest_added_sig = 0;
	}

	if (priv->dest_removed_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_removed_sig);
		priv->dest_removed_sig = 0;	
	}

	former = parrillada_drive_get_medium (priv->settings->burner);
	if (former)
		former = g_object_ref (former);

	parrillada_session_settings_clean (priv->settings);

	settings = priv->pile_settings->data;
	priv->pile_settings = g_slist_remove (priv->pile_settings, settings);
	parrillada_session_settings_copy (priv->settings, settings);

	parrillada_session_settings_free (settings);
	if (priv->settings->burner) {
		priv->dest_added_sig = g_signal_connect (priv->settings->burner,
							 "medium-added",
							 G_CALLBACK (parrillada_burn_session_dest_media_added),
							 self);
		priv->dest_removed_sig = g_signal_connect (priv->settings->burner,
							   "medium-removed",
							   G_CALLBACK (parrillada_burn_session_dest_media_removed),
							   self);
	}

	g_signal_emit (self,
		       parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0,
		       former);

	if (former)
		g_object_unref (former);
}

void
parrillada_burn_session_push_tracks (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;
	GSList *iter;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	parrillada_burn_session_stop_tracks_monitoring (self);

	priv->pile_tracks = g_slist_prepend (priv->pile_tracks, priv->tracks);
	iter = priv->tracks;
	priv->tracks = NULL;

	for (; iter; iter = iter->next) {
		ParrilladaTrack *track;

		track = iter->data;
		g_signal_emit (self,
			       parrillada_burn_session_signals [TRACK_REMOVED_SIGNAL],
			       0,
			       track,
			       0);
	}
}

ParrilladaBurnResult
parrillada_burn_session_pop_tracks (ParrilladaBurnSession *self)
{
	GSList *sources;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), FALSE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* Don't go further if there is no list of tracks on the pile */
	if (!priv->pile_tracks)
		return PARRILLADA_BURN_OK;

	if (priv->tracks)
		parrillada_burn_session_free_tracks (self);

	sources = priv->pile_tracks->data;
	priv->pile_tracks = g_slist_remove (priv->pile_tracks, sources);
	priv->tracks = sources;

	for (; sources; sources = sources->next) {
		ParrilladaTrack *track;

		track = sources->data;
		parrillada_burn_session_start_track_monitoring (self, track);
		g_signal_emit (self,
			       parrillada_burn_session_signals [TRACK_ADDED_SIGNAL],
			       0,
			       track);
	}

	return PARRILLADA_BURN_RETRY;
}

gboolean
parrillada_burn_session_is_dest_file (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), FALSE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv);
}

ParrilladaMedia
parrillada_burn_session_get_dest_media (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;
	ParrilladaMedium *medium;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), PARRILLADA_MEDIUM_NONE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (PARRILLADA_BURN_SESSION_WRITE_TO_FILE (priv))
		return PARRILLADA_MEDIUM_FILE;

	medium = parrillada_drive_get_medium (priv->settings->burner);

	return parrillada_medium_get_status (medium);
}

/**
 * parrillada_burn_session_get_available_medium_space:
 * @session: a #ParrilladaBurnSession
 *
 * Returns the maximum space available for the
 * medium currently inserted in the #ParrilladaDrive
 * set as burner with parrillada_burn_session_set_burner ().
 * This takes into account flags.
 *
 * Return value: a #goffset.
 **/

goffset
parrillada_burn_session_get_available_medium_space (ParrilladaBurnSession *session)
{
	ParrilladaDrive *burner;
	ParrilladaBurnFlag flags;
	ParrilladaMedium *medium;
	goffset available_blocks = 0;

	/* Retrieve the size available for burning */
	burner = parrillada_burn_session_get_burner (session);
	if (!burner)
		return 0;

	medium = parrillada_drive_get_medium (burner);
	if (!medium)
		return 0;

	flags = parrillada_burn_session_get_flags (session);
	if (flags & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND))
		parrillada_medium_get_free_space (medium, NULL, &available_blocks);
	else if (parrillada_burn_session_can_blank (session) == PARRILLADA_BURN_OK)
		parrillada_medium_get_capacity (medium, NULL, &available_blocks);
	else
		parrillada_medium_get_free_space (medium, NULL, &available_blocks);

	PARRILLADA_BURN_LOG ("Available space on medium %" G_GINT64_FORMAT, available_blocks);
	return available_blocks;
}

ParrilladaMedium *
parrillada_burn_session_get_src_medium (ParrilladaBurnSession *self)
{
	ParrilladaTrack *track;
	ParrilladaDrive *drive;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* to be able to burn to a DVD we must:
	 * - have only one track
	 * - not have any audio track */

	if (!priv->tracks)
		return NULL;

	if (g_slist_length (priv->tracks) != 1)
		return NULL;

	track = priv->tracks->data;
	if (!PARRILLADA_IS_TRACK_DISC (track))
		return NULL;

	drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
	return parrillada_drive_get_medium (drive);
}

ParrilladaDrive *
parrillada_burn_session_get_src_drive (ParrilladaBurnSession *self)
{
	ParrilladaTrack *track;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* to be able to burn to a DVD we must:
	 * - have only one track
	 * - not have any audio track */

	if (!priv->tracks)
		return NULL;

	if (g_slist_length (priv->tracks) != 1)
		return NULL;

	track = priv->tracks->data;
	if (!PARRILLADA_IS_TRACK_DISC (track))
		return NULL;

	return parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
}

gboolean
parrillada_burn_session_same_src_dest_drive (ParrilladaBurnSession *self)
{
	ParrilladaTrack *track;
	ParrilladaDrive *drive;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), FALSE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* to be able to burn to a DVD we must:
	 * - have only one track
	 * - not have any audio track 
	 */

	if (!priv->tracks)
		return FALSE;

	if (g_slist_length (priv->tracks) > 1)
		return FALSE;

	track = priv->tracks->data;
	if (!PARRILLADA_IS_TRACK_DISC (track))
		return FALSE;

	drive = parrillada_track_disc_get_drive (PARRILLADA_TRACK_DISC (track));
	if (!drive)
		return FALSE;

	return (priv->settings->burner == drive);
}


/****************************** this part is for log ***************************/
void
parrillada_burn_session_logv (ParrilladaBurnSession *self,
			   const gchar *format,
			   va_list arg_list)
{
	int len;
	int wlen;
	gchar *message;
	gchar *offending;
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	if (!format)
		return;

	if (!priv->session)
		return;

	message = g_strdup_vprintf (format, arg_list);

	/* we also need to validate the messages to be in UTF-8 */
	if (!g_utf8_validate (message, -1, (const gchar**) &offending))
		*offending = '\0';

	len = strlen (message);
	wlen = write (priv->session, message, len);
	if (wlen != len) {
		int errnum = errno;

		g_warning ("Some log data couldn't be written: %s (%i out of %i) (%s)\n",
		           message, wlen, len,
		           strerror (errnum));
	}

	g_free (message);

	if (write (priv->session, "\n", 1) != 1)
		g_warning ("Some log data could not be written");
}

void
parrillada_burn_session_log (ParrilladaBurnSession *self,
			  const gchar *format,
			  ...)
{
	va_list args;
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	va_start (args, format);
	parrillada_burn_session_logv (self, format, args);
	va_end (args);
}

const gchar *
parrillada_burn_session_get_log_path (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), NULL);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	return priv->session_path;
}

gboolean
parrillada_burn_session_start (ParrilladaBurnSession *self)
{
	ParrilladaTrackType *type = NULL;
	ParrilladaBurnSessionPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (self), FALSE);

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);

	/* This must obey the path of the temporary directory if possible */
	priv->session_path = g_build_path (G_DIR_SEPARATOR_S,
					   priv->tmpdir,
					   PARRILLADA_BURN_TMP_FILE_NAME,
					   NULL);
	priv->session = g_mkstemp_full (priv->session_path,
	                                O_CREAT|O_WRONLY,
	                                S_IRWXU);

	if (priv->session < 0) {
		g_free (priv->session_path);

		priv->session_path = g_build_path (G_DIR_SEPARATOR_S,
						   g_get_tmp_dir (),
						   PARRILLADA_BURN_TMP_FILE_NAME,
						   NULL);
		priv->session = g_mkstemp_full (priv->session_path,
		                                O_CREAT|O_WRONLY,
		                                S_IRWXU);
	}

	if (priv->session < 0) {
		g_free (priv->session_path);
		priv->session_path = NULL;

		g_warning ("Impossible to open a session file\n");
		return FALSE;
	}

	PARRILLADA_BURN_LOG ("Session starting:");

	type = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (self, type);

	PARRILLADA_BURN_LOG_TYPE (type, "Input\t=");
	PARRILLADA_BURN_LOG_FLAGS (priv->settings->flags, "flags\t=");

	/* also try all known tags */
	if (parrillada_track_type_get_has_stream (type)
	&&  PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (type))) {
		GValue *value;

		PARRILLADA_BURN_LOG ("Tags set:");

		value = NULL;
		parrillada_burn_session_tag_lookup (self,
						 PARRILLADA_DVD_STREAM_FORMAT,
						 &value);
		if (value)
			PARRILLADA_BURN_LOG ("Stream format %i", g_value_get_int (value));

		value = NULL;
		parrillada_burn_session_tag_lookup (self,
						 PARRILLADA_VCD_TYPE,
						 &value);
		if (value)
			PARRILLADA_BURN_LOG ("(S)VCD type %i", g_value_get_int (value));

		value = NULL;
		parrillada_burn_session_tag_lookup (self,
						 PARRILLADA_VIDEO_OUTPUT_FRAMERATE,
						 &value);
		if (value)
			PARRILLADA_BURN_LOG ("Framerate %i", g_value_get_int (value));

		value = NULL;
		parrillada_burn_session_tag_lookup (self,
						 PARRILLADA_VIDEO_OUTPUT_ASPECT,
						 &value);
		if (value)
			PARRILLADA_BURN_LOG ("Aspect %i", g_value_get_int (value));
	}

	if (!parrillada_burn_session_is_dest_file (self)) {
		ParrilladaMedium *medium;

		medium = parrillada_drive_get_medium (priv->settings->burner);
		PARRILLADA_BURN_LOG_DISC_TYPE (parrillada_medium_get_status (medium), "media type\t=");
		PARRILLADA_BURN_LOG ("speed\t= %i", priv->settings->rate);
	}
	else {
		parrillada_track_type_set_has_image (type);
		parrillada_track_type_set_image_format (type, parrillada_burn_session_get_output_format (self));
		PARRILLADA_BURN_LOG_TYPE (type, "output format\t=");
	}

	parrillada_track_type_free (type);

	return TRUE;
}

void
parrillada_burn_session_stop (ParrilladaBurnSession *self)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (self));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (self);
	if (priv->session > 0) {
		close (priv->session);
		priv->session = -1;
	}

	if (priv->session_path) {
		g_free (priv->session_path);
		priv->session_path = NULL;
	}
}

static void
parrillada_burn_session_track_list_free (GSList *list)
{
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

/**
 * Utility to clean tmp files
 */

static gboolean
parrillada_burn_session_clean (const gchar *path);

static gboolean
parrillada_burn_session_clean_directory (const gchar *path)
{
	GDir *dir;
	const gchar *name;

	dir = g_dir_open (path, 0, NULL);
	if (!dir)
		return FALSE;

	while ((name = g_dir_read_name (dir))) {
		gchar *tmp;

		tmp = g_build_filename (G_DIR_SEPARATOR_S,
					path,
					name,
					NULL);

		if (!parrillada_burn_session_clean (tmp)) {
			g_dir_close (dir);
			g_free (tmp);
			return FALSE;
		}

		g_free (tmp);
	}

	g_dir_close (dir);
	return TRUE;
}

static gboolean
parrillada_burn_session_clean (const gchar *path)
{
	gboolean result = TRUE;

	if (!path)
		return TRUE;

	PARRILLADA_BURN_LOG ("Cleaning %s", path);

	/* NOTE: g_file_test follows symbolic links */
	if (g_file_test (path, G_FILE_TEST_IS_DIR)
	&& !g_file_test (path, G_FILE_TEST_IS_SYMLINK))
		parrillada_burn_session_clean_directory (path);

	/* NOTE : we don't follow paths as certain files are simply linked */
	if (g_remove (path)) {
		PARRILLADA_BURN_LOG ("Cannot remove file %s (%s)", path, g_strerror (errno));
		result = FALSE;
	}

	return result;
}

static void
parrillada_burn_session_finalize (GObject *object)
{
	ParrilladaBurnSessionPrivate *priv;
	GSList *iter;

	PARRILLADA_BURN_LOG ("Cleaning session");

	priv = PARRILLADA_BURN_SESSION_PRIVATE (object);

	if (priv->tags) {
		g_hash_table_destroy (priv->tags);
		priv->tags = NULL;
	}

	if (priv->dest_added_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_added_sig);
		priv->dest_added_sig = 0;
	}

	if (priv->dest_removed_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_removed_sig);
		priv->dest_removed_sig = 0;	
	}

	parrillada_burn_session_stop_tracks_monitoring (PARRILLADA_BURN_SESSION (object));

	if (priv->pile_tracks) {
		g_slist_foreach (priv->pile_tracks,
				(GFunc) parrillada_burn_session_track_list_free,
				NULL);

		g_slist_free (priv->pile_tracks);
		priv->pile_tracks = NULL;
	}

	if (priv->tracks) {
		g_slist_foreach (priv->tracks,
				 (GFunc) g_object_unref,
				 NULL);
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	if (priv->pile_settings) {
		g_slist_foreach (priv->pile_settings,
				(GFunc) parrillada_session_settings_free,
				NULL);
		g_slist_free (priv->pile_settings);
		priv->pile_settings = NULL;
	}

	if (priv->tmpdir) {
		g_free (priv->tmpdir);
		priv->tmpdir = NULL;
	}

	/* clean tmpfiles */
	for (iter = priv->tmpfiles; iter; iter = iter->next) {
		gchar *tmpfile;

		tmpfile = iter->data;

		parrillada_burn_session_clean (tmpfile);
		g_free (tmpfile);
	}
	g_slist_free (priv->tmpfiles);

	if (priv->session > 0) {
		close (priv->session);
		priv->session = -1;
	}

	if (priv->session_path) {
		g_remove (priv->session_path);
		g_free (priv->session_path);
		priv->session_path = NULL;
	}

	parrillada_session_settings_clean (priv->settings);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_burn_session_init (ParrilladaBurnSession *obj)
{
	ParrilladaBurnSessionPrivate *priv;

	priv = PARRILLADA_BURN_SESSION_PRIVATE (obj);
	priv->session = -1;
}

static void
parrillada_burn_session_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (object));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_TMPDIR:
		parrillada_burn_session_set_tmpdir (PARRILLADA_BURN_SESSION (object),
		                                 g_value_get_string (value));
		break;
	case PROP_RATE:
		parrillada_burn_session_set_rate (PARRILLADA_BURN_SESSION (object),
		                               g_value_get_int64 (value));
		break;
	case PROP_FLAGS:
		parrillada_burn_session_set_flags (PARRILLADA_BURN_SESSION (object),
		                                g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_burn_session_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	ParrilladaBurnSessionPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_BURN_SESSION (object));

	priv = PARRILLADA_BURN_SESSION_PRIVATE (object);

	/* Here we do not call the accessors functions to honour the 0/NULL value
	 * which means use system default. */
	switch (prop_id)
	{
	case PROP_TMPDIR:
		g_value_set_string (value, priv->tmpdir);
		break;
	case PROP_RATE:
		g_value_set_int64 (value, priv->settings->rate);
		break;
	case PROP_FLAGS:
		g_value_set_int (value, priv->settings->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_burn_session_class_init (ParrilladaBurnSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaBurnSessionPrivate));

	parent_class = g_type_class_peek_parent(klass);

	object_class->finalize = parrillada_burn_session_finalize;
	object_class->set_property = parrillada_burn_session_set_property;
	object_class->get_property = parrillada_burn_session_get_property;

	klass->get_output_path = parrillada_burn_session_get_output_path_real;
	klass->get_output_format = parrillada_burn_session_get_output_format_real;
	klass->set_output_image = parrillada_burn_session_set_output_image_real;

	/**
 	* ParrilladaBurnSession::output-changed:
 	* @session: the object which received the signal
  	* @former_medium: the medium which was previously set
	*
 	* This signal gets emitted when the medium to burn to changed.
 	*
 	*/
	parrillada_burn_session_signals [OUTPUT_CHANGED_SIGNAL] =
	    g_signal_new ("output_changed",
			  PARRILLADA_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (ParrilladaBurnSessionClass, output_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  PARRILLADA_TYPE_MEDIUM);

	/**
 	* ParrilladaBurnSession::track-added:
 	* @session: the object which received the signal
  	* @track: the track that was added
	*
 	* This signal gets emitted when a track is added to @session.
 	*
 	*/
	parrillada_burn_session_signals [TRACK_ADDED_SIGNAL] =
	    g_signal_new ("track_added",
			  PARRILLADA_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (ParrilladaBurnSessionClass, track_added),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  PARRILLADA_TYPE_TRACK);

	/**
 	* ParrilladaBurnSession::track-removed:
 	* @session: the object which received the signal
  	* @track: the track that was removed
	* @former_position: the former position of @track
	*
 	* This signal gets emitted when a track is removed from @session.
 	*
 	*/
	parrillada_burn_session_signals [TRACK_REMOVED_SIGNAL] =
	    g_signal_new ("track_removed",
			  PARRILLADA_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (ParrilladaBurnSessionClass, track_removed),
			  NULL,
			  NULL,
			  parrillada_marshal_VOID__OBJECT_UINT,
			  G_TYPE_NONE,
			  2,
			  PARRILLADA_TYPE_TRACK,
			  G_TYPE_UINT);

	/**
 	* ParrilladaBurnSession::track-changed:
 	* @session: the object which received the signal
  	* @track: the track that changed
	*
 	* This signal gets emitted when the contents of a track changed.
 	*
 	*/
	parrillada_burn_session_signals [TRACK_CHANGED_SIGNAL] =
	    g_signal_new ("track_changed",
			  PARRILLADA_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (ParrilladaBurnSessionClass, track_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  PARRILLADA_TYPE_TRACK);

	/**
 	* ParrilladaBurnSession::tag-changed:
 	* @session: the object which received the signal
	*
 	* This signal gets emitted when a tag changed for @session (whether it
	* was removed, added, or it changed).
 	*
 	*/
	parrillada_burn_session_signals [TAG_CHANGED_SIGNAL] =
	    g_signal_new ("tag_changed",
			  PARRILLADA_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
	                  G_TYPE_STRING);

	g_object_class_install_property (object_class,
	                                 PROP_TMPDIR,
	                                 g_param_spec_string ("tmpdir",
	                                                      "Temporary directory",
	                                                      "The path to the temporary directory",
	                                                      NULL,
	                                                      G_PARAM_READABLE|G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
	                                 PROP_RATE,
	                                 g_param_spec_int64 ("speed",
	                                                     "Burning speed",
	                                                     "The speed at which a disc should be burned",
	                                                     0,
	                                                     G_MAXINT64,
	                                                     0,
	                                                     G_PARAM_READABLE|G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
	                                 PROP_FLAGS,
	                                 g_param_spec_int ("flags",
	                                                   "Burning flags",
	                                                   "The flags that will be used to burn",
	                                                   0,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READABLE|G_PARAM_WRITABLE));
}

/**
 * parrillada_burn_session_new:
 *
 * Returns a new #ParrilladaBurnSession object.
 *
 * Return value: a #ParrilladaBurnSession.
 **/

ParrilladaBurnSession *
parrillada_burn_session_new ()
{
	ParrilladaBurnSession *obj;
	
	obj = PARRILLADA_BURN_SESSION (g_object_new (PARRILLADA_TYPE_BURN_SESSION, NULL));
	
	return obj;
}


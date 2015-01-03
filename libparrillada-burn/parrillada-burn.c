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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "parrillada-burn.h"

#include "libparrillada-marshal.h"
#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-dbus.h"
#include "burn-task-ctx.h"
#include "burn-task.h"
#include "parrillada-caps-burn.h"

#include "parrillada-drive-priv.h"

#include "parrillada-volume.h"
#include "parrillada-drive.h"

#include "parrillada-tags.h"
#include "parrillada-track.h"
#include "parrillada-session.h"
#include "parrillada-track-image.h"
#include "parrillada-track-disc.h"
#include "parrillada-session-helper.h"

G_DEFINE_TYPE (ParrilladaBurn, parrillada_burn, G_TYPE_OBJECT);

typedef struct _ParrilladaBurnPrivate ParrilladaBurnPrivate;
struct _ParrilladaBurnPrivate {
	ParrilladaBurnCaps *caps;
	ParrilladaBurnSession *session;

	GMainLoop *sleep_loop;
	guint timeout_id;

	guint tasks_done;
	guint task_nb;
	ParrilladaTask *task;

	ParrilladaDrive *src;
	ParrilladaDrive *dest;

	gint appcookie;

	guint64 session_start;
	guint64 session_end;

	guint mounted_by_us:1;
};

#define PARRILLADA_BURN_NOT_SUPPORTED_LOG(burn)					\
	{									\
		parrillada_burn_log (burn,						\
				  "unsupported operation (in %s at %s)",	\
				  G_STRFUNC,					\
				  G_STRLOC);					\
		return PARRILLADA_BURN_NOT_SUPPORTED;				\
	}

#define PARRILLADA_BURN_NOT_READY_LOG(burn)					\
	{									\
		parrillada_burn_log (burn,						\
				  "not ready to operate (in %s at %s)",		\
				  G_STRFUNC,					\
				  G_STRLOC);					\
		return PARRILLADA_BURN_NOT_READY;					\
	}

#define PARRILLADA_BURN_DEBUG(burn, message, ...)					\
	{									\
		gchar *format;							\
		PARRILLADA_BURN_LOG (message, ##__VA_ARGS__);			\
		format = g_strdup_printf ("%s (%s %s)",				\
					  message,				\
					  G_STRFUNC,				\
					  G_STRLOC);				\
		parrillada_burn_log (burn,						\
				  format,					\
				  ##__VA_ARGS__);				\
		g_free (format);						\
	}

typedef enum {
	ASK_DISABLE_JOLIET_SIGNAL,
	WARN_DATA_LOSS_SIGNAL,
	WARN_PREVIOUS_SESSION_LOSS_SIGNAL,
	WARN_AUDIO_TO_APPENDABLE_SIGNAL,
	WARN_REWRITABLE_SIGNAL,
	INSERT_MEDIA_REQUEST_SIGNAL,
	LOCATION_REQUEST_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	ACTION_CHANGED_SIGNAL,
	DUMMY_SUCCESS_SIGNAL,
	EJECT_FAILURE_SIGNAL,
	BLANK_FAILURE_SIGNAL,
	INSTALL_MISSING_SIGNAL,
	LAST_SIGNAL
} ParrilladaBurnSignalType;

static guint parrillada_burn_signals [LAST_SIGNAL] = { 0 };

#define PARRILLADA_BURN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_BURN, ParrilladaBurnPrivate))

#define MAX_EJECT_ATTEMPTS	5
#define MAX_MOUNT_ATTEMPTS	40

#define MOUNT_TIMEOUT		500

static GObjectClass *parent_class = NULL;

static void
parrillada_burn_powermanagement (ParrilladaBurn *self,
			      gboolean wake)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (self);

	if (wake)
	  	priv->appcookie = parrillada_inhibit_suspend (_("Burning CD/DVD"));
	else
		parrillada_uninhibit_suspend (priv->appcookie); 
}

/**
 * parrillada_burn_new:
 *
 *  Creates a new #ParrilladaBurn object.
 *
 * Return value: a #ParrilladaBurn object.
 **/

ParrilladaBurn *
parrillada_burn_new ()
{
	return g_object_new (PARRILLADA_TYPE_BURN, NULL);
}

static void
parrillada_burn_log (ParrilladaBurn *burn,
		  const gchar *format,
		  ...)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	va_list arg_list;

	va_start (arg_list, format);
	parrillada_burn_session_logv (priv->session, format, arg_list);
	va_end (arg_list);
}

static ParrilladaBurnResult
parrillada_burn_emit_signal (ParrilladaBurn *burn,
                          guint signal,
                          ParrilladaBurnResult default_answer)
{
	GValue instance_and_params;
	GValue return_value;

	instance_and_params.g_type = 0;
	g_value_init (&instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (&instance_and_params, burn);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, default_answer);

	g_signal_emitv (&instance_and_params,
			parrillada_burn_signals [signal],
			0,
			&return_value);

	g_value_unset (&instance_and_params);

	return g_value_get_int (&return_value);
}

static void
parrillada_burn_action_changed_real (ParrilladaBurn *burn,
				  ParrilladaBurnAction action)
{
	g_signal_emit (burn,
		       parrillada_burn_signals [ACTION_CHANGED_SIGNAL],
		       0,
		       action);

	if (action == PARRILLADA_BURN_ACTION_FINISHED)
		g_signal_emit (burn,
		               parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
		               0,
		               1.0,
		               1.0,
		               -1L);
	else if (action == PARRILLADA_BURN_ACTION_EJECTING)
		g_signal_emit (burn,
			       parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
			       0,
			       -1.0,
			       -1.0,
			       -1L);
}

static gboolean
parrillada_burn_wakeup (ParrilladaBurn *burn)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	if (priv->sleep_loop)
		g_main_loop_quit (priv->sleep_loop);

	priv->timeout_id = 0;
	return FALSE;
}

static ParrilladaBurnResult
parrillada_burn_sleep (ParrilladaBurn *burn, gint msec)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	GMainLoop *loop;

	priv->sleep_loop = g_main_loop_new (NULL, FALSE);
	priv->timeout_id = g_timeout_add (msec,
					  (GSourceFunc) parrillada_burn_wakeup,
					  burn);

	/* Keep a reference to the loop in case we are cancelled to destroy it */
	loop = priv->sleep_loop;
	g_main_loop_run (loop);

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	g_main_loop_unref (loop);
	if (priv->sleep_loop) {
		priv->sleep_loop = NULL;
		return PARRILLADA_BURN_OK;
	}

	/* if sleep_loop = NULL => We've been cancelled */
	return PARRILLADA_BURN_CANCEL;
}

static ParrilladaBurnResult
parrillada_burn_reprobe (ParrilladaBurn *burn)
{
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	priv = PARRILLADA_BURN_PRIVATE (burn);

	PARRILLADA_BURN_LOG ("Reprobing for medium");

	/* reprobe the medium and wait for it to be probed */
	parrillada_drive_reprobe (priv->dest);
	while (parrillada_drive_probing (priv->dest)) {
		result = parrillada_burn_sleep (burn, 250);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return result;
}

static ParrilladaBurnResult
parrillada_burn_unmount (ParrilladaBurn *self,
                      ParrilladaMedium *medium,
                      GError **error)
{
	guint counter = 0;

	if (!medium)
		return PARRILLADA_BURN_OK;

	/* Retry several times, since sometimes the drives are really busy */
	while (parrillada_volume_is_mounted (PARRILLADA_VOLUME (medium))) {
		GError *ret_error;
		ParrilladaBurnResult result;

		counter ++;
		if (counter > MAX_EJECT_ATTEMPTS) {
			PARRILLADA_BURN_LOG ("Max attempts reached at unmounting");
			if (error && !(*error))
				g_set_error (error,
					     PARRILLADA_BURN_ERROR,
					     PARRILLADA_BURN_ERROR_DRIVE_BUSY,
					     "%s. %s",
					     _("The drive is busy"),
					     _("Make sure another application is not using it"));
			return PARRILLADA_BURN_ERR;
		}

		PARRILLADA_BURN_LOG ("Retrying unmounting");

		ret_error = NULL;
		parrillada_volume_umount (PARRILLADA_VOLUME (medium), TRUE, NULL);

		if (ret_error) {
			PARRILLADA_BURN_LOG ("Ejection error: %s", ret_error->message);
			g_error_free (ret_error);
		}

		result = parrillada_burn_sleep (self, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_emit_eject_failure_signal (ParrilladaBurn *burn,
                                        ParrilladaDrive *drive) 
{
	GValue instance_and_params [4];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (instance_and_params, burn);
	
	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_FROM_INSTANCE (drive));
	g_value_set_instance (instance_and_params + 1, drive);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_CANCEL);

	g_signal_emitv (instance_and_params,
			parrillada_burn_signals [EJECT_FAILURE_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	return g_value_get_int (&return_value);
}

static ParrilladaBurnResult
parrillada_burn_eject (ParrilladaBurn *self,
		    ParrilladaDrive *drive,
		    GError **error)
{
	guint counter = 0;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;

	PARRILLADA_BURN_LOG ("Ejecting drive/medium");

	/* Unmount, ... */
	medium = parrillada_drive_get_medium (drive);
	result = parrillada_burn_unmount (self, medium, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* Release lock, ... */
	if (parrillada_drive_is_locked (drive, NULL)) {
		if (!parrillada_drive_unlock (drive)) {
			gchar *name;

			name = parrillada_drive_get_display_name (drive);
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("\"%s\" cannot be unlocked"),
				     name);
			g_free (name);
			return PARRILLADA_BURN_ERR;
		}
	}

	/* Retry several times, since sometimes the drives are really busy */
	while (parrillada_drive_get_medium (drive) || parrillada_drive_probing (drive)) {
		GError *ret_error;

		/* Don't interrupt a probe */
		if (parrillada_drive_probing (drive)) {
			result = parrillada_burn_sleep (self, 500);
			if (result != PARRILLADA_BURN_OK)
				return result;

			continue;
		}

		counter ++;
		if (counter == 1)
			parrillada_burn_action_changed_real (self, PARRILLADA_BURN_ACTION_EJECTING);

		if (counter > MAX_EJECT_ATTEMPTS) {
			PARRILLADA_BURN_LOG ("Max attempts reached at ejecting");

			result = parrillada_burn_emit_eject_failure_signal (self, drive);
			if (result != PARRILLADA_BURN_OK)
				return result;

			continue;
		}

		PARRILLADA_BURN_LOG ("Retrying ejection");
		ret_error = NULL;
		parrillada_drive_eject (drive, TRUE, &ret_error);

		if (ret_error) {
			PARRILLADA_BURN_LOG ("Ejection error: %s", ret_error->message);
			g_error_free (ret_error);
		}

		result = parrillada_burn_sleep (self, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_ask_for_media (ParrilladaBurn *burn,
			    ParrilladaDrive *drive,
			    ParrilladaBurnError error_type,
			    ParrilladaMedia required_media,
			    GError **error)
{
	GValue instance_and_params [4];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (instance_and_params, burn);
	
	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_FROM_INSTANCE (drive));
	g_value_set_instance (instance_and_params + 1, drive);
	
	instance_and_params [2].g_type = 0;
	g_value_init (instance_and_params + 2, G_TYPE_INT);
	g_value_set_int (instance_and_params + 2, error_type);
	
	instance_and_params [3].g_type = 0;
	g_value_init (instance_and_params + 3, G_TYPE_INT);
	g_value_set_int (instance_and_params + 3, required_media);
	
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_CANCEL);

	g_signal_emitv (instance_and_params,
			parrillada_burn_signals [INSERT_MEDIA_REQUEST_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (instance_and_params + 1);

	return g_value_get_int (&return_value);
}

static ParrilladaBurnResult
parrillada_burn_ask_for_src_media (ParrilladaBurn *burn,
				ParrilladaBurnError error_type,
				ParrilladaMedia required_media,
				GError **error)
{
	ParrilladaMedium *medium;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	medium = parrillada_drive_get_medium (priv->src);
	if (parrillada_medium_get_status (medium) != PARRILLADA_MEDIUM_NONE
	||  parrillada_drive_probing (priv->src)) {
		ParrilladaBurnResult result;
		result = parrillada_burn_eject (burn, priv->src, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return parrillada_burn_ask_for_media (burn,
					   priv->src,
					   error_type,
					   required_media,
					   error);
}

static ParrilladaBurnResult
parrillada_burn_ask_for_dest_media (ParrilladaBurn *burn,
				 ParrilladaBurnError error_type,
				 ParrilladaMedia required_media,
				 GError **error)
{
	ParrilladaDrive *drive;
	ParrilladaMedium *medium;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	/* Since in some cases (like when we reload
	 * a medium after a copy), the destination
	 * medium may not be locked yet we use 
	 * separate variable drive. */
	if (!priv->dest) {
		drive = parrillada_burn_session_get_burner (priv->session);
		if (!drive) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_OUTPUT_NONE,
				     "%s", _("No burner specified"));
			return PARRILLADA_BURN_ERR;
		}
	}
	else
		drive = priv->dest;

	medium = parrillada_drive_get_medium (drive);
	if (parrillada_medium_get_status (medium) != PARRILLADA_MEDIUM_NONE
	||  parrillada_drive_probing (drive)) {
		ParrilladaBurnResult result;

		result = parrillada_burn_eject (burn, drive, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return parrillada_burn_ask_for_media (burn,
					   drive,
					   error_type,
					   required_media,
					   error);
}

static ParrilladaBurnResult
parrillada_burn_lock_src_media (ParrilladaBurn *burn,
			     GError **error)
{
	gchar *failure = NULL;
	ParrilladaMedia media;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaBurnError error_type;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	priv->src = parrillada_burn_session_get_src_drive (priv->session);
	if (!priv->src) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("No source drive specified"));
		return PARRILLADA_BURN_ERR;
	}


again:

	while (parrillada_drive_probing (priv->src)) {
		result = parrillada_burn_sleep (burn, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	medium = parrillada_drive_get_medium (priv->src);
	if (parrillada_volume_is_mounted (PARRILLADA_VOLUME (medium))) {
		if (!parrillada_volume_umount (PARRILLADA_VOLUME (medium), TRUE, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   parrillada_drive_get_device (priv->src));
	}

	/* NOTE: we used to unmount the media before now we shouldn't need that
	 * get any information from the drive */
	media = parrillada_medium_get_status (medium);
	if (media == PARRILLADA_MEDIUM_NONE)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_NONE;
	else if (media == PARRILLADA_MEDIUM_BUSY)
		error_type = PARRILLADA_BURN_ERROR_DRIVE_BUSY;
	else if (media == PARRILLADA_MEDIUM_UNSUPPORTED)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
	else if (media & PARRILLADA_MEDIUM_BLANK)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_NO_DATA;
	else
		error_type = PARRILLADA_BURN_ERROR_NONE;

	if (media & PARRILLADA_MEDIUM_BLANK) {
		result = parrillada_burn_ask_for_src_media (burn,
							 PARRILLADA_BURN_ERROR_MEDIUM_NO_DATA,
							 PARRILLADA_MEDIUM_HAS_DATA,
							 error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto again;
	}

	if (!parrillada_drive_is_locked (priv->src, NULL)
	&&  !parrillada_drive_lock (priv->src, _("Ongoing copying process"), &failure)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("The drive cannot be locked (%s)"),
			     failure);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_reload_src_media (ParrilladaBurn *burn,
			       ParrilladaBurnError error_code,
			       GError **error)
{
	ParrilladaBurnResult result;

	result = parrillada_burn_ask_for_src_media (burn,
						 error_code,
						 PARRILLADA_MEDIUM_HAS_DATA,
						 error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	result = parrillada_burn_lock_src_media (burn, error);
	return result;
}

static ParrilladaBurnResult
parrillada_burn_lock_rewritable_media (ParrilladaBurn *burn,
				    GError **error)
{
	gchar *failure;
	ParrilladaMedia media;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaBurnError error_type;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	priv->dest = parrillada_burn_session_get_burner (priv->session);
	if (!priv->dest) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_OUTPUT_NONE,
			     "%s", _("No burner specified"));
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

 again:

	while (parrillada_drive_probing (priv->dest)) {
		result = parrillada_burn_sleep (burn, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	medium = parrillada_drive_get_medium (priv->dest);
	if (!parrillada_medium_can_be_rewritten (medium)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_MEDIUM_NOT_REWRITABLE,
			     "%s", _("The drive has no rewriting capabilities"));
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	if (parrillada_volume_is_mounted (PARRILLADA_VOLUME (medium))) {
		if (!parrillada_volume_umount (PARRILLADA_VOLUME (medium), TRUE, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   parrillada_drive_get_device (priv->dest));
	}

	media = parrillada_medium_get_status (medium);
	if (media == PARRILLADA_MEDIUM_NONE)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_NONE;
	else if (media == PARRILLADA_MEDIUM_BUSY)
		error_type = PARRILLADA_BURN_ERROR_DRIVE_BUSY;
	else if (media == PARRILLADA_MEDIUM_UNSUPPORTED)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
	else if (!(media & PARRILLADA_MEDIUM_REWRITABLE))
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_NOT_REWRITABLE;
	else
		error_type = PARRILLADA_BURN_ERROR_NONE;

	if (error_type != PARRILLADA_BURN_ERROR_NONE) {
		result = parrillada_burn_ask_for_dest_media (burn,
							  error_type,
							  PARRILLADA_MEDIUM_REWRITABLE|
							  PARRILLADA_MEDIUM_HAS_DATA,
							  error);

		if (result != PARRILLADA_BURN_OK)
			return result;

		goto again;
	}

	if (!parrillada_drive_is_locked (priv->dest, NULL)
	&&  !parrillada_drive_lock (priv->dest, _("Ongoing blanking process"), &failure)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("The drive cannot be locked (%s)"),
			     failure);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_lock_dest_media (ParrilladaBurn *burn,
			      ParrilladaBurnError *ret_error,
			      GError **error)
{
	gchar *failure;
	ParrilladaMedia media;
	ParrilladaBurnError berror;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaTrackType *input = NULL;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	priv->dest = parrillada_burn_session_get_burner (priv->session);
	if (!priv->dest) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_OUTPUT_NONE,
			     "%s", _("No burner specified"));
		return PARRILLADA_BURN_ERR;
	}

	/* Check the capabilities of the drive */
	if (!parrillada_drive_can_write (priv->dest)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("The drive cannot burn"));
		PARRILLADA_BURN_NOT_SUPPORTED_LOG (burn);
	}

	/* NOTE: don't lock the drive here yet as
	 * otherwise we'd be probing forever. */
	while (parrillada_drive_probing (priv->dest)) {
		result = parrillada_burn_sleep (burn, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	medium = parrillada_drive_get_medium (priv->dest);
	if (!medium) {
		result = PARRILLADA_BURN_NEED_RELOAD;
		berror = PARRILLADA_BURN_ERROR_MEDIUM_NONE;
		goto end;
	}

	/* unmount the medium */
	result = parrillada_burn_unmount (burn, medium, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	result = PARRILLADA_BURN_OK;
	berror = PARRILLADA_BURN_ERROR_NONE;

	media = parrillada_medium_get_status (medium);
	PARRILLADA_BURN_LOG_WITH_FULL_TYPE (PARRILLADA_TRACK_TYPE_DISC,
					 media,
					 PARRILLADA_PLUGIN_IO_NONE,
					 "Media inserted is");

	if (media == PARRILLADA_MEDIUM_NONE) {
		result = PARRILLADA_BURN_NEED_RELOAD;
		berror = PARRILLADA_BURN_ERROR_MEDIUM_NONE;
		goto end;
	}

	if (media == PARRILLADA_MEDIUM_UNSUPPORTED) {
		result = PARRILLADA_BURN_NEED_RELOAD;
		berror = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
		goto end;
	}

	if (media == PARRILLADA_MEDIUM_BUSY) {
		result = PARRILLADA_BURN_NEED_RELOAD;
		berror = PARRILLADA_BURN_ERROR_DRIVE_BUSY;
		goto end;
	}

	/* Make sure that media is supported and
	 * can be written to */

	/* NOTE: Since we did not check the flags
	 * and since they might change, check if the
	 * session is supported without the flags.
	 * We use quite a strict checking though as
	 * from now on we require plugins to be
	 * ready. */
	result = parrillada_burn_session_can_burn (priv->session, FALSE);
	if (result != PARRILLADA_BURN_OK) {
		PARRILLADA_BURN_LOG ("Inserted media is not supported");
		result = PARRILLADA_BURN_NEED_RELOAD;
		berror = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
		goto end;
	}

	input = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (priv->session, input);

	if (parrillada_track_type_get_has_image (input)) {
		goffset medium_sec = 0;
		goffset session_sec = 0;

		/* This test is only valid when it's an image
		 * as input which comes handy when copying
		 * from the same drive as we're sure we do 
		 * have an image. */
		parrillada_medium_get_capacity (medium,
		                             NULL,
		                             &medium_sec);
		parrillada_burn_session_get_size (priv->session,
		                               &session_sec,
		                               NULL);

		if (session_sec > medium_sec) {
			PARRILLADA_BURN_LOG ("Not enough space for image %"G_GOFFSET_FORMAT"/%"G_GOFFSET_FORMAT, session_sec, medium_sec);
			berror = PARRILLADA_BURN_ERROR_MEDIUM_SPACE;
			result = PARRILLADA_BURN_NEED_RELOAD;
			goto end;
		}
	}

	/* Only lock the drive after all checks succeeded */
	if (!parrillada_drive_is_locked (priv->dest, NULL)
	&&  !parrillada_drive_lock (priv->dest, _("Ongoing burning process"), &failure)) {
		parrillada_track_type_free (input);

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("The drive cannot be locked (%s)"),
			     failure);
		return PARRILLADA_BURN_ERR;
	}

end:

	if (result != PARRILLADA_BURN_OK
	&& parrillada_drive_is_locked (priv->dest, NULL))
		parrillada_drive_unlock (priv->dest);

	if (result == PARRILLADA_BURN_NEED_RELOAD && ret_error)
		*ret_error = berror;

	parrillada_track_type_free (input);

	return result;
}

static ParrilladaBurnResult
parrillada_burn_reload_dest_media (ParrilladaBurn *burn,
				ParrilladaBurnError error_code,
				GError **error)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	ParrilladaMedia required_media;
	ParrilladaBurnResult result;

again:

	/* eject and ask the user to reload a disc */
	required_media = parrillada_burn_session_get_required_media_type (priv->session);
	required_media &= (PARRILLADA_MEDIUM_WRITABLE|
			   PARRILLADA_MEDIUM_CD|
			   PARRILLADA_MEDIUM_DVD);

	if (required_media == PARRILLADA_MEDIUM_NONE)
		required_media = PARRILLADA_MEDIUM_WRITABLE;

	result = parrillada_burn_ask_for_dest_media (burn,
						  error_code,
						  required_media,
						  error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	result = parrillada_burn_lock_dest_media (burn,
					       &error_code,
					       error);
	if (result == PARRILLADA_BURN_NEED_RELOAD)
		goto again;

	return result;
}

static ParrilladaBurnResult
parrillada_burn_lock_checksum_media (ParrilladaBurn *burn,
				  GError **error)
{
	gchar *failure;
	ParrilladaMedia media;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaBurnError error_type;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	priv->dest = parrillada_burn_session_get_src_drive (priv->session);

again:

	while (parrillada_drive_probing (priv->dest)) {
		result = parrillada_burn_sleep (burn, 500);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	medium = parrillada_drive_get_medium (priv->dest);
	media = parrillada_medium_get_status (medium);

	error_type = PARRILLADA_BURN_ERROR_NONE;
	PARRILLADA_BURN_LOG_DISC_TYPE (media, "Waiting for media to checksum");

	if (media == PARRILLADA_MEDIUM_NONE) {
		/* NOTE: that's done on purpose since here if the drive is empty
		 * that's because we ejected it */
		result = parrillada_burn_ask_for_dest_media (burn,
							  PARRILLADA_BURN_WARNING_CHECKSUM,
							  PARRILLADA_MEDIUM_NONE,
							  error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}
	else if (media == PARRILLADA_MEDIUM_BUSY)
		error_type = PARRILLADA_BURN_ERROR_DRIVE_BUSY;
	else if (media == PARRILLADA_MEDIUM_UNSUPPORTED)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
	else if (media & PARRILLADA_MEDIUM_BLANK)
		error_type = PARRILLADA_BURN_ERROR_MEDIUM_NO_DATA;

	if (error_type != PARRILLADA_BURN_ERROR_NONE) {
		result = parrillada_burn_ask_for_dest_media (burn,
							  PARRILLADA_BURN_WARNING_CHECKSUM,
							  PARRILLADA_MEDIUM_NONE,
							  error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto again;
	}

	if (!parrillada_drive_is_locked (priv->dest, NULL)
	&&  !parrillada_drive_lock (priv->dest, _("Ongoing checksumming operation"), &failure)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("The drive cannot be locked (%s)"),
			     failure);
		return PARRILLADA_BURN_ERR;
	}

	/* if drive is mounted then unmount before checking anything */
/*	if (parrillada_volume_is_mounted (PARRILLADA_VOLUME (medium))
	&& !parrillada_volume_umount (PARRILLADA_VOLUME (medium), TRUE, NULL))
		g_warning ("Couldn't unmount volume in drive: %s",
			   parrillada_drive_get_device (priv->dest));
*/

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_unlock_src_media (ParrilladaBurn *burn,
			       GError **error)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	ParrilladaMedium *medium;

	if (!priv->src)
		return PARRILLADA_BURN_OK;

	/* If we mounted it ourselves, unmount it */
	medium = parrillada_drive_get_medium (priv->src);
	if (priv->mounted_by_us) {
		parrillada_burn_unmount (burn, medium, error);
		priv->mounted_by_us = FALSE;
	}

	if (parrillada_drive_is_locked (priv->src, NULL))
		parrillada_drive_unlock (priv->src);

	/* Never eject the source if we don't need to. Let the user do that. For
	 * one thing it avoids breaking other applications that are using it
	 * like for example totem. */
	/* if (PARRILLADA_BURN_SESSION_EJECT (priv->session))
		parrillada_drive_eject (PARRILLADA_VOLUME (medium), FALSE, error); */

	priv->src = NULL;
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_unlock_dest_media (ParrilladaBurn *burn,
				GError **error)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	if (!priv->dest)
		return PARRILLADA_BURN_OK;

	if (parrillada_drive_is_locked (priv->dest, NULL))
		parrillada_drive_unlock (priv->dest);

	if (!PARRILLADA_BURN_SESSION_EJECT (priv->session))
		parrillada_drive_reprobe (priv->dest);
	else
		parrillada_burn_eject (burn, priv->dest, error);

	priv->dest = NULL;
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_unlock_medias (ParrilladaBurn *burn,
			    GError **error)
{
	parrillada_burn_unlock_dest_media (burn, error);
	parrillada_burn_unlock_src_media (burn, error);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_burn_progress_changed (ParrilladaTaskCtx *task,
			       ParrilladaBurn *burn)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	gdouble overall_progress = -1.0;
	gdouble task_progress = -1.0;
	glong time_remaining = -1;

	/* get the task current progress */
	if (parrillada_task_ctx_get_progress (task, &task_progress) == PARRILLADA_BURN_OK) {
		parrillada_task_ctx_get_remaining_time (task, &time_remaining);
		overall_progress = (task_progress + (gdouble) priv->tasks_done) /
				   (gdouble) priv->task_nb;
	}
	else
		overall_progress =  (gdouble) priv->tasks_done /
				    (gdouble) priv->task_nb;

	g_signal_emit (burn,
		       parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
		       0,
		       overall_progress,
		       task_progress,
		       time_remaining);
}

static void
parrillada_burn_action_changed (ParrilladaTask *task,
			     ParrilladaBurnAction action,
			     ParrilladaBurn *burn)
{
	parrillada_burn_action_changed_real (burn, action);
}

/**
 * parrillada_burn_get_action_string:
 * @burn: a #ParrilladaBurn
 * @action: a #ParrilladaBurnAction
 * @string: a #gchar **
 *
 * This function returns the current action (in @string)  of
 * an ongoing operation performed by @burn.
 * @action is used to set a default string in case there was
 * no string set by the backend to describe the current
 * operation.
 *
 **/

void
parrillada_burn_get_action_string (ParrilladaBurn *burn,
				ParrilladaBurnAction action,
				gchar **string)
{
	ParrilladaBurnPrivate *priv;

	g_return_if_fail (PARRILLADA_BURN (burn));
	g_return_if_fail (string != NULL);

	priv = PARRILLADA_BURN_PRIVATE (burn);
	if (action == PARRILLADA_BURN_ACTION_FINISHED || !priv->task)
		(*string) = g_strdup (parrillada_burn_action_to_string (action));
	else
		parrillada_task_ctx_get_current_action_string (PARRILLADA_TASK_CTX (priv->task),
							    action,
							    string);
}

/**
 * parrillada_burn_status:
 * @burn: a #ParrilladaBurn
 * @media: a #ParrilladaMedia or NULL
 * @isosize: a #goffset or NULL
 * @written: a #goffset or NULL
 * @rate: a #guint64 or NULL
 *
 * Returns various information about the current operation 
 * in @media (the current media type being burnt),
 * @isosize (the size of the data being burnt), @written (the
 * number of bytes having been written so far) and @rate
 * (the speed at which data are written).
 *
 * Return value: a #ParrilladaBurnResult. PARRILLADA_BURN_OK if there is
 * an ongoing operation; PARRILLADA_BURN_NOT_READY otherwise.
 **/

ParrilladaBurnResult
parrillada_burn_status (ParrilladaBurn *burn,
		     ParrilladaMedia *media,
		     goffset *isosize,
		     goffset *written,
		     guint64 *rate)
{
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result;

	g_return_val_if_fail (PARRILLADA_BURN (burn), PARRILLADA_BURN_ERR);
	
	priv = PARRILLADA_BURN_PRIVATE (burn);

	if (!priv->task)
		return PARRILLADA_BURN_NOT_READY;

	if (isosize) {
		goffset size_local = 0;

		result = parrillada_task_ctx_get_session_output_size (PARRILLADA_TASK_CTX (priv->task),
								   NULL,
								   &size_local);
		if (result != PARRILLADA_BURN_OK)
			*isosize = -1;
		else
			*isosize = size_local;
	}

	if (!parrillada_task_is_running (priv->task))
		return PARRILLADA_BURN_NOT_READY;

	if (rate)
		parrillada_task_ctx_get_rate (PARRILLADA_TASK_CTX (priv->task), rate);

	if (written) {
		gint64 written_local = 0;

		result = parrillada_task_ctx_get_written (PARRILLADA_TASK_CTX (priv->task), &written_local);

		if (result != PARRILLADA_BURN_OK)
			*written = -1;
		else
			*written = written_local;
	}

	if (!media)
		return PARRILLADA_BURN_OK;

	/* return the disc we burn to if:
	 * - that's the last task to perform
	 * - parrillada_burn_session_is_dest_file returns FALSE
	 */
	if (priv->tasks_done < priv->task_nb - 1) {
		ParrilladaTrackType *input = NULL;

		input = parrillada_track_type_new ();
		parrillada_burn_session_get_input_type (priv->session, input);
		if (parrillada_track_type_get_has_medium (input))
			*media = parrillada_track_type_get_medium_type (input);
		else
			*media = PARRILLADA_MEDIUM_NONE;

		parrillada_track_type_free (input);
	}
	else if (parrillada_burn_session_is_dest_file (priv->session))
		*media = PARRILLADA_MEDIUM_FILE;
	else
		*media = parrillada_burn_session_get_dest_media (priv->session);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_ask_for_joliet (ParrilladaBurn *burn)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	ParrilladaBurnResult result;
	GSList *tracks;
	GSList *iter;

	result = parrillada_burn_emit_signal (burn, ASK_DISABLE_JOLIET_SIGNAL, PARRILLADA_BURN_CANCEL);
	if (result != PARRILLADA_BURN_OK)
		return result;

	tracks = parrillada_burn_session_get_tracks (priv->session);
	for (iter = tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;

		track = iter->data;
		parrillada_track_data_rm_fs (PARRILLADA_TRACK_DATA (track), PARRILLADA_IMAGE_FS_JOLIET);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_ask_for_location (ParrilladaBurn *burn,
			       GError *received_error,
			       gboolean is_temporary,
			       GError **error)
{
	GValue instance_and_params [3];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (instance_and_params, burn);
	
	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_POINTER);
	g_value_set_pointer (instance_and_params + 1, received_error);
	
	instance_and_params [2].g_type = 0;
	g_value_init (instance_and_params + 2, G_TYPE_BOOLEAN);
	g_value_set_boolean (instance_and_params + 2, is_temporary);
	
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_CANCEL);

	g_signal_emitv (instance_and_params,
			parrillada_burn_signals [LOCATION_REQUEST_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (instance_and_params + 1);

	return g_value_get_int (&return_value);
}

static ParrilladaBurnResult
parrillada_burn_run_eraser (ParrilladaBurn *burn, GError **error)
{
	ParrilladaDrive *drive;
	ParrilladaMedium *medium;
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result;

	priv = PARRILLADA_BURN_PRIVATE (burn);

	drive = parrillada_burn_session_get_burner (priv->session);
	medium = parrillada_drive_get_medium (drive);

	result = parrillada_burn_unmount (burn, medium, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	result = parrillada_task_run (priv->task, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* Reprobe. It can happen (like with dvd+rw-format) that
	 * for the whole OS, the disc doesn't exist during the 
	 * formatting. Wait for the disc to reappear */
	/*  Likewise, this is necessary when we do a
	 * simulation before blanking since it blanked the disc
	 * and then to create all tasks necessary for the real
	 * burning operation, we'll need the real medium status 
	 * not to include a blanking job again. */
	return parrillada_burn_reprobe (burn);
}

static ParrilladaBurnResult
parrillada_burn_run_imager (ParrilladaBurn *burn,
			 gboolean fake,
			 GError **error)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);
	ParrilladaBurnError error_code;
	ParrilladaBurnResult result;
	GError *ret_error = NULL;
	ParrilladaMedium *medium;
	ParrilladaDrive *src;

	src = parrillada_burn_session_get_src_drive (priv->session);

start:

	medium = parrillada_drive_get_medium (src);
	if (medium) {
		/* This is just in case */
		result = parrillada_burn_unmount (burn, medium, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	/* If it succeeds then the new track(s) will be at the top of
	 * session tracks stack and therefore usable by the recorder.
	 * NOTE: it's up to the job to push the current tracks. */
	if (fake)
		result = parrillada_task_check (priv->task, &ret_error);
	else
		result = parrillada_task_run (priv->task, &ret_error);

	if (result == PARRILLADA_BURN_OK) {
		if (!fake) {
			g_signal_emit (burn,
				       parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
				       0,
				       1.0,
				       1.0,
				       -1L);
		}
		return PARRILLADA_BURN_OK;
	}

	if (result != PARRILLADA_BURN_ERR) {
		if (error && ret_error)
			g_propagate_error (error, ret_error);

		return result;
	}

	if (!ret_error)
		return result;

	if (parrillada_burn_session_is_dest_file (priv->session)) {
		gchar *image = NULL;
		gchar *toc = NULL;

		/* If it was an image that was output, remove it. If that was
		 * a temporary image, it will be removed by ParrilladaBurnSession 
		 * object. But if it was a final image, it would be left and
		 * would clutter the disk, wasting space. */
		parrillada_burn_session_get_output (priv->session,
						 &image,
						 &toc);
		if (image)
			g_remove (image);
		if (toc)
			g_remove (toc);
	}

	/* See if we can recover from the error */
	error_code = ret_error->code;
	if (error_code == PARRILLADA_BURN_ERROR_IMAGE_JOLIET) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* some files are not conforming to Joliet standard see
		 * if the user wants to carry on with a non joliet disc */
		result = parrillada_burn_ask_for_joliet (burn);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == PARRILLADA_BURN_ERROR_MEDIUM_NO_DATA) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* The media hasn't data on it: ask for a new one. */
		result = parrillada_burn_reload_src_media (burn,
							error_code,
							error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == PARRILLADA_BURN_ERROR_DISK_SPACE
	     ||  error_code == PARRILLADA_BURN_ERROR_PERMISSION
	     ||  error_code == PARRILLADA_BURN_ERROR_TMP_DIRECTORY) {
		gboolean is_temp;

		/* That's an imager (outputs an image to the disc) so that means
		 * that here the problem comes from the hard drive being too
		 * small or we don't have the right permission. */

		/* NOTE: Image file creation is always the last to take place 
		 * when it's not temporary. Another job should not take place
		 * afterwards */
		if (!parrillada_burn_session_is_dest_file (priv->session))
			is_temp = TRUE;
		else
			is_temp = FALSE;

		result = parrillada_burn_ask_for_location (burn,
							ret_error,
							is_temp,
							error);

		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}

	/* If we reached this point that means the error was not recoverable.
	 * Propagate the error. */
	if (error && ret_error)
		g_propagate_error (error, ret_error);

	return PARRILLADA_BURN_ERR;
}

static ParrilladaBurnResult
parrillada_burn_can_use_drive_exclusively (ParrilladaBurn *burn,
					ParrilladaDrive *drive)
{
	ParrilladaBurnResult result;

	if (!drive)
		return PARRILLADA_BURN_OK;

	while (!parrillada_drive_can_use_exclusively (drive)) {
		PARRILLADA_BURN_LOG ("Device busy, retrying in 250 ms");
		result = parrillada_burn_sleep (burn, 250);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_run_recorder (ParrilladaBurn *burn, GError **error)
{
	gint error_code;
	ParrilladaDrive *src;
	gboolean has_slept;
	ParrilladaDrive *burner;
	GError *ret_error = NULL;
	ParrilladaBurnResult result;
	ParrilladaMedium *src_medium;
	ParrilladaMedium *burnt_medium;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	has_slept = FALSE;

	src = parrillada_burn_session_get_src_drive (priv->session);
	src_medium = parrillada_drive_get_medium (src);

	burner = parrillada_burn_session_get_burner (priv->session);
	burnt_medium = parrillada_drive_get_medium (burner);

start:

	/* this is just in case */
	if (PARRILLADA_BURN_SESSION_NO_TMP_FILE (priv->session)) {
		result = parrillada_burn_unmount (burn, src_medium, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	result = parrillada_burn_unmount (burn, burnt_medium, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* before we start let's see if that drive can be used exclusively.
	 * Of course, it's not really safe since a process could take a lock
	 * just after us but at least it'll give some time to HAL and friends
	 * to finish what they're doing. 
	 * This was done because more than often backends wouldn't be able to 
	 * get a lock on a medium after a simulation. */
	result = parrillada_burn_can_use_drive_exclusively (burn, burner);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* actual running of task */
	result = parrillada_task_run (priv->task, &ret_error);

	/* let's see the results */
	if (result == PARRILLADA_BURN_OK) {
		g_signal_emit (burn,
			       parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
			       0,
			       1.0,
			       1.0,
			       -1L);
		return PARRILLADA_BURN_OK;
	}

	if (result != PARRILLADA_BURN_ERR
	|| !ret_error
	||  ret_error->domain != PARRILLADA_BURN_ERROR) {
		if (ret_error)
			g_propagate_error (error, ret_error);

		return result;
	}

	/* see if error is recoverable */
	error_code = ret_error->code;
	if (error_code == PARRILLADA_BURN_ERROR_IMAGE_JOLIET) {
		/* NOTE: this error can only come from the source when 
		 * burning on the fly => no need to recreate an imager */

		/* some files are not conforming to Joliet standard see
		 * if the user wants to carry on with a non joliet disc */
		result = parrillada_burn_ask_for_joliet (burn);
		if (result != PARRILLADA_BURN_OK) {
			if (ret_error)
				g_propagate_error (error, ret_error);

			return result;
		}

		g_error_free (ret_error);
		ret_error = NULL;
		goto start;
	}
	else if (error_code == PARRILLADA_BURN_ERROR_MEDIUM_NEED_RELOADING) {
		/* NOTE: this error can only come from the source when 
		 * burning on the fly => no need to recreate an imager */

		/* The source media (when copying on the fly) is empty 
		 * so ask the user to reload another media with data */
		g_error_free (ret_error);
		ret_error = NULL;

		result = parrillada_burn_reload_src_media (burn,
							error_code,
							error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == PARRILLADA_BURN_ERROR_SLOW_DMA) {
		guint64 rate;

		/* The whole system has just made a great effort. Sometimes it 
		 * helps to let it rest for a sec or two => that's what we do
		 * before retrying. (That's why usually cdrecord waits a little
		 * bit but sometimes it doesn't). Another solution would be to
		 * lower the speed a little (we could do both) */
		g_error_free (ret_error);
		ret_error = NULL;

		result = parrillada_burn_sleep (burn, 2000);
		if (result != PARRILLADA_BURN_OK)
			return result;

		has_slept = TRUE;

		/* set speed at 8x max and even less if speed  */
		rate = parrillada_burn_session_get_rate (priv->session);
		if (rate <= PARRILLADA_SPEED_TO_RATE_CD (8)) {
			rate = rate * 3 / 4;
			if (rate < CD_RATE)
				rate = CD_RATE;
		}
		else
			rate = PARRILLADA_SPEED_TO_RATE_CD (8);

		parrillada_burn_session_set_rate (priv->session, rate);
		goto start;
	}
	else if (error_code == PARRILLADA_BURN_ERROR_MEDIUM_SPACE) {
		/* NOTE: this error can only come from the dest drive */

		/* clean error and indicates this is a recoverable error */
		g_error_free (ret_error);
		ret_error = NULL;

		/* the space left on the media is insufficient (that's strange
		 * since we checked):
		 * the disc is either not rewritable or is too small anyway then
		 * we ask for a new media.
		 * It raises the problem of session merging. Indeed at this
		 * point an image can have been generated that was specifically
		 * generated for the inserted media. So if we have MERGE/APPEND
		 * that should fail.
		 */
		if (parrillada_burn_session_get_flags (priv->session) &
		   (PARRILLADA_BURN_FLAG_APPEND|PARRILLADA_BURN_FLAG_MERGE)) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_MEDIUM_SPACE,
				     "%s. %s",
				     _("Merging data is impossible with this disc"),
				     _("Not enough space available on the disc"));
			return PARRILLADA_BURN_ERR;
		}

		/* ask for the destination media reload */
		result = parrillada_burn_reload_dest_media (burn,
							 error_code,
							 error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code >= PARRILLADA_BURN_ERROR_MEDIUM_NONE
	     &&  error_code <=  PARRILLADA_BURN_ERROR_MEDIUM_NEED_RELOADING) {
		/* NOTE: these errors can only come from the dest drive */

		/* clean error and indicates this is a recoverable error */
		g_error_free (ret_error);
		ret_error = NULL;

		/* ask for the destination media reload */
		result = parrillada_burn_reload_dest_media (burn,
							 error_code,
							 error);

		if (result != PARRILLADA_BURN_OK)
			return result;

		goto start;
	}

	if (ret_error)
		g_propagate_error (error, ret_error);

	return PARRILLADA_BURN_ERR;
}

static ParrilladaBurnResult
parrillada_burn_install_missing (ParrilladaPluginErrorType error,
			      const gchar *details,
			      gpointer user_data)
{
	GValue instance_and_params [3];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (user_data));
	g_value_set_instance (instance_and_params, user_data);
	
	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_INT);
	g_value_set_int (instance_and_params + 1, error);

	instance_and_params [2].g_type = 0;
	g_value_init (instance_and_params + 2, G_TYPE_STRING);
	g_value_set_string (instance_and_params + 2, details);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_ERROR);

	g_signal_emitv (instance_and_params,
			parrillada_burn_signals [INSTALL_MISSING_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	return g_value_get_int (&return_value);		       
}

static ParrilladaBurnResult
parrillada_burn_list_missing (ParrilladaPluginErrorType type,
			   const gchar *detail,
			   gpointer user_data)
{
	GString *string = user_data;

	if (type == PARRILLADA_PLUGIN_ERROR_MISSING_APP ||
	    type == PARRILLADA_PLUGIN_ERROR_SYMBOLIC_LINK_APP ||
	    type == PARRILLADA_PLUGIN_ERROR_WRONG_APP_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing application */
		g_string_append_printf (string, _("%s (application)"), detail);
	}
	else if (type == PARRILLADA_PLUGIN_ERROR_MISSING_LIBRARY ||
	         type == PARRILLADA_PLUGIN_ERROR_LIBRARY_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing library */
		g_string_append_printf (string, _("%s (library)"), detail);
	}
	else if (type == PARRILLADA_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing GStreamer plugin */
		g_string_append_printf (string, _("%s (GStreamer plugin)"), detail);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_check_session_consistency (ParrilladaBurn *burn,
                                        ParrilladaTrackType *output,
					GError **error)
{
	ParrilladaBurnFlag flag;
	ParrilladaBurnFlag flags;
	ParrilladaBurnFlag retval;
	ParrilladaBurnResult result;
	ParrilladaTrackType *input = NULL;
	ParrilladaBurnFlag supported = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnFlag compulsory = PARRILLADA_BURN_FLAG_NONE;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	PARRILLADA_BURN_DEBUG (burn, "Checking session consistency");

	/* make sure there is a track in the session. */
	input = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (priv->session, input);

	if (parrillada_track_type_is_empty (input)) {
		parrillada_track_type_free (input);

		PARRILLADA_BURN_DEBUG (burn, "No track set");
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("There is no track to burn"));
		return PARRILLADA_BURN_ERR;
	}

	/* No need to check if a burner was set as this
	 * is done when locking. */

	/* save then wipe out flags from session to check them one by one */
	flags = parrillada_burn_session_get_flags (priv->session);
	parrillada_burn_session_set_flags (PARRILLADA_BURN_SESSION (priv->session), PARRILLADA_BURN_FLAG_NONE);

	if (!output || parrillada_track_type_get_has_medium (output))
		result = parrillada_burn_session_get_burn_flags (priv->session,
							      &supported,
							      &compulsory);
	else
		result = parrillada_caps_session_get_image_flags (input,
		                                              output,
		                                              &supported,
		                                              &compulsory);

	if (result != PARRILLADA_BURN_OK) {
		parrillada_track_type_free (input);
		return result;
	}

	for (flag = 1; flag < PARRILLADA_BURN_FLAG_LAST; flag <<= 1) {
		/* see if this flag was originally set */
		if (!(flags & flag))
			continue;

		/* Check each flag before re-adding it. Emit warnings to user
		 * to know if he wants to carry on for some flags when they are
		 * not supported; namely DUMMY. Other flags trigger an error.
		 * No need for BURNPROOF since that usually means it is just the
		 * media type that doesn't need it. */
		if (supported & flag) {
			parrillada_burn_session_add_flag (priv->session, flag);

			if (!output || parrillada_track_type_get_has_medium (output))
				result = parrillada_burn_session_get_burn_flags (priv->session,
									      &supported,
									      &compulsory);
			else
				result = parrillada_caps_session_get_image_flags (input,
									      output,
									      &supported,
									      &compulsory);
		}
		else {
			PARRILLADA_BURN_LOG_FLAGS (flag, "Flag set but not supported:");

			if (flag & PARRILLADA_BURN_FLAG_DUMMY) {
				/* This is simply a warning that it's not possible */

			}
			else if (flag & PARRILLADA_BURN_FLAG_MERGE) {
				parrillada_track_type_free (input);

				/* we pay attention to one flag in particular
				 * (MERGE) if it was set then it must be
				 * supported. Otherwise error out. */
				g_set_error (error,
					     PARRILLADA_BURN_ERROR,
					     PARRILLADA_BURN_ERROR_GENERAL,
					     "%s", _("Merging data is impossible with this disc"));
				return PARRILLADA_BURN_ERR;
			}
			/* No need to tell the user burnproof is not supported
			 * as these drives handle errors differently which makes
			 * burnproof useless for them. */
		}
	}
	parrillada_track_type_free (input);

	retval = parrillada_burn_session_get_flags (priv->session);
	if (retval != flags)
		PARRILLADA_BURN_LOG_FLAGS (retval, "Some flags were not supported. Corrected to");

	if (retval != (retval | compulsory)) {
		retval |= compulsory;
		PARRILLADA_BURN_LOG_FLAGS (retval, "Some compulsory flags were forgotten. Corrected to");
	}

	parrillada_burn_session_set_flags (priv->session, retval);
	PARRILLADA_BURN_LOG_FLAGS (retval, "Flags after checking =");

	/* Check missing applications/GStreamer plugins.
	 * This is the best place. */
	parrillada_burn_session_set_strict_support (PARRILLADA_BURN_SESSION (priv->session), TRUE);
	result = parrillada_burn_session_can_burn (priv->session, FALSE);
	parrillada_burn_session_set_strict_support (PARRILLADA_BURN_SESSION (priv->session), FALSE);

	if (result == PARRILLADA_BURN_OK)
		return result;

	result = parrillada_burn_session_can_burn (priv->session, FALSE);
	if (result != PARRILLADA_BURN_OK)
		return result;

	result = parrillada_session_foreach_plugin_error (priv->session,
	                                               parrillada_burn_install_missing,
	                                               burn);
	if (result != PARRILLADA_BURN_OK) {
		if (result != PARRILLADA_BURN_CANCEL) {
			GString *string;

			string = g_string_new (_("Please install the following required applications and libraries manually and try again:"));
			parrillada_session_foreach_plugin_error (priv->session,
			                                      parrillada_burn_list_missing,
	        			                      string);
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_MISSING_APP_AND_PLUGIN,
				     "%s", string->str);

			g_string_free (string, TRUE);
		}

		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_burn_check_data_loss (ParrilladaBurn *burn,
                              ParrilladaTrackType *temp_output,
                              GError **error)
{
	ParrilladaMedia media;
	ParrilladaBurnFlag flags;
	ParrilladaTrackType *input;
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	if (!temp_output) {
		ParrilladaTrackType *output;

		output = parrillada_track_type_new ();
		parrillada_burn_session_get_output_type (priv->session, output);
		if (!parrillada_track_type_get_has_medium (output)) {
			parrillada_track_type_free (output);
			return PARRILLADA_BURN_OK;
		}

		media = parrillada_track_type_get_medium_type (output);
		parrillada_track_type_free (output);
	}
	else {
		if (!parrillada_track_type_get_has_medium (temp_output))
			return PARRILLADA_BURN_OK;

		media = parrillada_track_type_get_medium_type (temp_output);
	}

	input = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (priv->session, input);
	flags = parrillada_burn_session_get_flags (priv->session);

	if (media & (PARRILLADA_MEDIUM_HAS_DATA|PARRILLADA_MEDIUM_HAS_AUDIO)) {
		if (flags & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE) {
			/* There is an error if APPEND was set since this disc is not
			 * supported without a prior blanking. */

			/* we warn the user is going to lose data even if in the case of
			 * DVD+/-RW we don't really blank the disc we rather overwrite */
			result = parrillada_burn_emit_signal (burn,
							   WARN_DATA_LOSS_SIGNAL,
							   PARRILLADA_BURN_CANCEL);
			if (result == PARRILLADA_BURN_NEED_RELOAD)
				goto reload;

			if (result != PARRILLADA_BURN_OK) {
				parrillada_track_type_free (input);
				return result;
			}
		}
		else {
			/* A few special warnings for the discs with data/audio on them
			 * that don't need prior blanking or can't be blanked */
			if ((media & PARRILLADA_MEDIUM_CD)
			&&  parrillada_track_type_get_has_stream (input)
			&& !PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (input))) {
				/* We'd rather blank and rewrite a disc rather than
				 * append audio to appendable disc. That's because audio
				 * tracks have little chance to be readable by common CD
				 * player as last tracks */
				result = parrillada_burn_emit_signal (burn,
								   WARN_AUDIO_TO_APPENDABLE_SIGNAL,
								   PARRILLADA_BURN_CANCEL);
				if (result == PARRILLADA_BURN_NEED_RELOAD)
					goto reload;

				if (result != PARRILLADA_BURN_OK) {
					parrillada_track_type_free (input);
					return result;
				}
			}

			/* NOTE: if input is AUDIO we don't care since the OS
			 * will load the last session of DATA anyway */
			if ((media & PARRILLADA_MEDIUM_HAS_DATA)
			&&   parrillada_track_type_get_has_data (input)
			&& !(flags & PARRILLADA_BURN_FLAG_MERGE)) {
				/* warn the users that their previous data
				 * session (s) will not be mounted by default by
				 * the OS and that it'll be invisible */
				result = parrillada_burn_emit_signal (burn,
								   WARN_PREVIOUS_SESSION_LOSS_SIGNAL,
								   PARRILLADA_BURN_CANCEL);

				if (result == PARRILLADA_BURN_RETRY) {
					/* Wipe out the current flags,
					 * Add a new one 
					 * Recheck the result */
					parrillada_burn_session_pop_settings (priv->session);
					parrillada_burn_session_push_settings (priv->session);
					parrillada_burn_session_add_flag (priv->session, PARRILLADA_BURN_FLAG_MERGE);
					result = parrillada_burn_check_session_consistency (burn, NULL, error);
					if (result != PARRILLADA_BURN_OK)
						return result;
				}

				if (result == PARRILLADA_BURN_NEED_RELOAD)
					goto reload;

				if (result != PARRILLADA_BURN_OK) {
					parrillada_track_type_free (input);
					return result;
				}
			}
		}
	}

	if (media & PARRILLADA_MEDIUM_REWRITABLE) {
		/* emits a warning for the user if it's a rewritable
		 * disc and he wants to write only audio tracks on it */

		/* NOTE: no need to error out here since the only thing
		 * we are interested in is if it is AUDIO or not or if
		 * the disc we are copying has audio tracks only or not */
		if (parrillada_track_type_get_has_stream (input)
		&& !PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (input))) {
			result = parrillada_burn_emit_signal (burn, 
			                                   WARN_REWRITABLE_SIGNAL,
			                                   PARRILLADA_BURN_CANCEL);

			if (result == PARRILLADA_BURN_NEED_RELOAD)
				goto reload;

			if (result != PARRILLADA_BURN_OK) {
				parrillada_track_type_free (input);
				return result;
			}
		}

		if (parrillada_track_type_get_has_medium (input)
		&& (parrillada_track_type_get_medium_type (input) & PARRILLADA_MEDIUM_HAS_AUDIO)) {
			result = parrillada_burn_emit_signal (burn,
			                                   WARN_REWRITABLE_SIGNAL,
			                                   PARRILLADA_BURN_CANCEL);

			if (result == PARRILLADA_BURN_NEED_RELOAD)
				goto reload;

			if (result != PARRILLADA_BURN_OK) {
				parrillada_track_type_free (input);
				return result;
			}
		}
	}

	parrillada_track_type_free (input);

	return PARRILLADA_BURN_OK;

reload:

	parrillada_track_type_free (input);

	result = parrillada_burn_reload_dest_media (burn, PARRILLADA_BURN_ERROR_NONE, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	return PARRILLADA_BURN_RETRY;
}

/* FIXME: at the moment we don't allow for mixed CD type */
static ParrilladaBurnResult
parrillada_burn_run_tasks (ParrilladaBurn *burn,
			gboolean erase_allowed,
                        ParrilladaTrackType *temp_output,
                        gboolean *dummy_session,
			GError **error)
{
	ParrilladaBurnResult result;
	GSList *tasks, *next, *iter;
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (burn);

	/* push the session settings to keep the original session untainted */
	parrillada_burn_session_push_settings (priv->session);

	/* check flags consistency */
	result = parrillada_burn_check_session_consistency (burn, temp_output, error);
	if (result != PARRILLADA_BURN_OK) {
		parrillada_burn_session_pop_settings (priv->session);
		return result;
	}

	/* performed some additional tests that can only be performed at this
	 * point. They are mere warnings. */
	result = parrillada_burn_check_data_loss (burn, temp_output, error);
	if (result != PARRILLADA_BURN_OK) {
		parrillada_burn_session_pop_settings (priv->session);
		return result;
	}

	tasks = parrillada_burn_caps_new_task (priv->caps,
					    priv->session,
	                                    temp_output,
					    error);
	if (!tasks) {
		parrillada_burn_session_pop_settings (priv->session);
		return PARRILLADA_BURN_NOT_SUPPORTED;
	}

	priv->tasks_done = 0;
	priv->task_nb = g_slist_length (tasks);
	PARRILLADA_BURN_LOG ("%i tasks to perform", priv->task_nb);

	/* run all imaging tasks first */
	for (iter = tasks; iter; iter = next) {
		goffset len = 0;
		ParrilladaDrive *drive;
		ParrilladaMedium *medium;
		ParrilladaTaskAction action;

		next = iter->next;
		priv->task = iter->data;
		tasks = g_slist_remove (tasks, priv->task);

		g_signal_connect (priv->task,
				  "progress-changed",
				  G_CALLBACK (parrillada_burn_progress_changed),
				  burn);
		g_signal_connect (priv->task,
				  "action-changed",
				  G_CALLBACK (parrillada_burn_action_changed),
				  burn);

		/* see what type of task it is. It could be a blank/erase one. */
		/* FIXME!
		 * If so then that's time to test the size of the image against
		 * the size of the disc since erasing/formatting is always left
		 * for the end, just before burning. We would not like to 
		 * blank a disc and tell the user right after that the size of
		 * the disc is not enough. */
		action = parrillada_task_ctx_get_action (PARRILLADA_TASK_CTX (priv->task));
		if (action == PARRILLADA_TASK_ACTION_ERASE) {
			ParrilladaTrackType *type;

			/* FIXME: how could it be possible for a drive to test
			 * with a CLOSED CDRW for example. Maybe we should
			 * format/blank anyway. */

			/* This is to avoid blanking a medium without knowing
			 * if the data will fit on it. At this point we do know 
			 * what the size of the data is going to be. */
			type = parrillada_track_type_new ();
			parrillada_burn_session_get_input_type (priv->session, type);
			if (parrillada_track_type_get_has_image (type)
			||  parrillada_track_type_get_has_medium (type)) {
				ParrilladaMedium *medium;
				goffset session_sec = 0;
				goffset medium_sec = 0;

				medium = parrillada_drive_get_medium (priv->dest);
				parrillada_medium_get_capacity (medium,
							     NULL,
							     &medium_sec);

				parrillada_burn_session_get_size (priv->session,
							       &session_sec,
							       NULL);

				if (session_sec > medium_sec) {
					PARRILLADA_BURN_LOG ("Not enough space on medium %"G_GOFFSET_FORMAT"/%"G_GOFFSET_FORMAT, session_sec, medium_sec);
					result = parrillada_burn_reload_dest_media (burn,
					                                         PARRILLADA_BURN_ERROR_MEDIUM_SPACE,
					                                         error);
					if (result == PARRILLADA_BURN_OK)
						result = PARRILLADA_BURN_RETRY;

					break;
				}
			}
			parrillada_track_type_free (type);

			/* This is to avoid a potential problem when running a 
			 * dummy session first. When running dummy session the 
			 * media gets erased if need be. Since it is not
			 * reloaded afterwards, for parrillada it has still got 
			 * data on it when we get to the real recording. */
			if (erase_allowed) {
				result = parrillada_burn_run_eraser (burn, error);
				if (result == PARRILLADA_BURN_CANCEL)
					return result;

				/* If the erasing process did not work then do
				 * not fail and cancel the entire session but
				 * ask the user if he wants to insert another
				 * disc instead. */
				if (result != PARRILLADA_BURN_OK) {
					ParrilladaBurnResult local_result;

					local_result = parrillada_burn_emit_signal (burn,
					                                         BLANK_FAILURE_SIGNAL,
					                                         PARRILLADA_BURN_ERR);
					if (local_result == PARRILLADA_BURN_OK) {
						local_result = parrillada_burn_reload_dest_media (burn,
						                                               PARRILLADA_BURN_ERROR_NONE,
						                                               NULL);
						if (local_result == PARRILLADA_BURN_OK)
							result = PARRILLADA_BURN_RETRY;
					}

					break;
				}

				/* Since we blanked/formatted we need to recheck the burn 
				 * flags with the new medium type as some flags could have
				 * been given the benefit of the double (MULTI with a CLOSED
				 * CD for example). Recheck the original flags as they were
				 * passed. */
				/* FIXME: for some important flags we should warn the user
				 * that it won't be possible */
				parrillada_burn_session_pop_settings (priv->session);
				parrillada_burn_session_push_settings (priv->session);
				result = parrillada_burn_check_session_consistency (burn, temp_output,error);
				if (result != PARRILLADA_BURN_OK)
					break;
			}
			else
				result = PARRILLADA_BURN_OK;

			g_object_unref (priv->task);
			priv->task = NULL;
			priv->tasks_done ++;

			continue;
		}

		/* Init the task and set the task output size. The task should
		 * then check that the disc has enough space. If the output is
		 * to the hard drive it will be done afterwards when not in fake
		 * mode. */
		result = parrillada_burn_run_imager (burn, TRUE, error);
		if (result != PARRILLADA_BURN_OK)
			break;

		/* try to get the output size */
		parrillada_task_ctx_get_session_output_size (PARRILLADA_TASK_CTX (priv->task),
							  &len,
							  NULL);

		drive = parrillada_burn_session_get_burner (priv->session);
		medium = parrillada_drive_get_medium (drive);

		if (parrillada_burn_session_get_flags (priv->session) & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND))
			priv->session_start = parrillada_medium_get_next_writable_address (medium);
		else
			priv->session_start = 0;

		priv->session_end = priv->session_start + len;

		PARRILLADA_BURN_LOG ("Burning from %lld to %lld",
				  priv->session_start,
				  priv->session_end);

		/* see if we reached a recording task: it's the last task */
		if (!next) {
			if (!parrillada_burn_session_is_dest_file (priv->session)) {
				*dummy_session = (parrillada_burn_session_get_flags (priv->session) & PARRILLADA_BURN_FLAG_DUMMY);
				result = parrillada_burn_run_recorder (burn, error);
			}
			else
				result = parrillada_burn_run_imager (burn, FALSE, error);

			if (result == PARRILLADA_BURN_OK)
				priv->tasks_done ++;

			break;
		}

		/* run the imager */
		result = parrillada_burn_run_imager (burn, FALSE, error);
		if (result != PARRILLADA_BURN_OK)
			break;

		g_object_unref (priv->task);
		priv->task = NULL;
		priv->tasks_done ++;
	}

	/* restore the session settings. Keep the used flags
	 * nevertheless to make sure we actually use the flags that were
	 * set after checking for session consistency. */
	parrillada_burn_session_pop_settings (priv->session);

	if (priv->task) {
		g_object_unref (priv->task);
		priv->task = NULL;
	}

	g_slist_foreach (tasks, (GFunc) g_object_unref, NULL);
	g_slist_free (tasks);

	return result;
}

static ParrilladaBurnResult
parrillada_burn_check_real (ParrilladaBurn *self,
			 ParrilladaTrack *track,
			 GError **error)
{
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv;
	ParrilladaChecksumType checksum_type;

	priv = PARRILLADA_BURN_PRIVATE (self);

	PARRILLADA_BURN_LOG ("Starting to check track integrity");

	checksum_type = parrillada_track_get_checksum_type (track);

	/* if the input is a DISC and there isn't any checksum specified that 
	 * means the checksum file is on the disc. */
	medium = parrillada_drive_get_medium (priv->dest);

	/* get the task and run it skip it otherwise */
	priv->task = parrillada_burn_caps_new_checksuming_task (priv->caps,
							     priv->session,
							     NULL);
	if (priv->task) {
		priv->task_nb = 1;
		priv->tasks_done = 0;
		g_signal_connect (priv->task,
				  "progress-changed",
				  G_CALLBACK (parrillada_burn_progress_changed),
				  self);
		g_signal_connect (priv->task,
				  "action-changed",
				  G_CALLBACK (parrillada_burn_action_changed),
				  self);


		/* make sure one last time it is not mounted IF and only IF the
		 * checksum type is NOT FILE_MD5 */
		/* it seems to work without unmounting ... */
		/* if (medium
		 * &&  parrillada_volume_is_mounted (PARRILLADA_VOLUME (medium))
		 * && !parrillada_volume_umount (PARRILLADA_VOLUME (medium), TRUE, NULL)) {
		 *	g_set_error (error,
		 *		     PARRILLADA_BURN_ERROR,
		 *		     PARRILLADA_BURN_ERROR_DRIVE_BUSY,
		 *		     "%s. %s",
		 *		     _("The drive is busy"),
		 *		     _("Make sure another application is not using it"));
		 *	return PARRILLADA_BURN_ERR;
		 * }
		 */

		result = parrillada_task_run (priv->task, error);
		g_signal_emit (self,
			       parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL],
			       0,
			       1.0,
			       1.0,
			       -1L);

		g_object_unref (priv->task);
		priv->task = NULL;
	}
	else {
		PARRILLADA_BURN_LOG ("The track cannot be checked");
		return PARRILLADA_BURN_OK;
	}

	return result;
}

static void
parrillada_burn_unset_checksums (ParrilladaBurn *self)
{
	GSList *tracks;
	ParrilladaTrackType *type;
	ParrilladaBurnPrivate *priv;

	priv = PARRILLADA_BURN_PRIVATE (self);

	tracks = parrillada_burn_session_get_tracks (priv->session);
	type = parrillada_track_type_new ();
	for (; tracks; tracks = tracks->next) {
		ParrilladaTrack *track;

		track = tracks->data;
		parrillada_track_get_track_type (track, type);
		if (!parrillada_track_type_get_has_image (type)
		&& !parrillada_track_type_get_has_medium (type))
			parrillada_track_set_checksum (track,
						    PARRILLADA_CHECKSUM_NONE,
						    NULL);
	}

	parrillada_track_type_free (type);
}

static ParrilladaBurnResult
parrillada_burn_record_session (ParrilladaBurn *burn,
			     gboolean erase_allowed,
                             ParrilladaTrackType *temp_output,
			     GError **error)
{
	gboolean dummy_session = FALSE;
	const gchar *checksum = NULL;
	ParrilladaTrack *track = NULL;
	ParrilladaChecksumType type;
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result;
	GError *ret_error = NULL;
	ParrilladaMedium *medium;
	GSList *tracks;

	priv = PARRILLADA_BURN_PRIVATE (burn);

	/* unset checksum since no image has the exact
	 * same even if it is created from the same files */
	parrillada_burn_unset_checksums (burn);

	do {
		if (ret_error) {
			g_error_free (ret_error);
			ret_error = NULL;
		}

		result = parrillada_burn_run_tasks (burn,
						 erase_allowed,
		                                 temp_output,
		                                 &dummy_session,
						 &ret_error);
	} while (result == PARRILLADA_BURN_RETRY);

	if (result != PARRILLADA_BURN_OK) {
		/* handle errors */
		if (ret_error) {
			g_propagate_error (error, ret_error);
			ret_error = NULL;
		}

		return result;
	}

	if (parrillada_burn_session_is_dest_file (priv->session))
		return PARRILLADA_BURN_OK;

	if (dummy_session) {
		/* if we are in dummy mode and successfully completed then:
		 * - no need to checksum the media afterward (done later)
		 * - no eject to have automatic real burning */

		PARRILLADA_BURN_DEBUG (burn, "Dummy session successfully finished");

		/* recording was successful, so tell it */
		parrillada_burn_action_changed_real (burn, PARRILLADA_BURN_ACTION_FINISHED);

		/* need to try again but this time for real */
		result = parrillada_burn_emit_signal (burn,
						   DUMMY_SUCCESS_SIGNAL,
						   PARRILLADA_BURN_OK);
		if (result != PARRILLADA_BURN_OK)
			return result;

		/* unset checksum since no image has the exact same even if it
		 * is created from the same files */
		parrillada_burn_unset_checksums (burn);

		/* remove dummy flag and restart real burning calling ourselves
		 * NOTE: don't bother to push the session. We know the changes 
		 * that were made. */
		parrillada_burn_session_remove_flag (priv->session, PARRILLADA_BURN_FLAG_DUMMY);
		result = parrillada_burn_record_session (burn, FALSE, temp_output, error);
		parrillada_burn_session_add_flag (priv->session, PARRILLADA_BURN_FLAG_DUMMY);

		return result;
	}

	/* see if we have a checksum generated for the session if so use
	 * it to check if the recording went well remaining on the top of
	 * the session should be the last track burnt/imaged */
	tracks = parrillada_burn_session_get_tracks (priv->session);
	if (g_slist_length (tracks) != 1)
		return PARRILLADA_BURN_OK;

	track = tracks->data;
	type = parrillada_track_get_checksum_type (track);
	if (type == PARRILLADA_CHECKSUM_MD5
	||  type == PARRILLADA_CHECKSUM_SHA1
	||  type == PARRILLADA_CHECKSUM_SHA256)
		checksum = parrillada_track_get_checksum (track);
	else if (type == PARRILLADA_CHECKSUM_MD5_FILE)
		checksum = PARRILLADA_MD5_FILE;
	else if (type == PARRILLADA_CHECKSUM_SHA1_FILE)
		checksum = PARRILLADA_SHA1_FILE;
	else if (type == PARRILLADA_CHECKSUM_SHA256_FILE)
		checksum = PARRILLADA_SHA256_FILE;
	else
		return PARRILLADA_BURN_OK;

	/* recording was successful, so tell it */
	parrillada_burn_action_changed_real (burn, PARRILLADA_BURN_ACTION_FINISHED);

	/* the idea is to push a new track on the stack with
	 * the current disc burnt and the checksum generated
	 * during the session recording */
	parrillada_burn_session_push_tracks (priv->session);

	track = PARRILLADA_TRACK (parrillada_track_disc_new ());
	parrillada_track_set_checksum (PARRILLADA_TRACK (track),
	                            type,
	                            checksum);

	parrillada_track_disc_set_drive (PARRILLADA_TRACK_DISC (track), parrillada_burn_session_get_burner (priv->session));
	parrillada_burn_session_add_track (priv->session, track, NULL);

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. ParrilladaBurnSession refs it. */
	g_object_unref (track);

	PARRILLADA_BURN_DEBUG (burn, "Preparing to checksum (type %i %s)", type, checksum);

	/* reprobe the medium and wait for it to be probed */
	result = parrillada_burn_reprobe (burn);
	if (result != PARRILLADA_BURN_OK) {
		parrillada_burn_session_pop_tracks (priv->session);
		return result;
	}

	medium = parrillada_drive_get_medium (priv->dest);

	/* Why do we do this?
	 * Because for a lot of medium types the size
	 * of the track return is not the real size of the
	 * data that was written; examples
	 * - CD that was written in SAO mode 
	 * - a DVD-R which usually aligns its track size
	 *   to a 16 block boundary
	 */
	if (type == PARRILLADA_CHECKSUM_MD5
	||  type == PARRILLADA_CHECKSUM_SHA1
	||  type == PARRILLADA_CHECKSUM_SHA256) {
		GValue *value;

		/* get the last written track address */
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_UINT64);

		PARRILLADA_BURN_LOG ("Start of last written track address == %lli", priv->session_start);
		g_value_set_uint64 (value, priv->session_start);
		parrillada_track_tag_add (track,
				       PARRILLADA_TRACK_MEDIUM_ADDRESS_START_TAG,
				       value);

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_UINT64);

		PARRILLADA_BURN_LOG ("End of last written track address == %lli", priv->session_end);
		g_value_set_uint64 (value, priv->session_end);
		parrillada_track_tag_add (track,
				       PARRILLADA_TRACK_MEDIUM_ADDRESS_END_TAG,
				       value);
	}

	result = parrillada_burn_check_real (burn, track, error);
	parrillada_burn_session_pop_tracks (priv->session);

	if (result == PARRILLADA_BURN_CANCEL) {
		/* change the result value so we won't stop here if there are 
		 * other copies to be made */
		result = PARRILLADA_BURN_OK;
	}

	return result;
}

/**
 * parrillada_burn_check:
 * @burn: a #ParrilladaBurn
 * @session: a #ParrilladaBurnSession
 * @error: a #GError
 *
 * Checks the integrity of a medium according to the parameters
 * set in @session. The medium must be inserted in the #ParrilladaDrive
 * set as the source of a #ParrilladaTrackDisc track inserted in @session.
 *
 * Return value: a #ParrilladaBurnResult. The result of the operation. 
 * PARRILLADA_BURN_OK if it was successful.
 **/

ParrilladaBurnResult
parrillada_burn_check (ParrilladaBurn *self,
		    ParrilladaBurnSession *session,
		    GError **error)
{
	GSList *tracks;
	ParrilladaTrack *track;
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN (self), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_PRIVATE (self);

	g_object_ref (session);
	priv->session = session;

	/* NOTE: no need to check for parameters here;
	 * that'll be done when asking for a task */
	tracks = parrillada_burn_session_get_tracks (priv->session);
	if (g_slist_length (tracks) != 1) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("Only one track at a time can be checked"));
		return PARRILLADA_BURN_ERR;
	}

	track = tracks->data;

	/* if the input is a DISC, ask/check there is one and lock it (as dest) */
	if (PARRILLADA_IS_TRACK_IMAGE (track)) {
		/* make sure there is a disc. If not, ask one and lock it */
		result = parrillada_burn_lock_checksum_media (self, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	parrillada_burn_powermanagement (self, TRUE);

	result = parrillada_burn_check_real (self, track, error);

	parrillada_burn_powermanagement (self, FALSE);

	if (result == PARRILLADA_BURN_OK)
		result = parrillada_burn_unlock_medias (self, error);
	else
		parrillada_burn_unlock_medias (self, NULL);

	/* no need to check the result of the comparison, it's set in session */

	if (result == PARRILLADA_BURN_OK)
		parrillada_burn_action_changed_real (self,
		                                  PARRILLADA_BURN_ACTION_FINISHED);

	/* NOTE: unref session only AFTER drives are unlocked */
	priv->session = NULL;
	g_object_unref (session);

	return result;
}

static ParrilladaBurnResult
parrillada_burn_same_src_dest_image (ParrilladaBurn *self,
				  GError **error)
{
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv;
	ParrilladaTrackType *output = NULL;

	/* we can't create a proper list of tasks here since we don't know the
	 * dest media type yet. So we try to find an intermediate image type and
	 * add it to the session as output */
	priv = PARRILLADA_BURN_PRIVATE (self);

	/* get the first possible format */
	output = parrillada_track_type_new ();
	result = parrillada_burn_session_get_tmp_image_type_same_src_dest (priv->session, output);
	if (result != PARRILLADA_BURN_OK) {
		parrillada_track_type_free (output);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("No format for the temporary image could be found"));
		return result;
	}

	/* lock drive */
	result = parrillada_burn_lock_src_media (self, error);
	if (result != PARRILLADA_BURN_OK)
		goto end;

	/* run */
	result = parrillada_burn_record_session (self, TRUE, output, error);

	/* Check the results right now. If there was
	 * an error the source medium will be dealt
	 * with in parrillada_burn_record () anyway 
	 * with parrillada_burn_unlock_medias () */
	if (result != PARRILLADA_BURN_OK)
		goto end;

	/* reset everything back to normal */
	result = parrillada_burn_eject (self, priv->src, error);
	if (result != PARRILLADA_BURN_OK)
		goto end;

	parrillada_burn_unlock_src_media (self, NULL);

	/* There should be (a) track(s) at the top of
	 * the session stack so no need to create a
	 * new one */

	parrillada_burn_action_changed_real (self, PARRILLADA_BURN_ACTION_FINISHED);

end:

	if (output)
		parrillada_track_type_free (output);

	return result;
}

static ParrilladaBurnResult
parrillada_burn_same_src_dest_reload_medium (ParrilladaBurn *burn,
					  GError **error)
{
	ParrilladaBurnError berror;
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaMedia required_media;

	priv = PARRILLADA_BURN_PRIVATE (burn);

	PARRILLADA_BURN_LOG ("Reloading medium after copy");

	/* Now there is the problem of flags... This really is a special
	 * case. we need to try to adjust the flags to the media type
	 * just after a new one is detected. For example there could be
	 * BURNPROOF/DUMMY whereas they are not supported for DVD+RW. So
	 * we need to be lenient. */

	/* Eject and ask the user to reload a disc */
	required_media = parrillada_burn_session_get_required_media_type (priv->session);
	required_media &= (PARRILLADA_MEDIUM_WRITABLE|
			   PARRILLADA_MEDIUM_CD|
			   PARRILLADA_MEDIUM_DVD|
			   PARRILLADA_MEDIUM_BD);

	/* There is sometimes no way to determine which type of media is
	 * required since some flags (that will be adjusted afterwards)
	 * prevent to reach some media type like BURNPROOF and DVD+RW. */
	if (required_media == PARRILLADA_MEDIUM_NONE)
		required_media = PARRILLADA_MEDIUM_WRITABLE;

	berror = PARRILLADA_BURN_WARNING_INSERT_AFTER_COPY;

again:

	result = parrillada_burn_ask_for_dest_media (burn,
						  berror,
						  required_media,
						  error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* Note: we don't need to update the flags anymore
	 * as they are updated in parrillada_burn_run_tasks ()
	 * anyway. */

	if (result != PARRILLADA_BURN_OK) {
		/* Tell the user his/her disc is not supported and reload */
		berror = PARRILLADA_BURN_ERROR_MEDIUM_INVALID;
		goto again;
	}

	result = parrillada_burn_lock_dest_media (burn, &berror, error);
	if (result == PARRILLADA_BURN_CANCEL)
		return result;

	if (result != PARRILLADA_BURN_OK) {
		/* Tell the user his/her disc is not supported and reload */
		goto again;
	}

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_burn_record:
 * @burn: a #ParrilladaBurn
 * @session: a #ParrilladaBurnSession
 * @error: a #GError
 *
 * Burns or creates a disc image according to the parameters
 * set in @session.
 *
 * Return value: a #ParrilladaBurnResult. The result of the operation. 
 * PARRILLADA_BURN_OK if it was successful.
 **/

ParrilladaBurnResult 
parrillada_burn_record (ParrilladaBurn *burn,
		     ParrilladaBurnSession *session,
		     GError **error)
{
	ParrilladaTrackType *type = NULL;
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_BURN (burn), PARRILLADA_BURN_ERR);
	g_return_val_if_fail (PARRILLADA_IS_BURN_SESSION (session), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_PRIVATE (burn);

	/* make sure we're ready */
	if (parrillada_burn_session_get_status (session, NULL) != PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_ERR;

	g_object_ref (session);
	priv->session = session;

	parrillada_burn_powermanagement (burn, TRUE);

	/* say to the whole world we started */
	parrillada_burn_action_changed_real (burn, PARRILLADA_BURN_ACTION_PREPARING);

	if (parrillada_burn_session_same_src_dest_drive (session)) {
		/* This is a special case */
		result = parrillada_burn_same_src_dest_image (burn, error);
		if (result != PARRILLADA_BURN_OK)
			goto end;

		result = parrillada_burn_same_src_dest_reload_medium (burn, error);
		if (result != PARRILLADA_BURN_OK)
			goto end;
	}
	else if (!parrillada_burn_session_is_dest_file (session)) {
		ParrilladaBurnError berror = PARRILLADA_BURN_ERROR_NONE;

		/* do some drive locking quite early to make sure we have a
		 * media in the drive so that we'll have all the necessary
		 * information */
		result = parrillada_burn_lock_dest_media (burn, &berror, error);
		while (result == PARRILLADA_BURN_NEED_RELOAD) {
			ParrilladaMedia required_media;

			required_media = parrillada_burn_session_get_required_media_type (priv->session);
			if (required_media == PARRILLADA_MEDIUM_NONE)
				required_media = PARRILLADA_MEDIUM_WRITABLE;

			result = parrillada_burn_ask_for_dest_media (burn,
								  berror,
								  required_media,
								  error);
			if (result != PARRILLADA_BURN_OK)
				goto end;

			result = parrillada_burn_lock_dest_media (burn, &berror, error);
		}

		if (result != PARRILLADA_BURN_OK)
			goto end;
	}

	type = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (session, type);
	if (parrillada_track_type_get_has_medium (type)) {
		result = parrillada_burn_lock_src_media (burn, error);
		if (result != PARRILLADA_BURN_OK)
			goto end;
	}

	/* burn the session except if dummy session */
	result = parrillada_burn_record_session (burn, TRUE, NULL, error);

end:

	parrillada_track_type_free (type);

	if (result == PARRILLADA_BURN_OK)
		result = parrillada_burn_unlock_medias (burn, error);
	else
		parrillada_burn_unlock_medias (burn, NULL);

	if (error && (*error) == NULL
	&& (result == PARRILLADA_BURN_NOT_READY
	||  result == PARRILLADA_BURN_NOT_SUPPORTED
	||  result == PARRILLADA_BURN_RUNNING
	||  result == PARRILLADA_BURN_NOT_RUNNING)) {
		PARRILLADA_BURN_LOG ("Internal error with result %i", result);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s", _("An internal error occurred"));
	}

	if (result == PARRILLADA_BURN_CANCEL) {
		PARRILLADA_BURN_DEBUG (burn, "Session cancelled by user");
	}
	else if (result != PARRILLADA_BURN_OK) {
		if (error && (*error)) {
			PARRILLADA_BURN_DEBUG (burn,
					    "Session error : %s",
					    (*error)->message);
		}
		else
			PARRILLADA_BURN_DEBUG (burn, "Session error : unknown");
	}
	else {
		PARRILLADA_BURN_DEBUG (burn, "Session successfully finished");
		parrillada_burn_action_changed_real (burn,
		                                  PARRILLADA_BURN_ACTION_FINISHED);
	}

	parrillada_burn_powermanagement (burn, FALSE);

	/* release session */
	g_object_unref (priv->session);
	priv->session = NULL;

	return result;
}

static ParrilladaBurnResult
parrillada_burn_blank_real (ParrilladaBurn *burn, GError **error)
{
	ParrilladaBurnResult result;
	ParrilladaBurnPrivate *priv;

	priv = PARRILLADA_BURN_PRIVATE (burn);

	priv->task = parrillada_burn_caps_new_blanking_task (priv->caps,
							  priv->session,
							  error);
	if (!priv->task)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	g_signal_connect (priv->task,
			  "progress-changed",
			  G_CALLBACK (parrillada_burn_progress_changed),
			  burn);
	g_signal_connect (priv->task,
			  "action-changed",
			  G_CALLBACK (parrillada_burn_action_changed),
			  burn);

	result = parrillada_burn_run_eraser (burn, error);
	g_object_unref (priv->task);
	priv->task = NULL;

	return result;
}

/**
 * parrillada_burn_blank:
 * @burn: a #ParrilladaBurn
 * @session: a #ParrilladaBurnSession
 * @error: a #GError
 *
 * Blanks a medium according to the parameters
 * set in @session. The medium must be inserted in the #ParrilladaDrive
 * set with parrillada_burn_session_set_burner ().
 *
 * Return value: a #ParrilladaBurnResult. The result of the operation. 
 * PARRILLADA_BURN_OK if it was successful.
 **/

ParrilladaBurnResult
parrillada_burn_blank (ParrilladaBurn *burn,
		    ParrilladaBurnSession *session,
		    GError **error)
{
	ParrilladaBurnPrivate *priv;
	ParrilladaBurnResult result;
	GError *ret_error = NULL;

	g_return_val_if_fail (burn != NULL, PARRILLADA_BURN_ERR);
	g_return_val_if_fail (session != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_PRIVATE (burn);

	g_object_ref (session);
	priv->session = session;

	parrillada_burn_powermanagement (burn, TRUE);

	/* we wait for the insertion of a media and lock it */
	result = parrillada_burn_lock_rewritable_media (burn, error);
	if (result != PARRILLADA_BURN_OK)
		goto end;

	result = parrillada_burn_blank_real (burn, &ret_error);
	while (result == PARRILLADA_BURN_ERR &&
	       ret_error &&
	       ret_error->code == PARRILLADA_BURN_ERROR_MEDIUM_NOT_REWRITABLE) {
		g_error_free (ret_error);
		ret_error = NULL;

		result = parrillada_burn_ask_for_dest_media (burn,
							  PARRILLADA_BURN_ERROR_MEDIUM_NOT_REWRITABLE,
							  PARRILLADA_MEDIUM_REWRITABLE|
							  PARRILLADA_MEDIUM_HAS_DATA,
							  error);
		if (result != PARRILLADA_BURN_OK)
			break;

		result = parrillada_burn_lock_rewritable_media (burn, error);
		if (result != PARRILLADA_BURN_OK)
			break;

		result = parrillada_burn_blank_real (burn, &ret_error);
	}

end:
	if (ret_error)
		g_propagate_error (error, ret_error);

	if (result == PARRILLADA_BURN_OK && !ret_error)
		result = parrillada_burn_unlock_medias (burn, error);
	else
		parrillada_burn_unlock_medias (burn, NULL);

	if (result == PARRILLADA_BURN_OK)
		parrillada_burn_action_changed_real (burn, PARRILLADA_BURN_ACTION_FINISHED);

	parrillada_burn_powermanagement (burn, FALSE);

	/* release session */
	g_object_unref (priv->session);
	priv->session = NULL;

	return result;
}

/**
 * parrillada_burn_cancel:
 * @burn: a #ParrilladaBurn
 * @protect: a #gboolean
 *
 * Cancels any ongoing operation. If @protect is TRUE then
 * cancellation will not take place for a "critical" task, a task whose interruption
 * could damage the medium or the drive.
 *
 * Return value: a #ParrilladaBurnResult. The result of the operation. 
 * PARRILLADA_BURN_OK if it was successful.
 **/

ParrilladaBurnResult
parrillada_burn_cancel (ParrilladaBurn *burn, gboolean protect)
{
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;
	ParrilladaBurnPrivate *priv;

	g_return_val_if_fail (PARRILLADA_BURN (burn), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_BURN_PRIVATE (burn);

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->sleep_loop) {
		g_main_loop_quit (priv->sleep_loop);
		priv->sleep_loop = NULL;
	}

	if (priv->dest)
		parrillada_drive_cancel_current_operation (priv->dest);

	if (priv->src)
		parrillada_drive_cancel_current_operation (priv->src);

	if (priv->task && parrillada_task_is_running (priv->task))
		result = parrillada_task_cancel (priv->task, protect);

	return result;
}

static void
parrillada_burn_finalize (GObject *object)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (object);

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->sleep_loop) {
		g_main_loop_quit (priv->sleep_loop);
		priv->sleep_loop = NULL;
	}

	if (priv->task) {
		g_object_unref (priv->task);
		priv->task = NULL;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->caps)
		g_object_unref (priv->caps);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_burn_class_init (ParrilladaBurnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaBurnPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_burn_finalize;

	parrillada_burn_signals [ASK_DISABLE_JOLIET_SIGNAL] =
		g_signal_new ("disable_joliet",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       ask_disable_joliet),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        parrillada_burn_signals [WARN_DATA_LOSS_SIGNAL] =
		g_signal_new ("warn_data_loss",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       warn_data_loss),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        parrillada_burn_signals [WARN_PREVIOUS_SESSION_LOSS_SIGNAL] =
		g_signal_new ("warn_previous_session_loss",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       warn_previous_session_loss),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        parrillada_burn_signals [WARN_AUDIO_TO_APPENDABLE_SIGNAL] =
		g_signal_new ("warn_audio_to_appendable",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       warn_audio_to_appendable),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
	parrillada_burn_signals [WARN_REWRITABLE_SIGNAL] =
		g_signal_new ("warn_rewritable",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       warn_rewritable),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
	parrillada_burn_signals [INSERT_MEDIA_REQUEST_SIGNAL] =
		g_signal_new ("insert_media",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       insert_media_request),
			      NULL, NULL,
			      parrillada_marshal_INT__OBJECT_INT_INT,
			      G_TYPE_INT, 
			      3,
			      PARRILLADA_TYPE_DRIVE,
			      G_TYPE_INT,
			      G_TYPE_INT);
	parrillada_burn_signals [LOCATION_REQUEST_SIGNAL] =
		g_signal_new ("location-request",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       location_request),
			      NULL, NULL,
			      parrillada_marshal_INT__POINTER_BOOLEAN,
			      G_TYPE_INT, 
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_INT);
	parrillada_burn_signals [PROGRESS_CHANGED_SIGNAL] =
		g_signal_new ("progress_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       progress_changed),
			      NULL, NULL,
			      parrillada_marshal_VOID__DOUBLE_DOUBLE_LONG,
			      G_TYPE_NONE, 
			      3,
			      G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE,
			      G_TYPE_LONG);
        parrillada_burn_signals [ACTION_CHANGED_SIGNAL] =
		g_signal_new ("action_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       action_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 
			      1,
			      G_TYPE_INT);
        parrillada_burn_signals [DUMMY_SUCCESS_SIGNAL] =
		g_signal_new ("dummy_success",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       dummy_success),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        parrillada_burn_signals [EJECT_FAILURE_SIGNAL] =
		g_signal_new ("eject_failure",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       eject_failure),
			      NULL, NULL,
			      parrillada_marshal_INT__OBJECT,
			      G_TYPE_INT, 1,
		              PARRILLADA_TYPE_DRIVE);
        parrillada_burn_signals [BLANK_FAILURE_SIGNAL] =
		g_signal_new ("blank_failure",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       blank_failure),
			      NULL, NULL,
			      parrillada_marshal_INT__VOID,
			      G_TYPE_INT, 0,
		              G_TYPE_NONE);
	parrillada_burn_signals [INSTALL_MISSING_SIGNAL] =
		g_signal_new ("install_missing",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ParrilladaBurnClass,
					       install_missing),
			      NULL, NULL,
			      parrillada_marshal_INT__INT_STRING,
			      G_TYPE_INT, 2,
		              G_TYPE_INT,
			      G_TYPE_STRING);
}

static void
parrillada_burn_init (ParrilladaBurn *obj)
{
	ParrilladaBurnPrivate *priv = PARRILLADA_BURN_PRIVATE (obj);

	priv->caps = parrillada_burn_caps_get_default ();
}

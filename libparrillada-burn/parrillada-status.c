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


#include "parrillada-status.h"


typedef struct _ParrilladaStatusPrivate ParrilladaStatusPrivate;
struct _ParrilladaStatusPrivate
{
	ParrilladaBurnResult res;
	GError * error;
	gdouble progress;
	gchar * current_action;
};

#define PARRILLADA_STATUS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_STATUS, ParrilladaStatusPrivate))

G_DEFINE_TYPE (ParrilladaStatus, parrillada_status, G_TYPE_OBJECT);


/**
 * parrillada_status_new:
 *
 * Creates a new #ParrilladaStatus object.
 *
 * Return value: a #ParrilladaStatus.
 **/

ParrilladaStatus *
parrillada_status_new (void)
{
	return g_object_new (PARRILLADA_TYPE_STATUS, NULL);
}

/**
 * parrillada_status_get_result:
 * @status: a #ParrilladaStatus.
 *
 * After an object (see parrillada_burn_track_get_status ()) has
 * been requested its status, this function returns that status.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if the object is ready.
 * PARRILLADA_BURN_NOT_READY if some time should be given to the object before it is ready.
 * PARRILLADA_BURN_ERR if there is an error.
 **/

ParrilladaBurnResult
parrillada_status_get_result (ParrilladaStatus *status)
{
	ParrilladaStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, PARRILLADA_BURN_ERR);
	g_return_val_if_fail (PARRILLADA_IS_STATUS (status), PARRILLADA_BURN_ERR);

	priv = PARRILLADA_STATUS_PRIVATE (status);
	return priv->res;
}

/**
 * parrillada_status_get_progress:
 * @status: a #ParrilladaStatus.
 *
 * If parrillada_status_get_result () returned PARRILLADA_BURN_NOT_READY,
 * this function returns the progress regarding the operation completion.
 *
 * Return value: a #gdouble
 **/

gdouble
parrillada_status_get_progress (ParrilladaStatus *status)
{
	ParrilladaStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, -1.0);
	g_return_val_if_fail (PARRILLADA_IS_STATUS (status), -1.0);

	priv = PARRILLADA_STATUS_PRIVATE (status);
	if (priv->res == PARRILLADA_BURN_OK)
		return 1.0;

	if (priv->res != PARRILLADA_BURN_NOT_READY)
		return -1.0;

	return priv->progress;
}

/**
 * parrillada_status_get_error:
 * @status: a #ParrilladaStatus.
 *
 * If parrillada_status_get_result () returned PARRILLADA_BURN_ERR,
 * this function returns the error.
 *
 * Return value: a #GError
 **/

GError *
parrillada_status_get_error (ParrilladaStatus *status)
{
	ParrilladaStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_STATUS (status), NULL);

	priv = PARRILLADA_STATUS_PRIVATE (status);
	if (priv->res != PARRILLADA_BURN_ERR)
		return NULL;

	return g_error_copy (priv->error);
}

/**
 * parrillada_status_get_current_action:
 * @status: a #ParrilladaStatus.
 *
 * If parrillada_status_get_result () returned PARRILLADA_BURN_NOT_READY,
 * this function returns a string describing the operation currently performed.
 * Free the string when it is not needed anymore.
 *
 * Return value: a #gchar.
 **/

gchar *
parrillada_status_get_current_action (ParrilladaStatus *status)
{
	gchar *string;
	ParrilladaStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, NULL);
	g_return_val_if_fail (PARRILLADA_IS_STATUS (status), NULL);

	priv = PARRILLADA_STATUS_PRIVATE (status);

	if (priv->res != PARRILLADA_BURN_NOT_READY)
		return NULL;

	string = g_strdup (priv->current_action);
	return string;

}

/**
 * parrillada_status_set_completed:
 * @status: a #ParrilladaStatus.
 *
 * Sets the status for a request to PARRILLADA_BURN_OK.
 *
 **/

void
parrillada_status_set_completed (ParrilladaStatus *status)
{
	ParrilladaStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (PARRILLADA_IS_STATUS (status));

	priv = PARRILLADA_STATUS_PRIVATE (status);

	priv->res = PARRILLADA_BURN_OK;
	priv->progress = 1.0;
}

/**
 * parrillada_status_set_not_ready:
 * @status: a #ParrilladaStatus.
 * @progress: a #gdouble or -1.0.
 * @current_action: a #gchar or NULL.
 *
 * Sets the status for a request to PARRILLADA_BURN_NOT_READY.
 * Allows to set a string describing the operation currently performed
 * as well as the progress regarding the operation completion.
 *
 **/

void
parrillada_status_set_not_ready (ParrilladaStatus *status,
			      gdouble progress,
			      const gchar *current_action)
{
	ParrilladaStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (PARRILLADA_IS_STATUS (status));

	priv = PARRILLADA_STATUS_PRIVATE (status);

	priv->res = PARRILLADA_BURN_NOT_READY;
	priv->progress = progress;

	if (priv->current_action)
		g_free (priv->current_action);
	priv->current_action = g_strdup (current_action);
}

/**
 * parrillada_status_set_running:
 * @status: a #ParrilladaStatus.
 * @progress: a #gdouble or -1.0.
 * @current_action: a #gchar or NULL.
 *
 * Sets the status for a request to PARRILLADA_BURN_RUNNING.
 * Allows to set a string describing the operation currently performed
 * as well as the progress regarding the operation completion.
 *
 **/

void
parrillada_status_set_running (ParrilladaStatus *status,
			    gdouble progress,
			    const gchar *current_action)
{
	ParrilladaStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (PARRILLADA_IS_STATUS (status));

	priv = PARRILLADA_STATUS_PRIVATE (status);

	priv->res = PARRILLADA_BURN_RUNNING;
	priv->progress = progress;

	if (priv->current_action)
		g_free (priv->current_action);
	priv->current_action = g_strdup (current_action);
}

/**
 * parrillada_status_set_error:
 * @status: a #ParrilladaStatus.
 * @error: a #GError or NULL.
 *
 * Sets the status for a request to PARRILLADA_BURN_ERR.
 *
 **/

void
parrillada_status_set_error (ParrilladaStatus *status,
			  GError *error)
{
	ParrilladaStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (PARRILLADA_IS_STATUS (status));

	priv = PARRILLADA_STATUS_PRIVATE (status);

	priv->res = PARRILLADA_BURN_ERR;
	priv->progress = -1.0;

	if (priv->error)
		g_error_free (priv->error);
	priv->error = error;
}

static void
parrillada_status_init (ParrilladaStatus *object)
{}

static void
parrillada_status_finalize (GObject *object)
{
	ParrilladaStatusPrivate *priv;

	priv = PARRILLADA_STATUS_PRIVATE (object);
	if (priv->error)
		g_error_free (priv->error);

	if (priv->current_action)
		g_free (priv->current_action);

	G_OBJECT_CLASS (parrillada_status_parent_class)->finalize (object);
}

static void
parrillada_status_class_init (ParrilladaStatusClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaStatusPrivate));

	object_class->finalize = parrillada_status_finalize;
}


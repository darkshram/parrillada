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

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-plugin-manager.h"
#include "parrillada-session.h"
#include "burn-plugin-manager.h"
#include "burn-image-format.h"
#include "libparrillada-marshal.h"

#include "parrillada-tags.h"
#include "parrillada-track-image.h"
#include "parrillada-track-data-cfg.h"
#include "parrillada-session-cfg.h"
#include "parrillada-burn-lib.h"
#include "parrillada-session-helper.h"


/**
 * SECTION:parrillada-session-cfg
 * @short_description: Configure automatically a #ParrilladaBurnSession object
 * @see_also: #ParrilladaBurn, #ParrilladaBurnSession
 * @include: parrillada-session-cfg.h
 *
 * This object configures automatically a session reacting to any change
 * made to the various parameters.
 **/

typedef struct _ParrilladaSessionCfgPrivate ParrilladaSessionCfgPrivate;
struct _ParrilladaSessionCfgPrivate
{
	ParrilladaBurnFlag supported;
	ParrilladaBurnFlag compulsory;

	gchar *output;

	ParrilladaSessionError is_valid;

	guint CD_TEXT_modified:1;
	guint configuring:1;
	guint disabled:1;
};

#define PARRILLADA_SESSION_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_SESSION_CFG, ParrilladaSessionCfgPrivate))

enum
{
	IS_VALID_SIGNAL,
	WRONG_EXTENSION_SIGNAL,
	LAST_SIGNAL
};


static guint session_cfg_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ParrilladaSessionCfg, parrillada_session_cfg, PARRILLADA_TYPE_SESSION_SPAN);

/**
 * This is to handle tags (and more particularly video ones)
 */

static void
parrillada_session_cfg_tag_changed (ParrilladaBurnSession *session,
                                 const gchar *tag)
{
	if (!strcmp (tag, PARRILLADA_VCD_TYPE)) {
		int svcd_type;

		svcd_type = parrillada_burn_session_tag_lookup_int (session, PARRILLADA_VCD_TYPE);
		if (svcd_type != PARRILLADA_SVCD)
			parrillada_burn_session_tag_add_int (session,
			                                  PARRILLADA_VIDEO_OUTPUT_ASPECT,
			                                  PARRILLADA_VIDEO_ASPECT_4_3);
	}
}

/**
 * parrillada_session_cfg_has_default_output_path:
 * @cfg: a #ParrilladaSessionCfg
 *
 * This function returns whether the path returned
 * by parrillada_burn_session_get_output () is an 
 * automatically created one.
 *
 * Return value: a #gboolean. TRUE if the path(s)
 * creation is handled by @session, FALSE if it was
 * set.
 **/

gboolean
parrillada_session_cfg_has_default_output_path (ParrilladaSessionCfg *session)
{
	ParrilladaBurnSessionClass *klass;
	ParrilladaBurnResult result;

	klass = PARRILLADA_BURN_SESSION_CLASS (parrillada_session_cfg_parent_class);
	result = klass->get_output_path (PARRILLADA_BURN_SESSION (session), NULL, NULL);
	return (result == PARRILLADA_BURN_OK);
}

static gboolean
parrillada_session_cfg_wrong_extension_signal (ParrilladaSessionCfg *session)
{
	GValue instance_and_params [1];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (session));
	g_value_set_instance (instance_and_params, session);
	
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&return_value, FALSE);

	g_signal_emitv (instance_and_params,
			session_cfg_signals [WRONG_EXTENSION_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	return g_value_get_boolean (&return_value);
}

static ParrilladaBurnResult
parrillada_session_cfg_set_output_image (ParrilladaBurnSession *session,
				      ParrilladaImageFormat format,
				      const gchar *image,
				      const gchar *toc)
{
	gchar *dot;
	gchar *set_toc = NULL;
	gchar * set_image = NULL;
	ParrilladaBurnResult result;
	ParrilladaBurnSessionClass *klass;
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };

	/* Make sure something actually changed */
	klass = PARRILLADA_BURN_SESSION_CLASS (parrillada_session_cfg_parent_class);
	klass->get_output_path (PARRILLADA_BURN_SESSION (session),
	                        &set_image,
	                        &set_toc);

	if (!set_image && !set_toc) {
		/* see if image and toc set paths differ */
		parrillada_burn_session_get_output (PARRILLADA_BURN_SESSION (session),
		                                 &set_image,
		                                 &set_toc);
		if (set_image && image && !strcmp (set_image, image)) {
			/* It's the same default path so no 
			 * need to carry on and actually set
			 * the path of image. */
			image = NULL;
		}

		if (set_toc && toc && !strcmp (set_toc, toc)) {
			/* It's the same default path so no 
			 * need to carry on and actually set
			 * the path of image. */
			toc = NULL;
		}
	}

	if (set_image)
		g_free (set_image);

	if (set_toc)
		g_free (set_toc);

	/* First set all information */
	result = klass->set_output_image (session,
					  format,
					  image,
					  toc);

	if (!image && !toc)
		return result;

	if (format == PARRILLADA_IMAGE_FORMAT_NONE)
		format = parrillada_burn_session_get_output_format (session);

	if (format == PARRILLADA_IMAGE_FORMAT_NONE)
		return result;

	if (format & PARRILLADA_IMAGE_FORMAT_BIN) {
		dot = g_utf8_strrchr (image, -1, '.');
		if (!dot || strcmp (suffixes [0], dot)) {
			gboolean res;

			res = parrillada_session_cfg_wrong_extension_signal (PARRILLADA_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = parrillada_image_format_fix_path_extension (format, FALSE, image);
				/* NOTE: call ourselves with the fixed path as this way,
				 * in case the path is the same as the default one after
				 * fixing the extension we'll keep on using default path */
				result = parrillada_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     fixed_path,
				                                                     toc);
				g_free (fixed_path);
			}
		}
	}
	else {
		dot = g_utf8_strrchr (toc, -1, '.');

		if (format & PARRILLADA_IMAGE_FORMAT_CLONE
		&& (!dot || strcmp (suffixes [1], dot))) {
			gboolean res;

			res = parrillada_session_cfg_wrong_extension_signal (PARRILLADA_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = parrillada_image_format_fix_path_extension (format, FALSE, toc);
				result = parrillada_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
		else if (format & PARRILLADA_IMAGE_FORMAT_CUE
		     && (!dot || strcmp (suffixes [2], dot))) {
			gboolean res;

			res = parrillada_session_cfg_wrong_extension_signal (PARRILLADA_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = parrillada_image_format_fix_path_extension (format, FALSE, toc);
				result = parrillada_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
		else if (format & PARRILLADA_IMAGE_FORMAT_CDRDAO
		     && (!dot || strcmp (suffixes [3], dot))) {
			gboolean res;

			res = parrillada_session_cfg_wrong_extension_signal (PARRILLADA_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = parrillada_image_format_fix_path_extension (format, FALSE, toc);
				result = parrillada_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
	}

	return result;
}

static ParrilladaBurnResult
parrillada_session_cfg_get_output_path (ParrilladaBurnSession *session,
				     gchar **image,
				     gchar **toc)
{
	gchar *path = NULL;
	ParrilladaBurnResult result;
	ParrilladaImageFormat format;
	ParrilladaBurnSessionClass *klass;

	klass = PARRILLADA_BURN_SESSION_CLASS (parrillada_session_cfg_parent_class);

	result = klass->get_output_path (session,
					 image,
					 toc);
	if (result == PARRILLADA_BURN_OK)
		return result;

	format = parrillada_burn_session_get_output_format (session);
	path = parrillada_image_format_get_default_path (format);

	switch (format) {
	case PARRILLADA_IMAGE_FORMAT_BIN:
		if (image)
			*image = path;
		break;

	case PARRILLADA_IMAGE_FORMAT_CDRDAO:
	case PARRILLADA_IMAGE_FORMAT_CLONE:
	case PARRILLADA_IMAGE_FORMAT_CUE:
		if (toc)
			*toc = path;

		if (image)
			*image = parrillada_image_format_get_complement (format, path);
		break;

	default:
		g_free (path);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaImageFormat
parrillada_session_cfg_get_output_format (ParrilladaBurnSession *session)
{
	ParrilladaBurnSessionClass *klass;
	ParrilladaImageFormat format;

	klass = PARRILLADA_BURN_SESSION_CLASS (parrillada_session_cfg_parent_class);
	format = klass->get_output_format (session);

	if (format == PARRILLADA_IMAGE_FORMAT_NONE)
		format = parrillada_burn_session_get_default_output_format (session);

	return format;
}

/**
 * parrillada_session_cfg_get_error:
 * @cfg: a #ParrilladaSessionCfg
 *
 * This function returns the current status and if
 * autoconfiguration is/was successful.
 *
 * Return value: a #ParrilladaSessionError.
 **/

ParrilladaSessionError
parrillada_session_cfg_get_error (ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	if (priv->is_valid == PARRILLADA_SESSION_VALID
	&&  priv->CD_TEXT_modified)
		return PARRILLADA_SESSION_NO_CD_TEXT;

	return priv->is_valid;
}

/**
 * parrillada_session_cfg_disable:
 * @cfg: a #ParrilladaSessionCfg
 *
 * This function disables autoconfiguration
 *
 **/

void
parrillada_session_cfg_disable (ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);
	priv->disabled = TRUE;
}

/**
 * parrillada_session_cfg_enable:
 * @cfg: a #ParrilladaSessionCfg
 *
 * This function (re)-enables autoconfiguration
 *
 **/

void
parrillada_session_cfg_enable (ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);
	priv->disabled = FALSE;
}

static void
parrillada_session_cfg_set_drive_properties_default_flags (ParrilladaSessionCfg *self)
{
	ParrilladaMedia media;
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	media = parrillada_burn_session_get_dest_media (PARRILLADA_BURN_SESSION (self));

	if (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS)
	||  PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_RESTRICTED)
	||  PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS_DL)) {
		/* This is a special case to favour libburn/growisofs
		 * wodim/cdrecord for these types of media. */
		if (priv->supported & PARRILLADA_BURN_FLAG_MULTI) {
			parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self),
						       PARRILLADA_BURN_FLAG_MULTI);

			priv->supported = PARRILLADA_BURN_FLAG_NONE;
			priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
			parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}
	}

	/* Always set this flag whenever possible */
	if (priv->supported & PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE) {
		parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self),
					       PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE);

		if (priv->supported & PARRILLADA_BURN_FLAG_FAST_BLANK
		&& (media & PARRILLADA_MEDIUM_UNFORMATTED) == 0)
			parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self),
						       PARRILLADA_BURN_FLAG_FAST_BLANK);

		priv->supported = PARRILLADA_BURN_FLAG_NONE;
		priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
		parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);
	}

#if 0
	
	/* NOTE: Stop setting DAO here except if it
	 * is declared compulsory (but that's handled
	 * somewhere else) or if it was explicity set.
	 * If we set it just  by  default when it's
	 * supported but not compulsory, MULTI
	 * becomes not supported anymore.
	 * For data the only way it'd be really useful
	 * is if we wanted to fit a selection on the disc.
	 * The problem here is that we don't know
	 * what the size of the final image is going
	 * to be.
	 * For images there are cases when after 
	 * writing an image the user may want to
	 * add some more data to it later. As in the
	 * case of data the only way this flag would
	 * be useful would be to help fit the image
	 * on the disc. But I doubt it would really
	 * help a lot.
	 * For audio we need it to write things like
	 * CD-TEXT but in this case the backend
	 * will return it as compulsory. */

	/* Another case when this flag is needed is
	 * for DVD-RW quick blanked as they only
	 * support DAO. But there again this should
	 * be covered by the backend that should
	 * return DAO as compulsory. */

	/* Leave the code as a reminder. */
	/* When copying with same drive don't set write mode, it'll be set later */
	if (!parrillada_burn_session_same_src_dest_drive (PARRILLADA_BURN_SESSION (self))
	&&  !(media & PARRILLADA_MEDIUM_DVD)) {
		/* use DAO whenever it's possible except for DVDs otherwise
		 * wodime which claims to support it will be used by default
		 * instead of say growisofs. */
		if (priv->supported & PARRILLADA_BURN_FLAG_DAO) {
			parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self), PARRILLADA_BURN_FLAG_DAO);

			priv->supported = PARRILLADA_BURN_FLAG_NONE;
			priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
			parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);

			/* NOTE: after setting DAO, some flags may become
			 * compulsory like MULTI. */
		}
	}
#endif
}

static void
parrillada_session_cfg_set_drive_properties_flags (ParrilladaSessionCfg *self,
						ParrilladaBurnFlag flags)
{
	ParrilladaDrive *drive;
	ParrilladaMedia media;
	ParrilladaBurnFlag flag;
	ParrilladaMedium *medium;
	ParrilladaBurnResult result;
	ParrilladaBurnFlag original_flags;
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	original_flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self));
	PARRILLADA_BURN_LOG ("Resetting all flags");
	PARRILLADA_BURN_LOG_FLAGS (original_flags, "Current are");
	PARRILLADA_BURN_LOG_FLAGS (flags, "New should be");

	drive = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (self));
	if (!drive) {
		PARRILLADA_BURN_LOG ("No drive");
		return;
	}

	medium = parrillada_drive_get_medium (drive);
	if (!medium) {
		PARRILLADA_BURN_LOG ("No medium");
		return;
	}

	media = parrillada_medium_get_status (medium);

	/* This prevents signals to be emitted while (re-) adding them one by one */
	g_object_freeze_notify (G_OBJECT (self));

	parrillada_burn_session_set_flags (PARRILLADA_BURN_SESSION (self), PARRILLADA_BURN_FLAG_NONE);

	priv->supported = PARRILLADA_BURN_FLAG_NONE;
	priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
	result = parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
						      &priv->supported,
						      &priv->compulsory);

	if (result != PARRILLADA_BURN_OK) {
		parrillada_burn_session_set_flags (PARRILLADA_BURN_SESSION (self), original_flags | flags);
		g_object_thaw_notify (G_OBJECT (self));
		return;
	}

	for (flag = PARRILLADA_BURN_FLAG_EJECT; flag < PARRILLADA_BURN_FLAG_LAST; flag <<= 1) {
		/* see if this flag was originally set */
		if ((flags & flag) == 0)
			continue;

		/* Don't set write modes now in this case */
		if (parrillada_burn_session_same_src_dest_drive (PARRILLADA_BURN_SESSION (self))
		&& (flag & (PARRILLADA_BURN_FLAG_DAO|PARRILLADA_BURN_FLAG_RAW)))
			continue;

		if (priv->compulsory
		&& (priv->compulsory & parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self))) != priv->compulsory) {
			parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self), priv->compulsory);

			priv->supported = PARRILLADA_BURN_FLAG_NONE;
			priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
			parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}

		if (priv->supported & flag) {
			parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self), flag);

			priv->supported = PARRILLADA_BURN_FLAG_NONE;
			priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
			parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}
	}

	if (original_flags & PARRILLADA_BURN_FLAG_DAO
	&&  priv->supported & PARRILLADA_BURN_FLAG_DAO) {
		/* Only set DAO if it was explicitely requested */
		parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self), PARRILLADA_BURN_FLAG_DAO);

		priv->supported = PARRILLADA_BURN_FLAG_NONE;
		priv->compulsory = PARRILLADA_BURN_FLAG_NONE;
		parrillada_burn_session_get_burn_flags (PARRILLADA_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);

		/* NOTE: after setting DAO, some flags may become
		 * compulsory like MULTI. */
	}

	parrillada_session_cfg_set_drive_properties_default_flags (self);

	/* These are always supported and better be set. */
	parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self),
	                               PARRILLADA_BURN_FLAG_CHECK_SIZE|
	                               PARRILLADA_BURN_FLAG_NOGRACE);

	/* This one is only supported when we are
	 * burning to a disc or copying a disc but it
	 * would better be set. */
	if (priv->supported & PARRILLADA_BURN_FLAG_EJECT)
		parrillada_burn_session_add_flag (PARRILLADA_BURN_SESSION (self),
		                               PARRILLADA_BURN_FLAG_EJECT);

	/* allow notify::flags signal again */
	g_object_thaw_notify (G_OBJECT (self));
}

static void
parrillada_session_cfg_add_drive_properties_flags (ParrilladaSessionCfg *self,
						ParrilladaBurnFlag flags)
{
	/* add flags then wipe out flags from session to check them one by one */
	flags |= parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self));
	parrillada_session_cfg_set_drive_properties_flags (self, flags);
}

static void
parrillada_session_cfg_rm_drive_properties_flags (ParrilladaSessionCfg *self,
					       ParrilladaBurnFlag flags)
{
	/* add flags then wipe out flags from session to check them one by one */
	flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self)) & (~flags);
	parrillada_session_cfg_set_drive_properties_flags (self, flags);
}

static void
parrillada_session_cfg_check_drive_settings (ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;
	ParrilladaBurnFlag flags;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	/* Try to properly update the flags for the current drive */
	flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self));
	if (parrillada_burn_session_same_src_dest_drive (PARRILLADA_BURN_SESSION (self))) {
		flags |= PARRILLADA_BURN_FLAG_BLANK_BEFORE_WRITE|
			 PARRILLADA_BURN_FLAG_FAST_BLANK;
	}

	/* check each flag before re-adding it */
	parrillada_session_cfg_add_drive_properties_flags (self, flags);
}

static ParrilladaSessionError
parrillada_session_cfg_check_size (ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;
	ParrilladaBurnFlag flags;
	ParrilladaMedium *medium;
	ParrilladaDrive *burner;
	GValue *value = NULL;
	/* in sectors */
	goffset session_size;
	goffset max_sectors;
	goffset disc_size;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	burner = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (self));
	if (!burner) {
		priv->is_valid = PARRILLADA_SESSION_NO_OUTPUT;
		return PARRILLADA_SESSION_NO_OUTPUT;
	}

	/* FIXME: here we could check the hard drive space */
	if (parrillada_drive_is_fake (burner)) {
		priv->is_valid = PARRILLADA_SESSION_VALID;
		return PARRILLADA_SESSION_VALID;
	}

	medium = parrillada_drive_get_medium (burner);
	if (!medium) {
		priv->is_valid = PARRILLADA_SESSION_NO_OUTPUT;
		return PARRILLADA_SESSION_NO_OUTPUT;
	}

	disc_size = parrillada_burn_session_get_available_medium_space (PARRILLADA_BURN_SESSION (self));
	if (disc_size < 0)
		disc_size = 0;

	/* get input track size */
	session_size = 0;

	if (parrillada_burn_session_tag_lookup (PARRILLADA_BURN_SESSION (self),
					     PARRILLADA_DATA_TRACK_SIZE_TAG,
					     &value) == PARRILLADA_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else if (parrillada_burn_session_tag_lookup (PARRILLADA_BURN_SESSION (self),
						  PARRILLADA_STREAM_TRACK_SIZE_TAG,
						  &value) == PARRILLADA_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else
		parrillada_burn_session_get_size (PARRILLADA_BURN_SESSION (self),
					       &session_size,
					       NULL);

	PARRILLADA_BURN_LOG ("Session size %lli/Disc size %lli",
			  session_size,
			  disc_size);

	if (session_size < disc_size) {
		priv->is_valid = PARRILLADA_SESSION_VALID;
		return PARRILLADA_SESSION_VALID;
	}

	/* Overburn is only for CDs */
	if ((parrillada_medium_get_status (medium) & PARRILLADA_MEDIUM_CD) == 0) {
		priv->is_valid = PARRILLADA_SESSION_INSUFFICIENT_SPACE;
		return PARRILLADA_SESSION_INSUFFICIENT_SPACE;
	}

	/* The idea would be to test write the disc with cdrecord from /dev/null
	 * until there is an error and see how much we were able to write. So,
	 * when we propose overburning to the user, we could ask if he wants
	 * us to determine how much data can be written to a particular disc
	 * provided he has chosen a real disc. */
	max_sectors = disc_size * 103 / 100;
	if (max_sectors < session_size) {
		priv->is_valid = PARRILLADA_SESSION_INSUFFICIENT_SPACE;
		return PARRILLADA_SESSION_INSUFFICIENT_SPACE;
	}

	flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self));
	if (!(flags & PARRILLADA_BURN_FLAG_OVERBURN)) {
		ParrilladaSessionCfgPrivate *priv;

		priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

		if (!(priv->supported & PARRILLADA_BURN_FLAG_OVERBURN)) {
			priv->is_valid = PARRILLADA_SESSION_INSUFFICIENT_SPACE;
			return PARRILLADA_SESSION_INSUFFICIENT_SPACE;
		}

		priv->is_valid = PARRILLADA_SESSION_OVERBURN_NECESSARY;
		return PARRILLADA_SESSION_OVERBURN_NECESSARY;
	}

	priv->is_valid = PARRILLADA_SESSION_VALID;
	return PARRILLADA_SESSION_VALID;
}

static void
parrillada_session_cfg_set_tracks_audio_format (ParrilladaBurnSession *session,
					     ParrilladaStreamFormat format)
{
	GSList *tracks;
	GSList *iter;

	tracks = parrillada_burn_session_get_tracks (session);
	for (iter = tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;

		track = iter->data;
		if (!PARRILLADA_IS_TRACK_STREAM (track))
			continue;

		parrillada_track_stream_set_format (PARRILLADA_TRACK_STREAM (track), format);
	}
}

static void
parrillada_session_cfg_update (ParrilladaSessionCfg *self)
{
	ParrilladaTrackType *source = NULL;
	ParrilladaSessionCfgPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaStatus *status;
	ParrilladaDrive *burner;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	if (priv->configuring)
		return;

	/* Make sure the session is ready */
	status = parrillada_status_new ();
	result = parrillada_burn_session_get_status (PARRILLADA_BURN_SESSION (self), status);
	if (result == PARRILLADA_BURN_NOT_READY || result == PARRILLADA_BURN_RUNNING) {
		g_object_unref (status);

		priv->is_valid = PARRILLADA_SESSION_NOT_READY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (result == PARRILLADA_BURN_ERR) {
		GError *error;

		error = parrillada_status_get_error (status);
		if (error) {
			if (error->code == PARRILLADA_BURN_ERROR_EMPTY) {
				g_object_unref (status);
				g_error_free (error);

				priv->is_valid = PARRILLADA_SESSION_EMPTY;
				g_signal_emit (self,
					       session_cfg_signals [IS_VALID_SIGNAL],
					       0);
				return;
			}

			g_error_free (error);
		}
	}
	g_object_unref (status);

	/* Make sure there is a source */
	source = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (PARRILLADA_BURN_SESSION (self), source);

	if (parrillada_track_type_is_empty (source)) {
		parrillada_track_type_free (source);

		priv->is_valid = PARRILLADA_SESSION_EMPTY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* it can be empty with just an empty track */
	if (parrillada_track_type_get_has_medium (source)
	&&  parrillada_track_type_get_medium_type (source) == PARRILLADA_MEDIUM_NONE) {
		parrillada_track_type_free (source);

		priv->is_valid = PARRILLADA_SESSION_NO_INPUT_MEDIUM;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (parrillada_track_type_get_has_image (source)
	&&  parrillada_track_type_get_image_format (source) == PARRILLADA_IMAGE_FORMAT_NONE) {
		gchar *uri;
		GSList *tracks;

		parrillada_track_type_free (source);

		tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (self));

		/* It can be two cases:
		 * - no image set
		 * - image format cannot be detected */
		if (tracks) {
			ParrilladaTrack *track;

			track = tracks->data;
			uri = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), TRUE);
			if (uri) {
				priv->is_valid = PARRILLADA_SESSION_UNKNOWN_IMAGE;
				g_free (uri);
			}
			else
				priv->is_valid = PARRILLADA_SESSION_NO_INPUT_IMAGE;
		}
		else
			priv->is_valid = PARRILLADA_SESSION_NO_INPUT_IMAGE;

		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* make sure there is an output set */
	burner = parrillada_burn_session_get_burner (PARRILLADA_BURN_SESSION (self));
	if (!burner) {
		parrillada_track_type_free (source);

		priv->is_valid = PARRILLADA_SESSION_NO_OUTPUT;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* Check that current input and output work */
	if (parrillada_track_type_get_has_stream (source)) {
		if (priv->CD_TEXT_modified) {
			/* Try to redo what we undid (after all a new plugin
			 * could have been activated in the mean time ...) and
			 * see what happens */
			parrillada_track_type_set_stream_format (source,
							      PARRILLADA_METADATA_INFO|
							      parrillada_track_type_get_stream_format (source));
			result = parrillada_burn_session_input_supported (PARRILLADA_BURN_SESSION (self), source, FALSE);
			if (result == PARRILLADA_BURN_OK) {
				priv->CD_TEXT_modified = FALSE;

				priv->configuring = TRUE;
				parrillada_session_cfg_set_tracks_audio_format (PARRILLADA_BURN_SESSION (self),
									     parrillada_track_type_get_stream_format (source));
				priv->configuring = FALSE;
			}
			else {
				/* No, nothing's changed */
				parrillada_track_type_set_stream_format (source,
								      (~PARRILLADA_METADATA_INFO) &
								      parrillada_track_type_get_stream_format (source));
				result = parrillada_burn_session_input_supported (PARRILLADA_BURN_SESSION (self), source, FALSE);
			}
		}
		else {
			result = parrillada_burn_session_can_burn (PARRILLADA_BURN_SESSION (self), FALSE);

			if (result != PARRILLADA_BURN_OK
			&& (parrillada_track_type_get_stream_format (source) & PARRILLADA_METADATA_INFO)) {
				/* Another special case in case some burning backends 
				 * don't support CD-TEXT for audio (libburn). If no
				 * other backend is available remove CD-TEXT option but
				 * tell user... */
				parrillada_track_type_set_stream_format (source,
								      (~PARRILLADA_METADATA_INFO) &
								      parrillada_track_type_get_stream_format (source));

				result = parrillada_burn_session_input_supported (PARRILLADA_BURN_SESSION (self), source, FALSE);

				PARRILLADA_BURN_LOG ("Tested support without Metadata information (result %d)", result);
				if (result == PARRILLADA_BURN_OK) {
					priv->CD_TEXT_modified = TRUE;

					priv->configuring = TRUE;
					parrillada_session_cfg_set_tracks_audio_format (PARRILLADA_BURN_SESSION (self),
										     parrillada_track_type_get_has_stream (source));
					priv->configuring = FALSE;
				}
			}
		}
	}
	else if (parrillada_track_type_get_has_medium (source)
	&&  (parrillada_track_type_get_medium_type (source) & PARRILLADA_MEDIUM_HAS_AUDIO)) {
		ParrilladaImageFormat format = PARRILLADA_IMAGE_FORMAT_NONE;

		/* If we copy an audio disc check the image
		 * type we're writing to as it may mean that
		 * CD-TEXT cannot be copied.
		 * Make sure that the writing backend
		 * supports writing CD-TEXT?
		 * no, if a backend reports it supports an
		 * image type it should be able to burn all
		 * its data. */
		if (!parrillada_burn_session_is_dest_file (PARRILLADA_BURN_SESSION (self))) {
			ParrilladaTrackType *tmp_type;

			tmp_type = parrillada_track_type_new ();

			/* NOTE: this is the same as parrillada_burn_session_supported () */
			result = parrillada_burn_session_get_tmp_image_type_same_src_dest (PARRILLADA_BURN_SESSION (self), tmp_type);
			if (result == PARRILLADA_BURN_OK) {
				if (parrillada_track_type_get_has_image (tmp_type)) {
					format = parrillada_track_type_get_image_format (tmp_type);
					priv->CD_TEXT_modified = (format & (PARRILLADA_IMAGE_FORMAT_CDRDAO|PARRILLADA_IMAGE_FORMAT_CUE)) == 0;
				}
				else if (parrillada_track_type_get_has_stream (tmp_type)) {
					/* FIXME: for the moment
					 * we consider that this
					 * type will always allow
					 * to copy CD-TEXT */
					priv->CD_TEXT_modified = FALSE;
				}
				else
					priv->CD_TEXT_modified = TRUE;
			}
			else
				priv->CD_TEXT_modified = TRUE;

			parrillada_track_type_free (tmp_type);

			PARRILLADA_BURN_LOG ("Temporary image type %i", format);
		}
		else {
			result = parrillada_burn_session_can_burn (PARRILLADA_BURN_SESSION (self), FALSE);
			format = parrillada_burn_session_get_output_format (PARRILLADA_BURN_SESSION (self));
			priv->CD_TEXT_modified = (format & (PARRILLADA_IMAGE_FORMAT_CDRDAO|PARRILLADA_IMAGE_FORMAT_CUE)) == 0;
		}
	}
	else {
		/* Don't use flags as they'll be adapted later. */
		priv->CD_TEXT_modified = FALSE;
		result = parrillada_burn_session_can_burn (PARRILLADA_BURN_SESSION (self), FALSE);
	}

	if (result != PARRILLADA_BURN_OK) {
		if (parrillada_track_type_get_has_medium (source)
		&& (parrillada_track_type_get_medium_type (source) & PARRILLADA_MEDIUM_PROTECTED)
		&&  parrillada_burn_library_input_supported (source) != PARRILLADA_BURN_OK) {
			/* This is a special case to display a helpful message */
			priv->is_valid = PARRILLADA_SESSION_DISC_PROTECTED;
			g_signal_emit (self,
				       session_cfg_signals [IS_VALID_SIGNAL],
				       0);
		}
		else {
			priv->is_valid = PARRILLADA_SESSION_NOT_SUPPORTED;
			g_signal_emit (self,
				       session_cfg_signals [IS_VALID_SIGNAL],
				       0);
		}

		parrillada_track_type_free (source);
		return;
	}

	/* Special case for video projects */
	if (parrillada_track_type_get_has_stream (source)
	&&  PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (source))) {
		/* Only set if it was not already set */
		if (parrillada_burn_session_tag_lookup (PARRILLADA_BURN_SESSION (self), PARRILLADA_VCD_TYPE, NULL) != PARRILLADA_BURN_OK)
			parrillada_burn_session_tag_add_int (PARRILLADA_BURN_SESSION (self),
							  PARRILLADA_VCD_TYPE,
							  PARRILLADA_SVCD);
	}

	parrillada_track_type_free (source);

	/* Configure flags */
	priv->configuring = TRUE;

	if (parrillada_drive_is_fake (burner))
		/* Remove some impossible flags */
		parrillada_burn_session_remove_flag (PARRILLADA_BURN_SESSION (self),
						  PARRILLADA_BURN_FLAG_DUMMY|
						  PARRILLADA_BURN_FLAG_NO_TMP_FILES);

	priv->configuring = FALSE;

	/* Finally check size */
	if (parrillada_burn_session_same_src_dest_drive (PARRILLADA_BURN_SESSION (self))) {
		priv->is_valid = PARRILLADA_SESSION_VALID;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
	}
	else {
		parrillada_session_cfg_check_size (self);
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
	}
}

static void
parrillada_session_cfg_session_loaded (ParrilladaTrackDataCfg *track,
				    ParrilladaMedium *medium,
				    gboolean is_loaded,
				    ParrilladaSessionCfg *session)
{
	ParrilladaBurnFlag session_flags;
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	session_flags = parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (session));
	if (is_loaded) {
		/* Set the correct medium and add the flag */
		parrillada_burn_session_set_burner (PARRILLADA_BURN_SESSION (session),
						 parrillada_medium_get_drive (medium));

		if ((session_flags & PARRILLADA_BURN_FLAG_MERGE) == 0)
			parrillada_session_cfg_add_drive_properties_flags (session, PARRILLADA_BURN_FLAG_MERGE);
	}
	else if ((session_flags & PARRILLADA_BURN_FLAG_MERGE) != 0)
		parrillada_session_cfg_rm_drive_properties_flags (session, PARRILLADA_BURN_FLAG_MERGE);
}

static void
parrillada_session_cfg_track_added (ParrilladaBurnSession *session,
				 ParrilladaTrack *track)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	if (PARRILLADA_IS_TRACK_DATA_CFG (track))
		g_signal_connect (track,
				  "session-loaded",
				  G_CALLBACK (parrillada_session_cfg_session_loaded),
				  session);

	/* A track was added: 
	 * - check if all flags are supported
	 * - check available formats for path
	 * - set one path */
	parrillada_session_cfg_check_drive_settings (PARRILLADA_SESSION_CFG (session));
	parrillada_session_cfg_update (PARRILLADA_SESSION_CFG (session));
}

static void
parrillada_session_cfg_track_removed (ParrilladaBurnSession *session,
				   ParrilladaTrack *track,
				   guint former_position)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* Just in case */
	g_signal_handlers_disconnect_by_func (track,
					      parrillada_session_cfg_session_loaded,
					      session);

	/* If there were several tracks and at least one remained there is no
	 * use checking flags since the source type has not changed anyway.
	 * If there is no more track, there is no use checking flags anyway. */
	parrillada_session_cfg_update (PARRILLADA_SESSION_CFG (session));
}

static void
parrillada_session_cfg_track_changed (ParrilladaBurnSession *session,
				   ParrilladaTrack *track)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* when that happens it's mostly because a medium source changed, or
	 * a new image was set. 
	 * - check if all flags are supported
	 * - check available formats for path
	 * - set one path if need be */
	parrillada_session_cfg_check_drive_settings (PARRILLADA_SESSION_CFG (session));
	parrillada_session_cfg_update (PARRILLADA_SESSION_CFG (session));
}

static void
parrillada_session_cfg_output_changed (ParrilladaBurnSession *session,
				    ParrilladaMedium *former)
{
	ParrilladaSessionCfgPrivate *priv;
	ParrilladaTrackType *type;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* Case for video project */
	type = parrillada_track_type_new ();
	parrillada_burn_session_get_input_type (session, type);

	if (parrillada_track_type_get_has_stream (type)
	&&  PARRILLADA_STREAM_FORMAT_HAS_VIDEO (parrillada_track_type_get_stream_format (type))) {
		ParrilladaMedia media;

		media = parrillada_burn_session_get_dest_media (session);
		if (media & PARRILLADA_MEDIUM_DVD)
			parrillada_burn_session_tag_add_int (session,
			                                  PARRILLADA_DVD_STREAM_FORMAT,
			                                  PARRILLADA_AUDIO_FORMAT_AC3);
		else if (media & PARRILLADA_MEDIUM_CD)
			parrillada_burn_session_tag_add_int (session,
							  PARRILLADA_DVD_STREAM_FORMAT,
							  PARRILLADA_AUDIO_FORMAT_MP2);
		else {
			ParrilladaImageFormat format;

			format = parrillada_burn_session_get_output_format (session);
			if (format == PARRILLADA_IMAGE_FORMAT_CUE)
				parrillada_burn_session_tag_add_int (session,
								  PARRILLADA_DVD_STREAM_FORMAT,
								  PARRILLADA_AUDIO_FORMAT_MP2);
			else
				parrillada_burn_session_tag_add_int (session,
								  PARRILLADA_DVD_STREAM_FORMAT,
								  PARRILLADA_AUDIO_FORMAT_AC3);
		}
	}
	parrillada_track_type_free (type);

	/* In this case need to :
	 * - check if all flags are supported
	 * - for images, set a path if it wasn't already set */
	parrillada_session_cfg_check_drive_settings (PARRILLADA_SESSION_CFG (session));
	parrillada_session_cfg_update (PARRILLADA_SESSION_CFG (session));
}

static void
parrillada_session_cfg_caps_changed (ParrilladaPluginManager *manager,
				  ParrilladaSessionCfg *self)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);
	if (priv->disabled)
		return;

	/* In this case we need to check if:
	 * - flags are supported or not supported anymore
	 * - image types as input/output are supported
	 * - if the current set of input/output still works */
	parrillada_session_cfg_check_drive_settings (self);
	parrillada_session_cfg_update (self);
}

/**
 * parrillada_session_cfg_add_flags:
 * @cfg: a #ParrilladaSessionCfg
 * @flags: a #ParrilladaBurnFlag
 *
 * Adds all flags from @flags that are supported.
 *
 **/

void
parrillada_session_cfg_add_flags (ParrilladaSessionCfg *self,
			       ParrilladaBurnFlag flags)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	if ((priv->supported & flags) != flags)
		return;

	if ((parrillada_burn_session_get_flags (PARRILLADA_BURN_SESSION (self)) & flags) == flags)
		return;

	parrillada_session_cfg_add_drive_properties_flags (self, flags);
	parrillada_session_cfg_update (self);
}

/**
 * parrillada_session_cfg_remove_flags:
 * @cfg: a #ParrilladaSessionCfg
 * @flags: a #ParrilladaBurnFlag
 *
 * Removes all flags that are not compulsory.
 *
 **/

void
parrillada_session_cfg_remove_flags (ParrilladaSessionCfg *self,
				  ParrilladaBurnFlag flags)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);

	parrillada_burn_session_remove_flag (PARRILLADA_BURN_SESSION (self), flags);

	/* For this case reset all flags as some flags could
	 * be made available after the removal of one flag
	 * Example: After the removal of MULTI, FAST_BLANK
	 * becomes available again for DVDRW sequential */
	parrillada_session_cfg_set_drive_properties_default_flags (self);
	parrillada_session_cfg_update (self);
}

/**
 * parrillada_session_cfg_is_supported:
 * @cfg: a #ParrilladaSessionCfg
 * @flag: a #ParrilladaBurnFlag
 *
 * Checks whether a particular flag is supported.
 *
 * Return value: a #gboolean. TRUE if it is supported;
 * FALSE otherwise.
 **/

gboolean
parrillada_session_cfg_is_supported (ParrilladaSessionCfg *self,
				  ParrilladaBurnFlag flag)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);
	return (priv->supported & flag) == flag;
}

/**
 * parrillada_session_cfg_is_compulsory:
 * @cfg: a #ParrilladaSessionCfg
 * @flag: a #ParrilladaBurnFlag
 *
 * Checks whether a particular flag is compulsory.
 *
 * Return value: a #gboolean. TRUE if it is compulsory;
 * FALSE otherwise.
 **/

gboolean
parrillada_session_cfg_is_compulsory (ParrilladaSessionCfg *self,
				   ParrilladaBurnFlag flag)
{
	ParrilladaSessionCfgPrivate *priv;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (self);
	return (priv->compulsory & flag) == flag;
}

static void
parrillada_session_cfg_init (ParrilladaSessionCfg *object)
{
	ParrilladaSessionCfgPrivate *priv;
	ParrilladaPluginManager *manager;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (object);

	manager = parrillada_plugin_manager_get_default ();
	g_signal_connect (manager,
	                  "caps-changed",
	                  G_CALLBACK (parrillada_session_cfg_caps_changed),
	                  object);
}

static void
parrillada_session_cfg_finalize (GObject *object)
{
	ParrilladaPluginManager *manager;
	ParrilladaSessionCfgPrivate *priv;
	GSList *tracks;

	priv = PARRILLADA_SESSION_CFG_PRIVATE (object);

	tracks = parrillada_burn_session_get_tracks (PARRILLADA_BURN_SESSION (object));
	for (; tracks; tracks = tracks->next) {
		ParrilladaTrack *track;

		track = tracks->data;
		g_signal_handlers_disconnect_by_func (track,
						      parrillada_session_cfg_session_loaded,
						      object);
	}

	manager = parrillada_plugin_manager_get_default ();
	g_signal_handlers_disconnect_by_func (manager,
	                                      parrillada_session_cfg_caps_changed,
	                                      object);

	G_OBJECT_CLASS (parrillada_session_cfg_parent_class)->finalize (object);
}

static void
parrillada_session_cfg_class_init (ParrilladaSessionCfgClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaBurnSessionClass *session_class = PARRILLADA_BURN_SESSION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaSessionCfgPrivate));

	object_class->finalize = parrillada_session_cfg_finalize;

	session_class->get_output_path = parrillada_session_cfg_get_output_path;
	session_class->get_output_format = parrillada_session_cfg_get_output_format;
	session_class->set_output_image = parrillada_session_cfg_set_output_image;

	session_class->track_added = parrillada_session_cfg_track_added;
	session_class->track_removed = parrillada_session_cfg_track_removed;
	session_class->track_changed = parrillada_session_cfg_track_changed;
	session_class->output_changed = parrillada_session_cfg_output_changed;
	session_class->tag_changed = parrillada_session_cfg_tag_changed;

	session_cfg_signals [WRONG_EXTENSION_SIGNAL] =
		g_signal_new ("wrong_extension",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              parrillada_marshal_BOOLEAN__VOID,
		              G_TYPE_BOOLEAN,
			      0,
		              G_TYPE_NONE);
	session_cfg_signals [IS_VALID_SIGNAL] =
		g_signal_new ("is_valid",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
			      0,
		              G_TYPE_NONE);
}

/**
 * parrillada_session_cfg_new:
 *
 *  Creates a new #ParrilladaSessionCfg object.
 *
 * Return value: a #ParrilladaSessionCfg object.
 **/

ParrilladaSessionCfg *
parrillada_session_cfg_new (void)
{
	return g_object_new (PARRILLADA_TYPE_SESSION_CFG, NULL);
}

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <libburn/libburn.h>

#include "parrillada-units.h"
#include "burn-job.h"
#include "burn-debug.h"
#include "parrillada-plugin-registration.h"
#include "burn-libburn-common.h"
#include "burn-libburnia.h"
#include "parrillada-track-image.h"
#include "parrillada-track-stream.h"


#define PARRILLADA_TYPE_LIBBURN         (parrillada_libburn_get_type ())
#define PARRILLADA_LIBBURN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_LIBBURN, ParrilladaLibburn))
#define PARRILLADA_LIBBURN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_LIBBURN, ParrilladaLibburnClass))
#define PARRILLADA_IS_LIBBURN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_LIBBURN))
#define PARRILLADA_IS_LIBBURN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_LIBBURN))
#define PARRILLADA_LIBBURN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_LIBBURN, ParrilladaLibburnClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaLibburn, parrillada_libburn, PARRILLADA_TYPE_JOB, ParrilladaJob);

#define PARRILLADA_PVD_SIZE	32ULL * 2048ULL

struct _ParrilladaLibburnPrivate {
	ParrilladaLibburnCtx *ctx;

	/* This buffer is used to capture Primary Volume Descriptor for
	 * for overwrite media so as to "grow" the latter. */
	unsigned char *pvd;

	guint sig_handler:1;
};
typedef struct _ParrilladaLibburnPrivate ParrilladaLibburnPrivate;

#define PARRILLADA_LIBBURN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_LIBBURN, ParrilladaLibburnPrivate))

/**
 * taken from scsi-get-configuration.h
 */

typedef enum {
PARRILLADA_SCSI_PROF_DVD_RW_RESTRICTED	= 0x0013,
PARRILLADA_SCSI_PROF_DVD_RW_PLUS		= 0x001A,
} ParrilladaScsiProfile;

static GObjectClass *parent_class = NULL;

struct _ParrilladaLibburnSrcData {
	int fd;
	off_t size;

	/* That's for the primary volume descriptor used for overwrite media */
	int pvd_size;						/* in blocks */
	unsigned char *pvd;

	int read_pvd:1;
};
typedef struct _ParrilladaLibburnSrcData ParrilladaLibburnSrcData;

static void
parrillada_libburn_src_free_data (struct burn_source *src)
{
	ParrilladaLibburnSrcData *data;

	data = src->data;
	close (data->fd);
	g_free (data);
}

static off_t
parrillada_libburn_src_get_size (struct burn_source *src)
{
	ParrilladaLibburnSrcData *data;

	data = src->data;
	return data->size;
}

static int
parrillada_libburn_src_set_size (struct burn_source *src,
			      off_t size)
{
	ParrilladaLibburnSrcData *data;

	data = src->data;
	data->size = size;
	return 1;
}

/**
 * This is a copy from burn-volume.c
 */

struct _ParrilladaVolDesc {
	guchar type;
	gchar id			[5];
	guchar version;
};
typedef struct _ParrilladaVolDesc ParrilladaVolDesc;

static int
parrillada_libburn_src_read_xt (struct burn_source *src,
			     unsigned char *buffer,
			     int size)
{
	int total;
	ParrilladaLibburnSrcData *data;

	data = src->data;

	total = 0;
	while (total < size) {
		int bytes;

		bytes = read (data->fd, buffer + total, size - total);
		if (bytes < 0)
			return -1;

		if (!bytes)
			break;

		total += bytes;
	}

	/* copy the primary volume descriptor if a buffer is provided */
	if (data->pvd
	&& !data->read_pvd
	&&  data->pvd_size < PARRILLADA_PVD_SIZE) {
		unsigned char *current_pvd;
		int i;

		current_pvd = data->pvd + data->pvd_size;

		/* read volume descriptors until we reach the end of the
		 * buffer or find a volume descriptor set end. */
		for (i = 0; (i << 11) < size && data->pvd_size + (i << 11) < PARRILLADA_PVD_SIZE; i ++) {
			ParrilladaVolDesc *desc;

			/* No need to check the first 16 blocks */
			if ((data->pvd_size >> 11) + i < 16)
				continue;

			desc = (ParrilladaVolDesc *) (buffer + sizeof (ParrilladaVolDesc) * i);
			if (desc->type == 255) {
				data->read_pvd = 1;
				PARRILLADA_BURN_LOG ("found volume descriptor set end");
				break;
			}
		}

		memcpy (current_pvd, buffer, i << 11);
		data->pvd_size += i << 11;
	}

	return total;
}

static struct burn_source *
parrillada_libburn_create_fd_source (int fd,
				  gint64 size,
				  unsigned char *pvd)
{
	struct burn_source *src;
	ParrilladaLibburnSrcData *data;

	data = g_new0 (ParrilladaLibburnSrcData, 1);
	data->fd = fd;
	data->size = size;
	data->pvd = pvd;

	/* FIXME: this could be wrapped into a fifo source to get a smoother
	 * data delivery. But that means another thread ... */
	src = g_new0 (struct burn_source, 1);
	src->version = 1;
	src->refcount = 1;
	src->read_xt = parrillada_libburn_src_read_xt;
	src->get_size = parrillada_libburn_src_get_size;
	src->set_size = parrillada_libburn_src_set_size;
	src->free_data = parrillada_libburn_src_free_data;
	src->data = data;

	return src;
}

static ParrilladaBurnResult
parrillada_libburn_add_track (struct burn_session *session,
			   struct burn_track *track,
			   struct burn_source *src,
			   gint mode,
			   GError **error)
{
	if (burn_track_set_source (track, src) != BURN_SOURCE_OK) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("libburn track could not be created"));
		return PARRILLADA_BURN_ERR;
	}

	if (!burn_session_add_track (session, track, BURN_POS_END)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("libburn track could not be created"));
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_libburn_add_fd_track (struct burn_session *session,
			      int fd,
			      gint mode,
			      gint64 size,
			      unsigned char *pvd,
			      GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	ParrilladaBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = parrillada_libburn_create_fd_source (fd, size, pvd);
	result = parrillada_libburn_add_track (session, track, src, mode, error);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

static ParrilladaBurnResult
parrillada_libburn_add_file_track (struct burn_session *session,
				const gchar *path,
				gint mode,
				off_t size,
				unsigned char *pvd,
				GError **error)
{
	int fd;

	fd = open (path, O_RDONLY);
	if (fd == -1) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     "%s",
			     g_strerror (errno));
		return PARRILLADA_BURN_ERR;
	}

	return parrillada_libburn_add_fd_track (session, fd, mode, size, pvd, error);
}

static ParrilladaBurnResult
parrillada_libburn_setup_session_fd (ParrilladaLibburn *self,
			          struct burn_session *session,
			          GError **error)
{
	int fd;
	goffset bytes = 0;
	ParrilladaLibburnPrivate *priv;
	ParrilladaTrackType *type = NULL;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	priv = PARRILLADA_LIBBURN_PRIVATE (self);

	parrillada_job_get_fd_in (PARRILLADA_JOB (self), &fd);

	/* need to find out what type of track the imager will output */
	type = parrillada_track_type_new ();
	parrillada_job_get_input_type (PARRILLADA_JOB (self), type);

	if (parrillada_track_type_get_has_image (type)) {
		gint mode;

		/* FIXME: implement other IMAGE types */
		if (parrillada_track_type_get_image_format (type) == PARRILLADA_IMAGE_FORMAT_BIN)
			mode = BURN_MODE1;
		else
			mode = BURN_MODE1|BURN_MODE_RAW|BURN_SUBCODE_R96;

		parrillada_track_type_free (type);

		parrillada_job_get_session_output_size (PARRILLADA_JOB (self),
						     NULL,
						     &bytes);

		result = parrillada_libburn_add_fd_track (session,
						       fd,
						       mode,
						       bytes,
						       priv->pvd,
						       error);
	}
	else if (parrillada_track_type_get_has_stream (type)) {
		GSList *tracks;
		guint64 length = 0;

		parrillada_track_type_free (type);

		parrillada_job_get_tracks (PARRILLADA_JOB (self), &tracks);
		for (; tracks; tracks = tracks->next) {
			ParrilladaTrack *track;

			track = tracks->data;
			parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (track), &length);
			bytes = PARRILLADA_DURATION_TO_BYTES (length);

			/* we dup the descriptor so the same 
			 * will be shared by all tracks */
			result = parrillada_libburn_add_fd_track (session,
							       dup (fd),
							       BURN_AUDIO,
							       bytes,
							       NULL,
							       error);
			if (result != PARRILLADA_BURN_OK)
				return result;
		}
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (self);

	return result;
}

static ParrilladaBurnResult
parrillada_libburn_setup_session_file (ParrilladaLibburn *self, 
				    struct burn_session *session,
				    GError **error)
{
	ParrilladaLibburnPrivate *priv;
	ParrilladaBurnResult result;
	GSList *tracks = NULL;

	priv = PARRILLADA_LIBBURN_PRIVATE (self);

	/* create the track(s) */
	result = PARRILLADA_BURN_OK;
	parrillada_job_get_tracks (PARRILLADA_JOB (self), &tracks);
	for (; tracks; tracks = tracks->next) {
		ParrilladaTrack *track;

		track = tracks->data;
		if (PARRILLADA_IS_TRACK_STREAM (track)) {
			gchar *audiopath;
			guint64 size;

			audiopath = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), FALSE);
			parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (track), &size);
			size = PARRILLADA_DURATION_TO_BYTES (size);

			result = parrillada_libburn_add_file_track (session,
								 audiopath,
								 BURN_AUDIO,
								 size,
								 NULL,
								 error);
			if (result != PARRILLADA_BURN_OK)
				break;
		}
		else if (PARRILLADA_IS_TRACK_IMAGE (track)) {
			ParrilladaImageFormat format;
			gchar *imagepath;
			goffset bytes;
			gint mode;

			format = parrillada_track_image_get_format (PARRILLADA_TRACK_IMAGE (track));
			if (format == PARRILLADA_IMAGE_FORMAT_BIN) {
				mode = BURN_MODE1;
				imagepath = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), FALSE);
			}
			else if (format == PARRILLADA_IMAGE_FORMAT_NONE) {
				mode = BURN_MODE1;
				imagepath = parrillada_track_image_get_source (PARRILLADA_TRACK_IMAGE (track), FALSE);
			}
			else
				PARRILLADA_JOB_NOT_SUPPORTED (self);

			if (!imagepath)
				return PARRILLADA_BURN_ERR;

			result = parrillada_track_get_size (track,
							 NULL,
							 &bytes);
			if (result != PARRILLADA_BURN_OK)
				return PARRILLADA_BURN_ERR;

			result = parrillada_libburn_add_file_track (session,
								 imagepath,
								 mode,
								 bytes,
								 priv->pvd,
								 error);
			g_free (imagepath);
		}
		else
			PARRILLADA_JOB_NOT_SUPPORTED (self);
	}

	return result;
}

static ParrilladaBurnResult
parrillada_libburn_create_disc (ParrilladaLibburn *self,
			     struct burn_disc **retval,
			     GError **error)
{
	struct burn_disc *disc;
	ParrilladaBurnResult result;
	struct burn_session *session;

	/* set the source image */
	disc = burn_disc_create ();

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (disc, session, BURN_POS_END);
	burn_session_free (session);

	if (parrillada_job_get_fd_in (PARRILLADA_JOB (self), NULL) == PARRILLADA_BURN_OK)
		result = parrillada_libburn_setup_session_fd (self, session, error);
	else
		result = parrillada_libburn_setup_session_file (self, session, error);

	if (result != PARRILLADA_BURN_OK) {
		burn_disc_free (disc);
		return result;
	}

	*retval = disc;
	return result;
}

static ParrilladaBurnResult
parrillada_libburn_start_record (ParrilladaLibburn *self,
			      GError **error)
{
	guint64 rate;
	goffset blocks = 0;
	ParrilladaMedia media;
	ParrilladaBurnFlag flags;
	ParrilladaBurnResult result;
	ParrilladaLibburnPrivate *priv;
	struct burn_write_opts *opts;
	gchar reason [BURN_REASONS_LEN];

	priv = PARRILLADA_LIBBURN_PRIVATE (self);

	/* if appending a DVD+-RW get PVD */
	parrillada_job_get_flags (PARRILLADA_JOB (self), &flags);
	parrillada_job_get_media (PARRILLADA_JOB (self), &media);

	if (flags & (PARRILLADA_BURN_FLAG_MERGE|PARRILLADA_BURN_FLAG_APPEND)
	&&  PARRILLADA_MEDIUM_RANDOM_WRITABLE (media)
	&& (media & PARRILLADA_MEDIUM_HAS_DATA))
		priv->pvd = g_new0 (unsigned char, PARRILLADA_PVD_SIZE);

	result = parrillada_libburn_create_disc (self, &priv->ctx->disc, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* Note: we don't need to call burn_drive_get_status nor
	 * burn_disc_get_status since we took care of the disc
	 * checking thing earlier ourselves. Now there is a proper
	 * disc and tray is locked. */
	opts = burn_write_opts_new (priv->ctx->drive);

	/* only turn this on for CDs */
	if ((media & PARRILLADA_MEDIUM_CD) != 0)
		burn_write_opts_set_perform_opc (opts, 0);
	else
		burn_write_opts_set_perform_opc (opts, 0);

	if (flags & PARRILLADA_BURN_FLAG_DAO)
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_SAO,
						BURN_BLOCK_SAO);
	else {
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_TAO,
						BURN_BLOCK_MODE1);

		/* we also set the start block to write from if MERGE is set.
		 * That only for random writable media; for other media libburn
		 * handles all by himself where to start writing. */
		if (PARRILLADA_MEDIUM_RANDOM_WRITABLE (media)
		&& (flags & PARRILLADA_BURN_FLAG_MERGE)) {
			goffset address = 0;

			parrillada_job_get_next_writable_address (PARRILLADA_JOB (self), &address);

			PARRILLADA_JOB_LOG (self, "Starting to write at block = %lli and byte %lli", address, address * 2048);
			burn_write_opts_set_start_byte (opts, address * 2048);
		}
	}

	if (!PARRILLADA_MEDIUM_RANDOM_WRITABLE (media)) {
		PARRILLADA_JOB_LOG (PARRILLADA_JOB (self), "Setting multi %i", (flags & PARRILLADA_BURN_FLAG_MULTI) != 0);
		burn_write_opts_set_multi (opts, (flags & PARRILLADA_BURN_FLAG_MULTI) != 0);
	}

	burn_write_opts_set_underrun_proof (opts, (flags & PARRILLADA_BURN_FLAG_BURNPROOF) != 0);
	PARRILLADA_JOB_LOG (PARRILLADA_JOB (self), "Setting burnproof %i", (flags & PARRILLADA_BURN_FLAG_BURNPROOF) != 0);

	burn_write_opts_set_simulate (opts, (flags & PARRILLADA_BURN_FLAG_DUMMY) != 0);
	PARRILLADA_JOB_LOG (PARRILLADA_JOB (self), "Setting dummy %i", (flags & PARRILLADA_BURN_FLAG_DUMMY) != 0);

	parrillada_job_get_rate (PARRILLADA_JOB (self), &rate);
	burn_drive_set_speed (priv->ctx->drive, rate, 0);

	if (burn_precheck_write (opts, priv->ctx->disc, reason, 0) < 1) {
		PARRILLADA_JOB_LOG (PARRILLADA_JOB (self), "Precheck failed %s", reason);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     reason);
		return PARRILLADA_BURN_ERR;
	}

	/* If we're writing to a disc remember that the session can't be under
	 * 300 sectors (= 614400 bytes) */
	parrillada_job_get_session_output_size (PARRILLADA_JOB (self), &blocks, NULL);
	if (blocks < 300)
		parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (self),
							       300L - blocks,
							       614400L - blocks * 2048);

	if (!priv->sig_handler) {
		burn_set_signal_handling ("parrillada", NULL, 0);
		priv->sig_handler = 1;
	}

	burn_disc_write (opts, priv->ctx->disc);
	burn_write_opts_free (opts);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_libburn_start_erase (ParrilladaLibburn *self,
			     GError **error)
{
	char reasons [BURN_REASONS_LEN];
	struct burn_session *session;
	struct burn_write_opts *opts;
	ParrilladaLibburnPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaBurnFlag flags;
	char prof_name [80];
	int profile;
	int fd;

	priv = PARRILLADA_LIBBURN_PRIVATE (self);
	if (burn_disc_get_profile (priv->ctx->drive, &profile, prof_name) <= 0) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_MEDIUM_INVALID,
			     _("The disc is not supported"));
		return PARRILLADA_BURN_ERR;
	}

	/* here we try to respect the current formatting of DVD-RW. For 
	 * overwritable media fast option means erase the first 64 Kib
	 * and long a forced reformatting */
	parrillada_job_get_flags (PARRILLADA_JOB (self), &flags);
	if (profile == PARRILLADA_SCSI_PROF_DVD_RW_RESTRICTED) {
		if (!(flags & PARRILLADA_BURN_FLAG_FAST_BLANK)) {
			/* leave libburn choose the best format */
			if (!priv->sig_handler) {
				burn_set_signal_handling ("parrillada", NULL, 0);
				priv->sig_handler = 1;
			}

			burn_disc_format (priv->ctx->drive,
					  (off_t) 0,
					  (1 << 4));
			return PARRILLADA_BURN_OK;
		}
	}
	else if (profile == PARRILLADA_SCSI_PROF_DVD_RW_PLUS) {
		if (!(flags & PARRILLADA_BURN_FLAG_FAST_BLANK)) {
			/* Bit 2 is for format max available size
			 * Bit 4 is enforce (re)-format if needed
			 * 0x26 is DVD+RW format is to be set from bit 8
			 * in the latter case bit 7 needs to be set as 
			 * well. */
			if (!priv->sig_handler) {
				burn_set_signal_handling ("parrillada", NULL, 0);
				priv->sig_handler = 1;
			}

			burn_disc_format (priv->ctx->drive,
					  (off_t) 0,
					  (1 << 2)|(1 << 4));
			return PARRILLADA_BURN_OK;
		}
	}
	else if (burn_disc_erasable (priv->ctx->drive)) {
		/* This is mainly for CDRW and sequential DVD-RW */
		if (!priv->sig_handler) {
			burn_set_signal_handling ("parrillada", NULL, 0);
			priv->sig_handler = 1;
		}

		/* NOTE: for an unknown reason (to me)
		 * libburn when minimally blanking a DVD-RW
		 * will only allow to write to it with DAO 
		 * afterwards... */
		burn_disc_erase (priv->ctx->drive, (flags & PARRILLADA_BURN_FLAG_FAST_BLANK) != 0);
		return PARRILLADA_BURN_OK;
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (self);

	/* This is the "fast option": basically we only write 64 Kib of 0 from
	 * /dev/null. If we reached that part it means we're dealing with
	 * overwrite media. */
	fd = open ("/dev/null", O_RDONLY);
	if (fd == -1) {
		int errnum = errno;

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: first %s is the filename, second %s is the error
			      * generated from errno */
			     _("\"%s\" could not be opened (%s)"),
			     "/dev/null",
			     g_strerror (errnum));
		return PARRILLADA_BURN_ERR;
	}

	priv->ctx->disc = burn_disc_create ();

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (priv->ctx->disc, session, BURN_POS_END);
	burn_session_free (session);

	result = parrillada_libburn_add_fd_track (session,
					       fd,
					       BURN_MODE1,
					       65536,		/* 32 blocks */
					       priv->pvd,
					       error);
	close (fd);

	opts = burn_write_opts_new (priv->ctx->drive);
	burn_write_opts_set_perform_opc (opts, 0);
	burn_write_opts_set_underrun_proof (opts, 1);
	burn_write_opts_set_simulate (opts, (flags & PARRILLADA_BURN_FLAG_DUMMY));

	burn_drive_set_speed (priv->ctx->drive, burn_drive_get_write_speed (priv->ctx->drive), 0);
	burn_write_opts_set_write_type (opts,
					BURN_WRITE_TAO,
					BURN_BLOCK_MODE1);

	if (burn_precheck_write (opts, priv->ctx->disc, reasons, 0) <= 0) {
		burn_write_opts_free (opts);

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: %s is the error returned by libburn */
			     _("An internal error occurred (%s)"),
			     reasons);
		return PARRILLADA_BURN_ERR;
	}

	if (!priv->sig_handler) {
		burn_set_signal_handling ("parrillada", NULL, 0);
		priv->sig_handler = 1;
	}

	burn_disc_write (opts, priv->ctx->disc);
	burn_write_opts_free (opts);

	return result;
}

static ParrilladaBurnResult
parrillada_libburn_start (ParrilladaJob *job,
		       GError **error)
{
	ParrilladaLibburn *self;
	ParrilladaJobAction action;
	ParrilladaBurnResult result;
	ParrilladaLibburnPrivate *priv;

	self = PARRILLADA_LIBBURN (job);
	priv = PARRILLADA_LIBBURN_PRIVATE (self);

	parrillada_job_get_action (job, &action);
	if (action == PARRILLADA_JOB_ACTION_RECORD) {
		GError *ret_error = NULL;

		/* TRUE is a context that helps to adapt action
		 * messages like for DVD+RW which need a
		 * pre-formatting before actually writing
		 * and without this we would not know if
		 * we are actually formatting or just pre-
		 * formatting == starting to record */
		priv->ctx = parrillada_libburn_common_ctx_new (job, TRUE, &ret_error);
		if (!priv->ctx) {
			if (ret_error && ret_error->code == PARRILLADA_BURN_ERROR_DRIVE_BUSY) {
				g_propagate_error (error, ret_error);
				return PARRILLADA_BURN_RETRY;
			}

			if (error)
				g_propagate_error (error, ret_error);
			return PARRILLADA_BURN_ERR;
		}

		result = parrillada_libburn_start_record (self, error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		parrillada_job_set_current_action (job,
						PARRILLADA_BURN_ACTION_START_RECORDING,
						NULL,
						FALSE);
	}
	else if (action == PARRILLADA_JOB_ACTION_ERASE) {
		GError *ret_error = NULL;

		priv->ctx = parrillada_libburn_common_ctx_new (job, FALSE, &ret_error);
		if (!priv->ctx) {
			if (ret_error && ret_error->code == PARRILLADA_BURN_ERROR_DRIVE_BUSY) {
				g_propagate_error (error, ret_error);
				return PARRILLADA_BURN_RETRY;
			}

			if (error)
				g_propagate_error (error, ret_error);
			return PARRILLADA_BURN_ERR;
		}

		result = parrillada_libburn_start_erase (self, error);
		if (result != PARRILLADA_BURN_OK)
			return result;

		parrillada_job_set_current_action (job,
						PARRILLADA_BURN_ACTION_BLANKING,
						NULL,
						FALSE);
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (self);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_libburn_stop (ParrilladaJob *job,
		      GError **error)
{
	ParrilladaLibburn *self;
	ParrilladaLibburnPrivate *priv;

	self = PARRILLADA_LIBBURN (job);
	priv = PARRILLADA_LIBBURN_PRIVATE (self);

	if (priv->sig_handler) {
		priv->sig_handler = 0;
		burn_set_signal_handling (NULL, NULL, 1);
	}

	if (priv->ctx) {
		parrillada_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->pvd) {
		g_free (priv->pvd);
		priv->pvd = NULL;
	}

	if (PARRILLADA_JOB_CLASS (parent_class)->stop)
		PARRILLADA_JOB_CLASS (parent_class)->stop (job, error);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_libburn_clock_tick (ParrilladaJob *job)
{
	ParrilladaLibburnPrivate *priv;
	ParrilladaBurnResult result;
	int ret;

	priv = PARRILLADA_LIBBURN_PRIVATE (job);
	result = parrillada_libburn_common_status (job, priv->ctx);

	if (result != PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_OK;

	/* Double check that everything went well */
	if (!burn_drive_wrote_well (priv->ctx->drive)) {
		PARRILLADA_JOB_LOG (job, "Something went wrong");
		parrillada_job_error (job,
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_WRITE_MEDIUM,
						_("An error occurred while writing to disc")));
		return PARRILLADA_BURN_OK;
	}

	/* That's finished */
	if (!priv->pvd) {
		parrillada_job_set_dangerous (job, FALSE);
		parrillada_job_finished_session (job);
		return PARRILLADA_BURN_OK;
	}

	/* In case we append data to a DVD+RW or DVD-RW
	 * (restricted overwrite) medium , we're not
	 * done since we need to overwrite the primary
	 * volume descriptor at sector 0.
	 * NOTE: This is a synchronous call but given the size of the buffer
	 * that shouldn't block.
	 * NOTE 2: in source we read the volume descriptors until we reached
	 * either the end of the buffer or the volume descriptor set end. That's
	 * kind of useless since for a DVD 16 blocks are written at a time. */
	PARRILLADA_JOB_LOG (job, "Starting to overwrite primary volume descriptor");
	ret = burn_random_access_write (priv->ctx->drive,
					0,
					(char*)priv->pvd,
					PARRILLADA_PVD_SIZE,
					0);
	if (ret != 1) {
		PARRILLADA_JOB_LOG (job, "Random write failed");
		parrillada_job_error (job,
				   g_error_new (PARRILLADA_BURN_ERROR,
						PARRILLADA_BURN_ERROR_WRITE_MEDIUM,
						_("An error occurred while writing to disc")));
		return PARRILLADA_BURN_OK;
	}

	parrillada_job_set_dangerous (job, FALSE);
	parrillada_job_finished_session (job);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_libburn_class_init (ParrilladaLibburnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaLibburnPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = parrillada_libburn_finalize;

	job_class->start = parrillada_libburn_start;
	job_class->stop = parrillada_libburn_stop;
	job_class->clock_tick = parrillada_libburn_clock_tick;
}

static void
parrillada_libburn_init (ParrilladaLibburn *obj)
{

}

static void
parrillada_libburn_finalize (GObject *object)
{
	ParrilladaLibburn *cobj;
	ParrilladaLibburnPrivate *priv;

	cobj = PARRILLADA_LIBBURN (object);
	priv = PARRILLADA_LIBBURN_PRIVATE (cobj);

	if (priv->ctx) {
		parrillada_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_libburn_export_caps (ParrilladaPlugin *plugin)
{
	const ParrilladaMedia media_cd = PARRILLADA_MEDIUM_CD|
				      PARRILLADA_MEDIUM_REWRITABLE|
				      PARRILLADA_MEDIUM_WRITABLE|
				      PARRILLADA_MEDIUM_BLANK|
				      PARRILLADA_MEDIUM_APPENDABLE|
				      PARRILLADA_MEDIUM_HAS_AUDIO|
				      PARRILLADA_MEDIUM_HAS_DATA;
	const ParrilladaMedia media_dvd_w = PARRILLADA_MEDIUM_DVD|
					 PARRILLADA_MEDIUM_PLUS|
					 PARRILLADA_MEDIUM_SEQUENTIAL|
					 PARRILLADA_MEDIUM_WRITABLE|
					 PARRILLADA_MEDIUM_APPENDABLE|
					 PARRILLADA_MEDIUM_HAS_DATA|
					 PARRILLADA_MEDIUM_BLANK;
	const ParrilladaMedia media_dvd_rw = PARRILLADA_MEDIUM_DVD|
					  PARRILLADA_MEDIUM_SEQUENTIAL|
					  PARRILLADA_MEDIUM_REWRITABLE|
					  PARRILLADA_MEDIUM_APPENDABLE|
					  PARRILLADA_MEDIUM_HAS_DATA|
					  PARRILLADA_MEDIUM_BLANK;
	const ParrilladaMedia media_dvd_rw_plus = PARRILLADA_MEDIUM_DVD|
					       PARRILLADA_MEDIUM_DUAL_L|
					       PARRILLADA_MEDIUM_PLUS|
					       PARRILLADA_MEDIUM_RESTRICTED|
					       PARRILLADA_MEDIUM_REWRITABLE|
					       PARRILLADA_MEDIUM_UNFORMATTED|
					       PARRILLADA_MEDIUM_BLANK|
					       PARRILLADA_MEDIUM_APPENDABLE|
					       PARRILLADA_MEDIUM_CLOSED|
					       PARRILLADA_MEDIUM_HAS_DATA;
	GSList *output;
	GSList *input;

	parrillada_plugin_define (plugin,
			       "libburn",
	                       NULL,
			       _("Burns, blanks and formats CDs, DVDs and BDs"),
			       "Philippe Rouquier",
			       15);

	/* libburn has no OVERBURN capabilities */

	/* CD(R)W */
	/* Use DAO for first session since AUDIO need it to write CD-TEXT
	 * Though libburn is unable to write CD-TEXT.... */
	/* Note: when burning multiple tracks to a CD (like audio for example)
	 * in dummy mode with TAO libburn will fail probably because it does
	 * not use a correct next writable address for the second track (it uses
	 * the same as for track #1). So remove dummy mode.
	 * This is probably the same reason why it fails at merging another
	 * session to a data CD in dummy mode. */
	PARRILLADA_PLUGIN_ADD_STANDARD_CDR_FLAGS (plugin,
	                                       PARRILLADA_BURN_FLAG_OVERBURN|
	                                       PARRILLADA_BURN_FLAG_DUMMY);
	PARRILLADA_PLUGIN_ADD_STANDARD_CDRW_FLAGS (plugin,
	                                        PARRILLADA_BURN_FLAG_OVERBURN|
	                                        PARRILLADA_BURN_FLAG_DUMMY);

	/* audio support for CDs only */
	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_PIPE|
					PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN);
	
	output = parrillada_caps_disc_new (media_cd);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	/* Image support for CDs ... */
	input = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_PIPE|
					PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_IMAGE_FORMAT_BIN);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... and DVD-R and DVD+R ... */
	output = parrillada_caps_disc_new (media_dvd_w);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	PARRILLADA_PLUGIN_ADD_STANDARD_DVDR_PLUS_FLAGS (plugin, PARRILLADA_BURN_FLAG_NONE);
	PARRILLADA_PLUGIN_ADD_STANDARD_DVDR_FLAGS (plugin, PARRILLADA_BURN_FLAG_NONE);

	/* ... and DVD-RW (sequential) */
	output = parrillada_caps_disc_new (media_dvd_rw);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_FLAGS (plugin, PARRILLADA_BURN_FLAG_NONE);

	/* for DVD+/-RW restricted */
	output = parrillada_caps_disc_new (media_dvd_rw_plus);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_RESTRICTED_FLAGS (plugin, PARRILLADA_BURN_FLAG_NONE);
	PARRILLADA_PLUGIN_ADD_STANDARD_DVDRW_PLUS_FLAGS (plugin, PARRILLADA_BURN_FLAG_NONE);

	/* add blank caps */
	output = parrillada_caps_disc_new (PARRILLADA_MEDIUM_CD|
					PARRILLADA_MEDIUM_REWRITABLE|
					PARRILLADA_MEDIUM_APPENDABLE|
					PARRILLADA_MEDIUM_CLOSED|
					PARRILLADA_MEDIUM_HAS_DATA|
					PARRILLADA_MEDIUM_HAS_AUDIO|
					PARRILLADA_MEDIUM_BLANK);
	parrillada_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	output = parrillada_caps_disc_new (PARRILLADA_MEDIUM_DVD|
					PARRILLADA_MEDIUM_PLUS|
					PARRILLADA_MEDIUM_SEQUENTIAL|
					PARRILLADA_MEDIUM_RESTRICTED|
					PARRILLADA_MEDIUM_REWRITABLE|
					PARRILLADA_MEDIUM_APPENDABLE|
					PARRILLADA_MEDIUM_CLOSED|
					PARRILLADA_MEDIUM_HAS_DATA|
					PARRILLADA_MEDIUM_UNFORMATTED|
				        PARRILLADA_MEDIUM_BLANK);
	parrillada_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	parrillada_plugin_set_blank_flags (plugin,
					PARRILLADA_MEDIUM_CD|
					PARRILLADA_MEDIUM_DVD|
					PARRILLADA_MEDIUM_SEQUENTIAL|
					PARRILLADA_MEDIUM_RESTRICTED|
					PARRILLADA_MEDIUM_REWRITABLE|
					PARRILLADA_MEDIUM_APPENDABLE|
					PARRILLADA_MEDIUM_CLOSED|
					PARRILLADA_MEDIUM_HAS_DATA|
					PARRILLADA_MEDIUM_HAS_AUDIO|
					PARRILLADA_MEDIUM_UNFORMATTED|
				        PARRILLADA_MEDIUM_BLANK,
					PARRILLADA_BURN_FLAG_NOGRACE|
					PARRILLADA_BURN_FLAG_FAST_BLANK,
					PARRILLADA_BURN_FLAG_NONE);

	/* no dummy mode for DVD+RW */
	parrillada_plugin_set_blank_flags (plugin,
					PARRILLADA_MEDIUM_DVDRW_PLUS|
					PARRILLADA_MEDIUM_APPENDABLE|
					PARRILLADA_MEDIUM_CLOSED|
					PARRILLADA_MEDIUM_HAS_DATA|
					PARRILLADA_MEDIUM_UNFORMATTED|
				        PARRILLADA_MEDIUM_BLANK,
					PARRILLADA_BURN_FLAG_NOGRACE|
					PARRILLADA_BURN_FLAG_FAST_BLANK,
					PARRILLADA_BURN_FLAG_NONE);

	parrillada_plugin_register_group (plugin, _(LIBBURNIA_DESCRIPTION));
}

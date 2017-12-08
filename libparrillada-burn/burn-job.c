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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "parrillada-drive.h"
#include "parrillada-medium.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "parrillada-session.h"
#include "parrillada-session-helper.h"
#include "parrillada-plugin-information.h"
#include "burn-job.h"
#include "burn-task-ctx.h"
#include "burn-task-item.h"
#include "libparrillada-marshal.h"

#include "parrillada-track-type-private.h"

typedef struct _ParrilladaJobOutput {
	gchar *image;
	gchar *toc;
} ParrilladaJobOutput;

typedef struct _ParrilladaJobInput {
	int out;
	int in;
} ParrilladaJobInput;

static void parrillada_job_iface_init_task_item (ParrilladaTaskItemIFace *iface);
G_DEFINE_TYPE_WITH_CODE (ParrilladaJob, parrillada_job, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (PARRILLADA_TYPE_TASK_ITEM,
						parrillada_job_iface_init_task_item));

typedef struct ParrilladaJobPrivate ParrilladaJobPrivate;
struct ParrilladaJobPrivate {
	ParrilladaJob *next;
	ParrilladaJob *previous;

	ParrilladaTaskCtx *ctx;

	/* used if job reads data from a pipe */
	ParrilladaJobInput *input;

	/* output type (sets at construct time) */
	ParrilladaTrackType type;

	/* used if job writes data to a pipe (link is then NULL) */
	ParrilladaJobOutput *output;
	ParrilladaJob *linked;
};

#define PARRILLADA_JOB_DEBUG(job_MACRO)						\
{										\
	const gchar *class_name_MACRO = NULL;					\
	if (PARRILLADA_IS_JOB (job_MACRO))						\
		class_name_MACRO = G_OBJECT_TYPE_NAME (job_MACRO);		\
	parrillada_job_log_message (job_MACRO, G_STRLOC,				\
				 "%s called %s", 				\
				 class_name_MACRO,				\
				 G_STRFUNC);					\
}

#define PARRILLADA_JOB_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_JOB, ParrilladaJobPrivate))

enum {
	PROP_NONE,
	PROP_OUTPUT,
};

typedef enum {
	ERROR_SIGNAL,
	LAST_SIGNAL
} ParrilladaJobSignalType;

static guint parrillada_job_signals [LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;


/**
 * Task item virtual functions implementation
 */

static ParrilladaTaskItem *
parrillada_job_item_next (ParrilladaTaskItem *item)
{
	ParrilladaJob *self;
	ParrilladaJobPrivate *priv;

	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->next)
		return NULL;

	return PARRILLADA_TASK_ITEM (priv->next);
}

static ParrilladaTaskItem *
parrillada_job_item_previous (ParrilladaTaskItem *item)
{
	ParrilladaJob *self;
	ParrilladaJobPrivate *priv;

	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->previous)
		return NULL;

	return PARRILLADA_TASK_ITEM (priv->previous);
}

static ParrilladaBurnResult
parrillada_job_item_link (ParrilladaTaskItem *input,
		       ParrilladaTaskItem *output)
{
	ParrilladaJobPrivate *priv_input;
	ParrilladaJobPrivate *priv_output;

	priv_input = PARRILLADA_JOB_PRIVATE (input);
	priv_output = PARRILLADA_JOB_PRIVATE (output);

	priv_input->next = PARRILLADA_JOB (output);
	priv_output->previous = PARRILLADA_JOB (input);

	g_object_ref (input);
	return PARRILLADA_BURN_OK;
}

static gboolean
parrillada_job_is_last_active (ParrilladaJob *self)
{
	ParrilladaJobPrivate *priv;
	ParrilladaJob *next;

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->ctx)
		return FALSE;

	next = priv->next;
	while (next) {
		priv = PARRILLADA_JOB_PRIVATE (next);
		if (priv->ctx)
			return FALSE;
		next = priv->next;
	}

	return TRUE;
}

static gboolean
parrillada_job_is_first_active (ParrilladaJob *self)
{
	ParrilladaJobPrivate *priv;
	ParrilladaJob *prev;

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->ctx)
		return FALSE;

	prev = priv->previous;
	while (prev) {
		priv = PARRILLADA_JOB_PRIVATE (prev);
		if (priv->ctx)
			return FALSE;
		prev = priv->previous;
	}

	return TRUE;
}

static void
parrillada_job_input_free (ParrilladaJobInput *input)
{
	if (!input)
		return;

	if (input->in > 0)
		close (input->in);

	if (input->out > 0)
		close (input->out);

	g_free (input);
}

static void
parrillada_job_output_free (ParrilladaJobOutput *output)
{
	if (!output)
		return;

	if (output->image) {
		g_free (output->image);
		output->image = NULL;
	}

	if (output->toc) {
		g_free (output->toc);
		output->toc = NULL;
	}

	g_free (output);
}

static void
parrillada_job_deactivate (ParrilladaJob *self)
{
	ParrilladaJobPrivate *priv;

	priv = PARRILLADA_JOB_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "deactivating");

	/* ::start hasn't been called yet */
	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->input) {
		parrillada_job_input_free (priv->input);
		priv->input = NULL;
	}

	if (priv->output) {
		parrillada_job_output_free (priv->output);
		priv->output = NULL;
	}

	if (priv->linked)
		priv->linked = NULL;
}

static ParrilladaBurnResult
parrillada_job_allow_deactivation (ParrilladaJob *self,
				ParrilladaBurnSession *session,
				GError **error)
{
	ParrilladaJobPrivate *priv;
	ParrilladaTrackType input;

	priv = PARRILLADA_JOB_PRIVATE (self);

	/* This job refused to work. This is allowed in three cases:
	 * - the job is the only one in the task (no other job linked) and the
	 *   track type as input is the same as the track type of the output
	 *   except if type is DISC as input or output
	 * - if the job hasn't got any job linked before the next linked job
	 *   accepts the track type of the session as input
	 * - the output of the previous job is the same as this job output type
	 */

	/* there can't be two recorders in a row so ... */
	if (priv->type.type == PARRILLADA_TRACK_TYPE_DISC)
		goto error;

	if (priv->previous) {
		ParrilladaJobPrivate *previous;
		previous = PARRILLADA_JOB_PRIVATE (priv->previous);
		memcpy (&input, &previous->type, sizeof (ParrilladaTrackType));
	}
	else
		parrillada_burn_session_get_input_type (session, &input);

	if (!parrillada_track_type_equal (&input, &priv->type))
		goto error;

	return PARRILLADA_BURN_NOT_RUNNING;

error:

	g_set_error (error,
		     PARRILLADA_BURN_ERR,
		     PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,
		     /* Translators: %s is the plugin name */
		     _("\"%s\" did not behave properly"),
		     G_OBJECT_TYPE_NAME (self));
	return PARRILLADA_BURN_ERR;
}

static ParrilladaBurnResult
parrillada_job_item_activate (ParrilladaTaskItem *item,
			   ParrilladaTaskCtx *ctx,
			   GError **error)
{
	ParrilladaJob *self;
	ParrilladaJobClass *klass;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnSession *session;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (ctx);

	g_object_ref (ctx);
	priv->ctx = ctx;

	klass = PARRILLADA_JOB_GET_CLASS (self);

	/* see if this job needs to be deactivated (if no function then OK) */
	if (klass->activate)
		result = klass->activate (self, error);
	else
		PARRILLADA_BURN_LOG ("no ::activate method %s",
				  G_OBJECT_TYPE_NAME (item));

	if (result != PARRILLADA_BURN_OK) {
		g_object_unref (ctx);
		priv->ctx = NULL;

		if (result == PARRILLADA_BURN_NOT_RUNNING)
			result = parrillada_job_allow_deactivation (self, session, error);

		return result;
	}

	return PARRILLADA_BURN_OK;
}

static gboolean
parrillada_job_item_is_active (ParrilladaTaskItem *item)
{
	ParrilladaJob *self;
	ParrilladaJobPrivate *priv;

	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);

	return (priv->ctx != NULL);
}

static ParrilladaBurnResult
parrillada_job_check_output_disc_space (ParrilladaJob *self,
				     GError **error)
{
	ParrilladaBurnSession *session;
	goffset output_blocks = 0;
	goffset media_blocks = 0;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnFlag flags;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;

	priv = PARRILLADA_JOB_PRIVATE (self);

	parrillada_task_ctx_get_session_output_size (priv->ctx,
						  &output_blocks,
						  NULL);

	session = parrillada_task_ctx_get_session (priv->ctx);
	drive = parrillada_burn_session_get_burner (session);
	medium = parrillada_drive_get_medium (drive);
	flags = parrillada_burn_session_get_flags (session);

	/* FIXME: if we can't recover the size of the medium 
	 * what should we do ? do as if we could ? */

	/* see if we are appending or not */
	if (flags & (PARRILLADA_BURN_FLAG_APPEND|PARRILLADA_BURN_FLAG_MERGE))
		parrillada_medium_get_free_space (medium, NULL, &media_blocks);
	else
		parrillada_medium_get_capacity (medium, NULL, &media_blocks);

	/* This is not really an error, we'll probably ask the 
	 * user to load a new disc */
	if (output_blocks > media_blocks) {
		gchar *media_blocks_str;
		gchar *output_blocks_str;

		PARRILLADA_BURN_LOG ("Insufficient space on disc %"G_GINT64_FORMAT"/%"G_GINT64_FORMAT,
				  media_blocks,
				  output_blocks);

		media_blocks_str = g_strdup_printf ("%"G_GINT64_FORMAT, media_blocks);
		output_blocks_str = g_strdup_printf ("%"G_GINT64_FORMAT, output_blocks);

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_MEDIUM_SPACE,
			     /* Translators: the first %s is the size of the free space on the medium
			      * and the second %s is the size of the space required by the data to be
			      * burnt. */
			     _("Not enough space available on the disc (%s available for %s)"),
			     media_blocks_str,
			     output_blocks_str);

		g_free (media_blocks_str);
		g_free (output_blocks_str);
		return PARRILLADA_BURN_NEED_RELOAD;
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_job_check_output_volume_space (ParrilladaJob *self,
				       GError **error)
{
	GFileInfo *info;
	gchar *directory;
	GFile *file = NULL;
	struct rlimit limit;
	guint64 vol_size = 0;
	gint64 output_size = 0;
	ParrilladaJobPrivate *priv;
	const gchar *filesystem;

	/* now that the job has a known output we must check that the volume the
	 * job is writing to has enough space for all output */

	priv = PARRILLADA_JOB_PRIVATE (self);

	/* Get the size and filesystem type for the volume first.
	 * NOTE: since any plugin must output anything LOCALLY, we can then use
	 * all libc API. */
	if (!priv->output)
		return PARRILLADA_BURN_ERR;

	directory = g_path_get_dirname (priv->output->image);
	file = g_file_new_for_path (directory);
	g_free (directory);

	if (file == NULL)
		goto error;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	if (!info)
		goto error;

	/* Check permissions first */
	if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
		PARRILLADA_JOB_LOG (self, "No permissions");

		g_object_unref (info);
		g_object_unref (file);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_PERMISSION,
			     _("You do not have the required permission to write at this location"));
		return PARRILLADA_BURN_ERR;
	}
	g_object_unref (info);

	/* Now size left */
	info = g_file_query_filesystem_info (file,
					     G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
					     G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					     NULL,
					     error);
	if (!info)
		goto error;

	g_object_unref (file);
	file = NULL;

	/* Now check the filesystem type: the problem here is that some
	 * filesystems have a maximum file size limit of 4 GiB and more than
	 * often we need a temporary file size of 4 GiB or more. */
	filesystem = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	PARRILLADA_BURN_LOG ("%s filesystem detected", filesystem);

	parrillada_job_get_session_output_size (self, NULL, &output_size);

	if (output_size >= 2147483648ULL
	&&  filesystem
	&& !strcmp (filesystem, "msdos")) {
		g_object_unref (info);
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_DISK_SPACE,
			     _("The filesystem you chose to store the temporary image on cannot hold files with a size over 2 GiB"));
		return PARRILLADA_BURN_ERR;
	}

	vol_size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	g_object_unref (info);

	/* get the size of the output this job is supposed to create */
	PARRILLADA_BURN_LOG ("Volume size %lli, output size %lli", vol_size, output_size);

	/* it's fine here to check size in bytes */
	if (output_size > vol_size) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_DISK_SPACE,
			     _("The location you chose to store the temporary image on does not have enough free space for the disc image (%ld MiB needed)"),
			     (unsigned long) output_size / 1048576);
		return PARRILLADA_BURN_ERR;
	}

	/* Last but not least, use getrlimit () to check that we are allowed to
	 * write a file of such length and that quotas won't get in our way */
	if (getrlimit (RLIMIT_FSIZE, &limit)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_DISK_SPACE,
			     "%s",
			     g_strerror (errno));
		return PARRILLADA_BURN_ERR;
	}

	if (limit.rlim_cur < output_size) {
		PARRILLADA_BURN_LOG ("User not allowed to write such a large file");

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_DISK_SPACE,
			     _("The location you chose to store the temporary image on does not have enough free space for the disc image (%ld MiB needed)"),
			     (unsigned long) output_size / 1048576);
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;

error:

	if (error && *error == NULL)
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("The size of the volume could not be retrieved"));

	if (file)
		g_object_unref (file);

	return PARRILLADA_BURN_ERR;
}

static ParrilladaBurnResult
parrillada_job_set_output_file (ParrilladaJob *self,
			     GError **error)
{
	ParrilladaTrackType *session_output;
	ParrilladaBurnSession *session;
	ParrilladaBurnResult result;
	ParrilladaJobPrivate *priv;
	goffset output_size = 0;
	gchar *image = NULL;
	gchar *toc = NULL;
	gboolean is_last;

	priv = PARRILLADA_JOB_PRIVATE (self);

	/* no next job so we need a file pad */
	session = parrillada_task_ctx_get_session (priv->ctx);

	/* If the plugin is not supposed to output anything, then don't test */
	parrillada_job_get_session_output_size (PARRILLADA_JOB (self), NULL, &output_size);

	/* This should be re-enabled when we make sure all plugins (like vcd)
	 * don't advertize an output of 0 whereas it's not true. Maybe we could
	 * use -1 for plugins that don't output. */
	/* if (!output_size)
		return PARRILLADA_BURN_OK; */

	/* check if that's the last task */
	session_output = parrillada_track_type_new ();
	parrillada_burn_session_get_output_type (session, session_output);
	is_last = parrillada_track_type_equal (session_output, &priv->type);
	parrillada_track_type_free (session_output);

	if (priv->type.type == PARRILLADA_TRACK_TYPE_IMAGE) {
		if (is_last) {
			ParrilladaTrackType input = { 0, };

			result = parrillada_burn_session_get_output (session,
								  &image,
								  &toc);

			/* check paths are set */
			if (!image
			|| (priv->type.subtype.img_format != PARRILLADA_IMAGE_FORMAT_BIN && !toc)) {
				g_set_error (error,
					     PARRILLADA_BURN_ERROR,
					     PARRILLADA_BURN_ERROR_OUTPUT_NONE,
					     _("No path was specified for the image output"));
				return PARRILLADA_BURN_ERR;
			}

			parrillada_burn_session_get_input_type (session,
							     &input);

			/* if input is the same as output, then that's a
			 * processing task and there's no need to check if the
			 * output already exists since it will (but that's OK) */
			if (input.type == PARRILLADA_TRACK_TYPE_IMAGE
			&&  input.subtype.img_format == priv->type.subtype.img_format) {
				PARRILLADA_BURN_LOG ("Processing task, skipping check size");
				priv->output = g_new0 (ParrilladaJobOutput, 1);
				priv->output->image = image;
				priv->output->toc = toc;
				return PARRILLADA_BURN_OK;
			}
		}
		else {
			/* NOTE: no need to check for the existence here */
			result = parrillada_burn_session_get_tmp_image (session,
								     priv->type.subtype.img_format,
								     &image,
								     &toc,
								     error);
			if (result != PARRILLADA_BURN_OK)
				return result;
		}

		PARRILLADA_JOB_LOG (self, "output set (IMAGE) image = %s toc = %s",
				 image,
				 toc ? toc : "none");
	}
	else if (priv->type.type == PARRILLADA_TRACK_TYPE_STREAM) {
		/* NOTE: this one can only a temporary file */
		result = parrillada_burn_session_get_tmp_file (session,
							    ".cdr",
							    &image,
							    error);
		PARRILLADA_JOB_LOG (self, "Output set (AUDIO) image = %s", image);
	}
	else /* other types don't need an output */
		return PARRILLADA_BURN_OK;

	if (result != PARRILLADA_BURN_OK)
		return result;

	priv->output = g_new0 (ParrilladaJobOutput, 1);
	priv->output->image = image;
	priv->output->toc = toc;

	if (parrillada_burn_session_get_flags (session) & PARRILLADA_BURN_FLAG_CHECK_SIZE)
		return parrillada_job_check_output_volume_space (self, error);

	return result;
}

static ParrilladaJob *
parrillada_job_get_next_active (ParrilladaJob *self)
{
	ParrilladaJobPrivate *priv;
	ParrilladaJob *next;

	/* since some jobs can return NOT_RUNNING after ::init, skip them */
	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->next)
		return NULL;

	next = priv->next;
	while (next) {
		priv = PARRILLADA_JOB_PRIVATE (next);

		if (priv->ctx)
			return next;

		next = priv->next;
	}

	return NULL;
}

static ParrilladaBurnResult
parrillada_job_item_start (ParrilladaTaskItem *item,
		        GError **error)
{
	ParrilladaJob *self;
	ParrilladaJobClass *klass;
	ParrilladaJobAction action;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result;

	/* This function is compulsory */
	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);

	/* skip jobs that are not active */
	if (!priv->ctx)
		return PARRILLADA_BURN_OK;

	/* set the output if need be */
	parrillada_job_get_action (self, &action);
	priv->linked = parrillada_job_get_next_active (self);

	if (!priv->linked) {
		/* that's the last job so is action is image it needs a file */
		if (action == PARRILLADA_JOB_ACTION_IMAGE) {
			result = parrillada_job_set_output_file (self, error);
			if (result != PARRILLADA_BURN_OK)
				return result;
		}
		else if (action == PARRILLADA_JOB_ACTION_RECORD) {
			ParrilladaBurnFlag flags;
			ParrilladaBurnSession *session;

			session = parrillada_task_ctx_get_session (priv->ctx);
			flags = parrillada_burn_session_get_flags (session);
			if (flags & PARRILLADA_BURN_FLAG_CHECK_SIZE
			&& !(flags & PARRILLADA_BURN_FLAG_OVERBURN)) {
				result = parrillada_job_check_output_disc_space (self, error);
				if (result != PARRILLADA_BURN_OK)
					return result;
			}
		}
	}
	else
		PARRILLADA_JOB_LOG (self, "linked to %s", G_OBJECT_TYPE_NAME (priv->linked));

	if (!parrillada_job_is_first_active (self)) {
		int fd [2];

		PARRILLADA_JOB_LOG (self, "creating input");

		/* setup a pipe */
		if (pipe (fd)) {
                        int errsv = errno;

			PARRILLADA_BURN_LOG ("A pipe couldn't be created");
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("An internal error occurred (%s)"),
				     g_strerror (errsv));

			return PARRILLADA_BURN_ERR;
		}

		/* NOTE: don't set O_NONBLOCK automatically as some plugins 
		 * don't like that (genisoimage, mkisofs) */
		priv->input = g_new0 (ParrilladaJobInput, 1);
		priv->input->in = fd [0];
		priv->input->out = fd [1];
	}

	klass = PARRILLADA_JOB_GET_CLASS (self);
	if (!klass->start) {
		PARRILLADA_JOB_LOG (self, "no ::start method");
		PARRILLADA_JOB_NOT_SUPPORTED (self);
	}

	result = klass->start (self, error);
	if (result == PARRILLADA_BURN_NOT_RUNNING) {
		/* this means that the task is already completed. This 
		 * must be returned by the last active job of the task
		 * (and usually the only one?) */

		if (priv->linked) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
			return PARRILLADA_BURN_ERR;
		}
	}

	if (result == PARRILLADA_BURN_NOT_SUPPORTED) {
		/* only forgive this error when that's the last job and we're
		 * searching for a job to set the current track size */
		if (action != PARRILLADA_JOB_ACTION_SIZE) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
			return PARRILLADA_BURN_ERR;
		}

		/* deactivate it */
		parrillada_job_deactivate (self);
	}

	return result;
}

static ParrilladaBurnResult
parrillada_job_item_clock_tick (ParrilladaTaskItem *item,
			     ParrilladaTaskCtx *ctx,
			     GError **error)
{
	ParrilladaJob *self;
	ParrilladaJobClass *klass;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->ctx)
		return PARRILLADA_BURN_OK;

	klass = PARRILLADA_JOB_GET_CLASS (self);
	if (klass->clock_tick)
		result = klass->clock_tick (self);

	return result;
}

static ParrilladaBurnResult
parrillada_job_disconnect (ParrilladaJob *self,
			GError **error)
{
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	priv = PARRILLADA_JOB_PRIVATE (self);

	/* NOTE: this function is only called when there are no more track to 
	 * process */

	if (priv->linked) {
		ParrilladaJobPrivate *priv_link;

		PARRILLADA_JOB_LOG (self,
				 "disconnecting %s from %s",
				 G_OBJECT_TYPE_NAME (self),
				 G_OBJECT_TYPE_NAME (priv->linked));

		priv_link = PARRILLADA_JOB_PRIVATE (priv->linked);

		/* only close the input to tell the other end that we're
		 * finished with writing to the pipe */
		if (priv_link->input->out > 0) {
			close (priv_link->input->out);
			priv_link->input->out = 0;
		}
	}
	else if (priv->output) {
		parrillada_job_output_free (priv->output);
		priv->output = NULL;
	}

	if (priv->input) {
		PARRILLADA_JOB_LOG (self,
				 "closing connection for %s",
				 G_OBJECT_TYPE_NAME (self));

		parrillada_job_input_free (priv->input);
		priv->input = NULL;
	}

	return result;
}

static ParrilladaBurnResult
parrillada_job_item_stop (ParrilladaTaskItem *item,
		       ParrilladaTaskCtx *ctx,
		       GError **error)
{
	ParrilladaJob *self;
	ParrilladaJobClass *klass;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;
	
	self = PARRILLADA_JOB (item);
	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->ctx)
		return PARRILLADA_BURN_OK;

	PARRILLADA_JOB_LOG (self, "stopping");

	/* the order is important here */
	klass = PARRILLADA_JOB_GET_CLASS (self);
	if (klass->stop)
		result = klass->stop (self, error);

	parrillada_job_disconnect (self, error);

	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	return result;
}

static void
parrillada_job_iface_init_task_item (ParrilladaTaskItemIFace *iface)
{
	iface->next = parrillada_job_item_next;
	iface->previous = parrillada_job_item_previous;
	iface->link = parrillada_job_item_link;
	iface->activate = parrillada_job_item_activate;
	iface->is_active = parrillada_job_item_is_active;
	iface->start = parrillada_job_item_start;
	iface->stop = parrillada_job_item_stop;
	iface->clock_tick = parrillada_job_item_clock_tick;
}

ParrilladaBurnResult
parrillada_job_tag_lookup (ParrilladaJob *self,
			const gchar *tag,
			GValue **value)
{
	ParrilladaJobPrivate *priv;
	ParrilladaBurnSession *session;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	session = parrillada_task_ctx_get_session (priv->ctx);
	return parrillada_burn_session_tag_lookup (session,
						tag,
						value);
}

ParrilladaBurnResult
parrillada_job_tag_add (ParrilladaJob *self,
		     const gchar *tag,
		     GValue *value)
{
	ParrilladaJobPrivate *priv;
	ParrilladaBurnSession *session;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!parrillada_job_is_last_active (self))
		return PARRILLADA_BURN_ERR;

	session = parrillada_task_ctx_get_session (priv->ctx);
	parrillada_burn_session_tag_add (session,
				      tag,
				      value);

	return PARRILLADA_BURN_OK;
}

/**
 * Means a job successfully completed its task.
 * track can be NULL, depending on whether or not the job created a track.
 */

ParrilladaBurnResult
parrillada_job_add_track (ParrilladaJob *self,
		       ParrilladaTrack *track)
{
	ParrilladaJobPrivate *priv;
	ParrilladaJobAction action;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	/* to add a track to the session, a job :
	 * - must be the last running in the chain
	 * - the action for the job must be IMAGE */

	action = PARRILLADA_JOB_ACTION_NONE;
	parrillada_job_get_action (self, &action);
	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		return PARRILLADA_BURN_ERR;

	if (!parrillada_job_is_last_active (self))
		return PARRILLADA_BURN_ERR;

	return parrillada_task_ctx_add_track (priv->ctx, track);
}

ParrilladaBurnResult
parrillada_job_finished_session (ParrilladaJob *self)
{
	GError *error = NULL;
	ParrilladaJobClass *klass;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result;

	priv = PARRILLADA_JOB_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Finished successfully session");

	if (parrillada_job_is_last_active (self))
		return parrillada_task_ctx_finished (priv->ctx);

	if (!parrillada_job_is_first_active (self)) {
		/* This job is apparently a go between job.
		 * It should only call for a stop on an error. */
		PARRILLADA_JOB_LOG (self, "is not a leader");
		error = g_error_new (PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
		return parrillada_task_ctx_error (priv->ctx,
					       PARRILLADA_BURN_ERR,
					       error);
	}

	/* call the stop method of the job since it's finished */ 
	klass = PARRILLADA_JOB_GET_CLASS (self);
	if (klass->stop) {
		result = klass->stop (self, &error);
		if (result != PARRILLADA_BURN_OK)
			return parrillada_task_ctx_error (priv->ctx,
						       result,
						       error);
	}

	/* this job is finished but it's not the leader so
	 * the task is not finished. Close the pipe on
	 * one side to let the next job know that there
	 * isn't any more data to be expected */
	result = parrillada_job_disconnect (self, &error);
	g_object_unref (priv->ctx);
	priv->ctx = NULL;

	if (result != PARRILLADA_BURN_OK)
		return parrillada_task_ctx_error (priv->ctx,
					       result,
					       error);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_finished_track (ParrilladaJob *self)
{
	GError *error = NULL;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnResult result;

	priv = PARRILLADA_JOB_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Finished track successfully");

	/* we first check if it's the first job */
	if (parrillada_job_is_first_active (self)) {
		ParrilladaJobClass *klass;

		/* call ::stop for the job since it's finished */ 
		klass = PARRILLADA_JOB_GET_CLASS (self);
		if (klass->stop) {
			result = klass->stop (self, &error);

			if (result != PARRILLADA_BURN_OK)
				return parrillada_task_ctx_error (priv->ctx,
							       result,
							       error);
		}

		/* see if there is another track to process */
		result = parrillada_task_ctx_next_track (priv->ctx);
		if (result == PARRILLADA_BURN_RETRY) {
			/* there is another track to process: don't close the
			 * input of the next connected job. Leave it active */
			return PARRILLADA_BURN_OK;
		}

		if (!parrillada_job_is_last_active (self)) {
			/* this job is finished but it's not the leader so the
			 * task is not finished. Close the pipe on one side to
			 * let the next job know that there isn't any more data
			 * to be expected */
			result = parrillada_job_disconnect (self, &error);

			parrillada_job_deactivate (self);

			if (result != PARRILLADA_BURN_OK)
				return parrillada_task_ctx_error (priv->ctx,
							       result,
							       error);

			return PARRILLADA_BURN_OK;
		}
	}
	else if (!parrillada_job_is_last_active (self)) {
		/* This job is apparently a go between job. It should only call
		 * for a stop on an error. */
		PARRILLADA_JOB_LOG (self, "is not a leader");
		error = g_error_new (PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
		return parrillada_task_ctx_error (priv->ctx, PARRILLADA_BURN_ERR, error);
	}

	return parrillada_task_ctx_finished (priv->ctx);
}

/**
 * means a job didn't successfully completed its task
 */

ParrilladaBurnResult
parrillada_job_error (ParrilladaJob *self, GError *error)
{
	GValue instance_and_params [2];
	ParrilladaJobPrivate *priv;
	GValue return_value;

	PARRILLADA_JOB_DEBUG (self);

	PARRILLADA_JOB_LOG (self, "finished with an error");

	priv = PARRILLADA_JOB_PRIVATE (self);

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_INT);

	if (error)
		g_value_set_int (instance_and_params + 1, error->code);
	else
		g_value_set_int (instance_and_params + 1, PARRILLADA_BURN_ERROR_GENERAL);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, PARRILLADA_BURN_ERR);

	/* There was an error: signal it. That's mainly done
	 * for ParrilladaBurnCaps to override the result value */
	g_signal_emitv (instance_and_params,
			parrillada_job_signals [ERROR_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	PARRILLADA_JOB_LOG (self,
			 "asked to stop because of an error\n"
			 "\terror\t\t= %i\n"
			 "\tmessage\t= \"%s\"",
			 error ? error->code:0,
			 error ? error->message:"no message");

	return parrillada_task_ctx_error (priv->ctx, g_value_get_int (&return_value), error);
}

/**
 * Used to retrieve config for a job
 * If the parameter is missing for the next 4 functions
 * it allows to test if they could be used
 */

ParrilladaBurnResult
parrillada_job_get_fd_in (ParrilladaJob *self, int *fd_in)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->input)
		return PARRILLADA_BURN_ERR;

	if (!fd_in)
		return PARRILLADA_BURN_OK;

	*fd_in = priv->input->in;
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_job_set_nonblocking_fd (int fd, GError **error)
{
	long flags = 0;

	if (fcntl (fd, F_GETFL, &flags) != -1) {
		/* Unfortunately some plugin (mkisofs/genisofs don't like 
		 * O_NONBLOCK (which is a shame) so we don't set them
		 * automatically but still offer that possibility. */
		flags |= O_NONBLOCK;
		if (fcntl (fd, F_SETFL, flags) == -1) {
			PARRILLADA_BURN_LOG ("couldn't set non blocking mode");
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("An internal error occurred"));
			return PARRILLADA_BURN_ERR;
		}
	}
	else {
		PARRILLADA_BURN_LOG ("couldn't get pipe flags");
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("An internal error occurred"));
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_set_nonblocking (ParrilladaJob *self,
			     GError **error)
{
	ParrilladaBurnResult result;
	int fd;

	PARRILLADA_JOB_DEBUG (self);

	fd = -1;
	if (parrillada_job_get_fd_in (self, &fd) == PARRILLADA_BURN_OK) {
		result = parrillada_job_set_nonblocking_fd (fd, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	fd = -1;
	if (parrillada_job_get_fd_out (self, &fd) == PARRILLADA_BURN_OK) {
		result = parrillada_job_set_nonblocking_fd (fd, error);
		if (result != PARRILLADA_BURN_OK)
			return result;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_current_track (ParrilladaJob *self,
			       ParrilladaTrack **track)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!track)
		return PARRILLADA_BURN_OK;

	return parrillada_task_ctx_get_current_track (priv->ctx, track);
}

ParrilladaBurnResult
parrillada_job_get_done_tracks (ParrilladaJob *self, GSList **tracks)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	/* tracks already done are those that are in session */
	priv = PARRILLADA_JOB_PRIVATE (self);
	return parrillada_task_ctx_get_stored_tracks (priv->ctx, tracks);
}

ParrilladaBurnResult
parrillada_job_get_tracks (ParrilladaJob *self, GSList **tracks)
{
	ParrilladaJobPrivate *priv;
	ParrilladaBurnSession *session;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (tracks != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	*tracks = parrillada_burn_session_get_tracks (session);
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_fd_out (ParrilladaJob *self, int *fd_out)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->linked)
		return PARRILLADA_BURN_ERR;

	if (!fd_out)
		return PARRILLADA_BURN_OK;

	priv = PARRILLADA_JOB_PRIVATE (priv->linked);
	if (!priv->input)
		return PARRILLADA_BURN_ERR;

	*fd_out = priv->input->out;
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_image_output (ParrilladaJob *self,
			      gchar **image,
			      gchar **toc)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->output)
		return PARRILLADA_BURN_ERR;

	if (image)
		*image = g_strdup (priv->output->image);

	if (toc)
		*toc = g_strdup (priv->output->toc);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_audio_output (ParrilladaJob *self,
			      gchar **path)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->output)
		return PARRILLADA_BURN_ERR;

	if (path)
		*path = g_strdup (priv->output->image);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_flags (ParrilladaJob *self, ParrilladaBurnFlag *flags)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (flags != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	*flags = parrillada_burn_session_get_flags (session);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_input_type (ParrilladaJob *self, ParrilladaTrackType *type)
{
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!priv->previous) {
		ParrilladaBurnSession *session;

		session = parrillada_task_ctx_get_session (priv->ctx);
		result = parrillada_burn_session_get_input_type (session, type);
	}
	else {
		ParrilladaJobPrivate *prev_priv;

		prev_priv = PARRILLADA_JOB_PRIVATE (priv->previous);
		memcpy (type, &prev_priv->type, sizeof (ParrilladaTrackType));
		result = PARRILLADA_BURN_OK;
	}

	return result;
}

ParrilladaBurnResult
parrillada_job_get_output_type (ParrilladaJob *self, ParrilladaTrackType *type)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	memcpy (type, &priv->type, sizeof (ParrilladaTrackType));
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_action (ParrilladaJob *self, ParrilladaJobAction *action)
{
	ParrilladaJobPrivate *priv;
	ParrilladaTaskAction task_action;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!parrillada_job_is_last_active (self)) {
		*action = PARRILLADA_JOB_ACTION_IMAGE;
		return PARRILLADA_BURN_OK;
	}

	task_action = parrillada_task_ctx_get_action (priv->ctx);
	switch (task_action) {
	case PARRILLADA_TASK_ACTION_NONE:
		*action = PARRILLADA_JOB_ACTION_SIZE;
		break;

	case PARRILLADA_TASK_ACTION_ERASE:
		*action = PARRILLADA_JOB_ACTION_ERASE;
		break;

	case PARRILLADA_TASK_ACTION_NORMAL:
		if (priv->type.type == PARRILLADA_TRACK_TYPE_DISC)
			*action = PARRILLADA_JOB_ACTION_RECORD;
		else
			*action = PARRILLADA_JOB_ACTION_IMAGE;
		break;

	case PARRILLADA_TASK_ACTION_CHECKSUM:
		*action = PARRILLADA_JOB_ACTION_CHECKSUM;
		break;

	default:
		*action = PARRILLADA_JOB_ACTION_NONE;
		break;
	}

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_bus_target_lun (ParrilladaJob *self, gchar **BTL)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaDrive *drive;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (BTL != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	drive = parrillada_burn_session_get_burner (session);
	*BTL = parrillada_drive_get_bus_target_lun_string (drive);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_device (ParrilladaJob *self, gchar **device)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaDrive *drive;
	const gchar *path;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (device != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	drive = parrillada_burn_session_get_burner (session);
	path = parrillada_drive_get_device (drive);
	*device = g_strdup (path);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_media (ParrilladaJob *self, ParrilladaMedia *media)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (media != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	*media = parrillada_burn_session_get_dest_media (session);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_medium (ParrilladaJob *job, ParrilladaMedium **medium)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaDrive *drive;

	PARRILLADA_JOB_DEBUG (job);

	g_return_val_if_fail (medium != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (job);
	session = parrillada_task_ctx_get_session (priv->ctx);
	drive = parrillada_burn_session_get_burner (session);
	*medium = parrillada_drive_get_medium (drive);

	if (!(*medium))
		return PARRILLADA_BURN_ERR;

	g_object_ref (*medium);
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_last_session_address (ParrilladaJob *self, goffset *address)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	drive = parrillada_burn_session_get_burner (session);
	medium = parrillada_drive_get_medium (drive);
	if (parrillada_medium_get_last_data_track_address (medium, NULL, address))
		return PARRILLADA_BURN_OK;

	return PARRILLADA_BURN_ERR;
}

ParrilladaBurnResult
parrillada_job_get_next_writable_address (ParrilladaJob *self, goffset *address)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	drive = parrillada_burn_session_get_burner (session);
	medium = parrillada_drive_get_medium (drive);
	*address = parrillada_medium_get_next_writable_address (medium);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_rate (ParrilladaJob *self, guint64 *rate)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	g_return_val_if_fail (rate != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	*rate = parrillada_burn_session_get_rate (session);

	return PARRILLADA_BURN_OK;
}

static int
_round_speed (float number)
{
	int retval = (gint) number;

	/* NOTE: number must always be positive */
	number -= (float) retval;
	if (number >= 0.5)
		return retval + 1;

	return retval;
}

ParrilladaBurnResult
parrillada_job_get_speed (ParrilladaJob *self, guint *speed)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaMedia media;
	guint64 rate;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	rate = parrillada_burn_session_get_rate (session);

	media = parrillada_burn_session_get_dest_media (session);
	if (media & PARRILLADA_MEDIUM_DVD)
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_DVD (rate));
	else if (media & PARRILLADA_MEDIUM_BD)
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_BD (rate));
	else
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_CD (rate));

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_max_rate (ParrilladaJob *self, guint64 *rate)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (rate != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	drive = parrillada_burn_session_get_burner (session);
	medium = parrillada_drive_get_medium (drive);

	if (!medium)
		return PARRILLADA_BURN_NOT_READY;

	*rate = parrillada_medium_get_max_write_speed (medium);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_max_speed (ParrilladaJob *self, guint *speed)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;
	ParrilladaMedium *medium;
	ParrilladaDrive *drive;
	ParrilladaMedia media;
	guint64 rate;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	drive = parrillada_burn_session_get_burner (session);
	medium = parrillada_drive_get_medium (drive);
	if (!medium)
		return PARRILLADA_BURN_NOT_READY;

	rate = parrillada_medium_get_max_write_speed (medium);
	media = parrillada_medium_get_status (medium);
	if (media & PARRILLADA_MEDIUM_DVD)
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_DVD (rate));
	else if (media & PARRILLADA_MEDIUM_BD)
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_BD (rate));
	else
		*speed = _round_speed (PARRILLADA_RATE_TO_SPEED_CD (rate));

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_tmp_file (ParrilladaJob *self,
			  const gchar *suffix,
			  gchar **output,
			  GError **error)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	return parrillada_burn_session_get_tmp_file (session,
						  suffix,
						  output,
						  error);
}

ParrilladaBurnResult
parrillada_job_get_tmp_dir (ParrilladaJob *self,
			 gchar **output,
			 GError **error)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);
	parrillada_burn_session_get_tmp_dir (session,
					  output,
					  error);

	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_audio_title (ParrilladaJob *self, gchar **album)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (album != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	*album = g_strdup (parrillada_burn_session_get_label (session));
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_data_label (ParrilladaJob *self, gchar **label)
{
	ParrilladaBurnSession *session;
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (label != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	*label = g_strdup (parrillada_burn_session_get_label (session));
	return PARRILLADA_BURN_OK;
}

ParrilladaBurnResult
parrillada_job_get_session_output_size (ParrilladaJob *self,
				     goffset *blocks,
				     goffset *bytes)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	return parrillada_task_ctx_get_session_output_size (priv->ctx, blocks, bytes);
}

/**
 * Starts task internal timer 
 */

ParrilladaBurnResult
parrillada_job_start_progress (ParrilladaJob *self,
			    gboolean force)
{
	ParrilladaJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_start_progress (priv->ctx, force);
}

ParrilladaBurnResult
parrillada_job_reset_progress (ParrilladaJob *self)
{
	ParrilladaJobPrivate *priv;

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_ERR;

	return parrillada_task_ctx_reset_progress (priv->ctx);
}

/**
 * these should be used to set the different values of the task by the jobs
 */

ParrilladaBurnResult
parrillada_job_set_progress (ParrilladaJob *self,
			  gdouble progress)
{
	ParrilladaJobPrivate *priv;

	/* Turn this off as it floods bug reports */
	//PARRILLADA_JOB_LOG (self, "Called parrillada_job_set_progress (%lf)", progress);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_ERR;

	if (progress < 0.0 || progress > 1.0) {
		PARRILLADA_JOB_LOG (self, "Tried to set an insane progress value (%lf)", progress);
		return PARRILLADA_BURN_ERR;
	}

	return parrillada_task_ctx_set_progress (priv->ctx, progress);
}

ParrilladaBurnResult
parrillada_job_set_current_action (ParrilladaJob *self,
				ParrilladaBurnAction action,
				const gchar *string,
				gboolean force)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (!parrillada_job_is_last_active (self))
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_set_current_action (priv->ctx,
						    action,
						    string,
						    force);
}

ParrilladaBurnResult
parrillada_job_get_current_action (ParrilladaJob *self,
				ParrilladaBurnAction *action)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, PARRILLADA_BURN_ERR);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!priv->ctx) {
		PARRILLADA_JOB_LOG (self,
				 "called %s whereas it wasn't running",
				 G_STRFUNC);
		return PARRILLADA_BURN_NOT_RUNNING;
	}

	return parrillada_task_ctx_get_current_action (priv->ctx, action);
}

ParrilladaBurnResult
parrillada_job_set_rate (ParrilladaJob *self,
		      gint64 rate)
{
	ParrilladaJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_set_rate (priv->ctx, rate);
}

ParrilladaBurnResult
parrillada_job_set_output_size_for_current_track (ParrilladaJob *self,
					       goffset sectors,
					       goffset bytes)
{
	ParrilladaJobPrivate *priv;

	/* this function can only be used by the last job which is not recording
	 * all other jobs trying to set this value will be ignored.
	 * It should be used mostly during a fake running. This value is stored
	 * by the task context as the amount of bytes/blocks produced by a task.
	 * That's why it's not possible to set the DATA type number of files.
	 * NOTE: the values passed on by this function to context may be added 
	 * to other when there are multiple tracks. */
	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);

	if (!parrillada_job_is_last_active (self))
		return PARRILLADA_BURN_ERR;

	return parrillada_task_ctx_set_output_size_for_current_track (priv->ctx,
								   sectors,
								   bytes);
}

ParrilladaBurnResult
parrillada_job_set_written_track (ParrilladaJob *self,
			       goffset written)
{
	ParrilladaJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_set_written_track (priv->ctx, written);
}

ParrilladaBurnResult
parrillada_job_set_written_session (ParrilladaJob *self,
				 goffset written)
{
	ParrilladaJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_set_written_session (priv->ctx, written);
}

ParrilladaBurnResult
parrillada_job_set_use_average_rate (ParrilladaJob *self, gboolean value)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->next)
		return PARRILLADA_BURN_NOT_RUNNING;

	return parrillada_task_ctx_set_use_average (priv->ctx, value);
}

void
parrillada_job_set_dangerous (ParrilladaJob *self, gboolean value)
{
	ParrilladaJobPrivate *priv;

	PARRILLADA_JOB_DEBUG (self);

	priv = PARRILLADA_JOB_PRIVATE (self);
	if (priv->ctx)
		parrillada_task_ctx_set_dangerous (priv->ctx, value);
}

/**
 * used for debugging
 */

void
parrillada_job_log_message (ParrilladaJob *self,
			 const gchar *location,
			 const gchar *format,
			 ...)
{
	va_list arg_list;
	ParrilladaJobPrivate *priv;
	ParrilladaBurnSession *session;

	g_return_if_fail (PARRILLADA_IS_JOB (self));
	g_return_if_fail (format != NULL);

	priv = PARRILLADA_JOB_PRIVATE (self);
	session = parrillada_task_ctx_get_session (priv->ctx);

	va_start (arg_list, format);
	parrillada_burn_session_logv (session, format, arg_list);
	va_end (arg_list);

	va_start (arg_list, format);
	parrillada_burn_debug_messagev (location, format, arg_list);
	va_end (arg_list);
}

/**
 * Object creation stuff
 */

static void
parrillada_job_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	ParrilladaJobPrivate *priv;
	ParrilladaTrackType *ptr;

	priv = PARRILLADA_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		memcpy (ptr, &priv->type, sizeof (ParrilladaTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_job_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	ParrilladaJobPrivate *priv;
	ParrilladaTrackType *ptr;

	priv = PARRILLADA_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		if (!ptr) {
			priv->type.type = PARRILLADA_TRACK_TYPE_NONE;
			priv->type.subtype.media = PARRILLADA_MEDIUM_NONE;
		}
		else
			memcpy (&priv->type, ptr, sizeof (ParrilladaTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_job_finalize (GObject *object)
{
	ParrilladaJobPrivate *priv;

	priv = PARRILLADA_JOB_PRIVATE (object);

	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->previous) {
		g_object_unref (priv->previous);
		priv->previous = NULL;
	}

	if (priv->input) {
		parrillada_job_input_free (priv->input);
		priv->input = NULL;
	}

	if (priv->linked)
		priv->linked = NULL;

	if (priv->output) {
		parrillada_job_output_free (priv->output);
		priv->output = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_job_class_init (ParrilladaJobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaJobPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_job_finalize;
	object_class->set_property = parrillada_job_set_property;
	object_class->get_property = parrillada_job_get_property;

	parrillada_job_signals [ERROR_SIGNAL] =
	    g_signal_new ("error",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (ParrilladaJobClass, error),
			  NULL, NULL,
			  parrillada_marshal_INT__INT,
			  G_TYPE_INT, 1, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_OUTPUT,
					 g_param_spec_pointer ("output",
							       "The type the job must output",
							       "The type the job must output",
							       G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
parrillada_job_init (ParrilladaJob *obj)
{ }

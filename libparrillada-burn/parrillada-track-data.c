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

#include <sys/param.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "parrillada-track-data.h"
#include "burn-mkisofs-base.h"
#include "burn-debug.h"

typedef struct _ParrilladaTrackDataPrivate ParrilladaTrackDataPrivate;
struct _ParrilladaTrackDataPrivate
{
	ParrilladaImageFS fs_type;
	GSList *grafts;
	GSList *excluded;

	guint file_num;
	guint64 data_blocks;
};

#define PARRILLADA_TRACK_DATA_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRACK_DATA, ParrilladaTrackDataPrivate))

G_DEFINE_TYPE (ParrilladaTrackData, parrillada_track_data, PARRILLADA_TYPE_TRACK);

/**
 * parrillada_graft_point_free:
 * @graft: a #ParrilladaGraftPt
 *
 * Frees @graft. Do not use @grafts afterwards.
 *
 **/

void
parrillada_graft_point_free (ParrilladaGraftPt *graft)
{
	if (graft->uri)
		g_free (graft->uri);

	g_free (graft->path);
	g_free (graft);
}

/**
 * parrillada_graft_point_copy:
 * @graft: a #ParrilladaGraftPt
 *
 * Copies @graft.
 *
 * Return value: a #ParrilladaGraftPt.
 **/

ParrilladaGraftPt *
parrillada_graft_point_copy (ParrilladaGraftPt *graft)
{
	ParrilladaGraftPt *newgraft;

	g_return_val_if_fail (graft != NULL, NULL);

	newgraft = g_new0 (ParrilladaGraftPt, 1);
	newgraft->path = g_strdup (graft->path);
	if (graft->uri)
		newgraft->uri = g_strdup (graft->uri);

	return newgraft;
}

static ParrilladaBurnResult
parrillada_track_data_set_source_real (ParrilladaTrackData *track,
				    GSList *grafts,
				    GSList *unreadable)
{
	ParrilladaTrackDataPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);

	if (priv->grafts) {
		g_slist_foreach (priv->grafts, (GFunc) parrillada_graft_point_free, NULL);
		g_slist_free (priv->grafts);
	}

	if (priv->excluded) {
		g_slist_foreach (priv->excluded, (GFunc) g_free, NULL);
		g_slist_free (priv->excluded);
	}

	priv->grafts = grafts;
	priv->excluded = unreadable;
	parrillada_track_changed (PARRILLADA_TRACK (track));

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_set_source:
 * @track: a #ParrilladaTrackData.
 * @grafts: (element-type ParrilladaBurn.GraftPt) (in) (transfer full): a #GSList of #ParrilladaGraftPt.
 * @unreadable: (element-type utf8) (allow-none) (in) (transfer full): a #GSList of URIS as strings or %NULL.
 *
 * Sets the lists of grafts points (@grafts) and excluded
 * URIs (@unreadable) to be used to create an image.
 *
 * Be careful @track takes ownership of @grafts and
 * @unreadable which must not be freed afterwards.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_set_source (ParrilladaTrackData *track,
			       GSList *grafts,
			       GSList *unreadable)
{
	ParrilladaTrackDataClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_ERR);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	return klass->set_source (track, grafts, unreadable);
}

static ParrilladaBurnResult
parrillada_track_data_add_fs_real (ParrilladaTrackData *track,
				ParrilladaImageFS fstype)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	priv->fs_type |= fstype;
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_add_fs:
 * @track: a #ParrilladaTrackData
 * @fstype: a #ParrilladaImageFS
 *
 * Adds one or more parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_add_fs (ParrilladaTrackData *track,
			   ParrilladaImageFS fstype)
{
	ParrilladaTrackDataClass *klass;
	ParrilladaImageFS fs_before;
	ParrilladaBurnResult result;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	fs_before = parrillada_track_data_get_fs (track);
	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	if (!klass->add_fs)
		return PARRILLADA_BURN_NOT_SUPPORTED;

	result = klass->add_fs (track, fstype);
	if (result != PARRILLADA_BURN_OK)
		return result;

	if (fs_before != parrillada_track_data_get_fs (track))
		parrillada_track_changed (PARRILLADA_TRACK (track));

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_rm_fs_real (ParrilladaTrackData *track,
			       ParrilladaImageFS fstype)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	priv->fs_type &= ~(fstype);
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_rm_fs:
 * @track: a #ParrilladaTrackData
 * @fstype: a #ParrilladaImageFS
 *
 * Removes one or more parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_rm_fs (ParrilladaTrackData *track,
			  ParrilladaImageFS fstype)
{
	ParrilladaTrackDataClass *klass;
	ParrilladaImageFS fs_before;
	ParrilladaBurnResult result;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	fs_before = parrillada_track_data_get_fs (track);
	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	if (!klass->rm_fs);
		return PARRILLADA_BURN_NOT_SUPPORTED;

	result = klass->rm_fs (track, fstype);
	if (result != PARRILLADA_BURN_OK)
		return result;

	if (fs_before != parrillada_track_data_get_fs (track))
		parrillada_track_changed (PARRILLADA_TRACK (track));

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_set_data_blocks:
 * @track: a #ParrilladaTrackData
 * @blocks: a #goffset
 *
 * Sets the size of the image to be created (in sectors of 2048 bytes).
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_set_data_blocks (ParrilladaTrackData *track,
				    goffset blocks)
{
	ParrilladaTrackDataPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	priv->data_blocks = blocks;

	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_set_file_num:
 * @track: a #ParrilladaTrackData
 * @number: a #guint64
 *
 * Sets the number of files (not directories) in @track.
 *
 * Return value: a #ParrilladaBurnResult.
 * PARRILLADA_BURN_OK if it was successful,
 * PARRILLADA_BURN_ERR otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_set_file_num (ParrilladaTrackData *track,
				 guint64 number)
{
	ParrilladaTrackDataPrivate *priv;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);

	priv->file_num = number;
	return PARRILLADA_BURN_OK;
}

/**
 * parrillada_track_data_get_fs:
 * @track: a #ParrilladaTrackData
 *
 * Returns the parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #ParrilladaImageFS.
 **/

ParrilladaImageFS
parrillada_track_data_get_fs (ParrilladaTrackData *track)
{
	ParrilladaTrackDataClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_IMAGE_FS_NONE);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	return klass->get_fs (track);
}

static ParrilladaImageFS
parrillada_track_data_get_fs_real (ParrilladaTrackData *track)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	return priv->fs_type;
}

static GHashTable *
parrillada_track_data_mangle_joliet_name (GHashTable *joliet,
				       const gchar *path,
				       gchar *buffer)
{
	gboolean has_slash = FALSE;
	gint dot_pos = -1;
	gint dot_len = -1;
	gchar *name;
	gint width;
	gint start;
	gint num;
	gint end;

	/* NOTE: this wouldn't work on windows (not a big deal) */
	end = strlen (path);
	if (!end) {
		buffer [0] = '\0';
		return joliet;
	}

	memcpy (buffer, path, MIN (end, MAXPATHLEN));
	buffer [MIN (end, MAXPATHLEN)] = '\0';

	/* move back until we find a character different from G_DIR_SEPARATOR */
	end --;
	while (end >= 0 && G_IS_DIR_SEPARATOR (path [end])) {
		end --;
		has_slash = TRUE;
	}

	/* There are only slashes */
	if (end == -1)
		return joliet;

	start = end - 1;
	while (start >= 0 && !G_IS_DIR_SEPARATOR (path [start])) {
		/* Find the extension while at it */
		if (dot_pos <= 0 && path [start] == '.')
			dot_pos = start;

		start --;
	}

	if (end - start <= 64)
		return joliet;

	name = buffer + start + 1;
	if (dot_pos > 0)
		dot_len = end - dot_pos + 1;

	if (dot_len > 1 && dot_len < 5)
		memcpy (name + 64 - dot_len,
			path + dot_pos,
			dot_len);

	name [64] = '\0';

	if (!joliet) {
		joliet = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						NULL);

		g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (1));
		if (has_slash)
			strcat (buffer, G_DIR_SEPARATOR_S);

		PARRILLADA_BURN_LOG ("Mangled name to %s (truncated)", buffer);
		return joliet;
	}

	/* see if this path was already used */
	num = GPOINTER_TO_INT (g_hash_table_lookup (joliet, buffer));
	if (!num) {
		g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (1));

		if (has_slash)
			strcat (buffer, G_DIR_SEPARATOR_S);

		PARRILLADA_BURN_LOG ("Mangled name to %s (truncated)", buffer);
		return joliet;
	}

	/* NOTE: g_hash_table_insert frees key_path */
	num ++;
	g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (num));

	width = 1;
	while (num / (width * 10)) width ++;

	/* try to keep the extension */
	if (dot_len < 5 && dot_len > 1 )
		sprintf (name + (64 - width - dot_len),
			 "%i%s",
			 num,
			 path + dot_pos);
	else
		sprintf (name + (64 - width),
			 "%i",
			 num);

	if (has_slash)
		strcat (buffer, G_DIR_SEPARATOR_S);

	PARRILLADA_BURN_LOG ("Mangled name to %s", buffer);
	return joliet;
}

/**
 * parrillada_track_data_get_grafts:
 * @track: a #ParrilladaTrackData
 *
 * Returns a list of #ParrilladaGraftPt.
 *
 * Do not free after usage as @track retains ownership.
 *
 * Return value: (transfer none) (element-type ParrilladaBurn.GraftPt) (allow-none): a #GSList of #ParrilladaGraftPt or %NULL if empty.
 **/

GSList *
parrillada_track_data_get_grafts (ParrilladaTrackData *track)
{
	ParrilladaTrackDataClass *klass;
	GHashTable *mangle = NULL;
	ParrilladaImageFS image_fs;
	GSList *grafts;
	GSList *iter;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), NULL);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	grafts = klass->get_grafts (track);

	image_fs = parrillada_track_data_get_fs (track);
	if ((image_fs & PARRILLADA_IMAGE_FS_JOLIET) == 0)
		return grafts;

	for (iter = grafts; iter; iter = iter->next) {
		ParrilladaGraftPt *graft;
		gchar newpath [MAXPATHLEN];

		graft = iter->data;
		mangle = parrillada_track_data_mangle_joliet_name (mangle,
								graft->path,
								newpath);

		g_free (graft->path);
		graft->path = g_strdup (newpath);
	}

	if (mangle)
		g_hash_table_destroy (mangle);

	return grafts;
}

static GSList *
parrillada_track_data_get_grafts_real (ParrilladaTrackData *track)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	return priv->grafts;
}

/**
 * parrillada_track_data_get_excluded_list:
 * @track: a #ParrilladaTrackData.
 *
 * Returns a list of URIs which must not be included in
 * the image to be created.
 * Do not free the list or any of the URIs after
 * usage as @track retains ownership.
 *
 * Return value: (transfer none) (element-type utf8) (allow-none): a #GSList of #gchar * or %NULL if no
 * URI should be excluded.
 **/

GSList *
parrillada_track_data_get_excluded_list (ParrilladaTrackData *track)
{
	ParrilladaTrackDataClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), NULL);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	return klass->get_excluded (track);
}

static GSList *
parrillada_track_data_get_excluded_real (ParrilladaTrackData *track)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	return priv->excluded;
}

/**
 * parrillada_track_data_write_to_paths:
 * @track: a #ParrilladaTrackData.
 * @grafts_path: a #gchar.
 * @excluded_path: a #gchar.
 * @emptydir: a #gchar.
 * @videodir: (allow-none): a #gchar or %NULL.
 * @error: a #GError.
 *
 * Write to @grafts_path (a path to a file) the graft points,
 * and to @excluded_path (a path to a file) the list of paths to
 * be excluded; @emptydir is (path) is an empty
 * directory to be used for created directories;
 * @videodir (a path) is a directory to be used to build the
 * the video image.
 *
 * This is mostly for internal use by mkisofs and similar.
 *
 * This function takes care of file name mangling.
 *
 * Return value: a #ParrilladaBurnResult.
 **/

ParrilladaBurnResult
parrillada_track_data_write_to_paths (ParrilladaTrackData *track,
                                   const gchar *grafts_path,
                                   const gchar *excluded_path,
                                   const gchar *emptydir,
                                   const gchar *videodir,
                                   GError **error)
{
	GSList *grafts;
	GSList *excluded;
	ParrilladaBurnResult result;
	ParrilladaTrackDataClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), PARRILLADA_BURN_NOT_SUPPORTED);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	grafts = klass->get_grafts (track);
	excluded = klass->get_excluded (track);

	result = parrillada_mkisofs_base_write_to_files (grafts,
						      excluded,
						      parrillada_track_data_get_fs (track),
						      emptydir,
						      videodir,
						      grafts_path,
						      excluded_path,
						      error);
	return result;
}

/**
 * parrillada_track_data_get_file_num:
 * @track: a #ParrilladaTrackData.
 * @file_num: (allow-none) (out): a #guint64 or %NULL.
 *
 * Sets the number of files (not directories) in @file_num.
 *
 * Return value: a #ParrilladaBurnResult. %TRUE if @file_num
 * was set, %FALSE otherwise.
 **/

ParrilladaBurnResult
parrillada_track_data_get_file_num (ParrilladaTrackData *track,
				 guint64 *file_num)
{
	ParrilladaTrackDataClass *klass;

	g_return_val_if_fail (PARRILLADA_IS_TRACK_DATA (track), 0);

	klass = PARRILLADA_TRACK_DATA_GET_CLASS (track);
	if (file_num)
		*file_num = klass->get_file_num (track);

	return PARRILLADA_BURN_OK;
}

static guint64
parrillada_track_data_get_file_num_real (ParrilladaTrackData *track)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);
	return priv->file_num;
}

static ParrilladaBurnResult
parrillada_track_data_get_size (ParrilladaTrack *track,
			     goffset *blocks,
			     goffset *block_size)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);

	if (*block_size)
		*block_size = 2048;

	if (*blocks)
		*blocks = priv->data_blocks;

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_get_track_type (ParrilladaTrack *track,
				   ParrilladaTrackType *type)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);

	parrillada_track_type_set_has_data (type);
	parrillada_track_type_set_data_fs (type, priv->fs_type);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_track_data_get_status (ParrilladaTrack *track,
			       ParrilladaStatus *status)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (track);

	if (!priv->grafts) {
		if (status)
			parrillada_status_set_error (status,
						  g_error_new (PARRILLADA_BURN_ERROR,
							       PARRILLADA_BURN_ERROR_EMPTY,
							       _("There are no files to write to disc")));
		return PARRILLADA_BURN_ERR;
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_track_data_init (ParrilladaTrackData *object)
{ }

static void
parrillada_track_data_finalize (GObject *object)
{
	ParrilladaTrackDataPrivate *priv;

	priv = PARRILLADA_TRACK_DATA_PRIVATE (object);
	if (priv->grafts) {
		g_slist_foreach (priv->grafts, (GFunc) parrillada_graft_point_free, NULL);
		g_slist_free (priv->grafts);
		priv->grafts = NULL;
	}

	if (priv->excluded) {
		g_slist_foreach (priv->excluded, (GFunc) g_free, NULL);
		g_slist_free (priv->excluded);
		priv->excluded = NULL;
	}

	G_OBJECT_CLASS (parrillada_track_data_parent_class)->finalize (object);
}

static void
parrillada_track_data_class_init (ParrilladaTrackDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaTrackClass *track_class = PARRILLADA_TRACK_CLASS (klass);
	ParrilladaTrackDataClass *track_data_class = PARRILLADA_TRACK_DATA_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTrackDataPrivate));

	object_class->finalize = parrillada_track_data_finalize;

	track_class->get_type = parrillada_track_data_get_track_type;
	track_class->get_status = parrillada_track_data_get_status;
	track_class->get_size = parrillada_track_data_get_size;

	track_data_class->set_source = parrillada_track_data_set_source_real;
	track_data_class->add_fs = parrillada_track_data_add_fs_real;
	track_data_class->rm_fs = parrillada_track_data_rm_fs_real;

	track_data_class->get_fs = parrillada_track_data_get_fs_real;
	track_data_class->get_grafts = parrillada_track_data_get_grafts_real;
	track_data_class->get_excluded = parrillada_track_data_get_excluded_real;
	track_data_class->get_file_num = parrillada_track_data_get_file_num_real;
}

/**
 * parrillada_track_data_new:
 *
 * Creates a new #ParrilladaTrackData.
 * 
 *This type of tracks is used to create a disc image
 * from or burn a selection of files.
 *
 * Return value: a #ParrilladaTrackData
 **/

ParrilladaTrackData *
parrillada_track_data_new (void)
{
	return g_object_new (PARRILLADA_TYPE_TRACK_DATA, NULL);
}

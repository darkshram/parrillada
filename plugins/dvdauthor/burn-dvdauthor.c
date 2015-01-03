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
#include <glib/gstdio.h>
#include <gmodule.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>

#include "parrillada-plugin-registration.h"
#include "burn-job.h"
#include "burn-process.h"
#include "parrillada-track-data.h"
#include "parrillada-track-stream.h"


#define PARRILLADA_TYPE_DVD_AUTHOR             (parrillada_dvd_author_get_type ())
#define PARRILLADA_DVD_AUTHOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_DVD_AUTHOR, ParrilladaDvdAuthor))
#define PARRILLADA_DVD_AUTHOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_DVD_AUTHOR, ParrilladaDvdAuthorClass))
#define PARRILLADA_IS_DVD_AUTHOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_DVD_AUTHOR))
#define PARRILLADA_IS_DVD_AUTHOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_DVD_AUTHOR))
#define PARRILLADA_DVD_AUTHOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_DVD_AUTHOR, ParrilladaDvdAuthorClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaDvdAuthor, parrillada_dvd_author, PARRILLADA_TYPE_PROCESS, ParrilladaProcess);

typedef struct _ParrilladaDvdAuthorPrivate ParrilladaDvdAuthorPrivate;
struct _ParrilladaDvdAuthorPrivate
{
	gchar *output;
};

#define PARRILLADA_DVD_AUTHOR_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_DVD_AUTHOR, ParrilladaDvdAuthorPrivate))

static ParrilladaProcessClass *parent_class = NULL;

static ParrilladaBurnResult
parrillada_dvd_author_add_track (ParrilladaJob *job)
{
	gchar *path;
	GSList *grafts = NULL;
	ParrilladaGraftPt *graft;
	ParrilladaTrackData *track;
	ParrilladaDvdAuthorPrivate *priv;

	priv = PARRILLADA_DVD_AUTHOR_PRIVATE (job);

	/* create the track */
	track = parrillada_track_data_new ();

	/* audio */
	graft = g_new (ParrilladaGraftPt, 1);
	path = g_build_path (G_DIR_SEPARATOR_S,
			     priv->output,
			     "AUDIO_TS",
			     NULL);
	graft->uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	graft->path = g_strdup ("/AUDIO_TS");
	grafts = g_slist_prepend (grafts, graft);

	PARRILLADA_JOB_LOG (job, "Adding graft point for %s", graft->uri);

	/* video */
	graft = g_new (ParrilladaGraftPt, 1);
	path = g_build_path (G_DIR_SEPARATOR_S,
			     priv->output,
			     "VIDEO_TS",
			     NULL);
	graft->uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	graft->path = g_strdup ("/VIDEO_TS");
	grafts = g_slist_prepend (grafts, graft);

	PARRILLADA_JOB_LOG (job, "Adding graft point for %s", graft->uri);

	parrillada_track_data_add_fs (track,
				   PARRILLADA_IMAGE_FS_ISO|
				   PARRILLADA_IMAGE_FS_UDF|
				   PARRILLADA_IMAGE_FS_VIDEO);
	parrillada_track_data_set_source (track,
				       grafts,
				       NULL);
	parrillada_job_add_track (job, PARRILLADA_TRACK (track));
	g_object_unref (track);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_dvd_author_read_stdout (ParrilladaProcess *process,
				const gchar *line)
{
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_dvd_author_read_stderr (ParrilladaProcess *process,
				const gchar *line)
{
	gint percent = 0;

	if (sscanf (line, "STAT: fixing VOBU at %*s (%*d/%*d, %d%%)", &percent) == 1) {
		parrillada_job_start_progress (PARRILLADA_JOB (process), FALSE);
		parrillada_job_set_progress (PARRILLADA_JOB (process),
					  (gdouble) ((gdouble) percent) / 100.0);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_dvd_author_generate_xml_file (ParrilladaProcess *process,
				      const gchar *path,
				      GError **error)
{
	ParrilladaDvdAuthorPrivate *priv;
	ParrilladaBurnResult result;
	GSList *tracks = NULL;
	xmlTextWriter *xml;
	gint success;
	GSList *iter;

	PARRILLADA_JOB_LOG (process, "Creating DVD layout xml file(%s)", path);

	xml = xmlNewTextWriterFilename (path, 0);
	if (!xml)
		return PARRILLADA_BURN_ERR;

	priv = PARRILLADA_DVD_AUTHOR_PRIVATE (process);

	xmlTextWriterSetIndent (xml, 1);
	xmlTextWriterSetIndentString (xml, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (xml,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	result = parrillada_job_get_tmp_dir (PARRILLADA_JOB (process),
					  &priv->output,
					  error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	/* let's start */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "dvdauthor");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteAttribute (xml,
					       (xmlChar *) "dest",
					       (xmlChar *) priv->output);
	if (success < 0)
		goto error;

	/* This is needed to finalize */
	success = xmlTextWriterWriteElement (xml, (xmlChar *) "vmgm", (xmlChar *) "");
	if (success < 0)
		goto error;

	/* the tracks */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "titleset");
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (xml, (xmlChar *) "titles");
	if (success < 0)
		goto error;

	/* get all tracks */
	parrillada_job_get_tracks (PARRILLADA_JOB (process), &tracks);
	for (iter = tracks; iter; iter = iter->next) {
		ParrilladaTrack *track;
		gchar *video;

		track = iter->data;
		success = xmlTextWriterStartElement (xml, (xmlChar *) "pgc");
		if (success < 0)
			goto error;

		success = xmlTextWriterStartElement (xml, (xmlChar *) "vob");
		if (success < 0)
			goto error;

		video = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), FALSE);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "file",
						       (xmlChar *) video);
		g_free (video);

		if (success < 0)
			goto error;

		/* vob */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;

		/* pgc */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* titles */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* titleset */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* close dvdauthor */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (xml);
	xmlFreeTextWriter (xml);

	return PARRILLADA_BURN_OK;

error:

	PARRILLADA_JOB_LOG (process, "Error");

	/* close everything */
	xmlTextWriterEndDocument (xml);
	xmlFreeTextWriter (xml);

	/* FIXME: get the error */

	return PARRILLADA_BURN_ERR;
}

static ParrilladaBurnResult
parrillada_dvd_author_set_argv (ParrilladaProcess *process,
			     GPtrArray *argv,
			     GError **error)
{
	ParrilladaDvdAuthorPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaJobAction action;
	gchar *output;

	priv = PARRILLADA_DVD_AUTHOR_PRIVATE (process);

	parrillada_job_get_action (PARRILLADA_JOB (process), &action);
	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		PARRILLADA_JOB_NOT_SUPPORTED (process);

	g_ptr_array_add (argv, g_strdup ("dvdauthor"));
	
	/* get all arguments to write XML file */
	result = parrillada_job_get_tmp_file (PARRILLADA_JOB (process),
					   NULL,
					   &output,
					   error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	g_ptr_array_add (argv, g_strdup ("-x"));
	g_ptr_array_add (argv, output);

	result = parrillada_dvd_author_generate_xml_file (process, output, error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	parrillada_job_set_current_action (PARRILLADA_JOB (process),
					PARRILLADA_BURN_ACTION_CREATING_IMAGE,
					_("Creating file layout"),
					FALSE);
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_dvd_author_post (ParrilladaJob *job)
{
	ParrilladaDvdAuthorPrivate *priv;

	priv = PARRILLADA_DVD_AUTHOR_PRIVATE (job);

	parrillada_dvd_author_add_track (job);

	if (priv->output) {
		g_free (priv->output);
		priv->output = NULL;
	}

	return parrillada_job_finished_session (job);
}

static void
parrillada_dvd_author_init (ParrilladaDvdAuthor *object)
{}

static void
parrillada_dvd_author_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_dvd_author_class_init (ParrilladaDvdAuthorClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaProcessClass* process_class = PARRILLADA_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaDvdAuthorPrivate));

	object_class->finalize = parrillada_dvd_author_finalize;

	process_class->stdout_func = parrillada_dvd_author_read_stdout;
	process_class->stderr_func = parrillada_dvd_author_read_stderr;
	process_class->set_argv = parrillada_dvd_author_set_argv;
	process_class->post = parrillada_dvd_author_post;
}

static void
parrillada_dvd_author_export_caps (ParrilladaPlugin *plugin)
{
	GSList *output;
	GSList *input;

	/* NOTE: it seems that cdrecord can burn cue files on the fly */
	parrillada_plugin_define (plugin,
			       "dvdauthor",
	                       NULL,
			       _("Creates disc images suitable for video DVDs"),
			       "Philippe Rouquier",
			       1);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_AC3|
					PARRILLADA_AUDIO_FORMAT_MP2|
					PARRILLADA_AUDIO_FORMAT_RAW|
					PARRILLADA_METADATA_INFO|
					PARRILLADA_VIDEO_FORMAT_VIDEO_DVD);

	output = parrillada_caps_data_new (PARRILLADA_IMAGE_FS_ISO|
					PARRILLADA_IMAGE_FS_UDF|
					PARRILLADA_IMAGE_FS_VIDEO);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_AC3|
					PARRILLADA_AUDIO_FORMAT_MP2|
					PARRILLADA_AUDIO_FORMAT_RAW|
					PARRILLADA_VIDEO_FORMAT_VIDEO_DVD);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* we only support DVDs */
	parrillada_plugin_set_flags (plugin,
  				  PARRILLADA_MEDIUM_FILE|
				  PARRILLADA_MEDIUM_DVDR|
				  PARRILLADA_MEDIUM_DVDR_PLUS|
				  PARRILLADA_MEDIUM_DUAL_L|
				  PARRILLADA_MEDIUM_BLANK|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_HAS_DATA,
				  PARRILLADA_BURN_FLAG_NONE,
				  PARRILLADA_BURN_FLAG_NONE);

	parrillada_plugin_set_flags (plugin,
				  PARRILLADA_MEDIUM_DVDRW|
				  PARRILLADA_MEDIUM_DVDRW_PLUS|
				  PARRILLADA_MEDIUM_DVDRW_RESTRICTED|
				  PARRILLADA_MEDIUM_DUAL_L|
				  PARRILLADA_MEDIUM_BLANK|
				  PARRILLADA_MEDIUM_CLOSED|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_HAS_DATA,
				  PARRILLADA_BURN_FLAG_NONE,
				  PARRILLADA_BURN_FLAG_NONE);
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	gint version [3] = { 0, 6, 0};
	parrillada_plugin_test_app (plugin,
	                         "dvdauthor",
	                         "-h",
	                         "DVDAuthor::dvdauthor, version %d.%d.%d.",
	                         version);
}

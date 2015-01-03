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

#include "parrillada-tags.h"
#include "parrillada-plugin-registration.h"
#include "burn-job.h"
#include "burn-process.h"
#include "parrillada-track-stream.h"


#define PARRILLADA_TYPE_VCD_IMAGER             (parrillada_vcd_imager_get_type ())
#define PARRILLADA_VCD_IMAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_VCD_IMAGER, ParrilladaVcdImager))
#define PARRILLADA_VCD_IMAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_VCD_IMAGER, ParrilladaVcdImagerClass))
#define PARRILLADA_IS_VCD_IMAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_VCD_IMAGER))
#define PARRILLADA_IS_VCD_IMAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_VCD_IMAGER))
#define PARRILLADA_VCD_IMAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_VCD_IMAGER, ParrilladaVcdImagerClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaVcdImager, parrillada_vcd_imager, PARRILLADA_TYPE_PROCESS, ParrilladaProcess);

typedef struct _ParrilladaVcdImagerPrivate ParrilladaVcdImagerPrivate;
struct _ParrilladaVcdImagerPrivate
{
	guint num_tracks;

	guint svcd:1;
};

#define PARRILLADA_VCD_IMAGER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_VCD_IMAGER, ParrilladaVcdImagerPrivate))

static ParrilladaProcessClass *parent_class = NULL;

static ParrilladaBurnResult
parrillada_vcd_imager_read_stdout (ParrilladaProcess *process,
				const gchar *line)
{
	gint percent = 0;
	guint track_num = 0;
	ParrilladaVcdImagerPrivate *priv;

	priv = PARRILLADA_VCD_IMAGER_PRIVATE (process);

	if (sscanf (line, "#scan[track-%d]: %*d/%*d (%d)", &track_num, &percent) == 2) {
		parrillada_job_start_progress (PARRILLADA_JOB (process), FALSE);
		parrillada_job_set_progress (PARRILLADA_JOB (process),
					  (gdouble) ((gdouble) percent) /
					  100.0 /
					  (gdouble) (priv->num_tracks + 1) +
					  (gdouble) (track_num) /
					  (gdouble) (priv->num_tracks + 1));
	}
	else if (sscanf (line, "#write[%*d/%*d]: %*d/%*d (%d)", &percent) == 1) {
		gdouble progress;

		/* NOTE: percent can be over 100% ???? */
		parrillada_job_start_progress (PARRILLADA_JOB (process), FALSE);
		progress = (gdouble) ((gdouble) percent) /
			   100.0 /
			   (gdouble) (priv->num_tracks + 1) +
			   (gdouble) (priv->num_tracks) /
			   (gdouble) (priv->num_tracks + 1);

		if (progress > 1.0)
			progress = 1.0;

		parrillada_job_set_progress (PARRILLADA_JOB (process), progress);
	}

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_vcd_imager_read_stderr (ParrilladaProcess *process,
				const gchar *line)
{
	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_vcd_imager_generate_xml_file (ParrilladaProcess *process,
				      const gchar *path,
				      GError **error)
{
	ParrilladaVcdImagerPrivate *priv;
	GSList *tracks = NULL;
	xmlTextWriter *xml;
	gchar buffer [64];
	gint success;
	GSList *iter;
	gchar *name;
	gint i;

	PARRILLADA_JOB_LOG (process, "Creating (S)VCD layout xml file (%s)", path);

	xml = xmlNewTextWriterFilename (path, 0);
	if (!xml)
		return PARRILLADA_BURN_ERR;

	priv = PARRILLADA_VCD_IMAGER_PRIVATE (process);

	xmlTextWriterSetIndent (xml, 1);
	xmlTextWriterSetIndentString (xml, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (xml,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteDTD (xml,
					(xmlChar *) "videocd",
					(xmlChar *) "-//GNU//DTD VideoCD//EN",
					(xmlChar *) "http://www.gnu.org/software/vcdimager/videocd.dtd",
					(xmlChar *) NULL);
	if (success < 0)
		goto error;

	/* let's start */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "videocd");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteAttribute (xml,
					       (xmlChar *) "xmlns",
					       (xmlChar *) "http://www.gnu.org/software/vcdimager/1.0/");
	if (success < 0)
		goto error;

	if (priv->svcd)
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "class",
						       (xmlChar *) "svcd");
	else
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "class",
						       (xmlChar *) "vcd");

	if (success < 0)
		goto error;

	if (priv->svcd)
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "version",
						       (xmlChar *) "1.0");
	else
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "version",
						       (xmlChar *) "2.0");
	if (success < 0)
		goto error;

	/* info part */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "info");
	if (success < 0)
		goto error;

	/* name of the volume */
	name = NULL;
	parrillada_job_get_audio_title (PARRILLADA_JOB (process), &name);
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "album-id",
					     (xmlChar *) name);
	g_free (name);
	if (success < 0)
		goto error;

	/* number of CDs */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-count",
					     (xmlChar *) "1");
	if (success < 0)
		goto error;

	/* CD number */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-number",
					     (xmlChar *) "1");
	if (success < 0)
		goto error;

	/* close info part */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* Primary Volume descriptor */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "pvd");
	if (success < 0)
		goto error;

	/* NOTE: no need to convert a possible non compliant name as this will 
	 * be done by vcdimager. */
	name = NULL;
	parrillada_job_get_audio_title (PARRILLADA_JOB (process), &name);
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-id",
					     (xmlChar *) name);
	g_free (name);
	if (success < 0)
		goto error;

	/* Makes it CD-i compatible */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "system-id",
					     (xmlChar *) "CD-RTOS CD-BRIDGE");
	if (success < 0)
		goto error;

	/* Close Primary Volume descriptor */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* the tracks */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "sequence-items");
	if (success < 0)
		goto error;

	/* get all tracks */
	parrillada_job_get_tracks (PARRILLADA_JOB (process), &tracks);
	priv->num_tracks = g_slist_length (tracks);
	for (i = 0, iter = tracks; iter; iter = iter->next, i++) {
		ParrilladaTrack *track;
		gchar *video;

		track = iter->data;
		success = xmlTextWriterStartElement (xml, (xmlChar *) "sequence-item");
		if (success < 0)
			goto error;

		video = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), FALSE);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "src",
						       (xmlChar *) video);
		g_free (video);

		if (success < 0)
			goto error;

		sprintf (buffer, "track-%i", i);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "id",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		/* close sequence-item */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* sequence-items */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* the navigation */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "pbc");
	if (success < 0)
		goto error;

	/* get all tracks */
	parrillada_job_get_tracks (PARRILLADA_JOB (process), &tracks);
	for (i = 0, iter = tracks; iter; iter = iter->next, i++) {
		ParrilladaTrack *track;

		track = iter->data;

		sprintf (buffer, "playlist-%i", i);
		success = xmlTextWriterStartElement (xml, (xmlChar *) "playlist");
		if (success < 0)
			goto error;

		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "id",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		success = xmlTextWriterWriteElement (xml,
						     (xmlChar *) "wait",
						     (xmlChar *) "0");
		if (success < 0)
			goto error;

		success = xmlTextWriterStartElement (xml, (xmlChar *) "play-item");
		if (success < 0)
			goto error;

		sprintf (buffer, "track-%i", i);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "ref",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		/* play-item */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;

		/* playlist */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* pbc */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* close videocd */
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
parrillada_vcd_imager_set_argv (ParrilladaProcess *process,
			     GPtrArray *argv,
			     GError **error)
{
	ParrilladaVcdImagerPrivate *priv;
	ParrilladaBurnResult result;
	ParrilladaJobAction action;
	ParrilladaMedia medium;
	gchar *output;
	gchar *image;
	gchar *toc;

	priv = PARRILLADA_VCD_IMAGER_PRIVATE (process);

	parrillada_job_get_action (PARRILLADA_JOB (process), &action);
	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		PARRILLADA_JOB_NOT_SUPPORTED (process);

	g_ptr_array_add (argv, g_strdup ("vcdxbuild"));

	g_ptr_array_add (argv, g_strdup ("--progress"));
	g_ptr_array_add (argv, g_strdup ("-v"));

	/* specifies output */
	image = toc = NULL;
	parrillada_job_get_image_output (PARRILLADA_JOB (process),
				      &image,
				      &toc);

	g_ptr_array_add (argv, g_strdup ("-c"));
	g_ptr_array_add (argv, toc);
	g_ptr_array_add (argv, g_strdup ("-b"));
	g_ptr_array_add (argv, image);

	/* get temporary file to write XML */
	result = parrillada_job_get_tmp_file (PARRILLADA_JOB (process),
					   NULL,
					   &output,
					   error);
	if (result != PARRILLADA_BURN_OK)
		return result;

	g_ptr_array_add (argv, output);

	parrillada_job_get_media (PARRILLADA_JOB (process), &medium);
	if (medium & PARRILLADA_MEDIUM_CD) {
		GValue *value = NULL;

		parrillada_job_tag_lookup (PARRILLADA_JOB (process),
					PARRILLADA_VCD_TYPE,
					&value);
		if (value)
			priv->svcd = (g_value_get_int (value) == PARRILLADA_SVCD);
	}

	result = parrillada_vcd_imager_generate_xml_file (process, output, error);
	if (result != PARRILLADA_BURN_OK)
		return result;
	
	parrillada_job_set_current_action (PARRILLADA_JOB (process),
					PARRILLADA_BURN_ACTION_CREATING_IMAGE,
					_("Creating file layout"),
					FALSE);
	return PARRILLADA_BURN_OK;
}

static void
parrillada_vcd_imager_init (ParrilladaVcdImager *object)
{}

static void
parrillada_vcd_imager_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_vcd_imager_class_init (ParrilladaVcdImagerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaProcessClass* process_class = PARRILLADA_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaVcdImagerPrivate));

	object_class->finalize = parrillada_vcd_imager_finalize;
	process_class->stdout_func = parrillada_vcd_imager_read_stdout;
	process_class->stderr_func = parrillada_vcd_imager_read_stderr;
	process_class->set_argv = parrillada_vcd_imager_set_argv;
	process_class->post = parrillada_job_finished_session;
}

static void
parrillada_vcd_imager_export_caps (ParrilladaPlugin *plugin)
{
	GSList *output;
	GSList *input;

	/* NOTE: it seems that cdrecord can burn cue files on the fly */
	parrillada_plugin_define (plugin,
			       "vcdimager",
	                       NULL,
			       _("Creates disc images suitable for SVCDs"),
			       "Philippe Rouquier",
			       1);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_MP2|
					PARRILLADA_AUDIO_FORMAT_44100|
					PARRILLADA_VIDEO_FORMAT_VCD|
					PARRILLADA_METADATA_INFO);

	output = parrillada_caps_image_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					 PARRILLADA_IMAGE_FORMAT_CUE);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_MP2|
					PARRILLADA_AUDIO_FORMAT_44100|
					PARRILLADA_VIDEO_FORMAT_VCD);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* we only support CDs they must be blank */
	parrillada_plugin_set_flags (plugin,
				  PARRILLADA_MEDIUM_CDRW|
				  PARRILLADA_MEDIUM_BLANK|
				  PARRILLADA_MEDIUM_CLOSED|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_HAS_DATA|
				  PARRILLADA_MEDIUM_HAS_AUDIO,
				  PARRILLADA_BURN_FLAG_NONE,
				  PARRILLADA_BURN_FLAG_NONE);

	parrillada_plugin_set_flags (plugin,
				  PARRILLADA_MEDIUM_FILE|
				  PARRILLADA_MEDIUM_CDR|
				  PARRILLADA_MEDIUM_BLANK|
				  PARRILLADA_MEDIUM_APPENDABLE|
				  PARRILLADA_MEDIUM_HAS_DATA|
				  PARRILLADA_MEDIUM_HAS_AUDIO,
				  PARRILLADA_BURN_FLAG_NONE,
				  PARRILLADA_BURN_FLAG_NONE);
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	gint version [3] = { 0, 7, 0};
	parrillada_plugin_test_app (plugin,
	                         "vcdimager",
	                         "--version",
	                         "vcdimager (GNU VCDImager) %d.%d.%d",
	                         version);
}

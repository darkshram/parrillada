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
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <gst/gst.h>

#include "parrillada-tags.h"

#include "burn-job.h"
#include "burn-normalize.h"
#include "parrillada-plugin-registration.h"


#define PARRILLADA_TYPE_NORMALIZE             (parrillada_normalize_get_type ())
#define PARRILLADA_NORMALIZE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PARRILLADA_TYPE_NORMALIZE, ParrilladaNormalize))
#define PARRILLADA_NORMALIZE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), PARRILLADA_TYPE_NORMALIZE, ParrilladaNormalizeClass))
#define PARRILLADA_IS_NORMALIZE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PARRILLADA_TYPE_NORMALIZE))
#define PARRILLADA_IS_NORMALIZE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), PARRILLADA_TYPE_NORMALIZE))
#define PARRILLADA_NORMALIZE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), PARRILLADA_TYPE_NORMALIZE, ParrilladaNormalizeClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaNormalize, parrillada_normalize, PARRILLADA_TYPE_JOB, ParrilladaJob);

typedef struct _ParrilladaNormalizePrivate ParrilladaNormalizePrivate;
struct _ParrilladaNormalizePrivate
{
	GstElement *pipeline;
	GstElement *analysis;
	GstElement *decode;
	GstElement *resample;

	GSList *tracks;
	ParrilladaTrack *track;

	gdouble album_peak;
	gdouble album_gain;
	gdouble track_peak;
	gdouble track_gain;
};

#define PARRILLADA_NORMALIZE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_NORMALIZE, ParrilladaNormalizePrivate))

static GObjectClass *parent_class = NULL;


static gboolean
parrillada_normalize_bus_messages (GstBus *bus,
				GstMessage *msg,
				ParrilladaNormalize *normalize);

static void
parrillada_normalize_stop_pipeline (ParrilladaNormalize *normalize)
{
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);
	if (!priv->pipeline)
		return;

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;
	priv->resample = NULL;
	priv->analysis = NULL;
	priv->decode = NULL;
}

static void
parrillada_normalize_new_decoded_pad_cb (GstElement *decode,
				      GstPad *pad,
				      gboolean arg2,
				      ParrilladaNormalize *normalize)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);

	sink = gst_element_get_pad (priv->resample, "sink");
	if (GST_PAD_IS_LINKED (sink)) {
		PARRILLADA_JOB_LOG (normalize, "New decoded pad already linked");
		return;
	}

	/* make sure we only have audio */
	caps = gst_pad_get_caps (pad);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure && g_strrstr (gst_structure_get_name (structure), "audio")) {
		if (gst_pad_link (pad, sink) != GST_PAD_LINK_OK) {
			PARRILLADA_JOB_LOG (normalize, "New decoded pad can't be linked");
			parrillada_job_error (PARRILLADA_JOB (normalize), NULL);
		}
		else
			PARRILLADA_JOB_LOG (normalize, "New decoded pad linked");
	}
	else
		PARRILLADA_JOB_LOG (normalize, "New decoded pad with unsupported stream time");

	gst_object_unref (sink);
	gst_caps_unref (caps);
}

  static gboolean
parrillada_normalize_build_pipeline (ParrilladaNormalize *normalize,
                                  const gchar *uri,
                                  GstElement *analysis,
                                  GError **error)
{
	GstBus *bus = NULL;
	GstElement *source;
	GstElement *decode;
	GstElement *pipeline;
	GstElement *sink = NULL;
	GstElement *convert = NULL;
	GstElement *resample = NULL;
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);

	PARRILLADA_JOB_LOG (normalize, "Creating new pipeline");

	/* create filesrc ! decodebin ! audioresample ! audioconvert ! rganalysis ! fakesink */
	pipeline = gst_pipeline_new (NULL);
	priv->pipeline = pipeline;

	/* a new source is created */
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
	if (source == NULL) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Source\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), source);
	g_object_set (source,
		      "typefind", FALSE,
		      NULL);

	/* decode */
	decode = gst_element_factory_make ("decodebin", NULL);
	if (decode == NULL) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Decodebin\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), decode);
	priv->decode = decode;

	if (!gst_element_link (source, decode)) {
		PARRILLADA_JOB_LOG (normalize, "Elements could not be linked");
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
		             _("Impossible to link plugin pads"));
		goto error;
	}

	/* audioconvert */
	convert = gst_element_factory_make ("audioconvert", NULL);
	if (convert == NULL) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Audioconvert\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), convert);

	/* audioresample */
	resample = gst_element_factory_make ("audioresample", NULL);
	if (resample == NULL) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Audioresample\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), resample);
	priv->resample = resample;

	/* rganalysis: set the number of tracks to be expected */
	priv->analysis = analysis;
	gst_bin_add (GST_BIN (pipeline), analysis);

	/* sink */
	sink = gst_element_factory_make ("fakesink", NULL);
	if (!sink) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Fakesink\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), sink);
	g_object_set (sink,
		      "sync", FALSE,
		      NULL);

	/* link everything */
	g_signal_connect (G_OBJECT (decode),
	                  "new-decoded-pad",
	                  G_CALLBACK (parrillada_normalize_new_decoded_pad_cb),
	                  normalize);
	if (!gst_element_link_many (resample,
	                            convert,
	                            analysis,
	                            sink,
	                            NULL)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
		             _("Impossible to link plugin pads"));
	}

	/* connect to the bus */	
	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) parrillada_normalize_bus_messages,
			   normalize);
	gst_object_unref (bus);

	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

	return TRUE;

error:

	if (error && (*error))
		PARRILLADA_JOB_LOG (normalize,
				 "can't create object : %s \n",
				 (*error)->message);

	gst_object_unref (GST_OBJECT (pipeline));
	return FALSE;
}

static ParrilladaBurnResult
parrillada_normalize_set_next_track (ParrilladaJob *job,
                                  GError **error)
{
	gchar *uri;
	GValue *value;
	GstElement *analysis;
	ParrilladaTrackType *type;
	ParrilladaTrack *track = NULL;
	gboolean dts_allowed = FALSE;
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (job);

	/* See if dts is allowed */
	value = NULL;
	parrillada_job_tag_lookup (job, PARRILLADA_SESSION_STREAM_AUDIO_FORMAT, &value);
	if (value)
		dts_allowed = (g_value_get_int (value) & PARRILLADA_AUDIO_FORMAT_DTS) != 0;

	type = parrillada_track_type_new ();
	while (priv->tracks && priv->tracks->data) {
		track = priv->tracks->data;
		priv->tracks = g_slist_remove (priv->tracks, track);

		parrillada_track_get_track_type (track, type);
		if (parrillada_track_type_get_has_stream (type)) {
			if (!dts_allowed)
				break;

			/* skip DTS tracks as we won't modify them */
			if ((parrillada_track_type_get_stream_format (type) & PARRILLADA_AUDIO_FORMAT_DTS) == 0) 
				break;

			PARRILLADA_JOB_LOG (job, "Skipped DTS track");
		}

		track = NULL;
	}
	parrillada_track_type_free (type);

	if (!track)
		return PARRILLADA_BURN_OK;

	if (!priv->analysis) {
		analysis = gst_element_factory_make ("rganalysis", NULL);
		if (analysis == NULL) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Rganalysis\"");
			return PARRILLADA_BURN_ERR;
		}

		g_object_set (analysis,
			      "num-tracks", g_slist_length (priv->tracks),
			      NULL);
	}
	else {
		/* destroy previous pipeline but save our plugin */
		analysis = g_object_ref (priv->analysis);

		/* NOTE: why lock state? because otherwise analysis would lose all 
		 * information about tracks already analysed by going into the NULL
		 * state. */
		gst_element_set_locked_state (analysis, TRUE);
		gst_bin_remove (GST_BIN (priv->pipeline), analysis);
		parrillada_normalize_stop_pipeline (PARRILLADA_NORMALIZE (job));
		gst_element_set_locked_state (analysis, FALSE);
	}

	/* create a new one */
	priv->track = track;
	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), TRUE);
	PARRILLADA_JOB_LOG (job, "Analysing track %s", uri);

	if (!parrillada_normalize_build_pipeline (PARRILLADA_NORMALIZE (job), uri, analysis, error)) {
		g_free (uri);
		return PARRILLADA_BURN_ERR;
	}

	g_free (uri);
	return PARRILLADA_BURN_RETRY;
}

static ParrilladaBurnResult
parrillada_normalize_stop (ParrilladaJob *job,
			GError **error)
{
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (job);

	parrillada_normalize_stop_pipeline (PARRILLADA_NORMALIZE (job));
	if (priv->tracks) {
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	priv->track = NULL;

	return PARRILLADA_BURN_OK;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     ParrilladaNormalize *normalize)
{
	gdouble value = 0.0;
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);

	/* Those next two are generated at the end only */
	if (!strcmp (tag, GST_TAG_ALBUM_GAIN)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->album_gain = value;
	}
	else if (!strcmp (tag, GST_TAG_ALBUM_PEAK)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->album_peak = value;
	}
	else if (!strcmp (tag, GST_TAG_TRACK_PEAK)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->track_peak = value;
	}
	else if (!strcmp (tag, GST_TAG_TRACK_GAIN)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->track_gain = value;
	}
}

static void
parrillada_normalize_song_end_reached (ParrilladaNormalize *normalize)
{
	GValue *value;
	GError *error = NULL;
	ParrilladaBurnResult result;
	ParrilladaNormalizePrivate *priv;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);
	
	/* finished track: set tags */
	PARRILLADA_JOB_LOG (normalize,
			 "Setting track peak (%lf) and gain (%lf)",
			 priv->track_peak,
			 priv->track_gain);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, priv->track_peak);
	parrillada_track_tag_add (priv->track,
			       PARRILLADA_TRACK_PEAK_VALUE,
			       value);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, priv->track_gain);
	parrillada_track_tag_add (priv->track,
			       PARRILLADA_TRACK_GAIN_VALUE,
			       value);

	priv->track_peak = 0.0;
	priv->track_gain = 0.0;

	result = parrillada_normalize_set_next_track (PARRILLADA_JOB (normalize), &error);
	if (result == PARRILLADA_BURN_OK) {
		PARRILLADA_JOB_LOG (normalize,
				 "Setting album peak (%lf) and gain (%lf)",
				 priv->album_peak,
				 priv->album_gain);

		/* finished: set tags */
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, priv->album_peak);
		parrillada_job_tag_add (PARRILLADA_JOB (normalize),
				     PARRILLADA_ALBUM_PEAK_VALUE,
				     value);

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, priv->album_gain);
		parrillada_job_tag_add (PARRILLADA_JOB (normalize),
				     PARRILLADA_ALBUM_GAIN_VALUE,
				     value);

		parrillada_job_finished_session (PARRILLADA_JOB (normalize));
		return;
	}

	/* jump to next track */
	if (result == PARRILLADA_BURN_ERR) {
		parrillada_job_error (PARRILLADA_JOB (normalize), error);
		return;
	}
}

static gboolean
parrillada_normalize_bus_messages (GstBus *bus,
				GstMessage *msg,
				ParrilladaNormalize *normalize)
{
	ParrilladaNormalizePrivate *priv;
	GstTagList *tags = NULL;
	GError *error = NULL;
	gchar *debug;

	priv = PARRILLADA_NORMALIZE_PRIVATE (normalize);
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_TAG:
		/* This is the information we've been waiting for.
		 * NOTE: levels for whole album is delivered at the end */
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, normalize);
		gst_tag_list_free (tags);
		return TRUE;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		PARRILLADA_JOB_LOG (normalize, debug);
		g_free (debug);

	        parrillada_job_error (PARRILLADA_JOB (normalize), error);
		return FALSE;

	case GST_MESSAGE_EOS:
		parrillada_normalize_song_end_reached (normalize);
		return FALSE;

	case GST_MESSAGE_STATE_CHANGED:
		break;

	default:
		return TRUE;
	}

	return TRUE;
}

static ParrilladaBurnResult
parrillada_normalize_start (ParrilladaJob *job,
			 GError **error)
{
	ParrilladaNormalizePrivate *priv;
	ParrilladaBurnResult result;

	priv = PARRILLADA_NORMALIZE_PRIVATE (job);

	priv->album_gain = -1.0;
	priv->album_peak = -1.0;

	/* get tracks */
	parrillada_job_get_tracks (job, &priv->tracks);
	if (!priv->tracks)
		return PARRILLADA_BURN_ERR;

	priv->tracks = g_slist_copy (priv->tracks);

	result = parrillada_normalize_set_next_track (job, error);
	if (result == PARRILLADA_BURN_ERR)
		return PARRILLADA_BURN_ERR;

	if (result == PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_NOT_RUNNING;

	/* ready to go */
	parrillada_job_set_current_action (job,
					PARRILLADA_BURN_ACTION_ANALYSING,
					_("Normalizing tracks"),
					FALSE);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_normalize_activate (ParrilladaJob *job,
			    GError **error)
{
	GSList *tracks;
	ParrilladaJobAction action;

	parrillada_job_get_action (job, &action);
	if (action != PARRILLADA_JOB_ACTION_IMAGE)
		return PARRILLADA_BURN_NOT_RUNNING;

	/* check we have more than one track */
	parrillada_job_get_tracks (job, &tracks);
	if (g_slist_length (tracks) < 2)
		return PARRILLADA_BURN_NOT_RUNNING;

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_normalize_clock_tick (ParrilladaJob *job)
{
	gint64 position = 0.0;
	gint64 duration = 0.0;
	ParrilladaNormalizePrivate *priv;
	GstFormat format = GST_FORMAT_TIME;

	priv = PARRILLADA_NORMALIZE_PRIVATE (job);

	gst_element_query_duration (priv->pipeline, &format, &duration);
	gst_element_query_position (priv->pipeline, &format, &position);

	if (duration > 0) {
		GSList *tracks;
		gdouble progress;

		parrillada_job_get_tracks (job, &tracks);
		progress = (gdouble) position / (gdouble) duration;

		if (tracks) {
			gdouble num_tracks;

			num_tracks = g_slist_length (tracks);
			progress = (gdouble) (num_tracks - 1.0 - (gdouble) g_slist_length (priv->tracks) + progress) / (gdouble) num_tracks;
			parrillada_job_set_progress (job, progress);
		}
	}

	return PARRILLADA_BURN_OK;
}

static void
parrillada_normalize_init (ParrilladaNormalize *object)
{}

static void
parrillada_normalize_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_normalize_class_init (ParrilladaNormalizeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaNormalizePrivate));

	object_class->finalize = parrillada_normalize_finalize;

	job_class->activate = parrillada_normalize_activate;
	job_class->start = parrillada_normalize_start;
	job_class->clock_tick = parrillada_normalize_clock_tick;
	job_class->stop = parrillada_normalize_stop;
}

static void
parrillada_normalize_export_caps (ParrilladaPlugin *plugin)
{
	GSList *input;

	parrillada_plugin_define (plugin,
	                       "normalize",
			       N_("Normalization"),
			       _("Sets consistent sound levels between tracks"),
			       "Philippe Rouquier",
			       0);

	/* Add dts to make sure that when they are mixed with regular songs
	 * this plugin will be called for the regular tracks */
	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_UNDEFINED|
	                                PARRILLADA_AUDIO_FORMAT_DTS|
					PARRILLADA_METADATA_INFO);
	parrillada_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* Add dts to make sure that when they are mixed with regular songs
	 * this plugin will be called for the regular tracks */
	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_UNDEFINED|
	                                PARRILLADA_AUDIO_FORMAT_DTS);
	parrillada_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* We should run first */
	parrillada_plugin_set_process_flags (plugin, PARRILLADA_PLUGIN_RUN_PREPROCESSING);

	parrillada_plugin_set_compulsory (plugin, FALSE);
}

G_MODULE_EXPORT void
parrillada_plugin_check_config (ParrilladaPlugin *plugin)
{
	parrillada_plugin_test_gstreamer_plugin (plugin, "rgvolume");
	parrillada_plugin_test_gstreamer_plugin (plugin, "rganalysis");
}

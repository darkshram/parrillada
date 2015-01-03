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
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <gst/gst.h>

#include "burn-basics.h"
#include "parrillada-medium.h"
#include "parrillada-tags.h"
#include "burn-job.h"
#include "parrillada-plugin-registration.h"
#include "burn-normalize.h"


#define PARRILLADA_TYPE_TRANSCODE         (parrillada_transcode_get_type ())
#define PARRILLADA_TRANSCODE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PARRILLADA_TYPE_TRANSCODE, ParrilladaTranscode))
#define PARRILLADA_TRANSCODE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PARRILLADA_TYPE_TRANSCODE, ParrilladaTranscodeClass))
#define PARRILLADA_IS_TRANSCODE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PARRILLADA_TYPE_TRANSCODE))
#define PARRILLADA_IS_TRANSCODE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PARRILLADA_TYPE_TRANSCODE))
#define PARRILLADA_TRANSCODE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PARRILLADA_TYPE_TRANSCODE, ParrilladaTranscodeClass))

PARRILLADA_PLUGIN_BOILERPLATE (ParrilladaTranscode, parrillada_transcode, PARRILLADA_TYPE_JOB, ParrilladaJob);

static gboolean parrillada_transcode_bus_messages (GstBus *bus,
						GstMessage *msg,
						ParrilladaTranscode *transcode);
static void parrillada_transcode_new_decoded_pad_cb (GstElement *decode,
						  GstPad *pad,
						  gboolean arg2,
						  ParrilladaTranscode *transcode);

struct ParrilladaTranscodePrivate {
	GstElement *pipeline;
	GstElement *convert;
	GstElement *source;
	GstElement *decode;
	GstElement *sink;

	/* element to link decode to */
	GstElement *link;

	gint pad_size;
	gint pad_fd;
	gint pad_id;

	gint64 size;
	gint64 pos;

	gulong probe;
	gint64 segment_start;
	gint64 segment_end;

	guint set_active_state:1;
	guint mp3_size_pipeline:1;
};
typedef struct ParrilladaTranscodePrivate ParrilladaTranscodePrivate;

#define PARRILLADA_TRANSCODE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_TRANSCODE, ParrilladaTranscodePrivate))

static GObjectClass *parent_class = NULL;

static gboolean
parrillada_transcode_buffer_handler (GstPad *pad,
				  GstBuffer *buffer,
				  ParrilladaTranscode *self)
{
	ParrilladaTranscodePrivate *priv;
	GstPad *peer;
	gint64 size;

	priv = PARRILLADA_TRANSCODE_PRIVATE (self);

	size = GST_BUFFER_SIZE (buffer);

	if (priv->segment_start <= 0 && priv->segment_end <= 0)
		return TRUE;

	/* what we do here is more or less what gstreamer does when seeking:
	 * it reads and process from 0 to the seek position (I tried).
	 * It even forwards the data before the seek position to the sink (which
	 * is a problem in our case as it would be written) */
	if (priv->size > priv->segment_end) {
		priv->size += size;
		return FALSE;
	}

	if (priv->size + size > priv->segment_end) {
		GstBuffer *new_buffer;
		int data_size;

		/* the entire the buffer is not interesting for us */
		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->segment_end - priv->size;
		new_buffer = gst_buffer_new_and_alloc (data_size);
		memcpy (GST_BUFFER_DATA (new_buffer), GST_BUFFER_DATA (buffer), data_size);

		/* Recursive: the following calls ourselves BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);

		priv->size += size - data_size;

		/* post an EOS event to stop pipeline */
		gst_pad_push_event (peer, gst_event_new_eos ());
		gst_object_unref (peer);
		return FALSE;
	}

	/* see if the buffer is in the segment */
	if (priv->size < priv->segment_start) {
		GstBuffer *new_buffer;
		gint data_size;

		/* see if all the buffer is interesting for us */
		if (priv->size + size < priv->segment_start) {
			priv->size += size;
			return FALSE;
		}

		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->size + size - priv->segment_start;
		new_buffer = gst_buffer_new_and_alloc (data_size);
		memcpy (GST_BUFFER_DATA (new_buffer),
			GST_BUFFER_DATA (buffer) +
			GST_BUFFER_SIZE (buffer) -
			data_size,
			data_size);
		GST_BUFFER_TIMESTAMP (new_buffer) = GST_BUFFER_TIMESTAMP (buffer) + data_size;

		/* move forward by the size of bytes we dropped */
		priv->size += size - data_size;

		/* this is recursive the following calls ourselves 
		 * BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);
		gst_object_unref (peer);

		return FALSE;
	}

	priv->size += size;
	priv->pos += size;

	return TRUE;
}

static ParrilladaBurnResult
parrillada_transcode_set_boundaries (ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	ParrilladaTrack *track;
	gint64 start;
	gint64 end;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	/* we need to reach the song start and set a possible end; this is only
	 * needed when it is decoding a song. Otherwise*/
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	start = parrillada_track_stream_get_start (PARRILLADA_TRACK_STREAM (track));
	end = parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (track));

	priv->segment_start = PARRILLADA_DURATION_TO_BYTES (start);
	priv->segment_end = PARRILLADA_DURATION_TO_BYTES (end);

	PARRILLADA_JOB_LOG (transcode, "settings track boundaries time = %lli %lli / bytes = %lli %lli",
			 start, end,
			 priv->segment_start, priv->segment_end);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_transcode_send_volume_event (ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	gdouble track_peak = 0.0;
	gdouble track_gain = 0.0;
	GstTagList *tag_list;
	ParrilladaTrack *track;
	GstEvent *event;
	GValue *value;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);

	PARRILLADA_JOB_LOG (transcode, "Sending audio levels tags");
	if (parrillada_track_tag_lookup (track, PARRILLADA_TRACK_PEAK_VALUE, &value) == PARRILLADA_BURN_OK)
		track_peak = g_value_get_double (value);

	if (parrillada_track_tag_lookup (track, PARRILLADA_TRACK_GAIN_VALUE, &value) == PARRILLADA_BURN_OK)
		track_gain = g_value_get_double (value);

	/* it's possible we fail */
	tag_list = gst_tag_list_new ();
	gst_tag_list_add (tag_list, GST_TAG_MERGE_REPLACE,
			  GST_TAG_TRACK_GAIN, track_gain,
			  GST_TAG_TRACK_PEAK, track_peak,
			  NULL);

	/* NOTE: that event is goind downstream */
	event = gst_event_new_tag (tag_list);
	if (!gst_element_send_event (priv->convert, event))
		PARRILLADA_JOB_LOG (transcode, "Couldn't send tags to rgvolume");

	PARRILLADA_JOB_LOG (transcode, "Set volume level %lf %lf", track_gain, track_peak);
}

static GstElement *
parrillada_transcode_create_volume (ParrilladaTranscode *transcode,
				 ParrilladaTrack *track)
{
	GstElement *volume = NULL;

	/* see if we need a volume object */
	if (parrillada_track_tag_lookup (track, PARRILLADA_TRACK_PEAK_VALUE, NULL) == PARRILLADA_BURN_OK
	||  parrillada_track_tag_lookup (track, PARRILLADA_TRACK_GAIN_VALUE, NULL) == PARRILLADA_BURN_OK) {
		PARRILLADA_JOB_LOG (transcode, "Found audio levels tags");
		volume = gst_element_factory_make ("rgvolume", NULL);
		if (volume)
			g_object_set (volume,
				      "album-mode", FALSE,
				      NULL);
		else
			PARRILLADA_JOB_LOG (transcode, "rgvolume object couldn't be created");
	}

	return volume;
}

static gboolean
parrillada_transcode_create_pipeline_size_mp3 (ParrilladaTranscode *transcode,
					    GstElement *pipeline,
					    GstElement *source,
                                            GError **error)
{
	ParrilladaTranscodePrivate *priv;
	GstElement *parse;
	GstElement *sink;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	PARRILLADA_JOB_LOG (transcode, "Creating specific pipeline for MP3s");

	parse = gst_element_factory_make ("mp3parse", NULL);
	if (!parse) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Mp3parse\"");
		g_object_unref (pipeline);
		return FALSE;
	}
	gst_bin_add (GST_BIN (pipeline), parse);

	sink = gst_element_factory_make ("fakesink", NULL);
	if (!sink) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Fakesink\"");
		g_object_unref (pipeline);
		return FALSE;
	}
	gst_bin_add (GST_BIN (pipeline), sink);

	/* Link */
	if (!gst_element_link_many (source, parse, sink, NULL)) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("Impossible to link plugin pads"));
		PARRILLADA_JOB_LOG (transcode, "Impossible to link elements");
		g_object_unref (pipeline);
		return FALSE;
	}

	priv->convert = NULL;

	priv->sink = sink;
	priv->source = source;
	priv->pipeline = pipeline;

	/* Get it going */
	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
	return TRUE;
}

static void
parrillada_transcode_error_on_pad_linking (ParrilladaTranscode *self,
                                        const gchar *function_name)
{
	ParrilladaTranscodePrivate *priv;
	GstMessage *message;
	GstBus *bus;

	priv = PARRILLADA_TRANSCODE_PRIVATE (self);

	PARRILLADA_JOB_LOG (self, "Error on pad linking");
	message = gst_message_new_error (GST_OBJECT (priv->pipeline),
					 g_error_new (PARRILLADA_BURN_ERROR,
						      PARRILLADA_BURN_ERROR_GENERAL,
						      /* Translators: This message is sent
						       * when parrillada could not link together
						       * two gstreamer plugins so that one
						       * sends its data to the second for further
						       * processing. This data transmission is
						       * done through a pad. Maybe this is a bit
						       * too technical and should be removed? */
						      _("Impossible to link plugin pads")),
					 function_name);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	gst_bus_post (bus, message);
	g_object_unref (bus);
}

static void
parrillada_transcode_wavparse_pad_added_cb (GstElement *wavparse,
                                         GstPad *new_pad,
                                         gpointer user_data)
{
	GstPad *pad = NULL;
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (user_data);

	pad = gst_element_get_static_pad (priv->sink, "sink");
	if (!pad) 
		goto error;

	if (gst_pad_link (new_pad, pad) != GST_PAD_LINK_OK)
		goto error;

	gst_element_set_state (priv->sink, GST_STATE_PLAYING);
	return;

error:

	if (pad)
		gst_object_unref (pad);

	parrillada_transcode_error_on_pad_linking (PARRILLADA_TRANSCODE (user_data), "Sent by parrillada_transcode_wavparse_pad_added_cb");
}

static gboolean
parrillada_transcode_create_pipeline (ParrilladaTranscode *transcode,
				   GError **error)
{
	gchar *uri;
	gboolean keep_dts;
	GstElement *decode;
	GstElement *source;
	GstBus *bus = NULL;
	GstCaps *filtercaps;
	GValue *value = NULL;
	GstElement *pipeline;
	GstElement *sink = NULL;
	ParrilladaJobAction action;
	GstElement *filter = NULL;
	GstElement *volume = NULL;
	GstElement *convert = NULL;
	ParrilladaTrack *track = NULL;
	GstElement *resample = NULL;
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	PARRILLADA_JOB_LOG (transcode, "Creating new pipeline");

	priv->set_active_state = 0;

	/* free the possible current pipeline and create a new one */
	if (priv->pipeline) {
		gst_element_set_state (priv->pipeline, GST_STATE_NULL);
		gst_object_unref (G_OBJECT (priv->pipeline));
		priv->link = NULL;
		priv->sink = NULL;
		priv->source = NULL;
		priv->convert = NULL;
		priv->pipeline = NULL;
	}

	/* create three types of pipeline according to the needs: (possibly adding grvolume)
	 * - filesrc ! decodebin ! audioconvert ! fakesink (find size) and filesrc!mp3parse!fakesink for mp3s
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw-int,rate=44100,width=16,depth=16,endianness=4321,signed ! filesink
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw-int,rate=44100,width=16,depth=16,endianness=4321,signed ! fdsink
	 */
	pipeline = gst_pipeline_new (NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) parrillada_transcode_bus_messages,
			   transcode);
	gst_object_unref (bus);

	/* source */
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), TRUE);
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
	g_free (uri);

	if (source == NULL) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: %s is the name of the object (as in
			      * GObject) from the Gstreamer library that could
			      * not be created */
			     _("%s element could not be created"),
			     "\"Source\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), source);
	g_object_set (source,
		      "typefind", FALSE,
		      NULL);

	/* sink */
	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);
	switch (action) {
	case PARRILLADA_JOB_ACTION_SIZE:
		if (priv->mp3_size_pipeline)
			return parrillada_transcode_create_pipeline_size_mp3 (transcode, pipeline, source, error);

		sink = gst_element_factory_make ("fakesink", NULL);
		break;

	case PARRILLADA_JOB_ACTION_IMAGE:
		volume = parrillada_transcode_create_volume (transcode, track);

		if (parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), NULL) != PARRILLADA_BURN_OK) {
			gchar *output;

			parrillada_job_get_image_output (PARRILLADA_JOB (transcode),
						      &output,
						      NULL);
			sink = gst_element_factory_make ("filesink", NULL);
			g_object_set (sink,
				      "location", output,
				      NULL);
			g_free (output);
		}
		else {
			int fd;

			parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), &fd);
			sink = gst_element_factory_make ("fdsink", NULL);
			g_object_set (sink,
				      "fd", fd,
				      NULL);
		}
		break;

	default:
		goto error;
	}

	if (!sink) {
		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Sink\"");
		goto error;
	}

	gst_bin_add (GST_BIN (pipeline), sink);
	g_object_set (sink,
		      "sync", FALSE,
		      NULL);

	parrillada_job_tag_lookup (PARRILLADA_JOB (transcode),
				PARRILLADA_SESSION_STREAM_AUDIO_FORMAT,
				&value);
	if (value)
		keep_dts = (g_value_get_int (value) & PARRILLADA_AUDIO_FORMAT_DTS) != 0;
	else
		keep_dts = FALSE;

	if (keep_dts
	&&  action == PARRILLADA_JOB_ACTION_IMAGE
	&& (parrillada_track_stream_get_format (PARRILLADA_TRACK_STREAM (track)) & PARRILLADA_AUDIO_FORMAT_DTS) != 0) {
		GstElement *wavparse;
		GstPad *sinkpad;

		PARRILLADA_JOB_LOG (transcode, "DTS wav pipeline");

		/* FIXME: volume normalization won't work here. We'd need to 
		 * reencode it afterwards otherwise. */
		/* This is a special case. This is DTS wav. So we only decode wav. */
		wavparse = gst_element_factory_make ("wavparse", NULL);
		if (wavparse == NULL) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Wavparse\"");
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), wavparse);

		if (!gst_element_link (source, wavparse)) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
			             _("Impossible to link plugin pads"));
			goto error;
		}

		g_signal_connect (wavparse,
		                  "pad-added",
		                  G_CALLBACK (parrillada_transcode_wavparse_pad_added_cb),
		                  transcode);

		/* This is an ugly workaround for the lack of accuracy with
		 * gstreamer. Yet this is unfortunately a necessary evil. */
		priv->pos = 0;
		priv->size = 0;
		sinkpad = gst_element_get_pad (sink, "sink");
		priv->probe = gst_pad_add_buffer_probe (sinkpad,
							G_CALLBACK (parrillada_transcode_buffer_handler),
							transcode);
		gst_object_unref (sinkpad);


		priv->link = NULL;
		priv->sink = sink;
		priv->decode = NULL;
		priv->source = source;
		priv->convert = NULL;
		priv->pipeline = pipeline;

		gst_element_set_state (pipeline, GST_STATE_PLAYING);
		return TRUE;
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

	if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		ParrilladaStreamFormat session_format;
		ParrilladaTrackType *output_type;

		output_type = parrillada_track_type_new ();
		parrillada_job_get_output_type (PARRILLADA_JOB (transcode), output_type);
		session_format = parrillada_track_type_get_stream_format (output_type);
		parrillada_track_type_free (output_type);

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

		/* filter */
		filter = gst_element_factory_make ("capsfilter", NULL);
		if (!filter) {
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Filter\"");
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), filter);
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
								   "channels", G_TYPE_INT, 2,
								   "width", G_TYPE_INT, 16,
								   "depth", G_TYPE_INT, 16,
								   /* NOTE: we use little endianness only for libburn which requires little */
								   "endianness", G_TYPE_INT, (session_format & PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN) != 0 ? 1234:4321,
								   "rate", G_TYPE_INT, 44100,
								   "signed", G_TYPE_BOOLEAN, TRUE,
								   NULL),
						NULL);
		g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
		gst_caps_unref (filtercaps);
	}

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

	if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		GstPad *sinkpad;
		gboolean res;

		if (!gst_element_link (source, decode)) {
			PARRILLADA_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
			             _("Impossible to link plugin pads"));
			goto error;
		}

		priv->link = resample;
		g_signal_connect (G_OBJECT (decode),
				  "new-decoded-pad",
				  G_CALLBACK (parrillada_transcode_new_decoded_pad_cb),
				  transcode);

		if (volume) {
			gst_bin_add (GST_BIN (pipeline), volume);
			res = gst_element_link_many (resample,
			                             volume,
						     convert,
			                             filter,
			                             sink,
			                             NULL);
		}
		else
			res = gst_element_link_many (resample,
			                             convert,
			                             filter,
			                             sink,
			                             NULL);

		if (!res) {
			PARRILLADA_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("Impossible to link plugin pads"));
			goto error;
		}

		/* This is an ugly workaround for the lack of accuracy with
		 * gstreamer. Yet this is unfortunately a necessary evil. */
		priv->pos = 0;
		priv->size = 0;
		sinkpad = gst_element_get_pad (sink, "sink");
		priv->probe = gst_pad_add_buffer_probe (sinkpad,
							G_CALLBACK (parrillada_transcode_buffer_handler),
							transcode);
		gst_object_unref (sinkpad);
	}
	else {
		if (!gst_element_link (source, decode)
		||  !gst_element_link (convert, sink)) {
			PARRILLADA_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     _("Impossible to link plugin pads"));
			goto error;
		}

		priv->link = convert;
		g_signal_connect (G_OBJECT (decode),
				  "new-decoded-pad",
				  G_CALLBACK (parrillada_transcode_new_decoded_pad_cb),
				  transcode);
	}

	priv->sink = sink;
	priv->decode = decode;
	priv->source = source;
	priv->convert = convert;
	priv->pipeline = pipeline;

	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	return TRUE;

error:

	if (error && (*error))
		PARRILLADA_JOB_LOG (transcode,
				 "can't create object : %s \n",
				 (*error)->message);

	gst_object_unref (GST_OBJECT (pipeline));
	return FALSE;
}

static void
parrillada_transcode_set_track_size (ParrilladaTranscode *transcode,
				  gint64 duration)
{
	gchar *uri;
	ParrilladaTrack *track;

	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	parrillada_track_stream_set_boundaries (PARRILLADA_TRACK_STREAM (track), -1, duration, -1);
	duration += parrillada_track_stream_get_gap (PARRILLADA_TRACK_STREAM (track));

	/* if transcoding on the fly we should add some length just to make
	 * sure we won't be too short (gstreamer duration discrepancy) */
	parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (transcode),
						       PARRILLADA_DURATION_TO_SECTORS (duration),
						       PARRILLADA_DURATION_TO_BYTES (duration));

	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), FALSE);
	PARRILLADA_JOB_LOG (transcode,
			 "Song %s"
			 "\nsectors %" G_GINT64_FORMAT
			 "\ntime %" G_GINT64_FORMAT, 
			 uri,
			 PARRILLADA_DURATION_TO_SECTORS (duration),
			 duration);
	g_free (uri);
}

/**
 * These functions are to deal with siblings
 */

static ParrilladaBurnResult
parrillada_transcode_create_sibling_size (ParrilladaTranscode *transcode,
				        ParrilladaTrack *src,
				        GError **error)
{
	ParrilladaTrack *dest;
	guint64 duration;

	/* it means the same file uri is in the selection and was already
	 * checked. Simply get the values for the length and other information
	 * and copy them. */
	/* NOTE: no need to copy the length since if they are sibling that means
	 * that they have the same length */
	parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (src), &duration);
	parrillada_job_set_output_size_for_current_track (PARRILLADA_JOB (transcode),
						       PARRILLADA_DURATION_TO_SECTORS (duration),
						       PARRILLADA_DURATION_TO_BYTES (duration));

	/* copy the info we are missing */
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &dest);
	parrillada_track_tag_copy_missing (dest, src);

	return PARRILLADA_BURN_OK;
}

static ParrilladaBurnResult
parrillada_transcode_create_sibling_image (ParrilladaTranscode *transcode,
					ParrilladaTrack *src,
					GError **error)
{
	ParrilladaTrackStream *dest;
	ParrilladaTrack *track;
	guint64 length = 0;
	gchar *path_dest;
	gchar *path_src;

	/* it means the file is already in the selection. Simply create a 
	 * symlink pointing to first file in the selection with the same uri */
	path_src = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (src), FALSE);
	parrillada_job_get_audio_output (PARRILLADA_JOB (transcode), &path_dest);

	if (symlink (path_src, path_dest) == -1) {
                int errsv = errno;

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: the %s is the error message from errno */
			     _("An internal error occurred (%s)"),
			     g_strerror (errsv));

		goto error;
	}

	dest = parrillada_track_stream_new ();
	parrillada_track_stream_set_source (dest, path_dest);

	/* FIXME: what if input had metadata ?*/
	parrillada_track_stream_set_format (dest, PARRILLADA_AUDIO_FORMAT_RAW);

	/* NOTE: there is no gap and start = 0 since these tracks are the result
	 * of the transformation of previous ones */
	parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (src), &length);
	parrillada_track_stream_set_boundaries (dest, 0, length, 0);

	/* copy all infos but from the current track */
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	parrillada_track_tag_copy_missing (PARRILLADA_TRACK (dest), track);
	parrillada_job_add_track (PARRILLADA_JOB (transcode), PARRILLADA_TRACK (dest));

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. ParrilladaTaskCtx refs it. */
	g_object_unref (dest);

	g_free (path_src);
	g_free (path_dest);

	return PARRILLADA_BURN_NOT_RUNNING;

error:
	g_free (path_src);
	g_free (path_dest);

	return PARRILLADA_BURN_ERR;
}

static ParrilladaTrack *
parrillada_transcode_search_for_sibling (ParrilladaTranscode *transcode)
{
	ParrilladaJobAction action;
	GSList *iter, *songs;
	ParrilladaTrack *track;
	gint64 start;
	gint64 end;
	gchar *uri;

	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);

	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	start = parrillada_track_stream_get_start (PARRILLADA_TRACK_STREAM (track));
	end = parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (track));
	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), TRUE);

	parrillada_job_get_done_tracks (PARRILLADA_JOB (transcode), &songs);

	for (iter = songs; iter; iter = iter->next) {
		gchar *iter_uri;
		gint64 iter_end;
		gint64 iter_start;
		ParrilladaTrack *iter_track;

		iter_track = iter->data;
		iter_uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (iter_track), TRUE);

		if (strcmp (iter_uri, uri))
			continue;

		iter_end = parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (iter_track));
		if (!iter_end)
			continue;

		if (iter_end != end)
			continue;

		iter_start = parrillada_track_stream_get_start (PARRILLADA_TRACK_STREAM (track));
		if (iter_start == start) {
			g_free (uri);
			return iter_track;
		}
	}

	g_free (uri);
	return NULL;
}

static ParrilladaBurnResult
parrillada_transcode_has_track_sibling (ParrilladaTranscode *transcode,
				     GError **error)
{
	ParrilladaJobAction action;
	ParrilladaTrack *sibling = NULL;
	ParrilladaBurnResult result = PARRILLADA_BURN_OK;

	if (parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), NULL) == PARRILLADA_BURN_OK)
		return PARRILLADA_BURN_OK;

	sibling = parrillada_transcode_search_for_sibling (transcode);
	if (!sibling)
		return PARRILLADA_BURN_OK;

	PARRILLADA_JOB_LOG (transcode, "found sibling: skipping");
	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);
	if (action == PARRILLADA_JOB_ACTION_IMAGE)
		result = parrillada_transcode_create_sibling_image (transcode,
								 sibling,
								 error);
	else if (action == PARRILLADA_JOB_ACTION_SIZE)
		result = parrillada_transcode_create_sibling_size (transcode,
								sibling,
								error);

	return result;
}

static ParrilladaBurnResult
parrillada_transcode_start (ParrilladaJob *job,
			 GError **error)
{
	ParrilladaTranscode *transcode;
	ParrilladaBurnResult result;
	ParrilladaJobAction action;

	transcode = PARRILLADA_TRANSCODE (job);

	parrillada_job_get_action (job, &action);
	parrillada_job_set_use_average_rate (job, TRUE);

	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		ParrilladaTrack *track;

		/* see if the track size was already set since then no need to 
		 * carry on with a lengthy get size and the library will do it
		 * itself. */
		parrillada_job_get_current_track (job, &track);
		if (parrillada_track_stream_get_end (PARRILLADA_TRACK_STREAM (track)) > 0)
			return PARRILLADA_BURN_NOT_SUPPORTED;

		if (!parrillada_transcode_create_pipeline (transcode, error))
			return PARRILLADA_BURN_ERR;

		parrillada_job_set_current_action (job,
						PARRILLADA_BURN_ACTION_GETTING_SIZE,
						NULL,
						TRUE);

		parrillada_job_start_progress (job, FALSE);
		return PARRILLADA_BURN_OK;
	}

	if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		/* Look for a sibling to avoid transcoding twice. In this case
		 * though start and end of this track must be inside start and
		 * end of the previous track. Of course if we are piping that
		 * operation is simply impossible. */
		if (parrillada_job_get_fd_out (job, NULL) != PARRILLADA_BURN_OK) {
			result = parrillada_transcode_has_track_sibling (PARRILLADA_TRANSCODE (job), error);
			if (result != PARRILLADA_BURN_OK)
				return result;
		}

		parrillada_transcode_set_boundaries (transcode);
		if (!parrillada_transcode_create_pipeline (transcode, error))
			return PARRILLADA_BURN_ERR;
	}
	else
		PARRILLADA_JOB_NOT_SUPPORTED (transcode);

	return PARRILLADA_BURN_OK;
}

static void
parrillada_transcode_stop_pipeline (ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	GstPad *sinkpad;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);
	if (!priv->pipeline)
		return;

	sinkpad = gst_element_get_pad (priv->sink, "sink");
	if (priv->probe)
		gst_pad_remove_buffer_probe (sinkpad, priv->probe);

	gst_object_unref (sinkpad);

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));

	priv->link = NULL;
	priv->sink = NULL;
	priv->source = NULL;
	priv->convert = NULL;
	priv->pipeline = NULL;

	priv->set_active_state = 0;
}

static ParrilladaBurnResult
parrillada_transcode_stop (ParrilladaJob *job,
			GError **error)
{
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (job);

	priv->mp3_size_pipeline = 0;

	if (priv->pad_id) {
		g_source_remove (priv->pad_id);
		priv->pad_id = 0;
	}

	parrillada_transcode_stop_pipeline (PARRILLADA_TRANSCODE (job));
	return PARRILLADA_BURN_OK;
}

/* we must make sure that the track size is a multiple
 * of 2352 to be burnt by cdrecord with on the fly */

static gint64
parrillada_transcode_pad_real (ParrilladaTranscode *transcode,
			    int fd,
			    gint64 bytes2write,
			    GError **error)
{
	const int buffer_size = 512;
	char buffer [buffer_size];
	gint64 b_written;
	gint64 size;

	b_written = 0;
	bzero (buffer, sizeof (buffer));
	for (; bytes2write; bytes2write -= b_written) {
		size = bytes2write > buffer_size ? buffer_size : bytes2write;
		b_written = write (fd, buffer, (int) size);

		PARRILLADA_JOB_LOG (transcode,
				 "written %" G_GINT64_FORMAT " bytes for padding",
				 b_written);

		/* we should not handle EINTR and EAGAIN as errors */
		if (b_written < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				PARRILLADA_JOB_LOG (transcode, "got EINTR / EAGAIN, retrying");
	
				/* we'll try later again */
				return bytes2write;
			}
		}

		if (size != b_written) {
                        int errsv = errno;

			g_set_error (error,
				     PARRILLADA_BURN_ERROR,
				     PARRILLADA_BURN_ERROR_GENERAL,
				     /* Translators: %s is the string error from errno */
				     _("Error while padding file (%s)"),
				     g_strerror (errsv));
			return -1;
		}
	}

	return 0;
}

static void
parrillada_transcode_push_track (ParrilladaTranscode *transcode)
{
	guint64 length = 0;
	gchar *output = NULL;
	ParrilladaTrack *src = NULL;
	ParrilladaTrackStream *track;

	parrillada_job_get_audio_output (PARRILLADA_JOB (transcode), &output);
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &src);

	parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (src), &length);

	track = parrillada_track_stream_new ();
	parrillada_track_stream_set_source (track, output);
	g_free (output);

	/* FIXME: what if input had metadata ?*/
	parrillada_track_stream_set_format (track, PARRILLADA_AUDIO_FORMAT_RAW);
	parrillada_track_stream_set_boundaries (track, 0, length, 0);
	parrillada_track_tag_copy_missing (PARRILLADA_TRACK (track), src);

	parrillada_job_add_track (PARRILLADA_JOB (transcode), PARRILLADA_TRACK (track));
	
	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. ParrilladaTaskCtx refs it. */
	g_object_unref (track);

	parrillada_job_finished_track (PARRILLADA_JOB (transcode));
}

static gboolean
parrillada_transcode_pad_idle (ParrilladaTranscode *transcode)
{
	gint64 bytes2write;
	GError *error = NULL;
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);
	bytes2write = parrillada_transcode_pad_real (transcode,
						  priv->pad_fd,
						  priv->pad_size,
						  &error);

	if (bytes2write == -1) {
		priv->pad_id = 0;
		parrillada_job_error (PARRILLADA_JOB (transcode), error);
		return FALSE;
	}

	if (bytes2write) {
		priv->pad_size = bytes2write;
		return TRUE;
	}

	/* we are finished with padding */
	priv->pad_id = 0;
	close (priv->pad_fd);
	priv->pad_fd = -1;

	/* set the next song or finish */
	parrillada_transcode_push_track (transcode);
	return FALSE;
}

static gboolean
parrillada_transcode_pad (ParrilladaTranscode *transcode, int fd, GError **error)
{
	guint64 length = 0;
	gint64 bytes2write = 0;
	ParrilladaTrack *track = NULL;
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);
	if (priv->pos < 0)
		return TRUE;

	/* Padding is important for two reasons:
	 * - first if didn't output enough bytes compared to what we should have
	 * - second we must output a multiple of 2352 to respect sector
	 *   boundaries */
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	parrillada_track_stream_get_length (PARRILLADA_TRACK_STREAM (track), &length);

	if (priv->pos < PARRILLADA_DURATION_TO_BYTES (length)) {
		gint64 b_written = 0;

		/* Check bytes boundary for length */
		b_written = PARRILLADA_DURATION_TO_BYTES (length);
		b_written += (b_written % 2352) ? 2352 - (b_written % 2352):0;
		bytes2write = b_written - priv->pos;

		PARRILLADA_JOB_LOG (transcode,
				 "wrote %lli bytes (= %lli ns) out of %lli (= %lli ns)"
				 "\n=> padding %lli bytes",
				 priv->pos,
				 PARRILLADA_BYTES_TO_DURATION (priv->pos),
				 PARRILLADA_DURATION_TO_BYTES (length),
				 length,
				 bytes2write);
	}
	else {
		gint64 b_written = 0;

		/* wrote more or the exact amount of bytes. Check bytes boundary */
		b_written = priv->pos;
		bytes2write = (b_written % 2352) ? 2352 - (b_written % 2352):0;
		PARRILLADA_JOB_LOG (transcode,
				 "wrote %lli bytes (= %lli ns)"
				 "\n=> padding %lli bytes",
				 b_written,
				 priv->pos,
				 bytes2write);
	}

	if (!bytes2write)
		return TRUE;

	bytes2write = parrillada_transcode_pad_real (transcode,
						  fd,
						  bytes2write,
						  error);
	if (bytes2write == -1)
		return TRUE;

	if (bytes2write) {
		ParrilladaTranscodePrivate *priv;

		priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);
		/* when writing to a pipe it can happen that its buffer is full
		 * because cdrecord is not fast enough. Therefore we couldn't
		 * write/pad it and we'll have to wait for the pipe to become
		 * available again */
		priv->pad_fd = fd;
		priv->pad_size = bytes2write;
		priv->pad_id = g_timeout_add (50,
					     (GSourceFunc) parrillada_transcode_pad_idle,
					      transcode);
		return FALSE;		
	}

	return TRUE;
}

static gboolean
parrillada_transcode_pad_pipe (ParrilladaTranscode *transcode, GError **error)
{
	int fd;
	gboolean result;

	parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), &fd);
	fd = dup (fd);

	result = parrillada_transcode_pad (transcode, fd, error);
	if (result)
		close (fd);

	return result;
}

static gboolean
parrillada_transcode_pad_file (ParrilladaTranscode *transcode, GError **error)
{
	int fd;
	gchar *output;
	gboolean result;

	output = NULL;
	parrillada_job_get_audio_output (PARRILLADA_JOB (transcode), &output);
	fd = open (output, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IROTH);
	g_free (output);

	if (fd == -1) {
                int errsv = errno;

		g_set_error (error,
			     PARRILLADA_BURN_ERROR,
			     PARRILLADA_BURN_ERROR_GENERAL,
			     /* Translators: %s is the string error from errno */
			     _("Error while padding file (%s)"),
			     g_strerror (errsv));
		return FALSE;
	}

	result = parrillada_transcode_pad (transcode, fd, error);
	if (result)
		close (fd);

	return result;
}

static gboolean
parrillada_transcode_is_mp3 (ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	GstElement *typefind;
	GstCaps *caps = NULL;
	const gchar *mime;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	/* find the type of the file */
	typefind = gst_bin_get_by_name (GST_BIN (priv->decode), "typefind");
	g_object_get (typefind, "caps", &caps, NULL);
	if (!caps) {
		gst_object_unref (typefind);
		return TRUE;
	}

	if (caps && gst_caps_get_size (caps) > 0) {
		mime = gst_structure_get_name (gst_caps_get_structure (caps, 0));
		gst_object_unref (typefind);

		if (mime && !strcmp (mime, "application/x-id3"))
			return TRUE;

		if (!strcmp (mime, "audio/mpeg"))
			return TRUE;
	}
	else
		gst_object_unref (typefind);

	return FALSE;
}

static gint64
parrillada_transcode_get_duration (ParrilladaTranscode *transcode)
{
	gint64 duration = -1;
	ParrilladaTranscodePrivate *priv;
	GstFormat format = GST_FORMAT_TIME;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	/* This part is specific to MP3s */
	if (priv->mp3_size_pipeline) {
		/* This is the most reliable way to get the duration for mp3
		 * read them till the end and get the position. */
		gst_element_query_position (priv->pipeline,
					    &format,
					    &duration);
	}

	/* This is for any sort of files */
	if (duration == -1 || duration == 0)
		gst_element_query_duration (priv->pipeline,
					    &format,
					    &duration);

	PARRILLADA_JOB_LOG (transcode, "got duration %"G_GINT64_FORMAT, duration);

	if (duration == -1 || duration == 0)	
	    parrillada_job_error (PARRILLADA_JOB (transcode),
			       g_error_new (PARRILLADA_BURN_ERROR,
					    PARRILLADA_BURN_ERROR_GENERAL,
					    _("Error while getting duration")));
	return duration;
}

static gboolean
parrillada_transcode_song_end_reached (ParrilladaTranscode *transcode)
{
	GError *error = NULL;
	ParrilladaJobAction action;

	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		gint64 duration;

		/* this is when we need to write infs:
		 * - when asked to create infs
		 * - when decoding to a file */
		duration = parrillada_transcode_get_duration (transcode);
		if (duration == -1)
			return FALSE;

		parrillada_transcode_set_track_size (transcode, duration);
		parrillada_job_finished_track (PARRILLADA_JOB (transcode));
		return TRUE;
	}

	if (action == PARRILLADA_JOB_ACTION_IMAGE) {
		gboolean result;

		/* pad file so it is a multiple of 2352 (= 1 sector) */
		if (parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), NULL) == PARRILLADA_BURN_OK)
			result = parrillada_transcode_pad_pipe (transcode, &error);
		else
			result = parrillada_transcode_pad_file (transcode, &error);
	
		if (error) {
			parrillada_job_error (PARRILLADA_JOB (transcode), error);
			return FALSE;
		}

		if (!result) {
			parrillada_transcode_stop_pipeline (transcode);
			return FALSE;
		}
	}

	parrillada_transcode_push_track (transcode);
	return TRUE;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     ParrilladaTranscode *transcode)
{
	ParrilladaTrack *track;
	ParrilladaJobAction action;

	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);
	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);

	PARRILLADA_JOB_LOG (transcode, "Retrieving tags");

	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (!parrillada_track_tag_lookup_string (track, PARRILLADA_TRACK_STREAM_TITLE_TAG)) {
			gchar *title = NULL;

			gst_tag_list_get_string (list, tag, &title);
			parrillada_track_tag_add_string (track,
						      PARRILLADA_TRACK_STREAM_TITLE_TAG,
						      title);
			g_free (title);
		}
	}
	else if (!strcmp (tag, GST_TAG_ARTIST)) {
		if (!parrillada_track_tag_lookup_string (track, PARRILLADA_TRACK_STREAM_ARTIST_TAG)) {
			gchar *artist = NULL;

			gst_tag_list_get_string (list, tag, &artist);
			parrillada_track_tag_add_string (track,
						      PARRILLADA_TRACK_STREAM_ARTIST_TAG,
						      artist);
			g_free (artist);
		}
	}
	else if (!strcmp (tag, GST_TAG_ISRC)) {
		if (!parrillada_track_tag_lookup_int (track, PARRILLADA_TRACK_STREAM_ISRC_TAG)) {
			gchar *isrc = NULL;

			gst_tag_list_get_string (list, tag, &isrc);
			parrillada_track_tag_add_int (track,
						   PARRILLADA_TRACK_STREAM_ISRC_TAG,
						   (int) g_ascii_strtoull (isrc, NULL, 10));
		}
	}
	else if (!strcmp (tag, GST_TAG_PERFORMER)) {
		if (!parrillada_track_tag_lookup_string (track, PARRILLADA_TRACK_STREAM_ARTIST_TAG)) {
			gchar *artist = NULL;

			gst_tag_list_get_string (list, tag, &artist);
			parrillada_track_tag_add_string (track,
						      PARRILLADA_TRACK_STREAM_ARTIST_TAG,
						      artist);
			g_free (artist);
		}
	}
	else if (action == PARRILLADA_JOB_ACTION_SIZE
	     &&  !strcmp (tag, GST_TAG_DURATION)) {
		guint64 duration;

		/* this is only useful when we try to have the size */
		gst_tag_list_get_uint64 (list, tag, &duration);
		parrillada_track_stream_set_boundaries (PARRILLADA_TRACK_STREAM (track), 0, duration, -1);
	}
}

/* NOTE: the return value is whether or not we should stop the bus callback */
static gboolean
parrillada_transcode_active_state (ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	gchar *name, *string, *uri;
	ParrilladaJobAction action;
	ParrilladaTrack *track;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	if (priv->set_active_state)
		return TRUE;

	priv->set_active_state = TRUE;

	parrillada_job_get_current_track (PARRILLADA_JOB (transcode), &track);
	uri = parrillada_track_stream_get_source (PARRILLADA_TRACK_STREAM (track), FALSE);

	parrillada_job_get_action (PARRILLADA_JOB (transcode), &action);
	if (action == PARRILLADA_JOB_ACTION_SIZE) {
		PARRILLADA_JOB_LOG (transcode,
				 "Analysing Track %s",
				 uri);

		if (priv->mp3_size_pipeline) {
			gchar *escaped_basename;

			/* Run the pipeline till the end */
			escaped_basename = g_path_get_basename (uri);
			name = g_uri_unescape_string (escaped_basename, NULL);
			g_free (escaped_basename);

			string = g_strdup_printf (_("Analysing \"%s\""), name);
			g_free (name);
		
			parrillada_job_set_current_action (PARRILLADA_JOB (transcode),
							PARRILLADA_BURN_ACTION_ANALYSING,
							string,
							TRUE);
			g_free (string);

			parrillada_job_start_progress (PARRILLADA_JOB (transcode), FALSE);
			g_free (uri);
			return TRUE;
		}

		if (parrillada_transcode_is_mp3 (transcode)) {
			GError *error = NULL;

			/* Rebuild another pipeline which is specific to MP3s. */
			priv->mp3_size_pipeline = TRUE;
			parrillada_transcode_stop_pipeline (transcode);

			if (!parrillada_transcode_create_pipeline (transcode, &error))
				parrillada_job_error (PARRILLADA_JOB (transcode), error);
		}
		else
			parrillada_transcode_song_end_reached (transcode);

		g_free (uri);
		return FALSE;
	}
	else {
		gchar *escaped_basename;

		escaped_basename = g_path_get_basename (uri);
		name = g_uri_unescape_string (escaped_basename, NULL);
		g_free (escaped_basename);

		string = g_strdup_printf (_("Transcoding \"%s\""), name);
		g_free (name);

		parrillada_job_set_current_action (PARRILLADA_JOB (transcode),
						PARRILLADA_BURN_ACTION_TRANSCODING,
						string,
						TRUE);
		g_free (string);
		parrillada_job_start_progress (PARRILLADA_JOB (transcode), FALSE);

		if (parrillada_job_get_fd_out (PARRILLADA_JOB (transcode), NULL) != PARRILLADA_BURN_OK) {
			gchar *dest = NULL;

			parrillada_job_get_audio_output (PARRILLADA_JOB (transcode), &dest);
			PARRILLADA_JOB_LOG (transcode,
					 "start decoding %s to %s",
					 uri,
					 dest);
			g_free (dest);
		}
		else
			PARRILLADA_JOB_LOG (transcode,
					 "start piping %s",
					 uri)
	}

	g_free (uri);
	return TRUE;
}

static gboolean
parrillada_transcode_bus_messages (GstBus *bus,
				GstMessage *msg,
				ParrilladaTranscode *transcode)
{
	ParrilladaTranscodePrivate *priv;
	GstTagList *tags = NULL;
	GError *error = NULL;
	GstState state;
	gchar *debug;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_TAG:
		/* we use the information to write an .inf file 
		 * for the time being just store the information */
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, transcode);
		gst_tag_list_free (tags);
		return TRUE;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		PARRILLADA_JOB_LOG (transcode, debug);
		g_free (debug);

	        parrillada_job_error (PARRILLADA_JOB (transcode), error);
		return FALSE;

	case GST_MESSAGE_EOS:
		parrillada_transcode_song_end_reached (transcode);
		return FALSE;

	case GST_MESSAGE_STATE_CHANGED: {
		GstStateChangeReturn result;

		result = gst_element_get_state (priv->pipeline,
						&state,
						NULL,
						1);

		if (result != GST_STATE_CHANGE_SUCCESS)
			return TRUE;

		if (state == GST_STATE_PLAYING)
			return parrillada_transcode_active_state (transcode);

		break;
	}

	default:
		return TRUE;
	}

	return TRUE;
}

static void
parrillada_transcode_new_decoded_pad_cb (GstElement *decode,
				      GstPad *pad,
				      gboolean arg2,
				      ParrilladaTranscode *transcode)
{
	GstCaps *caps;
	GstStructure *structure;
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (transcode);

	PARRILLADA_JOB_LOG (transcode, "New pad");

	/* make sure we only have audio */
	caps = gst_pad_get_caps (pad);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure) {
		if (g_strrstr (gst_structure_get_name (structure), "audio")) {
			GstPad *sink;
			GstElement *queue;
			GstPadLinkReturn res;

			/* before linking pads (before any data reach grvolume), send tags */
			parrillada_transcode_send_volume_event (transcode);

			/* This is necessary in case there is a video stream
			 * (see parrillada-metadata.c). we need to queue to avoid
			 * a deadlock. */
			queue = gst_element_factory_make ("queue", NULL);
			gst_bin_add (GST_BIN (priv->pipeline), queue);
			if (!gst_element_link (queue, priv->link)) {
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");
				goto end;
			}

			sink = gst_element_get_pad (queue, "sink");
			if (GST_PAD_IS_LINKED (sink)) {
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");
				goto end;
			}

			res = gst_pad_link (pad, sink);
			if (res == GST_PAD_LINK_OK)
				gst_element_set_state (queue, GST_STATE_PLAYING);
			else
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");

			gst_object_unref (sink);
		}
		/* For video streams add a fakesink (see parrillada-metadata.c) */
		else if (g_strrstr (gst_structure_get_name (structure), "video")) {
			GstPad *sink;
			GstElement *fakesink;
			GstPadLinkReturn res;

			PARRILLADA_JOB_LOG (transcode, "Adding a fakesink for video stream");

			fakesink = gst_element_factory_make ("fakesink", NULL);
			if (!fakesink) {
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");
				goto end;
			}

			sink = gst_element_get_static_pad (fakesink, "sink");
			if (!sink) {
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");
				gst_object_unref (fakesink);
				goto end;
			}

			gst_bin_add (GST_BIN (priv->pipeline), fakesink);
			res = gst_pad_link (pad, sink);

			if (res == GST_PAD_LINK_OK)
				gst_element_set_state (fakesink, GST_STATE_PLAYING);
			else
				parrillada_transcode_error_on_pad_linking (transcode, "Sent by parrillada_transcode_new_decoded_pad_cb");

			gst_object_unref (sink);
		}
	}

end:
	gst_caps_unref (caps);
}

static ParrilladaBurnResult
parrillada_transcode_clock_tick (ParrilladaJob *job)
{
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (job);

	if (!priv->pipeline)
		return PARRILLADA_BURN_ERR;

	parrillada_job_set_written_track (job, priv->pos);
	return PARRILLADA_BURN_OK;
}

static void
parrillada_transcode_class_init (ParrilladaTranscodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ParrilladaJobClass *job_class = PARRILLADA_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaTranscodePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = parrillada_transcode_finalize;

	job_class->start = parrillada_transcode_start;
	job_class->clock_tick = parrillada_transcode_clock_tick;
	job_class->stop = parrillada_transcode_stop;
}

static void
parrillada_transcode_init (ParrilladaTranscode *obj)
{ }

static void
parrillada_transcode_finalize (GObject *object)
{
	ParrilladaTranscodePrivate *priv;

	priv = PARRILLADA_TRANSCODE_PRIVATE (object);

	if (priv->pad_id) {
		g_source_remove (priv->pad_id);
		priv->pad_id = 0;
	}

	parrillada_transcode_stop_pipeline (PARRILLADA_TRANSCODE (object));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parrillada_transcode_export_caps (ParrilladaPlugin *plugin)
{
	GSList *input;
	GSList *output;

	parrillada_plugin_define (plugin,
			       "transcode",
	                       NULL,
			       _("Converts any song file into a format suitable for audio CDs"),
			       "Philippe Rouquier",
			       1);

	output = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					 PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					 PARRILLADA_AUDIO_FORMAT_RAW|
					 PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
					 PARRILLADA_METADATA_INFO);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_UNDEFINED|
					PARRILLADA_METADATA_INFO);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_DTS|
					PARRILLADA_METADATA_INFO);
	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	output = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE|
					 PARRILLADA_PLUGIN_IO_ACCEPT_PIPE,
					 PARRILLADA_AUDIO_FORMAT_RAW|
					 PARRILLADA_AUDIO_FORMAT_RAW_LITTLE_ENDIAN);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_UNDEFINED);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = parrillada_caps_audio_new (PARRILLADA_PLUGIN_IO_ACCEPT_FILE,
					PARRILLADA_AUDIO_FORMAT_DTS);

	parrillada_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);
}

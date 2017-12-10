/***************************************************************************
 *            player-bacon.c
 *
 *  ven déc 30 11:29:33 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Parrillada is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Parrillada is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
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

#include <gdk/gdkx.h>

#include <gtk/gtk.h>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include "parrillada-player-bacon.h"
#include "parrillada-setting.h"


struct ParrilladaPlayerBaconPrivate {
	GstElement *pipe;
	GstState state;

	GstVideoOverlay *xoverlay;
	XID xid;

	gchar *uri;
	gint64 end;
};

enum {
	PROP_NONE,
	PROP_URI
};

typedef enum {
	STATE_CHANGED_SIGNAL,
	EOF_SIGNAL,
	LAST_SIGNAL
} ParrilladaPlayerBaconSignalType;

static guint parrillada_player_bacon_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ParrilladaPlayerBacon, parrillada_player_bacon, GTK_TYPE_WIDGET)

static void
parrillada_player_bacon_set_property (GObject *obj,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	ParrilladaPlayerBacon *bacon;
	const char *uri;

	bacon = PARRILLADA_PLAYER_BACON (obj);

	switch (prop_id) {
	case PROP_URI:
		uri = g_value_get_string (value);
		parrillada_player_bacon_set_uri (bacon, uri);
		break;

	default:
		break;
	}	
}

static void
parrillada_player_bacon_get_property (GObject *obj,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	ParrilladaPlayerBacon *bacon;

	bacon = PARRILLADA_PLAYER_BACON (obj);
	switch (prop_id) {
	case PROP_URI:
		g_value_set_string (value, bacon->priv->uri);
		break;

	default:
		break;
	}
}

static void
parrillada_player_bacon_unrealize (GtkWidget *widget)
{
	ParrilladaPlayerBacon *bacon;

	bacon = PARRILLADA_PLAYER_BACON (widget);

	/* Stop the pipeline as otherwise it would try to write video to a destroyed window */
	gst_element_set_state (bacon->priv->pipe, GST_STATE_READY);
	bacon->priv->xid = 0;

	if (GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->unrealize)
		GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->unrealize (widget);
}

static void
parrillada_player_bacon_realize (GtkWidget *widget)
{
	GdkWindow *window;
	gint attributes_mask;
	GtkAllocation allocation;
	GdkWindowAttr attributes;
	gfloat screen_width, screen_height;

	attributes.window_type = GDK_WINDOW_CHILD;

	gtk_widget_get_allocation (widget, &allocation);

	screen_width = allocation.width;
	screen_height = allocation.height;

	attributes.x = allocation.x + (allocation.width - (gint) screen_width) / 2;
	attributes.y = allocation.y + (allocation.height - (gint) screen_height) / 2;
	attributes.width = screen_width;
	attributes.height = screen_height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK|
				 GDK_BUTTON_PRESS_MASK|
				 GDK_BUTTON_RELEASE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	gtk_widget_set_window (widget, gdk_window_new (gtk_widget_get_parent_window (widget),
						       &attributes,
						       attributes_mask));
	window = gtk_widget_get_window (widget);
	gdk_window_set_user_data (window, widget);
	gdk_window_show (gtk_widget_get_window (widget));
	gtk_widget_set_realized (widget, TRUE);
}

static gboolean
parrillada_player_bacon_draw (GtkWidget *widget, cairo_t *cr)
{
	ParrilladaPlayerBacon *bacon;
	GdkWindow *window;

	g_return_val_if_fail (widget != NULL, FALSE);

	bacon = PARRILLADA_PLAYER_BACON (widget);

	window = gtk_widget_get_window (widget);
	if (window)
		bacon->priv->xid = gdk_x11_window_get_xid (window);

	if (bacon->priv->xoverlay
	&&  GST_IS_VIDEO_OVERLAY (bacon->priv->xoverlay)
	&&  bacon->priv->state >= GST_STATE_PAUSED)
		gst_video_overlay_expose (bacon->priv->xoverlay);
	else if (window)
		gtk_widget_queue_draw (GTK_WIDGET (widget));

	return TRUE;
}

static void
parrillada_player_bacon_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
	g_return_if_fail (widget != NULL);

	*minimum = *natural = PLAYER_BACON_WIDTH;

	if (GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->get_preferred_width)
		GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->get_preferred_width (widget, minimum, natural);
}

static void
parrillada_player_bacon_get_preferred_height (GtkWidget *widget,
                                           gint      *minimum,
                                          gint      *natural)
{
	g_return_if_fail (widget != NULL);

	*minimum = *natural = PLAYER_BACON_WIDTH;

	if (GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->get_preferred_height)
		GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->get_preferred_height (widget, minimum, natural);
}

static void
parrillada_player_bacon_size_allocate (GtkWidget *widget,
                                    GtkAllocation *allocation)
{
	int screen_x, screen_y;
	ParrilladaPlayerBacon *bacon;
	gfloat screen_width, screen_height, ratio;

	g_return_if_fail (widget != NULL);

	if (!gtk_widget_get_realized (widget))
		return;

	bacon = PARRILLADA_PLAYER_BACON (widget);
	if (bacon->priv->xoverlay) {
		screen_width = allocation->width;
		screen_height = allocation->height;
	
		if ((gfloat) screen_width / PLAYER_BACON_WIDTH > 
		    (gfloat) screen_height / PLAYER_BACON_HEIGHT)
			ratio = (gfloat) screen_height / PLAYER_BACON_HEIGHT;
		else
			ratio = (gfloat) screen_width / PLAYER_BACON_WIDTH;
		
		screen_width = (gdouble) PLAYER_BACON_WIDTH * ratio;
		screen_height = (gdouble) PLAYER_BACON_HEIGHT * ratio;
		screen_x = allocation->x + (allocation->width - (gint) screen_width) / 2;
		screen_y = allocation->y + (allocation->height - (gint) screen_height) / 2;
	
		gdk_window_move_resize (gtk_widget_get_window (widget),
					screen_x,
					screen_y,
					(gint) screen_width,
					(gint) screen_height);
		gtk_widget_set_allocation (widget, allocation);
	}
	else if (GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->size_allocate)
		GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->size_allocate (widget, allocation);
}

/* FIXME: we could get rid of this by setting the XID directly on playbin
 * right after creation, since it proxies the video overlay interface now */
static GstBusSyncReply
parrillada_player_bacon_bus_messages_handler (GstBus *bus,
					   GstMessage *message,
					   ParrilladaPlayerBacon *bacon)
{
	if (!gst_is_video_overlay_prepare_window_handle_message (message))
		return GST_BUS_PASS;

	/* NOTE: apparently GDK does not like to be asked to retrieve the XID
	 * in a thread so we do it in the callback of the expose event. */
	bacon->priv->xoverlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
	gst_video_overlay_set_window_handle (bacon->priv->xoverlay,
	                                     bacon->priv->xid);

	return GST_BUS_DROP;
}

#if 0
static void
parrillada_player_bacon_clear_pipe (ParrilladaPlayerBacon *bacon)
{
	bacon->priv->xoverlay = NULL;

	bacon->priv->state = GST_STATE_NULL;

	if (bacon->priv->pipe) {
		GstBus *bus;

		bus = gst_pipeline_get_bus (GST_PIPELINE (bacon->priv->pipe));
		gst_bus_set_flushing (bus, TRUE);
		gst_object_unref (bus);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
		gst_object_unref (bacon->priv->pipe);
		bacon->priv->pipe = NULL;
	}

	if (bacon->priv->uri) {
		g_free (bacon->priv->uri);
		bacon->priv->uri = NULL;
	}
}
#endif

void
parrillada_player_bacon_set_uri (ParrilladaPlayerBacon *bacon, const gchar *uri)
{
	if (bacon->priv->uri) {
		g_free (bacon->priv->uri);
		bacon->priv->uri = NULL;
	}

	if (uri) {
		bacon->priv->uri = g_strdup (uri);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
		bacon->priv->state = GST_STATE_NULL;

		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "uri", uri,
			      NULL);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_PAUSED);
	}
	else if (bacon->priv->pipe)
		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
}

gboolean
parrillada_player_bacon_set_boundaries (ParrilladaPlayerBacon *bacon, gint64 start, gint64 end)
{
	if (!bacon->priv->pipe)
		return FALSE;

	bacon->priv->end = end;
	return gst_element_seek (bacon->priv->pipe,
				 1.0,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH|
				 GST_SEEK_FLAG_ACCURATE,
				 GST_SEEK_TYPE_SET,
				 start,
				 GST_SEEK_TYPE_SET,
				 end);
}

void
parrillada_player_bacon_set_volume (ParrilladaPlayerBacon *bacon,
                                 gdouble volume)
{
	if (!bacon->priv->pipe)
		return;

	volume = CLAMP (volume, 0, 1.0);
	g_object_set (bacon->priv->pipe,
		      "volume", volume,
		      NULL);
}

gdouble
parrillada_player_bacon_get_volume (ParrilladaPlayerBacon *bacon)
{
	gdouble volume;

	if (!bacon->priv->pipe)
		return -1.0;

	g_object_get (bacon->priv->pipe,
		      "volume", &volume,
		      NULL);

	return volume;
}

static gboolean
parrillada_player_bacon_bus_messages (GstBus *bus,
				   GstMessage *msg,
				   ParrilladaPlayerBacon *bacon)
{
	ParrilladaPlayerBaconState value;
	GstStateChangeReturn result;
	GError *error = NULL;
	GstState state;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_signal_emit (bacon,
			       parrillada_player_bacon_signals [EOF_SIGNAL],
			       0);
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, NULL);
		g_warning ("%s", error->message);

		g_signal_emit (bacon,
			       parrillada_player_bacon_signals [STATE_CHANGED_SIGNAL],
			       0,
			       BACON_STATE_ERROR);
		gtk_widget_queue_resize (GTK_WIDGET (bacon));
		break;

	case GST_MESSAGE_STATE_CHANGED:
		result = gst_element_get_state (GST_ELEMENT (bacon->priv->pipe),
						&state,
						NULL,
						500);

		if (result != GST_STATE_CHANGE_SUCCESS)
			break;

		if (bacon->priv->state == state || state < GST_STATE_PAUSED)
			break;

		if (state == GST_STATE_PLAYING)
			value = BACON_STATE_PLAYING;
		else
			value = BACON_STATE_PAUSED;
				
		if (bacon->priv->xoverlay)
			gtk_widget_queue_resize (GTK_WIDGET (bacon));

		bacon->priv->state = state;
		g_signal_emit (bacon,
			       parrillada_player_bacon_signals [STATE_CHANGED_SIGNAL],
			       0,
			       value);
		break;

	default:
		break;
	}

	return TRUE;
}

gboolean
parrillada_player_bacon_play (ParrilladaPlayerBacon *bacon)
{
	GstStateChangeReturn result;

	if (!bacon->priv->pipe)
		return FALSE;

	result = gst_element_set_state (bacon->priv->pipe, GST_STATE_PLAYING);
	return (result != GST_STATE_CHANGE_FAILURE);
}

gboolean
parrillada_player_bacon_stop (ParrilladaPlayerBacon *bacon)
{
	GstStateChangeReturn result;

	if (!bacon->priv->pipe)
		return FALSE;

	result = gst_element_set_state (bacon->priv->pipe, GST_STATE_PAUSED);
	return (result != GST_STATE_CHANGE_FAILURE);
}

gboolean
parrillada_player_bacon_forward (ParrilladaPlayerBacon *bacon,
                              gint64 pos)
{
	gint64 cur;

	if (!bacon->priv->pipe)
		return FALSE;

	if (!gst_element_query_position (bacon->priv->pipe, GST_FORMAT_TIME, &cur))
		return FALSE;

	return gst_element_seek_simple (bacon->priv->pipe,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH,
				 cur + pos);
}

gboolean
parrillada_player_bacon_backward (ParrilladaPlayerBacon *bacon,
                               gint64 pos)
{
	gint64 cur;

	if (!bacon->priv->pipe)
		return FALSE;

	if (!gst_element_query_position (bacon->priv->pipe, GST_FORMAT_TIME, &cur))
		return FALSE;

	return gst_element_seek_simple (bacon->priv->pipe,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH,
				 MAX (0, cur - pos));
}

gboolean
parrillada_player_bacon_set_pos (ParrilladaPlayerBacon *bacon,
			      gdouble pos)
{
	if (!bacon->priv->pipe)
		return FALSE;

	return gst_element_seek (bacon->priv->pipe,
				 1.0,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH,
				 GST_SEEK_TYPE_SET,
				 (gint64) pos,
				 bacon->priv->end ? GST_SEEK_TYPE_SET:GST_SEEK_TYPE_NONE,
				 bacon->priv->end);
}

gboolean
parrillada_player_bacon_get_pos (ParrilladaPlayerBacon *bacon,
			      gint64 *pos)
{
	gboolean result;
	gint64 value;

	if (!bacon->priv->pipe)
		return FALSE;

	if (pos) {
		result = gst_element_query_position (bacon->priv->pipe,
						     GST_FORMAT_TIME,
						     &value);
		if (!result)
			return FALSE;

		*pos = value;
	}

	return TRUE;
}

static void
parrillada_player_bacon_setup_pipe (ParrilladaPlayerBacon *bacon)
{
	GstElement *video_sink, *audio_sink;
	GstBus *bus = NULL;
	gdouble volume;
	gpointer value;

	bacon->priv->pipe = gst_element_factory_make ("playbin", NULL);
	if (!bacon->priv->pipe) {
		g_warning ("Pipe creation error : can't create pipe.\n");
		return;
	}

	audio_sink = gst_element_factory_make ("gsettingsaudiosink", NULL);
	if (audio_sink)
		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "audio-sink", audio_sink,
			      NULL);
/* FIXME: fall back on autoaudiosink for now, since gsettings plugin is
 * not ported yet */
#if 0
	else
		goto error;
#endif

	video_sink = gst_element_factory_make ("gsettingsvideosink", NULL);
	if (video_sink) {
		GstElement *element;

		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "video-sink", video_sink,
			      NULL);

		element = gst_bin_get_by_interface (GST_BIN (video_sink),
						    GST_TYPE_VIDEO_OVERLAY);
		if (element && GST_IS_VIDEO_OVERLAY (element))
			bacon->priv->xoverlay = GST_VIDEO_OVERLAY (element);
	}

	bus = gst_pipeline_get_bus (GST_PIPELINE (bacon->priv->pipe));
	/* FIXME: we should just set the XID directly on playbin right after
	 * creation, since it proxies the video overlay interface now, and get
	 * rid of the sync message handler and the stuff above */
	gst_bus_set_sync_handler (bus,
				  (GstBusSyncHandler) parrillada_player_bacon_bus_messages_handler,
				  bacon, NULL);
	gst_bus_add_watch (bus,
			   (GstBusFunc) parrillada_player_bacon_bus_messages,
			   bacon);
	gst_object_unref (bus);

	/* set saved volume */
	parrillada_setting_get_value (parrillada_setting_get_default (),
	                           PARRILLADA_SETTING_PLAYER_VOLUME,
	                           &value);
	volume = GPOINTER_TO_INT (value);
	volume = CLAMP (volume, 0, 100);
	g_object_set (bacon->priv->pipe,
		      "volume", (gdouble) volume / 100.0,
		      NULL);

	return;

#if 0
error:
	g_message ("player creation error");
	parrillada_player_bacon_clear_pipe (bacon);
	g_signal_emit (bacon,
		       parrillada_player_bacon_signals [STATE_CHANGED_SIGNAL],
		       0,
		       BACON_STATE_ERROR);
	gtk_widget_queue_resize (GTK_WIDGET (bacon));
#endif
}

static void
parrillada_player_bacon_init (ParrilladaPlayerBacon *obj)
{
	gtk_widget_set_double_buffered (GTK_WIDGET (obj), FALSE);

	obj->priv = g_new0 (ParrilladaPlayerBaconPrivate, 1);
	parrillada_player_bacon_setup_pipe (obj);
}

static void
parrillada_player_bacon_destroy (GtkWidget *obj)
{
	ParrilladaPlayerBacon *cobj;

	cobj = PARRILLADA_PLAYER_BACON (obj);

	/* save volume */
	if (cobj->priv->pipe) {
		gdouble volume;

		g_object_get (cobj->priv->pipe,
			      "volume", &volume,
			      NULL);
		parrillada_setting_set_value (parrillada_setting_get_default (),
		                           PARRILLADA_SETTING_PLAYER_VOLUME,
		                           GINT_TO_POINTER ((gint)(volume * 100)));
	}

	if (cobj->priv->xoverlay
	&&  GST_IS_VIDEO_OVERLAY (cobj->priv->xoverlay))
		cobj->priv->xoverlay = NULL;

	if (cobj->priv->pipe) {
		gst_element_set_state (cobj->priv->pipe, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (cobj->priv->pipe));
		cobj->priv->pipe = NULL;
	}

	if (cobj->priv->uri) {
		g_free (cobj->priv->uri);
		cobj->priv->uri = NULL;
	}

	if (GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->destroy)
		GTK_WIDGET_CLASS (parrillada_player_bacon_parent_class)->destroy (obj);
}

static void
parrillada_player_bacon_finalize (GObject *object)
{
	ParrilladaPlayerBacon *cobj;

	cobj = PARRILLADA_PLAYER_BACON (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parrillada_player_bacon_parent_class)->finalize (object);
}

static void
parrillada_player_bacon_class_init (ParrilladaPlayerBaconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = parrillada_player_bacon_finalize;
	object_class->set_property = parrillada_player_bacon_set_property;
	object_class->get_property = parrillada_player_bacon_get_property;

	widget_class->destroy = parrillada_player_bacon_destroy;
	widget_class->draw = parrillada_player_bacon_draw;
	widget_class->realize = parrillada_player_bacon_realize;
	widget_class->unrealize = parrillada_player_bacon_unrealize;
        widget_class->get_preferred_width = parrillada_player_bacon_get_preferred_width;
        widget_class->get_preferred_height = parrillada_player_bacon_get_preferred_height;
	widget_class->size_allocate = parrillada_player_bacon_size_allocate;

	parrillada_player_bacon_signals [STATE_CHANGED_SIGNAL] = 
			g_signal_new ("state-change",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (ParrilladaPlayerBaconClass, state_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);

	parrillada_player_bacon_signals [EOF_SIGNAL] = 
			g_signal_new ("eof",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (ParrilladaPlayerBaconClass, eof),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

	g_object_class_install_property (object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "The uri of the media",
							      "The uri of the media",
							      NULL,
							      G_PARAM_READWRITE));
}

GtkWidget *
parrillada_player_bacon_new (void)
{
	ParrilladaPlayerBacon *obj;
	
	obj = PARRILLADA_PLAYER_BACON (g_object_new (PARRILLADA_TYPE_PLAYER_BACON, NULL));
	
	return GTK_WIDGET (obj);
}

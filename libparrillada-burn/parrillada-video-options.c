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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "parrillada-misc.h"
#include "parrillada-tags.h"
#include "parrillada-session.h"
#include "parrillada-session-helper.h"
#include "parrillada-video-options.h"

typedef struct _ParrilladaVideoOptionsPrivate ParrilladaVideoOptionsPrivate;
struct _ParrilladaVideoOptionsPrivate
{
	ParrilladaBurnSession *session;

	GtkWidget *video_options;
	GtkWidget *vcd_label;
	GtkWidget *vcd_button;
	GtkWidget *svcd_button;

	GtkWidget *button_native;
	GtkWidget *button_ntsc;
	GtkWidget *button_pal;
	GtkWidget *button_4_3;
	GtkWidget *button_16_9;
};

#define PARRILLADA_VIDEO_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PARRILLADA_TYPE_VIDEO_OPTIONS, ParrilladaVideoOptionsPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (ParrilladaVideoOptions, parrillada_video_options, GTK_TYPE_ALIGNMENT);

static void
parrillada_video_options_update_from_tag (ParrilladaVideoOptions *options,
                                       const gchar *tag)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!tag)
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	
	if (!strcmp (tag, PARRILLADA_VCD_TYPE)) {
		ParrilladaMedia media;
		gint svcd_type;

		media = parrillada_burn_session_get_dest_media (priv->session);
		if (media & PARRILLADA_MEDIUM_DVD) {
			/* Don't change anything in this case
			 * as the tag has no influence over 
			 * this type of image */
			return;
		}
		else if (media & PARRILLADA_MEDIUM_FILE) {
			ParrilladaImageFormat format;

			format = parrillada_burn_session_get_output_format (priv->session);

			/* Same as above for the following case */
			if (format == PARRILLADA_IMAGE_FORMAT_BIN)
				return;
		}

		svcd_type = parrillada_burn_session_tag_lookup_int (priv->session, tag);
		if (svcd_type == PARRILLADA_SVCD) {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->svcd_button)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->svcd_button), TRUE);

				gtk_widget_set_sensitive (priv->button_4_3, TRUE);
				gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
		else {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->vcd_button)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->vcd_button), TRUE);

			gtk_widget_set_sensitive (priv->button_4_3, FALSE);
			gtk_widget_set_sensitive (priv->button_16_9, FALSE);
		}
	}
	else if (!strcmp (tag, PARRILLADA_VIDEO_OUTPUT_FRAMERATE)) {
		GValue *value = NULL;

		parrillada_burn_session_tag_lookup (priv->session,
						 tag,
						 &value);
		if (value) {
			if (g_value_get_int (value) == PARRILLADA_VIDEO_FRAMERATE_NTSC) {
				if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_ntsc)))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_ntsc), TRUE);
			}
			else {
				if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_pal)))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_pal), TRUE);
			}
		}
		else if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_native)))
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_native), TRUE);
	}
	else if (!strcmp (tag, PARRILLADA_VIDEO_OUTPUT_ASPECT)) {
		gint aspect_type = parrillada_burn_session_tag_lookup_int (priv->session, tag);

		if (aspect_type == PARRILLADA_VIDEO_ASPECT_16_9) {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_16_9)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_16_9), TRUE);
		}
		else {
			if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_4_3)))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_4_3), TRUE);
		}
	}
}

static void
parrillada_video_options_update (ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;
	ParrilladaMedia media;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);

	/* means we haven't initialized yet */
	if (!priv->vcd_label)
		return;

	media = parrillada_burn_session_get_dest_media (priv->session);
	if (media & PARRILLADA_MEDIUM_DVD) {
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);

		gtk_widget_set_sensitive (priv->button_4_3, TRUE);
		gtk_widget_set_sensitive (priv->button_16_9, TRUE);
	}
	else if (media & PARRILLADA_MEDIUM_CD) {
		gtk_widget_show (priv->vcd_label);
		gtk_widget_show (priv->vcd_button);
		gtk_widget_show (priv->svcd_button);

		parrillada_video_options_update_from_tag (options, PARRILLADA_VCD_TYPE);
	}
	else if (media & PARRILLADA_MEDIUM_FILE) {
		ParrilladaImageFormat format;

		/* Hide any options about (S)VCD type
		 * as this is handled in ParrilladaImageTypeChooser 
		 * object */
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);

		format = parrillada_burn_session_get_output_format (priv->session);
		if (format == PARRILLADA_IMAGE_FORMAT_BIN) {
			gtk_widget_set_sensitive (priv->button_4_3, TRUE);
			gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
		else if (format == PARRILLADA_IMAGE_FORMAT_CUE)
			parrillada_video_options_update_from_tag (options, PARRILLADA_VCD_TYPE);
	}
}

static void
parrillada_video_options_output_changed_cb (ParrilladaBurnSession *session,
                                         ParrilladaMedium *former_medium,
                                         ParrilladaVideoOptions *options)
{
	parrillada_video_options_update (options);
}

static void
parrillada_video_options_tag_changed_cb (ParrilladaBurnSession *session,
                                      const gchar *tag,
                                      ParrilladaVideoOptions *options)
{
	parrillada_video_options_update_from_tag (options, tag);
}

static void
parrillada_video_options_SVCD (GtkToggleButton *button,
			    ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VCD_TYPE,
	                                  PARRILLADA_SVCD);

	/* NOTE: this is only possible when that's
	 * not an image */

	gtk_widget_set_sensitive (priv->button_4_3, TRUE);
	gtk_widget_set_sensitive (priv->button_16_9, TRUE);
}

static void
parrillada_video_options_VCD (GtkToggleButton *button,
			   ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VCD_TYPE,
	                                  PARRILLADA_VCD_V2);

	/* NOTE: this is only possible when that's
	 * not an image */
	gtk_widget_set_sensitive (priv->button_4_3, FALSE);
	gtk_widget_set_sensitive (priv->button_16_9, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_4_3), TRUE);
}

static void
parrillada_video_options_NTSC (GtkToggleButton *button,
			    ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VIDEO_OUTPUT_FRAMERATE,
	                                  PARRILLADA_VIDEO_FRAMERATE_NTSC);
}

static void
parrillada_video_options_PAL_SECAM (GtkToggleButton *button,
				 ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VIDEO_OUTPUT_FRAMERATE,
	                                  PARRILLADA_VIDEO_FRAMERATE_PAL_SECAM);
}

static void
parrillada_video_options_native_framerate (GtkToggleButton *button,
					ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_remove (priv->session, PARRILLADA_VIDEO_OUTPUT_FRAMERATE);
}

static void
parrillada_video_options_16_9 (GtkToggleButton *button,
			    ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VIDEO_OUTPUT_ASPECT,
	                                  PARRILLADA_VIDEO_ASPECT_16_9);
}

static void
parrillada_video_options_4_3 (GtkToggleButton *button,
			   ParrilladaVideoOptions *options)
{
	ParrilladaVideoOptionsPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	parrillada_burn_session_tag_add_int (priv->session,
	                                  PARRILLADA_VIDEO_OUTPUT_ASPECT,
	                                  PARRILLADA_VIDEO_ASPECT_4_3);
}

void
parrillada_video_options_set_session (ParrilladaVideoOptions *options,
                                   ParrilladaBurnSession *session)
{
	ParrilladaVideoOptionsPrivate *priv;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (options);
	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_video_options_output_changed_cb,
		                                      options);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_video_options_tag_changed_cb,
		                                      options);
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (session) {
		priv->session = g_object_ref (session);
		parrillada_video_options_update (options);

		if (parrillada_burn_session_tag_lookup (session, PARRILLADA_VIDEO_OUTPUT_FRAMERATE, NULL) == PARRILLADA_BURN_OK)
			parrillada_video_options_update_from_tag (options, PARRILLADA_VIDEO_OUTPUT_FRAMERATE);

		/* If session has tag update UI otherwise update _from_ UI */
		if (parrillada_burn_session_tag_lookup (session, PARRILLADA_VIDEO_OUTPUT_ASPECT, NULL) == PARRILLADA_BURN_OK)
			parrillada_video_options_update_from_tag (options, PARRILLADA_VIDEO_OUTPUT_ASPECT);
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->button_4_3)))
			parrillada_burn_session_tag_add_int (priv->session,
			                                  PARRILLADA_VIDEO_OUTPUT_ASPECT,
	        			                  PARRILLADA_VIDEO_ASPECT_4_3);
		else
			parrillada_burn_session_tag_add_int (priv->session,
				                          PARRILLADA_VIDEO_OUTPUT_ASPECT,
				                          PARRILLADA_VIDEO_ASPECT_16_9);

		g_signal_connect (priv->session,
		                  "output-changed",
		                  G_CALLBACK (parrillada_video_options_output_changed_cb),
		                  options);
		g_signal_connect (priv->session,
		                  "tag-changed",
		                  G_CALLBACK (parrillada_video_options_tag_changed_cb),
		                  options);
	}
}

static void
parrillada_video_options_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	g_return_if_fail (PARRILLADA_IS_VIDEO_OPTIONS (object));

	switch (prop_id)
	{
	case PROP_SESSION:
		parrillada_video_options_set_session (PARRILLADA_VIDEO_OPTIONS (object),
		                                   g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_video_options_get_property (GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	ParrilladaVideoOptionsPrivate *priv;

	g_return_if_fail (PARRILLADA_IS_VIDEO_OPTIONS (object));

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		g_object_ref (priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
parrillada_video_options_init (ParrilladaVideoOptions *object)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *widget;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	ParrilladaVideoOptionsPrivate *priv;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (object);

	gtk_container_set_border_width (GTK_CONTAINER (object), 6);
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	table = gtk_table_new (3, 4, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);

	label = gtk_label_new (_("Video format:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL, _("_NTSC"));
	priv->button_ntsc = button1;
	gtk_widget_set_tooltip_text (button1, _("Format used mostly on the North American continent"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (parrillada_video_options_NTSC),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  3, 4,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1), _("_PAL/SECAM"));
	priv->button_pal = button2;
	gtk_widget_set_tooltip_text (button2, _("Format used mostly in Europe"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (parrillada_video_options_PAL_SECAM),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button3 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("Native _format"));
	priv->button_native = button3;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button3), TRUE);
	g_signal_connect (button3,
			  "toggled",
			  G_CALLBACK (parrillada_video_options_native_framerate),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button3,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	label = gtk_label_new (_("Aspect ratio:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL, _("_4:3"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (parrillada_video_options_4_3),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_4_3 = button1;

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("_16:9"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (parrillada_video_options_16_9),
			  object);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_16_9 = button2;

	/* Video options for (S)VCD */
	label = gtk_label_new (_("VCD type:"));
	priv->vcd_label = label;

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic_from_widget (NULL, _("Create an SVCD"));
	priv->svcd_button = button1;
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button1,
			  "clicked",
			  G_CALLBACK (parrillada_video_options_SVCD),
			  object);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1), _("Create a VCD"));
	priv->vcd_button = button2;
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button2,
			  "clicked",
			  G_CALLBACK (parrillada_video_options_VCD),
			  object);

	gtk_box_pack_start (GTK_BOX (widget), table, FALSE, FALSE, 0);

	/* NOTE: audio options for DVDs were removed. For SVCD that is MP2 and
	 * for Video DVD even if we have a choice AC3 is the most widespread
	 * audio format. So use AC3 by default. */

	gtk_widget_show_all (widget);
	gtk_container_add (GTK_CONTAINER (object), widget);
}

static void
parrillada_video_options_finalize (GObject *object)
{
	ParrilladaVideoOptionsPrivate *priv;

	priv = PARRILLADA_VIDEO_OPTIONS_PRIVATE (object);
	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_video_options_output_changed_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      parrillada_video_options_tag_changed_cb,
		                                      object);
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (parrillada_video_options_parent_class)->finalize (object);
}

static void
parrillada_video_options_class_init (ParrilladaVideoOptionsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ParrilladaVideoOptionsPrivate));

	object_class->finalize = parrillada_video_options_finalize;
	object_class->set_property = parrillada_video_options_set_property;
	object_class->get_property = parrillada_video_options_get_property;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      PARRILLADA_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE));
}

GtkWidget *
parrillada_video_options_new (ParrilladaBurnSession *session)
{
	return g_object_new (PARRILLADA_TYPE_VIDEO_OPTIONS, "session", session, NULL);
}

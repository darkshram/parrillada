/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libparrillada-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libparrillada-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libparrillada-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libparrillada-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libparrillada-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libparrillada-media is distributed in the hope that it will be useful,
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
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "parrillada-media.h"
#include "parrillada-media-private.h"

static gboolean debug = 0;

#define PARRILLADA_MEDIUM_TRUE_RANDOM_WRITABLE(media)				\
	(PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_RESTRICTED) ||		\
	 PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS) ||		\
	 PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVD_RAM) || 			\
	 PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_BDRE))

static const GOptionEntry options [] = {
	{ "parrillada-media-debug", 0, 0, G_OPTION_ARG_NONE, &debug,
	  N_("Display debug statements on stdout for Parrillada media library"),
	  NULL },
	{ NULL }
};

void
parrillada_media_library_set_debug (gboolean value)
{
	debug = value;
}

static GSList *
parrillada_media_add_to_list (GSList *retval,
			   ParrilladaMedia media)
{
	retval = g_slist_prepend (retval, GINT_TO_POINTER (media));
	return retval;
}

static GSList *
parrillada_media_new_status (GSList *retval,
			  ParrilladaMedia media,
			  ParrilladaMedia type)
{
	if ((type & PARRILLADA_MEDIUM_BLANK)
	&& !(media & PARRILLADA_MEDIUM_ROM)) {
		/* If media is blank there is no other possible property. */
		retval = parrillada_media_add_to_list (retval,
						    media|
						    PARRILLADA_MEDIUM_BLANK);

		/* NOTE about BR-R they can be "formatted" but they are never
		 * unformatted since by default they'll be used as sequential.
		 * We don't consider DVD-RW as unformatted as in this case
		 * they are treated as DVD-RW in sequential mode and therefore
		 * don't require any formatting. */
		if (!(media & PARRILLADA_MEDIUM_RAM)
		&&   (PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_DVDRW_PLUS)
		||    PARRILLADA_MEDIUM_IS (media, PARRILLADA_MEDIUM_BD|PARRILLADA_MEDIUM_REWRITABLE))) {
			if (type & PARRILLADA_MEDIUM_UNFORMATTED)
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_BLANK|
								    PARRILLADA_MEDIUM_UNFORMATTED);
		}
	}

	if (type & PARRILLADA_MEDIUM_CLOSED) {
		if (media & (PARRILLADA_MEDIUM_DVD|PARRILLADA_MEDIUM_BD))
			retval = parrillada_media_add_to_list (retval,
							    media|
							    PARRILLADA_MEDIUM_CLOSED|
							    (type & PARRILLADA_MEDIUM_HAS_DATA)|
							    (type & PARRILLADA_MEDIUM_PROTECTED));
		else {
			if (type & PARRILLADA_MEDIUM_HAS_AUDIO)
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_CLOSED|
								    PARRILLADA_MEDIUM_HAS_AUDIO);
			if (type & PARRILLADA_MEDIUM_HAS_DATA)
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_CLOSED|
								    PARRILLADA_MEDIUM_HAS_DATA);
			if (PARRILLADA_MEDIUM_IS (type, PARRILLADA_MEDIUM_HAS_AUDIO|PARRILLADA_MEDIUM_HAS_DATA))
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_CLOSED|
								    PARRILLADA_MEDIUM_HAS_DATA|
								    PARRILLADA_MEDIUM_HAS_AUDIO);
		}
	}

	if ((type & PARRILLADA_MEDIUM_APPENDABLE)
	&& !(media & PARRILLADA_MEDIUM_ROM)
	&& !PARRILLADA_MEDIUM_TRUE_RANDOM_WRITABLE (media)) {
		if (media & (PARRILLADA_MEDIUM_BD|PARRILLADA_MEDIUM_DVD))
			retval = parrillada_media_add_to_list (retval,
							    media|
							    PARRILLADA_MEDIUM_APPENDABLE|
							    PARRILLADA_MEDIUM_HAS_DATA);
		else {
			if (type & PARRILLADA_MEDIUM_HAS_AUDIO)
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_APPENDABLE|
								    PARRILLADA_MEDIUM_HAS_AUDIO);
			if (type & PARRILLADA_MEDIUM_HAS_DATA)
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_APPENDABLE|
								    PARRILLADA_MEDIUM_HAS_DATA);
			if (PARRILLADA_MEDIUM_IS (type, PARRILLADA_MEDIUM_HAS_AUDIO|PARRILLADA_MEDIUM_HAS_DATA))
				retval = parrillada_media_add_to_list (retval,
								    media|
								    PARRILLADA_MEDIUM_HAS_DATA|
								    PARRILLADA_MEDIUM_APPENDABLE|
								    PARRILLADA_MEDIUM_HAS_AUDIO);
		}
	}

	return retval;
}

static GSList *
parrillada_media_new_attribute (GSList *retval,
			     ParrilladaMedia media,
			     ParrilladaMedia type)
{
	/* NOTE: never reached by BDs, ROMs (any) or Restricted Overwrite
	 * and DVD- dual layer */

	/* NOTE: there is no dual layer DVD-RW */
	if (type & PARRILLADA_MEDIUM_REWRITABLE)
		retval = parrillada_media_new_status (retval,
						   media|PARRILLADA_MEDIUM_REWRITABLE,
						   type);

	if (type & PARRILLADA_MEDIUM_WRITABLE)
		retval = parrillada_media_new_status (retval,
						   media|PARRILLADA_MEDIUM_WRITABLE,
						   type);

	return retval;
}

static GSList *
parrillada_media_new_subtype (GSList *retval,
			   ParrilladaMedia media,
			   ParrilladaMedia type)
{
	if (media & PARRILLADA_MEDIUM_BD) {
		/* There seems to be Dual layers BDs as well */

		if (type & PARRILLADA_MEDIUM_ROM) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_ROM,
							   type);
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_ROM|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		if (type & PARRILLADA_MEDIUM_RANDOM) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_RANDOM|
							   PARRILLADA_MEDIUM_WRITABLE,
							   type);
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_RANDOM|
								   PARRILLADA_MEDIUM_WRITABLE|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		if (type & PARRILLADA_MEDIUM_SRM) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_SRM|
							   PARRILLADA_MEDIUM_WRITABLE,
							   type);
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_SRM|
								   PARRILLADA_MEDIUM_WRITABLE|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		if (type & PARRILLADA_MEDIUM_POW) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_POW|
							   PARRILLADA_MEDIUM_WRITABLE,
							   type);
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_POW|
								   PARRILLADA_MEDIUM_WRITABLE|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		/* BD-RE */
		if (type & PARRILLADA_MEDIUM_REWRITABLE) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_REWRITABLE,
							   type);
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_REWRITABLE|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}
	}

	if (media & PARRILLADA_MEDIUM_DVD) {
		/* There is no such thing as DVD-RW DL nor DVD-RAM DL*/

		/* The following is always a DVD-R dual layer */
		if (type & PARRILLADA_MEDIUM_JUMP)
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_JUMP|
							   PARRILLADA_MEDIUM_DUAL_L|
							   PARRILLADA_MEDIUM_WRITABLE,
							   type);

		if (type & PARRILLADA_MEDIUM_SEQUENTIAL) {
			retval = parrillada_media_new_attribute (retval,
							      media|
							      PARRILLADA_MEDIUM_SEQUENTIAL,
							      type);

			/* This one has to be writable only, no RW */
			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_SEQUENTIAL|
								   PARRILLADA_MEDIUM_WRITABLE|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		/* Restricted Overwrite media are always rewritable */
		if (type & PARRILLADA_MEDIUM_RESTRICTED)
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_RESTRICTED|
							   PARRILLADA_MEDIUM_REWRITABLE,
							   type);

		if (type & PARRILLADA_MEDIUM_PLUS) {
			retval = parrillada_media_new_attribute (retval,
							      media|
							      PARRILLADA_MEDIUM_PLUS,
							      type);

			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_attribute (retval,
								      media|
								      PARRILLADA_MEDIUM_PLUS|
								      PARRILLADA_MEDIUM_DUAL_L,
								      type);

		}

		if (type & PARRILLADA_MEDIUM_ROM) {
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_ROM,
							   type);

			if (type & PARRILLADA_MEDIUM_DUAL_L)
				retval = parrillada_media_new_status (retval,
								   media|
								   PARRILLADA_MEDIUM_ROM|
								   PARRILLADA_MEDIUM_DUAL_L,
								   type);
		}

		/* RAM media are always rewritable */
		if (type & PARRILLADA_MEDIUM_RAM)
			retval = parrillada_media_new_status (retval,
							   media|
							   PARRILLADA_MEDIUM_RAM|
							   PARRILLADA_MEDIUM_REWRITABLE,
							   type);
	}

	return retval;
}

GSList *
parrillada_media_get_all_list (ParrilladaMedia type)
{
	GSList *retval = NULL;

	if (type & PARRILLADA_MEDIUM_FILE)
		retval = parrillada_media_add_to_list (retval, PARRILLADA_MEDIUM_FILE);					       

	if (type & PARRILLADA_MEDIUM_CD) {
		if (type & PARRILLADA_MEDIUM_ROM)
			retval = parrillada_media_new_status (retval,
							   PARRILLADA_MEDIUM_CD|
							   PARRILLADA_MEDIUM_ROM,
							   type);

		retval = parrillada_media_new_attribute (retval,
						      PARRILLADA_MEDIUM_CD,
						      type);
	}

	if (type & PARRILLADA_MEDIUM_DVD)
		retval = parrillada_media_new_subtype (retval,
						    PARRILLADA_MEDIUM_DVD,
						    type);


	if (type & PARRILLADA_MEDIUM_BD)
		retval = parrillada_media_new_subtype (retval,
						    PARRILLADA_MEDIUM_BD,
						    type);

	return retval;
}

GQuark
parrillada_media_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("ParrilladaMediaError");

	return quark;
}

void
parrillada_media_to_string (ParrilladaMedia media,
			 gchar *buffer)
{
	if (media & PARRILLADA_MEDIUM_FILE)
		strcat (buffer, "file ");

	if (media & PARRILLADA_MEDIUM_CD)
		strcat (buffer, "CD ");

	if (media & PARRILLADA_MEDIUM_DVD)
		strcat (buffer, "DVD ");

	if (media & PARRILLADA_MEDIUM_RAM)
		strcat (buffer, "RAM ");

	if (media & PARRILLADA_MEDIUM_BD)
		strcat (buffer, "BD ");

	if (media & PARRILLADA_MEDIUM_DUAL_L)
		strcat (buffer, "DL ");

	/* DVD subtypes */
	if (media & PARRILLADA_MEDIUM_PLUS)
		strcat (buffer, "+ ");

	if (media & PARRILLADA_MEDIUM_SEQUENTIAL)
		strcat (buffer, "- (sequential) ");

	if (media & PARRILLADA_MEDIUM_RESTRICTED)
		strcat (buffer, "- (restricted) ");

	if (media & PARRILLADA_MEDIUM_JUMP)
		strcat (buffer, "- (jump) ");

	/* BD subtypes */
	if (media & PARRILLADA_MEDIUM_SRM)
		strcat (buffer, "SRM ");

	if (media & PARRILLADA_MEDIUM_POW)
		strcat (buffer, "POW ");

	if (media & PARRILLADA_MEDIUM_RANDOM)
		strcat (buffer, "RANDOM ");

	/* discs attributes */
	if (media & PARRILLADA_MEDIUM_REWRITABLE)
		strcat (buffer, "RW ");

	if (media & PARRILLADA_MEDIUM_WRITABLE)
		strcat (buffer, "W ");

	if (media & PARRILLADA_MEDIUM_ROM)
		strcat (buffer, "ROM ");

	/* status of the disc */
	if (media & PARRILLADA_MEDIUM_CLOSED)
		strcat (buffer, "closed ");

	if (media & PARRILLADA_MEDIUM_BLANK)
		strcat (buffer, "blank ");

	if (media & PARRILLADA_MEDIUM_APPENDABLE)
		strcat (buffer, "appendable ");

	if (media & PARRILLADA_MEDIUM_PROTECTED)
		strcat (buffer, "protected ");

	if (media & PARRILLADA_MEDIUM_HAS_DATA)
		strcat (buffer, "with data ");

	if (media & PARRILLADA_MEDIUM_HAS_AUDIO)
		strcat (buffer, "with audio ");

	if (media & PARRILLADA_MEDIUM_UNFORMATTED)
		strcat (buffer, "Unformatted ");
}

/**
 * parrillada_media_get_option_group:
 *
 * Returns a GOptionGroup for the commandline arguments recognized by libparrillada-media.
 * You should add this to your GOptionContext if your are using g_option_context_parse ()
 * to parse your commandline arguments.
 *
 * Return value: a #GOptionGroup *
 **/
GOptionGroup *
parrillada_media_get_option_group (void)
{
	GOptionGroup *group;

	group = g_option_group_new ("parrillada-media",
				    N_("Parrillada optical media library"),
				    N_("Display options for Parrillada media library"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, options);
	return group;
}

void
parrillada_media_message (const gchar *location,
		       const gchar *format,
		       ...)
{
	va_list arg_list;
	gchar *format_real;

	if (!debug)
		return;

	format_real = g_strdup_printf ("ParrilladaMedia: (at %s) %s\n",
				       location,
				       format);

	va_start (arg_list, format);
	vprintf (format_real, arg_list);
	va_end (arg_list);

	g_free (format_real);
}

#include <gtk/gtk.h>

#include "parrillada-medium-monitor.h"

static ParrilladaMediumMonitor *default_monitor = NULL;

/**
 * parrillada_media_library_start:
 *
 * Initialize the library.
 *
 * You should call this function before using any other from the library.
 *
 * Rename to: init
 **/
void
parrillada_media_library_start (void)
{
	if (default_monitor) {
		g_object_ref (default_monitor);
		return;
	}

	PARRILLADA_MEDIA_LOG ("Initializing Parrillada-media %i.%i.%i",
	                    PARRILLADA_MAJOR_VERSION,
	                    PARRILLADA_MINOR_VERSION,
	                    PARRILLADA_SUB);

#if defined(HAVE_STRUCT_USCSI_CMD)
	/* Work around: because on OpenSolaris parrillada posiblely be run
	 * as root for a user with 'Primary Administrator' profile,
	 * a root dbus session will be autospawned at that time.
	 * This fix is to work around
	 * http://bugzilla.gnome.org/show_bug.cgi?id=526454
	 */
	g_setenv ("DBUS_SESSION_BUS_ADDRESS", "autolaunch:", TRUE);
#endif

	/* Initialize external libraries (threads... */
	if (!g_thread_supported ())
		g_thread_init (NULL);

	/* Initialize i18n */
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Initialize icon-theme */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PARRILLADA_DATADIR "/icons");

	/* Take a reference for the monitoring library */
	default_monitor = parrillada_medium_monitor_get_default ();
}

/**
 * parrillada_media_library_stop:
 *
 * De-initialize the library once you do not need the library anymore.
 *
 * Rename to: deinit
 **/
void
parrillada_media_library_stop (void)
{
	g_object_unref (default_monitor);
	default_monitor = NULL;
}

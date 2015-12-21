
AM_CPPFLAGS = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libparrillada-media/					\
	-I$(top_builddir)/libparrillada-media/		\
	-I$(top_srcdir)/libparrillada-burn				\
	-I$(top_builddir)/libparrillada-burn/				\
	-DPARRILLADA_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DPARRILLADA_PREFIX=\"$(prefix)\"           		\
	-DPARRILLADA_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DPARRILLADA_DATADIR=\"$(datadir)/parrillada\"     	    	\
	-DPARRILLADA_LIBDIR=\"$(libdir)\"  	         	\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)				\
	$(PARRILLADA_GLIB_CFLAGS)				\
	$(PARRILLADA_GSTREAMER_CFLAGS)

transcodedir = $(PARRILLADA_PLUGIN_DIRECTORY)
transcode_LTLIBRARIES = libparrillada-transcode.la

libparrillada_transcode_la_SOURCES = burn-transcode.c burn-normalize.h 
libparrillada_transcode_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GSTREAMER_LIBS)
libparrillada_transcode_la_LDFLAGS = -module -avoid-version

normalizedir = $(PARRILLADA_PLUGIN_DIRECTORY)
normalize_LTLIBRARIES = libparrillada-normalize.la

libparrillada_normalize_la_SOURCES = burn-normalize.c burn-normalize.h
libparrillada_normalize_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GSTREAMER_LIBS)
libparrillada_normalize_la_LDFLAGS = -module -avoid-version

vobdir = $(PARRILLADA_PLUGIN_DIRECTORY)
vob_LTLIBRARIES = libparrillada-vob.la

libparrillada_vob_la_SOURCES = burn-vob.c 
libparrillada_vob_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GSTREAMER_LIBS)
libparrillada_vob_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

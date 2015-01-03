
INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libparrillada-media/					\
	-I$(top_builddir)/libparrillada-media/			\
	-I$(top_srcdir)/libparrillada-burn				\
	-I$(top_builddir)/libparrillada-burn/				\
	-DPARRILLADA_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DPARRILLADA_PREFIX=\"$(prefix)\"           		\
	-DPARRILLADA_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DPARRILLADA_DATADIR=\"$(datadir)/parrillada\"     	    	\
	-DPARRILLADA_LIBDIR=\"$(libdir)\"  	         	\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)					\
	$(PARRILLADA_GLIB_CFLAGS)					\
	$(PARRILLADA_GIO_CFLAGS)

#local-track
local_trackdir = $(PARRILLADA_PLUGIN_DIRECTORY)
local_track_LTLIBRARIES = libparrillada-local-track.la

libparrillada_local_track_la_SOURCES = burn-local-image.c 
libparrillada_local_track_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GIO_LIBS)
libparrillada_local_track_la_LDFLAGS = -module -avoid-version

#burn-uri
burn_uridir = $(PARRILLADA_PLUGIN_DIRECTORY)
burn_uri_LTLIBRARIES = libparrillada-burn-uri.la

libparrillada_burn_uri_la_SOURCES = burn-uri.c 
libparrillada_burn_uri_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GIO_LIBS)
libparrillada_burn_uri_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

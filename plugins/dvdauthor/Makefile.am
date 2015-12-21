
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
	$(PARRILLADA_LIBXML_CFLAGS)

dvdauthordir = $(PARRILLADA_PLUGIN_DIRECTORY)
dvdauthor_LTLIBRARIES = libparrillada-dvdauthor.la

libparrillada_dvdauthor_la_SOURCES = burn-dvdauthor.c 
libparrillada_dvdauthor_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_LIBXML_LIBS)
libparrillada_dvdauthor_la_LDFLAGS = -module -avoid-version


-include $(top_srcdir)/git.mk

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
	$(PARRILLADA_GLIB_CFLAGS)

plugindir = $(PARRILLADA_PLUGIN_DIRECTORY)
plugin_LTLIBRARIES = libparrillada-dvdcss.la
libparrillada_dvdcss_la_SOURCES = burn-dvdcss.c \
	burn-dvdcss-private.h 
libparrillada_dvdcss_la_LIBADD = ../../libparrillada-media/libparrillada-media3.la ../../libparrillada-burn/libparrillada-burn3.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GMODULE_LIBS)
libparrillada_dvdcss_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

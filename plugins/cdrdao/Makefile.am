
INCLUDES = \
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

#cdrdao
cdrdaodir = $(PARRILLADA_PLUGIN_DIRECTORY)
cdrdao_LTLIBRARIES = libparrillada-cdrdao.la
libparrillada_cdrdao_la_SOURCES = burn-cdrdao.c 
libparrillada_cdrdao_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_cdrdao_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

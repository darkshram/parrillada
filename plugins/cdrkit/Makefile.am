
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

#wodim
wodimdir = $(PARRILLADA_PLUGIN_DIRECTORY)
wodim_LTLIBRARIES = libparrillada-wodim.la
libparrillada_wodim_la_SOURCES = burn-wodim.c \
	burn-cdrkit.h 
libparrillada_wodim_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_wodim_la_LDFLAGS = -module -avoid-version

#genisoimage
genisoimagedir = $(PARRILLADA_PLUGIN_DIRECTORY)
genisoimage_LTLIBRARIES = libparrillada-genisoimage.la
libparrillada_genisoimage_la_SOURCES = burn-genisoimage.c \
	burn-cdrkit.h 
libparrillada_genisoimage_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_genisoimage_la_LDFLAGS = -module -avoid-version

#readom
readomdir = $(PARRILLADA_PLUGIN_DIRECTORY)
readom_LTLIBRARIES = libparrillada-readom.la
libparrillada_readom_la_SOURCES = burn-readom.c \
	burn-cdrkit.h 
libparrillada_readom_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_readom_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

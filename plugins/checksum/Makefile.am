
AM_CPPFLAGS = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libparrillada-media/					\
	-I$(top_builddir)/libparrillada-media/				\
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
	$(PARRILLADA_GTK_CFLAGS)

checksumdir = $(PARRILLADA_PLUGIN_DIRECTORY)
checksum_LTLIBRARIES = libparrillada-checksum.la
libparrillada_checksum_la_SOURCES = burn-checksum-image.c	

libparrillada_checksum_la_LDFLAGS = -module -avoid-version
libparrillada_checksum_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)

checksumfiledir = $(PARRILLADA_PLUGIN_DIRECTORY)
checksumfile_LTLIBRARIES = libparrillada-checksum-file.la
libparrillada_checksum_file_la_SOURCES = burn-checksum-files.c	\
				      burn-volume-read.c  \
				      burn-volume-read.h

libparrillada_checksum_file_la_LDFLAGS = -module -avoid-version
libparrillada_checksum_file_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_GTK_LIBS)

-include $(top_srcdir)/git.mk

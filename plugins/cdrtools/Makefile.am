
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

#cdrecord
cdrecorddir = $(PARRILLADA_PLUGIN_DIRECTORY)
cdrecord_LTLIBRARIES = libparrillada-cdrecord.la
libparrillada_cdrecord_la_SOURCES = burn-cdrecord.c \
	burn-cdrtools.h 
libparrillada_cdrecord_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_cdrecord_la_LDFLAGS = -module -avoid-version

#mkisofs
mkisofsdir = $(PARRILLADA_PLUGIN_DIRECTORY)
mkisofs_LTLIBRARIES = libparrillada-mkisofs.la
libparrillada_mkisofs_la_SOURCES = burn-mkisofs.c \
	burn-cdrtools.h 
libparrillada_mkisofs_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_mkisofs_la_LDFLAGS = -module -avoid-version

#readcd
readcddir = $(PARRILLADA_PLUGIN_DIRECTORY)
readcd_LTLIBRARIES = libparrillada-readcd.la
libparrillada_readcd_la_SOURCES = burn-readcd.c \
	burn-cdrtools.h 
libparrillada_readcd_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_readcd_la_LDFLAGS = -module -avoid-version

#cdda2wav
cdda2wavdir = $(PARRILLADA_PLUGIN_DIRECTORY)
cdda2wav_LTLIBRARIES = libparrillada-cdda2wav.la
libparrillada_cdda2wav_la_SOURCES = burn-cdda2wav.c burn-cdrtools.h 
libparrillada_cdda2wav_la_LIBADD = ../../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS)
libparrillada_cdda2wav_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

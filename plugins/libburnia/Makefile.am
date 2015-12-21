
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
	$(PARRILLADA_LIBISOFS_CFLAGS)			\
	$(PARRILLADA_LIBBURN_CFLAGS)			\
	$(PARRILLADA_GLIB_CFLAGS)

#libburn
libburndir = $(PARRILLADA_PLUGIN_DIRECTORY)
libburn_LTLIBRARIES = libparrillada-libburn.la
libparrillada_libburn_la_SOURCES = burn-libburn.c 		\
				burn-libburn-common.c	\
				burn-libburn-common.h  \
				burn-libburnia.h 
libparrillada_libburn_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_LIBBURNIA_LIBS)
libparrillada_libburn_la_LDFLAGS = -module -avoid-version

#libisofs (apparently libisofs needs one libburn function)
libisofsdir = $(PARRILLADA_PLUGIN_DIRECTORY)
libisofs_LTLIBRARIES = libparrillada-libisofs.la
libparrillada_libisofs_la_SOURCES = burn-libisofs.c                       \
	burn-libburn-common.c burn-libburn-common.h			\
	burn-libburnia.h 
libparrillada_libisofs_la_LIBADD = ../../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_LIBBURNIA_LIBS)
libparrillada_libisofs_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

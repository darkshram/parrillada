
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

#growisofs
growisofsdir = $(PARRILLADA_PLUGIN_DIRECTORY)
growisofs_LTLIBRARIES = libparrillada-growisofs.la
libparrillada_growisofs_la_SOURCES = burn-growisofs.c \
	burn-growisofs-common.h 
libparrillada_growisofs_la_LIBADD = ../../libparrillada-burn/libparrillada-burn3.la $(PARRILLADA_GLIB_LIBS)
libparrillada_growisofs_la_LDFLAGS = -module -avoid-version

#dvdrwformat
dvdrwformatdir = $(PARRILLADA_PLUGIN_DIRECTORY)
dvdrwformat_LTLIBRARIES = libparrillada-dvdrwformat.la
libparrillada_dvdrwformat_la_SOURCES = burn-dvd-rw-format.c 
libparrillada_dvdrwformat_la_LIBADD = ../../libparrillada-burn/libparrillada-burn3.la $(PARRILLADA_GLIB_LIBS)
libparrillada_dvdrwformat_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk

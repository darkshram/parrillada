module_flags = -avoid-version -module

AM_CPPFLAGS =					\
	-I$(top_srcdir)/libparrillada-utils/	\
	-I$(top_builddir)/libparrillada-utils/	\
	-I$(top_srcdir)/libparrillada-media/	\
	-I$(top_builddir)/libparrillada-media/	\
	-I$(top_srcdir)/libparrillada-burn/	\
	-I$(top_builddir)/libparrillada-burn/	\
	-I$(top_srcdir)/src/			\
	-I$(top_builddir)/src/			\
	$(CAJA_EXTENSION_CFLAGS)		\
	$(WARN_CFLAGS)							\
	$(WERROR)				\
	-D_FILE_OFFSET_BITS=64			\
	-D_GNU_SOURCE				\
	-D_LARGEFILE64_SOURCE			\
	-D_REENTRANT				\
	-D_POSIX_PTHREAD_SEMANTICS		\
	-DPREFIX=\"$(prefix)\"			\
	-DBINDIR=\""$(bindir)"\"		\
	-DLIBEXECDIR=\""$(libexecdir)"\"	\
	-DSHAREDIR=\""$(datadir)"\"		\
	-DDATADIR=\""$(datadir)/parrillada"\"	\
	$(DISABLE_DEPRECATED)

@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
desktop_in_files = parrillada-caja.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(desktop_in_files)
CLEANFILES = $(desktop_DATA)
DISTCLEANFILES = $(NULL)

### Caja Extension
caja_extensiondir = $(CAJADIR)
caja_extension_LTLIBRARIES = libcaja-parrillada-extension.la

libcaja_parrillada_extension_la_SOURCES = 	\
	caja-burn-bar.c			\
	caja-burn-bar.h			\
	caja-burn-extension.c			\
	$(top_srcdir)/src/parrillada-project-name.c                       \
	$(top_srcdir)/src/parrillada-project-name.h	\
	$(top_srcdir)/src/parrillada-drive-settings.h	\
	$(top_srcdir)/src/parrillada-drive-settings.c

libcaja_parrillada_extension_la_LIBADD =	\
	../libparrillada-utils/libparrillada-utils@PARRILLADA_LIBRARY_SUFFIX@.la	\
	../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la	\
	../libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la	\
	$(CAJA_EXTENSION_LIBS)		
	
libcaja_parrillada_extension_la_LDFLAGS = $(module_flags)

-include $(top_srcdir)/git.mk

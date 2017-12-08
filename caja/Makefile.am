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
	$(CAJA_EXTENSION_CFLAGS)		\
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
	caja-burn-extension.c

libcaja_parrillada_extension_la_LIBADD =	\
	$(CAJA_EXTENSION_LIBS)		\
	../libparrillada-utils/libparrillada-utils3.la	\
	../libparrillada-media/libparrillada-media3.la	\
	../libparrillada-burn/libparrillada-burn3.la
	
libcaja_parrillada_extension_la_LDFLAGS = $(module_flags)

-include $(top_srcdir)/git.mk

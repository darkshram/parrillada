
INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-DPARRILLADA_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DPARRILLADA_PREFIX=\"$(prefix)\"           			\
	-DPARRILLADA_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DPARRILLADA_DATADIR=\"$(datadir)/parrillada\"     	    		\
	-DPARRILLADA_LIBDIR=\"$(libdir)\"  	         		\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(PARRILLADA_GLIB_CFLAGS)						\
	$(PARRILLADA_GMODULE_EXPORT_CFLAGS)					\
	$(PARRILLADA_GIO_CFLAGS)						\
	$(PARRILLADA_GTK_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libparrillada-marshal.h libparrillada-marshal.c

libparrillada-marshal.h: libparrillada-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/libparrillada-marshal.list --header > libparrillada-marshal.h )
libparrillada-marshal.c: libparrillada-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/libparrillada-marshal.list --body --header > libparrillada-marshal.c )

headerdir=$(includedir)/parrillada@PARRILLADA_LIBRARY_SUFFIX@
header_HEADERS =	parrillada-drive.h			\
		parrillada-medium-monitor.h		\
		parrillada-media.h			\
		parrillada-units.h			\
		parrillada-medium.h			\
		parrillada-volume.h		\
		parrillada-medium-selection.h	\
		parrillada-drive-selection.h

lib_LTLIBRARIES = 				\
	libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la

libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_LIBADD =                                     \
	$(PARRILLADA_GLIB_LIBS)                                     \
	$(PARRILLADA_GMODULE_EXPORT_LIBS)                                     \
	$(PARRILLADA_GTHREAD_LIBS)                                     \
	$(PARRILLADA_GIO_LIBS)                                     \
	$(PARRILLADA_GTK_LIBS)                                     \
	$(PARRILLADA_SCSI_LIBS)

libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_LDFLAGS =					\
	-version-info $(LIBPARRILLADA_LT_VERSION)			\
	-no-undefined
	$(AM_LDFLAGS)
#	-export-symbols ./libparrillada-media.symbols

libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES =		\
	libparrillada-marshal.c		\
	libparrillada-marshal.h		\
	parrillada-drive.h			\
	parrillada-medium-monitor.h		\
	parrillada-media.h			\
	parrillada-units.h			\
	parrillada-medium.h			\
	parrillada-volume.h		\
	parrillada-medium-selection.h	\
	scsi-base.h         \
	scsi-command.h         \
	scsi-error.h         \
	scsi-get-configuration.c         \
	scsi-get-configuration.h         \
	scsi-mmc1.h         \
	scsi-mmc2.h         \
	scsi-opcodes.h         \
	scsi-read-disc-info.c         \
	scsi-read-disc-info.h         \
	scsi-read-toc-pma-atip.c         \
	scsi-read-toc-pma-atip.h         \
	scsi-sense-data.c         \
	scsi-sense-data.h         \
	scsi-utils.h         \
	scsi-q-subchannel.h         \
	scsi-error.c         \
	scsi-read-track-information.c         \
	scsi-read-track-information.h         \
	scsi-get-performance.c         \
	scsi-get-performance.h         \
	scsi-mmc3.h         \
	scsi-mode-pages.h         \
	scsi-mode-sense.c         \
	scsi-status-page.h         \
	scsi-spc1.h         \
	scsi-read-capacity.c         \
	scsi-read-capacity.h         \
	scsi-read-disc-structure.c         \
	scsi-read-disc-structure.h         \
	scsi-dvd-structures.h         \
	scsi-read-format-capacities.c         \
	scsi-read-format-capacities.h         \
	scsi-read-cd.h	\
	scsi-read-cd.c	\
	scsi-device.h         \
	scsi-mech-status.c         \
	scsi-mech-status.h         \
	scsi-write-page.h         \
	scsi-mode-select.c         \
	scsi-read10.c         \
	scsi-sbc.h		\
	scsi-test-unit-ready.c           \
	parrillada-media.c           \
	parrillada-medium-monitor.c         \
	burn-susp.c         \
	burn-susp.h         \
	burn-iso-field.c         \
	burn-iso-field.h         \
	burn-iso9660.c         \
	burn-iso9660.h         \
	burn-volume-source.c         \
	burn-volume-source.h         \
	burn-volume.c         \
	burn-volume.h         \
	parrillada-medium.c         \
	parrillada-volume.c         \
	parrillada-drive.c         \
	parrillada-medium-selection.c         \
	parrillada-units.c         \
	parrillada-drive-selection.c         \
	parrillada-drive-selection.h         \
	parrillada-media-private.h         \
	parrillada-medium-selection-priv.h         \
	parrillada-gio-operation.h         \
	parrillada-gio-operation.c         \
	scsi-inquiry.c         \
	scsi-prevent-allow-medium-removal.c         \
	scsi-inquiry.h         \
	parrillada-drive-priv.h

# FreeBSD's SCSI CAM interface
if HAVE_CAM_LIB_H
libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES += scsi-cam.c
endif

# Linux's SCSI CAM interface
if HAVE_SG_IO_HDR_T
libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES += scsi-sg.c
endif

# NetBSD's scsi(4)
if HAVE_SCSIIO_H
libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES += scsi-netbsd.c
endif

# Solaris's USCSI interface
if HAVE_USCSI_H
libparrillada_media@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES += scsi-uscsi.c
endif

if HAVE_INTROSPECTION
girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = ParrilladaMedia-@PARRILLADA_VERSION@.gir

typelibsdir = $(INTROSPECTION_TYPELIBDIR)
typelibs_DATA = ParrilladaMedia-@PARRILLADA_VERSION@.typelib

ParrilladaMedia-@PARRILLADA_VERSION@.gir: libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la $(INTROSPECTION_SCANNER)
	$(INTROSPECTION_SCANNER) -v --namespace=ParrilladaMedia --nsversion=1.0 \
            --include=Gtk-@GTK_API_VERSION@ \
	    -I$(top_srcdir)/libparrillada-media	\
	    --library=parrillada-media@PARRILLADA_LIBRARY_SUFFIX@ \
	    --libtool="$(LIBTOOL)" \
            --output $@ \
            --pkg=gtk+-@GTK_API_VERSION@  \
	    --strip-prefix=Parrillada	\
            $(top_srcdir)/libparrillada-media/parrillada-drive.h			\
	    $(top_srcdir)/libparrillada-media/parrillada-medium-monitor.h		\
	    $(top_builddir)/libparrillada-media/parrillada-media.h			\
	    $(top_srcdir)/libparrillada-media/parrillada-units.h			\
	    $(top_srcdir)/libparrillada-media/parrillada-medium.h			\
	    $(top_srcdir)/libparrillada-media/parrillada-volume.h		\
	    $(top_srcdir)/libparrillada-media/parrillada-medium-selection.h	\
	    $(top_srcdir)/libparrillada-media/parrillada-drive-selection.h	\
            $(top_srcdir)/libparrillada-media/*.c	    

ParrilladaMedia-@PARRILLADA_VERSION@.typelib: $(gir_DATA) $(INTROSPECTION_COMPILER)
	$(INTROSPECTION_COMPILER) $(top_builddir)/libparrillada-media/$(gir_DATA) --shared-library=libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@ -o $(typelibs_DATA)
endif

EXTRA_DIST =			\
	libparrillada-marshal.list
#	libparrillada-media.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)  \
	$(gir_DATA)         \
	$(typelibs_DATA)

-include $(top_srcdir)/git.mk

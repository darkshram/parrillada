INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/libparrillada-utils/				\
	-I$(top_builddir)/libparrillada-utils/				\
	-I$(top_srcdir)/libparrillada-media/				\
	-I$(top_builddir)/libparrillada-media/				\
	-DPARRILLADA_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DPARRILLADA_PREFIX=\"$(prefix)\"           			\
	-DPARRILLADA_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DPARRILLADA_DATADIR=\"$(datadir)/parrillada\"     	    		\
	-DPARRILLADA_LIBDIR=\"$(libdir)\"  	         		\
	-DPARRILLADA_PLUGIN_DIRECTORY=\"$(PARRILLADA_PLUGIN_DIRECTORY)\"	\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(PARRILLADA_GMODULE_CFLAGS)					\
	$(PARRILLADA_GLIB_CFLAGS)						\
	$(PARRILLADA_GIO_CFLAGS)						\
	$(PARRILLADA_GTK_CFLAGS)						\
	$(PARRILLADA_GSTREAMER_CFLAGS)					\
	$(PARRILLADA_CANBERRA_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libparrillada-marshal.h libparrillada-marshal.c

libparrillada-marshal.h: libparrillada-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/libparrillada-marshal.list --header > libparrillada-marshal.h )
libparrillada-marshal.c: libparrillada-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/libparrillada-marshal.list --body --header > libparrillada-marshal.c )

headerdir=$(includedir)/parrillada@PARRILLADA_LIBRARY_SUFFIX@
header_HEADERS =	parrillada-tags.h						\
		parrillada-enums.h						\
		parrillada-error.h						\
		parrillada-status.h					\
		parrillada-burn-lib.h					\
		parrillada-track-type.h					\
		parrillada-track.h						\
		parrillada-track-data.h                 			\
		parrillada-track-data-cfg.h				\
		parrillada-track-disc.h                 			\
		parrillada-track-stream.h                 			\
		parrillada-track-stream-cfg.h             			\
		parrillada-track-image.h					\
		parrillada-track-image-cfg.h				\
		parrillada-session.h                 			\
		parrillada-session-span.h					\
		parrillada-session-cfg.h					\
		parrillada-burn.h						\
		parrillada-burn-options.h        			        \
		parrillada-burn-dialog.h					\
		parrillada-tool-dialog.h					\
		parrillada-sum-dialog.h					\
		parrillada-blank-dialog.h

lib_LTLIBRARIES = \
	libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la

libparrillada_burn@PARRILLADA_LIBRARY_SUFFIX@_la_LIBADD =					\
	../libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la			\
	../libparrillada-utils/libparrillada-utils@PARRILLADA_LIBRARY_SUFFIX@.la			\
	$(PARRILLADA_GLIB_LIBS)					\
	$(PARRILLADA_GTHREAD_LIBS)					\
	$(PARRILLADA_GMODULE_LIBS)					\
	$(PARRILLADA_GIO_LIBS)					\
	$(PARRILLADA_GTK_LIBS)					\
	$(PARRILLADA_GSTREAMER_LIBS)				\
	$(PARRILLADA_CANBERRA_LIBS)

libparrillada_burn@PARRILLADA_LIBRARY_SUFFIX@_la_LDFLAGS =					\
	-version-info $(LIBPARRILLADA_LT_VERSION)			\
	-no-undefined						\
	-export-dynamic
#	-export-symbols ./libparrillada-burn.symbols

libparrillada_burn@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES = 		\
	libparrillada-marshal.c		\
	libparrillada-marshal.h		\
	parrillada-tool-dialog.c         \
	parrillada-tool-dialog.h         \
	parrillada-sum-dialog.c         \
	parrillada-sum-dialog.h         \
	parrillada-blank-dialog.c         \
	parrillada-blank-dialog.h         \
	parrillada-burn.c			\
	parrillada-burn.h			\
	parrillada-xfer.c			\
	parrillada-xfer.h			\
	burn-basics.h                 \
	burn-caps.h                 \
	burn-dbus.h                 \
	burn-debug.h                 \
	burn-image-format.h                 \
	burn-job.h                 \
	burn-mkisofs-base.h                 \
	burn-plugin-manager.h                 \
	burn-process.h                 \
	parrillada-session.h                 \
	burn-task.h                 \
	burn-task-ctx.h                 \
	burn-task-item.h                 \
	parrillada-track.h                 \
	parrillada-session.c                 \
	parrillada-track.c                 \
	burn-basics.c                 \
	burn-caps.c                 \
	burn-dbus.c                 \
	burn-debug.c                 \
	burn-image-format.c                 \
	burn-job.c                 \
	burn-mkisofs-base.c                 \
	burn-plugin.c                 \
	burn-plugin-manager.c                 \
	burn-process.c                 \
	burn-task.c                 \
	burn-task-ctx.c                 \
	burn-task-item.c                 \
	parrillada-burn-dialog.c                 \
	parrillada-burn-dialog.h                 \
	parrillada-burn-options.c                 \
	parrillada-burn-options.h                 \
	parrillada-dest-selection.c                 \
	parrillada-dest-selection.h                 \
	parrillada-drive-properties.c                 \
	parrillada-drive-properties.h                 \
	parrillada-image-properties.c                 \
	parrillada-image-properties.h                 \
	parrillada-image-type-chooser.c                 \
	parrillada-image-type-chooser.h                 \
	parrillada-medium-properties.c                 \
	parrillada-medium-properties.h                 \
	parrillada-progress.c                 \
	parrillada-progress.h                 \
	parrillada-session-cfg.c                 \
	parrillada-session-cfg.h                 \
	parrillada-src-image.c                 \
	parrillada-src-image.h                 \
	parrillada-src-selection.c                 \
	parrillada-src-selection.h                 \
	parrillada-tray.c                 \
	parrillada-tray.h                 \
	parrillada-burn-lib.h                 \
	parrillada-error.h                 \
	parrillada-enums.h                 \
	parrillada-tags.h                 \
	parrillada-cover.c                 \
	parrillada-cover.h                 \
	parrillada-track-disc.c                 \
	parrillada-track-disc.h                 \
	parrillada-track-stream.c                 \
	parrillada-track-stream.h                 \
	parrillada-track-image.c                 \
	parrillada-track-image.h                 \
	parrillada-track-data.c                 \
	parrillada-track-data.h                 \
	parrillada-track-image-cfg.c                 \
	parrillada-track-image-cfg.h                 \
	parrillada-caps-plugin.c                 \
	parrillada-caps-burn.c                 \
	parrillada-caps-session.c                 \
	parrillada-caps-burn.h                 \
	parrillada-plugin.h                 \
	parrillada-plugin-information.h                 \
	parrillada-plugin-registration.h                 \
	parrillada-track-type.c                 \
	parrillada-track-type.h                 \
	parrillada-track-type-private.h                 \
	parrillada-status.c                 \
	parrillada-status.h                 \
	parrillada-status-dialog.c                 \
	parrillada-status-dialog.h                 \
	parrillada-session-helper.h                 \
	parrillada-data-project.c                 \
	parrillada-data-project.h                 \
	parrillada-data-session.c                 \
	parrillada-data-session.h                 \
	parrillada-data-vfs.c                 \
	parrillada-data-vfs.h                 \
	parrillada-file-node.c                 \
	parrillada-file-node.h                 \
	parrillada-data-tree-model.c                 \
	parrillada-data-tree-model.h                 \
	parrillada-track-data-cfg.c                 \
	parrillada-track-data-cfg.h                 \
	parrillada-filtered-uri.c                 \
	parrillada-filtered-uri.h                 \
	parrillada-track-stream-cfg.c                 \
	parrillada-track-stream-cfg.h                 \
	parrillada-tool-dialog-private.h                 \
	parrillada-video-options.h                 \
	parrillada-video-options.c                 \
	parrillada-session-span.h                 \
	parrillada-session-span.c                 \
	parrillada-plugin-private.h                 

if BUILD_INOTIFY
libparrillada_burn@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES += parrillada-file-monitor.c parrillada-file-monitor.h
endif

EXTRA_DIST =			\
	libparrillada-marshal.list
#	libparrillada-burn.symbols

if HAVE_INTROSPECTION
girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = ParrilladaBurn-@PARRILLADA_VERSION@.gir

typelibsdir = $(INTROSPECTION_TYPELIBDIR)
typelibs_DATA = ParrilladaBurn-@PARRILLADA_VERSION@.typelib

ParrilladaBurn-@PARRILLADA_VERSION@.gir: libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la $(INTROSPECTION_SCANNER)
	$(INTROSPECTION_SCANNER) -v --namespace=ParrilladaBurn --nsversion=1.0 \
	    -I$(top_srcdir)/libparrillada-media	\
	    -I$(top_builddir)/libparrillada-media	\
	    -I$(top_srcdir)/libparrillada-burn		\
	    -I$(top_builddir)/libparrillada-burn	\
            --include=Gtk-@GTK_API_VERSION@ \
            --add-include-path=$(top_builddir)/libparrillada-media	\
	    --include=ParrilladaMedia-@PARRILLADA_VERSION@ \
	    --library=parrillada-burn@PARRILLADA_LIBRARY_SUFFIX@ \
	    --libtool="$(LIBTOOL)" \
            --output $@ \
            --pkg=gtk+-@GTK_API_VERSION@  \
	    --strip-prefix=Parrillada	\
            $(top_srcdir)/libparrillada-burn/parrillada-tags.h						\
            $(top_srcdir)/libparrillada-burn/parrillada-enums.h						\
            $(top_srcdir)/libparrillada-burn/parrillada-error.h						\
            $(top_srcdir)/libparrillada-burn/parrillada-status.h					\
            $(top_builddir)/libparrillada-burn/parrillada-burn-lib.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-track-type.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-track.h						\
            $(top_srcdir)/libparrillada-burn/parrillada-track-data.h                 			\
            $(top_srcdir)/libparrillada-burn/parrillada-track-data-cfg.h				\
            $(top_srcdir)/libparrillada-burn/parrillada-track-disc.h                 			\
            $(top_srcdir)/libparrillada-burn/parrillada-track-stream.h                 			\
            $(top_srcdir)/libparrillada-burn/parrillada-track-stream-cfg.h             			\
            $(top_srcdir)/libparrillada-burn/parrillada-track-image.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-track-image-cfg.h				\
            $(top_srcdir)/libparrillada-burn/parrillada-session.h                 			\
            $(top_srcdir)/libparrillada-burn/parrillada-session-span.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-session-cfg.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-burn.h						\
            $(top_srcdir)/libparrillada-burn/parrillada-burn-options.h        			        \
            $(top_srcdir)/libparrillada-burn/parrillada-burn-dialog.h					\
            $(top_srcdir)/libparrillada-burn/parrillada-tool-dialog.h					\
	    $(top_srcdir)/libparrillada-burn/parrillada-sum-dialog.h					\
	    $(top_srcdir)/libparrillada-burn/*.c	    

ParrilladaBurn-@PARRILLADA_VERSION@.typelib: $(gir_DATA) $(INTROSPECTION_COMPILER)
	$(INTROSPECTION_COMPILER) $(top_builddir)/libparrillada-burn/$(gir_DATA) --shared-library=libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@ -o $(typelibs_DATA) --includedir=$(top_builddir)/libparrillada-media
endif

CLEANFILES =			\
	$(RECMARSHALFILES)  \
	$(gir_DATA)         \
	$(typelibs_DATA)


-include $(top_srcdir)/git.mk


INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libparrillada-utils/				\
	-I$(top_builddir)/libparrillada-utils/				\
	-I$(top_srcdir)/libparrillada-media/				\
	-I$(top_builddir)/libparrillada-media/				\
	-I$(top_srcdir)/libparrillada-burn/				\
	-I$(top_builddir)/libparrillada-burn/				\
	-DPARRILLADA_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DPARRILLADA_PREFIX=\"$(prefix)\"           			\
	-DPARRILLADA_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DPARRILLADA_DATADIR=\"$(datadir)/parrillada\"     	    		\
	-DPARRILLADA_LIBDIR=\"$(libdir)\"  	         		\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(PARRILLADA_GLIB_CFLAGS)						\
	$(PARRILLADA_GIO_CFLAGS)						\
	$(PARRILLADA_GSTREAMER_CFLAGS)					\
	$(PARRILLADA_GSTREAMER_BASE_CFLAGS)				\
	$(PARRILLADA_GTK_CFLAGS)						\
	$(PARRILLADA_DBUS_CFLAGS)						\
	$(PARRILLADA_LIBXML_CFLAGS)					\
	$(PARRILLADA_LIBUNIQUE_CFLAGS) 					\
	$(PARRILLADA_SEARCH_CFLAGS)					\
	$(PARRILLADA_PL_PARSER_CFLAGS)					\
	$(PARRILLADA_SM_CFLAGS)						\
	$(PARRILLADA_ICE_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = parrillada-marshal.h parrillada-marshal.c

parrillada-marshal.h: parrillada-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/parrillada-marshal.list --header > parrillada-marshal.h )
parrillada-marshal.c: parrillada-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=parrillada_marshal $(srcdir)/parrillada-marshal.list --body --header > parrillada-marshal.c )

bin_PROGRAMS = parrillada

parrillada_SOURCES = \
	parrillada-marshal.c	\
	parrillada-marshal.h	\
	main.c		\
	parrillada-utils.h		\
	parrillada-utils.c         \
	parrillada-player.c         \
	parrillada-player.h         \
	parrillada-eject-dialog.c		\
	parrillada-eject-dialog.h		\
	parrillada-disc.c         \
	parrillada-disc.h         \
	parrillada-audio-disc.c         \
	parrillada-audio-disc.h         \
	parrillada-project.c         \
	parrillada-project.h         \
	parrillada-project-type-chooser.c         \
	parrillada-project-type-chooser.h         \
	parrillada-player-bacon.c         \
	parrillada-player-bacon.h         \
	parrillada-song-properties.c         \
	parrillada-song-properties.h         \
	parrillada-uri-container.c         \
	parrillada-uri-container.h         \
	parrillada-project-manager.c         \
	parrillada-project-manager.h         \
	parrillada-layout.c         \
	parrillada-layout.h         \
	parrillada-file-chooser.c         \
	parrillada-file-chooser.h         \
	eggtreemultidnd.c         \
	eggtreemultidnd.h         \
	parrillada-multi-dnd.c         \
	parrillada-multi-dnd.h         \
	parrillada-layout-object.c         \
	parrillada-layout-object.h         \
	parrillada-plugin-manager-ui.c         \
	parrillada-plugin-manager-ui.h         \
	parrillada-pref.h         \
	parrillada-pref.c         \
	parrillada-plugin-option.h         \
	parrillada-plugin-option.c         \
	parrillada-split-dialog.h         \
	parrillada-split-dialog.c         \
	parrillada-time-button.h         \
	parrillada-time-button.c         \
	parrillada-preview.h         \
	parrillada-preview.c         \
	parrillada-data-disc.h         \
	parrillada-data-disc.c         \
	parrillada-file-filtered.h         \
	parrillada-file-filtered.c         \
	parrillada-filter-option.h         \
	parrillada-filter-option.c         \
	baobab-cell-renderer-progress.c         \
	baobab-cell-renderer-progress.h         \
	parrillada-multi-song-props.h         \
	parrillada-multi-song-props.c         \
	parrillada-rename.h         \
	parrillada-rename.c         \
	parrillada-video-disc.c         \
	parrillada-video-disc.h         \
	parrillada-video-tree-model.c         \
	parrillada-video-tree-model.h         \
	parrillada-app.c		\
	parrillada-app.h           \
	parrillada-project-name.c           \
	parrillada-project-name.h           \
	parrillada-project-parse.c           \
	parrillada-project-parse.h           \
	parrillada-xsession.c           \
	parrillada-xsession.h		\
	eggdesktopfile.c        \
	eggdesktopfile.h        \
	eggsmclient.c        \
	eggsmclient.h        \
	eggsmclient-private.h        \
	eggsmclient-xsmp.c        \
	parrillada-setting.h        \
	parrillada-setting.c        \
	parrillada-search-engine.h        \
	parrillada-search-engine.c        \
	parrillada-drive-settings.h        \
	parrillada-drive-settings.c	\
	parrillada-song-control.h        \
	parrillada-song-control.c        \
	parrillada-cli.c        \
	parrillada-cli.h

if BUILD_PLAYLIST
parrillada_SOURCES += \
	parrillada-playlist.c         \
	parrillada-playlist.h

endif

if BUILD_TRACKER
parrillada_SOURCES += \
		   parrillada-search-tracker.h	\
		   parrillada-search-tracker.c
endif

if BUILD_BEAGLE
parrillada_SOURCES += 				\
		   parrillada-search-beagle.h	\
		   parrillada-search-beagle.c
endif

if BUILD_SEARCH

parrillada_SOURCES += \
		   parrillada-mime-filter.c         \
		   parrillada-mime-filter.h         \
		   parrillada-search-entry.c         \
		   parrillada-search-entry.h         \
		   parrillada-search.h		\
		   parrillada-search.c
endif

parrillada_LDADD =						\
	$(top_builddir)/libparrillada-media/libparrillada-media@PARRILLADA_LIBRARY_SUFFIX@.la	\
	$(top_builddir)/libparrillada-burn/libparrillada-burn@PARRILLADA_LIBRARY_SUFFIX@.la	\
	$(top_builddir)/libparrillada-utils/libparrillada-utils@PARRILLADA_LIBRARY_SUFFIX@.la	\
	$(PARRILLADA_GLIB_LIBS)		\
	$(PARRILLADA_GTHREAD_LIBS)				\
	$(PARRILLADA_GIO_LIBS)		\
	$(PARRILLADA_GSTREAMER_LIBS)	\
	$(PARRILLADA_GSTREAMER_BASE_LIBS)	\
	$(PARRILLADA_GTK_LIBS)		\
	$(PARRILLADA_GMODULE_LIBS)		\
	$(PARRILLADA_DBUS_LIBS)		\
	$(PARRILLADA_LIBXML_LIBS)		\
	$(PARRILLADA_LIBUNIQUE_LIBS) 	\
	$(PARRILLADA_SEARCH_LIBS)		\
	$(PARRILLADA_PL_PARSER_LIBS)	\
	$(PARRILLADA_SM_LIBS)		\
	$(PARRILLADA_ICE_LIBS)

EXTRA_DIST =			\
	parrillada-marshal.list

CLEANFILES =			\
	$(RECMARSHALFILES)


-include $(top_srcdir)/git.mk

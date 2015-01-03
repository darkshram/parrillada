
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
	$(PARRILLADA_GIO_CFLAGS)						\
	$(PARRILLADA_PL_PARSER_CFLAGS)					\
	$(PARRILLADA_GSTREAMER_CFLAGS)					\
	$(PARRILLADA_GSTREAMER_BASE_CFLAGS)				\
	$(PARRILLADA_GTK_CFLAGS)


lib_LTLIBRARIES = \
	libparrillada-utils@PARRILLADA_LIBRARY_SUFFIX@.la

libparrillada_utils@PARRILLADA_LIBRARY_SUFFIX@_la_LIBADD =					\
	$(PARRILLADA_GLIB_LIBS)					\
	$(PARRILLADA_GIO_LIBS)		\
	$(PARRILLADA_GSTREAMER_LIBS)	\
	$(PARRILLADA_GSTREAMER_BASE_LIBS)	\
	$(PARRILLADA_PL_PARSER_LIBS)	\
	$(PARRILLADA_GTK_LIBS)

libparrillada_utils@PARRILLADA_LIBRARY_SUFFIX@_la_LDFLAGS =					\
	-version-info $(LIBPARRILLADA_LT_VERSION)			\
	-no-undefined
#	-export-symbols ./libparrillada-utils.symbols

libparrillada_utils@PARRILLADA_LIBRARY_SUFFIX@_la_SOURCES =		\
	parrillada-disc-message.c        \
	parrillada-disc-message.h        \
	parrillada-misc.c        \
	parrillada-misc.h        \
	parrillada-notify.c        \
	parrillada-notify.h        \
	parrillada-jacket-background.c        \
	parrillada-jacket-background.h        \
	parrillada-jacket-buffer.c        \
	parrillada-jacket-buffer.h        \
	parrillada-jacket-edit.c        \
	parrillada-jacket-edit.h        \
	parrillada-jacket-font.c        \
	parrillada-jacket-font.h        \
	parrillada-jacket-view.c        \
	parrillada-jacket-view.h        \
	parrillada-tool-color-picker.c        \
	parrillada-tool-color-picker.h        \
	parrillada-async-task-manager.c        \
	parrillada-async-task-manager.h        \
	parrillada-io.c        \
	parrillada-io.h        \
	parrillada-metadata.c        \
	parrillada-metadata.h        \
	parrillada-pk.c        \
	parrillada-pk.h

# EXTRA_DIST =			\
#	libparrillada-utils.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)

-include $(top_srcdir)/git.mk

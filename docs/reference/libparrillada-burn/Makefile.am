## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=libparrillada-burn

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=../../../libparrillada-burn

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=--extra-dir=../libparrillada-media/html

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/libparrillada-burn/*.h
CFILE_GLOB=$(top_srcdir)/libparrillada-burn/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=	libparrillada-marshal.h	\
		parrillada-caps-burn.h		\
		parrillada-cover.h		\
		parrillada-data-project.h		\
		parrillada-data-session.h		\
		parrillada-data-tree-model.h		\
		parrillada-data-vfs.h		\
		parrillada-dest-selection.h		\
		parrillada-drive-properties.h		\
		parrillada-file-monitor.h		\
		parrillada-file-node.h		\
		parrillada-filtered-uri.h		\
		parrillada-image-properties.h		\
		parrillada-image-type-chooser.h		\
		parrillada-medium-properties.h		\
		parrillada-plugin.h		\
		parrillada-plugin-information.h		\
		parrillada-plugin-registration.h		\
		parrillada-progress.h		\
		parrillada-session-helper.h		\
		parrillada-src-image.h		\
		parrillada-src-selection.h		\
		parrillada-status-dialog.h		\
		parrillada-tool-dialog-private.h		\
		parrillada-track-type-private.h		\
		parrillada-tray.h		\
		parrillada-video-options.h		\
		parrillada-xfer.h		\
		burn-basics.h		\
		burn-caps.h		\
		burn-dbus.h		\
		burn-debug.h		\
		burn-image-format.h		\
		burn-job.h		\
		burn-mkisofs-base.h		\
		burn-plugin-manager.h		\
		burn-process.h		\
		burn-task-ctx.h		\
		burn-task.h		\
		burn-task-item.h


# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=version.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
INCLUDES=								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	$(PARRILLADA_GLIB_CFLAGS)						\
	$(PARRILLADA_IO_CFLAGS)						\
	$(PARRILLADA_GTK_CFLAGS)

GTKDOC_LIBS=$(top_builddir)/libparrillada-media/libparrillada-media.la $(top_builddir)/libparrillada-burn/libparrillada-burn.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Comment this out if you want your docs-status tested during 'make check'
#TESTS = $(GTKDOC_CHECK)



-include $(top_srcdir)/git.mk

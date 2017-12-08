
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
	$(DISABLE_DEPRECATED)					\
	$(PARRILLADA_GLIB_CFLAGS)					\
	$(PARRILLADA_LIBXML_CFLAGS)

vcdimagerdir = $(PARRILLADA_PLUGIN_DIRECTORY)
vcdimager_LTLIBRARIES = libparrillada-vcdimager.la

libparrillada_vcdimager_la_SOURCES = burn-vcdimager.c 
libparrillada_vcdimager_la_LIBADD = ../../libparrillada-burn/libparrillada-burn3.la $(PARRILLADA_GLIB_LIBS) $(PARRILLADA_LIBXML_LIBS)
libparrillada_vcdimager_la_LDFLAGS = -module -avoid-version


-include $(top_srcdir)/git.mk

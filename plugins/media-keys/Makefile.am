icondir = $(datadir)/icons/cafe
context = actions

NULL =

plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	csd_media-keys-manager-glue.h	\
	csd_marshal.h			\
	csd_marshal.c			\
	$(NULL)

csd_media-keys-manager-glue.h: csd_media-keys-manager.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=csd_media_keys_manager --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ ) \
	&& rm -f xgen-$(@F)

csd_marshal.c: csd_marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --body --prototypes --internal > $@

csd_marshal.h: csd_marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --header --internal > $@

libmedia_keys_la_SOURCES = 		\
	csd_media-keys-plugin.h		\
	csd_media-keys-plugin.c		\
	csd_media-keys-manager.h	\
	csd_media-keys-manager.c	\
	csd_media-keys-window.h		\
	csd_media-keys-window.c		\
	acme.h				\
	$(BUILT_SOURCES)		\
	$(NULL)

libmedia_keys_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DCTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libmedia_keys_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBCAFEMIXER_CFLAGS)		\
	$(LIBKANBERRA_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libmedia_keys_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = \
	$(top_builddir)/plugins/common/libcommon.la \
	$(SETTINGS_PLUGIN_LIBS) \
	$(LIBCAFEMIXER_LIBS) \
	$(LIBKANBERRA_LIBS) \
	-lm

plugin_in_files = 		\
	media-keys.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

noinst_PROGRAMS =				\
	test-media-keys				\
	test-media-window			\
	$(NULL)

test_media_window_SOURCES =			\
	csd_media-keys-window.c			\
	csd_media-keys-window.h			\
	test-media-window.c			\
	$(NULL)

test_media_window_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DDATADIR=\""$(datadir)"\"				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DCTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_window_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

test_media_window_LDADD = \
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(GST_LIBS)				\
	-lm

test_media_keys_SOURCES =			\
	csd_media-keys-manager.c		\
	csd_media-keys-manager.h		\
	csd_media-keys-window.h			\
	csd_media-keys-window.c			\
	test-media-keys.c			\
	$(BUILT_SOURCES)			\
	$(NULL)

test_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DCTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_keys_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBCAFEMIXER_CFLAGS)		\
	$(LIBKANBERRA_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

test_media_keys_LDADD = \
	$(top_builddir)/cafe-settings-daemon/libcsd_profile.la \
	$(top_builddir)/plugins/common/libcommon.la \
	$(SETTINGS_DAEMON_LIBS) \
	$(SETTINGS_PLUGIN_LIBS) \
	$(LIBCAFEMIXER_LIBS) \
	$(LIBKANBERRA_LIBS) \
	-lm

ctkbuilderdir = $(pkgdatadir)
ctkbuilder_DATA =	\
	acme.ui		\
	$(NULL)

EXTRA_DIST = 				\
	csd_media-keys-manager.xml	\
	csd_marshal.list		\
	$(plugin_in_files)		\
	$(ctkbuilder_DATA)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

plugin_LTLIBRARIES = \
	libdummy.la

libdummy_la_SOURCES = 		\
	csd-dummy-manager.c	\
	csd-dummy-manager.h	\
	csd-dummy-plugin.c	\
	csd-dummy-plugin.h

libdummy_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libdummy_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libdummy_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libdummy_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	dummy.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

# override to _not_ install the test plugin
# do not copy into your plugin
install-pluginDATA:
install-pluginLTLIBRARIES:

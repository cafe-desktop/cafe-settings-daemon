plugin_name = ally-settings

plugin_LTLIBRARIES = \
	liba11y-settings.la

liba11y_settings_la_SOURCES = 		\
	csd-a11y-settings-manager.c	\
	csd-a11y-settings-manager.h	\
	csd-a11y-settings-plugin.c	\
	csd-a11y-settings-plugin.h

liba11y_settings_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

liba11y_settings_la_CFLAGS =		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

liba11y_settings_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

liba11y_settings_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	a11y-settings.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

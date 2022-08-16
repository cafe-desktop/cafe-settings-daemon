NULL =

plugin_LTLIBRARIES =		\
	libxsettings.la		\
	$(NULL)

libxsettings_la_SOURCES =	\
	csd-xsettings-plugin.h	\
	csd-xsettings-plugin.c	\
	csd-xsettings-manager.h	\
	csd-xsettings-manager.c	\
	xsettings-common.h	\
	xsettings-common.c	\
	xsettings-manager.h	\
	xsettings-manager.c	\
	fontconfig-monitor.h	\
	fontconfig-monitor.c	\
	wm-common.h		\
	wm-common.c		\
	$(NULL)

libxsettings_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libxsettings_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(FONTCONFIG_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libxsettings_la_LDFLAGS = 	\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libxsettings_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(FONTCONFIG_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	xsettings.cafe-settings-plugin.desktop.in \
	$(NULL)

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

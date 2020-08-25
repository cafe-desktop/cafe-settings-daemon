plugin_LTLIBRARIES = \
	libmpris.la

libmpris_la_SOURCES = 		\
	bus-watch-namespace.c	\
	bus-watch-namespace.h	\
	msd-mpris-manager.c	\
	msd-mpris-manager.h	\
	msd-mpris-plugin.c	\
	msd-mpris-plugin.h

libmpris_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libmpris_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libmpris_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

libmpris_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	mpris.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@


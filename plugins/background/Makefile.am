NULL =

noinst_PROGRAMS = 			\
	test-background			\
	$(NULL)

test_background_SOURCES = 		\
	test-background.c		\
	csd_background-manager.h	\
	csd_background-manager.c	\
	$(NULL)

test_background_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

test_background_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(CAFE_DESKTOP_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

test_background_LDADD =		\
	$(top_builddir)/cafe-settings-daemon/libcsd_profile.la	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(CAFE_DESKTOP_LIBS)		\
	$(X11_LIBS) \
	$(NULL)

plugin_LTLIBRARIES = \
	libbackground.la		\
	$(NULL)

libbackground_la_SOURCES = 		\
	csd_background-plugin.h		\
	csd_background-plugin.c		\
	csd_background-manager.h	\
	csd_background-manager.c	\
	$(NULL)

libbackground_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-I$(top_srcdir)/plugins/background/libbackground   \
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libbackground_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(CAFE_DESKTOP_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libbackground_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)		\
	$(NULL)

libbackground_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(CAFE_DESKTOP_LIBS)		\
	$(NULL)

plugin_in_files = 		\
	background.cafe-settings-plugin.desktop.in	\
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

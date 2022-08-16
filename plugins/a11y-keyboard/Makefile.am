NULL =

ctkbuilderdir = $(pkgdatadir)
ctkbuilder_DATA = 			\
	csd-a11y-preferences-dialog.ui	\
	$(NULL)

noinst_PROGRAMS =				\
	test-a11y-preferences-dialog		\
	$(NULL)

test_a11y_preferences_dialog_SOURCES =		\
	csd-a11y-preferences-dialog.c		\
	csd-a11y-preferences-dialog.h		\
	test-a11y-preferences-dialog.c		\
	$(NULL)

test_a11y_preferences_dialog_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon			\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DCTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_a11y_preferences_dialog_CFLAGS =	\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

test_a11y_preferences_dialog_LDADD = \
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(NULL)

plugin_LTLIBRARIES = \
	liba11y-keyboard.la		\
	$(NULL)

liba11y_keyboard_la_SOURCES = 		\
	csd-a11y-keyboard-plugin.h	\
	csd-a11y-keyboard-plugin.c	\
	csd-a11y-keyboard-manager.h	\
	csd-a11y-keyboard-manager.c	\
	csd-a11y-preferences-dialog.h	\
	csd-a11y-preferences-dialog.c	\
	$(NULL)

liba11y_keyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DCTKBUILDERDIR=\""$(ctkbuilderdir)"\" \
	$(AM_CPPFLAGS)

liba11y_keyboard_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

liba11y_keyboard_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)		\
	$(NULL)

liba11y_keyboard_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(NULL)

plugin_in_files = 		\
	a11y-keyboard.cafe-settings-plugin.desktop.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(ctkbuilder_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

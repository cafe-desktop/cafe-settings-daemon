plugin_LTLIBRARIES = libhousekeeping.la

libhousekeeping_la_SOURCES = 		\
	csd-ldsm-dialog.c	\
	csd-ldsm-dialog.h	\
	csd-ldsm-trash-empty.c	\
	csd-ldsm-trash-empty.h	\
	csd-disk-space.c		\
	csd-disk-space.h		\
	csd-housekeeping-manager.c	\
	csd-housekeeping-manager.h	\
	csd-housekeeping-plugin.c	\
	csd-housekeeping-plugin.h

libhousekeeping_la_CPPFLAGS = 					\
	-I$(top_srcdir)/cafe-settings-daemon			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libhousekeeping_la_CFLAGS = 		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libhousekeeping_la_LDFLAGS = $(CSD_PLUGIN_LDFLAGS)

libhousekeeping_la_LIBADD = $(SETTINGS_PLUGIN_LIBS) $(GIOUNIX_LIBS) $(LIBNOTIFY_LIBS)

plugin_in_files = housekeeping.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)

DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

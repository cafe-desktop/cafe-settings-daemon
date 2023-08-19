NULL =

plugin_LTLIBRARIES = \
	libclipboard.la		\
	$(NULL)

libclipboard_la_SOURCES = 	\
	csd_clipboard-plugin.h	\
	csd_clipboard-plugin.c	\
	csd_clipboard-manager.h	\
	csd_clipboard-manager.c	\
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(NULL)

libclipboard_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libclipboard_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libclipboard_la_LDFLAGS = 	\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libclipboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	clipboard.cafe-settings-plugin.desktop.in	\
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

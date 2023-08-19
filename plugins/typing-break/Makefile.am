NULL =

plugin_LTLIBRARIES =		\
	libtyping-break.la	\
	$(NULL)

libtyping_break_la_SOURCES = 		\
	csd_typing-break-plugin.h	\
	csd_typing-break-plugin.c	\
	csd_typing-break-manager.h	\
	csd_typing-break-manager.c	\
	$(NULL)

libtyping_break_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libtyping_break_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libtyping_break_la_LDFLAGS =	\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libtyping_break_la_LIBADD =	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	typing-break.cafe-settings-plugin.desktop.in	\
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

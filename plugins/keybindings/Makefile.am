NULL =

plugin_LTLIBRARIES = \
	libkeybindings.la		\
	$(NULL)

libkeybindings_la_SOURCES = 		\
	dconf-util.h				\
	dconf-util.c				\
	csd-keybindings-plugin.h	\
	csd-keybindings-plugin.c	\
	csd-keybindings-manager.h	\
	csd-keybindings-manager.c	\
	$(NULL)

libkeybindings_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libkeybindings_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(DCONF_CFLAGS)			\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libkeybindings_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libkeybindings_la_LIBADD  = 		\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(DCONF_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	keybindings.cafe-settings-plugin.desktop.in	\
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

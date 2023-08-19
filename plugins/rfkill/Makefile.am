plugin_LTLIBRARIES = librfkill.la

librfkill_la_SOURCES = 		\
	csd_rfkill-plugin.c	\
	csd_rfkill-plugin.h	\
	csd_rfkill-manager.c	\
	csd_rfkill-manager.h	\
	rfkill-glib.c		\
	rfkill-glib.h		\
	rfkill.h

librfkill_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-I$(top_srcdir)/data/				\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

librfkill_la_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(RFKILL_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

librfkill_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

librfkill_la_LIBADD  =						\
	$(RFKILL_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = rfkill.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

udevrulesdir = $(prefix)/lib/udev/rules.d
udevrules_DATA = 61-cafe-settings-daemon-rfkill.rules

EXTRA_DIST = $(plugin_in_files) $(udevrules_DATA)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

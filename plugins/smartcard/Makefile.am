plugin_LTLIBRARIES = \
	libsmartcard.la

libsmartcard_la_SOURCES = \
	csd_smartcard-plugin.h  \
	csd_smartcard-plugin.c  \
	csd_smartcard.h         \
	csd_smartcard.c         \
	csd_smartcard-manager.h \
	csd_smartcard-manager.c

libsmartcard_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon \
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DCSD_SMARTCARD_MANAGER_NSS_DB=\""$(NSS_DATABASE)"\" \
	$(AM_CPPFLAGS)

libsmartcard_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(CAFE_CFLAGS)			\
	$(NSS_CFLAGS)			\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libsmartcard_la_LDFLAGS = \
	$(CSD_PLUGIN_LDFLAGS)

libsmartcard_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NSS_LIBS)

plugin_in_files = \
	smartcard.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

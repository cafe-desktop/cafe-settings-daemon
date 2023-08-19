icondir = $(datadir)/icons/hicolor
context = apps

BUILT_SOURCES =			\
	csd_xrandr-manager-glue.h

ICON_FILES = 			\
	csd_xrandr-16.png	\
	csd_xrandr-22.png	\
	csd_xrandr-24.png	\
	csd_xrandr-32.png	\
	csd_xrandr-48.png	\
	csd_xrandr-64.png	\
	csd_xrandr-96.png	\
	csd_xrandr.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/cafe-settings-daemon/xrandr
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/48x48/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/64x64/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/96x96/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-48.png $(DESTDIR)$(icondir)/48x48/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-64.png $(DESTDIR)$(icondir)/64x64/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr-96.png $(DESTDIR)$(icondir)/96x96/$(context)/csd_xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd_xrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/csd_xrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/48x48/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/64x64/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/96x96/$(context)/csd_xrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/csd_xrandr.svg

plugin_LTLIBRARIES = \
	libxrandr.la

csd_xrandr-manager-glue.h: csd_xrandr-manager.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=csd_xrandr_manager --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ ) \
	&& rm -f xgen-$(@F)

libxrandr_la_SOURCES = 		\
	$(BUILT_SOURCES)	\
	csd_xrandr-plugin.h	\
	csd_xrandr-plugin.c	\
	csd_xrandr-manager.h	\
	csd_xrandr-manager.c

libxrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/cafe-settings-daemon			\
	-DBINDIR=\"$(bindir)\"					\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libxrandr_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(RDA_CFLAGS)			\
	$(CAFE_DESKTOP_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libxrandr_la_LDFLAGS = 			\
	$(CSD_PLUGIN_LDFLAGS)

libxrandr_la_LIBADD  =			\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(RDA_LIBS)			\
	$(CAFE_DESKTOP_LIBS)

plugin_in_files =			\
	xrandr.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES) csd_xrandr-manager.xml
CLEANFILES = $(plugin_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

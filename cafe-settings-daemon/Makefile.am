NULL =

AM_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\"				\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DCAFE_SETTINGS_PLUGINDIR=\""$(plugindir)"\"		\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(SETTINGS_DAEMON_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)						\
	$(NULL)

noinst_LTLIBRARIES = 			\
	libcsd_profile.la		\
	$(NULL)

libcsd_profile_la_SOURCES =		\
	cafe-settings-profile.c	\
	cafe-settings-profile.h	\
	$(NULL)

libcsd_profile_la_CPPFLAGS = 		\
	$(AM_CPPFLAGS)
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(NULL)

libcsd_profile_la_CFLAGS =		\
	$(SIMPLE_GREETER_CFLAGS)	\
	$(NULL)

libcsd_profile_la_LIBADD =		\
	$(NULL)

libcsd_profile_la_LDFLAGS = 	\
	-export-dynamic 	\
	$(NULL)

csddir = $(libexecdir)

csd_PROGRAMS = \
	cafe-settings-daemon

cafe-settings-manager-glue.h: cafe-settings-manager.xml Makefile.am
	$(AM_V_GEN) dbus-binding-tool --prefix=cafe_settings_manager --mode=glib-server $< > $@

cafe-settings-client.h: cafe-settings-manager.xml Makefile.am
	$(AM_V_GEN) dbus-binding-tool --prefix=cafe_settings_manager --mode=glib-client $< > $@

BUILT_SOURCES = 			\
	cafe-settings-manager-glue.h	\
	cafe-settings-client.h		\
	$(NULL)

apidir   = $(includedir)/cafe-settings-daemon
api_DATA = 				\
	cafe-settings-client.h		\
	cafe-settings-plugin.h

cafe_settings_daemon_SOURCES =		\
	main.c				\
	cafe-settings-bus.c	\
	cafe-settings-bus.h	\
	cafe-settings-manager.c	\
	cafe-settings-manager.h	\
	cafe-settings-plugin.c		\
	cafe-settings-plugin.h		\
	cafe-settings-plugin-info.c	\
	cafe-settings-plugin-info.h	\
	cafe-settings-module.c		\
	cafe-settings-module.h		\
	$(NULL)

cafe_settings_daemon_CPPFLAGS = \
	$(AM_CPPFLAGS)

cafe_settings_daemon_CFLAGS = 		\
	$(CAFE_DESKTOP_CFLAGS)          \
	$(AM_CFLAGS)

cafe_settings_daemon_LDFLAGS = 	\
	$(AM_LDFLAGS)

cafe_settings_daemon_LDADD = 		\
	libcsd_profile.la		\
	$(SETTINGS_DAEMON_LIBS)	\
	$(CAFE_DESKTOP_LIBS)    \
	$(LIBNOTIFY_LIBS)

EXTRA_DIST = 				\
	$(Dbusapi_DATA)			\
	cafe-settings-manager.xml	\
	$(NULL)

CLEANFILES = 				\
	$(BUILT_SOURCES)		\
	$(NULL)


# vim: ts=8

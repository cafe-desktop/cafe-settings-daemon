plugin_LTLIBRARIES = libmouse.la

libmouse_la_SOURCES = 		\
	csd_mouse-plugin.h	\
	csd_mouse-plugin.c	\
	csd_mouse-manager.h	\
	csd_mouse-manager.c

libmouse_la_CPPFLAGS = \
	-I$(top_srcdir)/cafe-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-DCAFE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libmouse_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libmouse_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libmouse_la_LIBADD  =							\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)						\
	$(X11_LIBS)							\
	$(XINPUT_LIBS)

plugin_in_files = mouse.cafe-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.cafe-settings-plugin.desktop.in=.cafe-settings-plugin)

libexec_PROGRAMS = csd_locate-pointer

csd_locate_pointer_SOURCES = 	\
	csd_locate-pointer.h	\
	csd_locate-pointer.c	\
	csd_timeline.h		\
	csd_timeline.c

csd_locate_pointer_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

csd_locate_pointer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(X11_LIBS)			\
	-lm

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

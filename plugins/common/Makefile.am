
noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	eggaccelerators.c	\
	eggaccelerators.h	\
	csd_keygrab.c		\
	csd_keygrab.h		\
	csd_input-helper.c	\
	csd_input-helper.h	\
	csd_osd-window.c	\
	csd_osd-window.h

libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libcommon_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(XINPUT_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libcommon_la_LDFLAGS = \
	$(CSD_PLUGIN_LDFLAGS) $(XINPUT_LIBS) $(X11_LIBS)

libcommon_la_LIBADD  = \
	$(SETTINGS_PLUGIN_LIBS)		\
	$(XINPUT_LIBS)

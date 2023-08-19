dbus_servicesdir = $(datadir)/dbus-1/system-services
dbus_confdir = $(DBUS_SYS_DIR)
polkitdir = $(datadir)/polkit-1/actions

dbus_services_in_files = org.cafe.SettingsDaemon.DateTimeMechanism.service.in
polkit_in_files = org.cafe.settingsdaemon.datetimemechanism.policy.in

csd_datetime-mechanism-glue.h: $(srcdir)/csd_datetime-mechanism.xml
	$(AM_V_GEN) dbus-binding-tool \
			--prefix=csd_datetime_mechanism --mode=glib-server	\
			--output=csd_datetime-mechanism-glue.h			\
			$(srcdir)/csd_datetime-mechanism.xml

if HAVE_POLKIT
libexec_PROGRAMS = csd_datetime-mechanism
endif

csd_datetime_mechanism_SOURCES =		\
	csd_datetime-mechanism.c		\
	csd_datetime-mechanism.h		\
	csd_datetime-mechanism-main.c	\
	system-timezone.c			\
	system-timezone.h


if HAVE_POLKIT
BUILT_SOURCES = csd_datetime-mechanism-glue.h
endif

AM_CFLAGS = $(WARN_CFLAGS) $(SETTINGS_PLUGIN_CFLAGS) $(POLKIT_CFLAGS)
csd_datetime_mechanism_LDADD = $(POLKIT_LIBS) $(SETTINGS_PLUGIN_LIBS)


if HAVE_POLKIT
dbus_services_DATA = $(dbus_services_in_files:.service.in=.service)

$(dbus_services_DATA): $(dbus_services_in_files)
	$(AM_V_GEN)sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

dbus_conf_DATA = org.cafe.SettingsDaemon.DateTimeMechanism.conf

polkit_DATA = $(polkit_in_files:.policy.in=.policy)
$(polkit_DATA): $(polkit_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
else
dbus_services_DATA =
dbus_conf_DATA =
polkit_DATA =
endif

EXTRA_DIST =						\
	$(dbus_services_in_files)			\
	org.cafe.SettingsDaemon.DateTimeMechanism.conf	\
	$(polkit_in_files)				\
	csd_datetime-mechanism.xml

CLEANFILES = 		\
	org.cafe.SettingsDaemon.DateTimeMechanism.service	\
	org.cafe.settingsdaemon.datetimemechanism.policy	\
	$(BUILT_SOURCES)

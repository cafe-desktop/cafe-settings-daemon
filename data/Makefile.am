NULL =

csd_gschemas_in = \
	org.cafe.applications-at.gschema.xml.in \
	org.cafe.font-rendering.gschema.xml.in \
	org.cafe.peripherals-keyboard.gschema.xml.in \
	org.cafe.peripherals-mouse.gschema.xml.in \
	org.cafe.peripherals-smartcard.gschema.xml.in \
	org.cafe.peripherals-touchpad.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.a11y-keyboard.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.a11y-settings.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.background.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.clipboard.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.datetime.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.housekeeping.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.keybindings.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.keyboard.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.media-keys.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.mpris.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.mouse.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.rfkill.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.smartcard.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.sound.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.typing-break.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.xrandr.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.xrdb.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.xsettings.gschema.xml.in \
	org.cafe.SettingsDaemon.plugins.gschema.xml.in \
	$(NULL)

gsettings_SCHEMAS = $(csd_gschemas_in:.xml.in=.xml)

@GSETTINGS_RULES@

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cafe-settings-daemon.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

servicedir = $(datadir)/dbus-1/services
service_in_files = org.cafe.SettingsDaemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.cafe.SettingsDaemon.service: org.cafe.SettingsDaemon.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cafe-settings-daemon.pc

man_MANS = \
	cafe-settings-daemon.1 \
	csd_datetime-mechanism.1 \
	csd_locate-pointer.1

#disabled until we support it
xmldir = @CAFE_KEYBINDINGS_KEYSDIR@
xml_in_files = 50-accessibility.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

$(xml_DATA): $(xml_in_files)
	$(AM_V_GEN) GETTEXTDATADIR=$(top_srcdir) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = 				\
	$(csd_gschemas_in)		\
	$(service_in_files)		\
	$(desktop_in_files)		\
	$(xml_in_files)			\
	cafe-settings-daemon.pc.in	\
	$(man_MANS)				\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)			\
	$(service_DATA)			\
	$(desktop_DATA)			\
	$(xml_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in

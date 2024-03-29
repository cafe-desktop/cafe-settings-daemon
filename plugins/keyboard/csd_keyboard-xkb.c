/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <ctk/ctk.h>
#include <gio/gio.h>

#include <libcafekbd/cafekbd-status.h>
#include <libcafekbd/cafekbd-keyboard-drawing.h>
#include <libcafekbd/cafekbd-desktop-config.h>
#include <libcafekbd/cafekbd-keyboard-config.h>
#include <libcafekbd/cafekbd-util.h>

#include "csd_keyboard-xkb.h"
#include "delayed-dialog.h"
#include "cafe-settings-profile.h"

#define CTK_RESPONSE_PRINT 2

#define CAFEKBD_DESKTOP_SCHEMA "org.cafe.peripherals-keyboard-xkb.general"
#define CAFEKBD_KBD_SCHEMA "org.cafe.peripherals-keyboard-xkb.kbd"

#define KNOWN_FILES_KEY "known-file-list"
#define DISABLE_INDICATOR_KEY "disable-indicator"
#define DUPLICATE_LEDS_KEY "duplicate-leds"

static CsdKeyboardManager* manager = NULL;

static GSettings* settings_desktop;
static GSettings* settings_kbd;

static XklEngine* xkl_engine;
static XklConfigRegistry* xkl_registry = NULL;

static CafekbdDesktopConfig current_desktop_config;
static CafekbdKeyboardConfig current_kbd_config;

/* never terminated */
static CafekbdKeyboardConfig initial_sys_kbd_config;

static gboolean inited_ok = FALSE;

static PostActivationCallback pa_callback = NULL;
static void *pa_callback_user_data = NULL;

static CtkStatusIcon* icon = NULL;

static GHashTable* preview_dialogs = NULL;

static Atom caps_lock;
static Atom num_lock;
static Atom scroll_lock;

static CtkStatusIcon* indicator_icons[3];
static const gchar* indicator_on_icon_names[] = {
	"kbd-scrolllock-on",
	"kbd-numlock-on",
	"kbd-capslock-on"
};

static const gchar* indicator_off_icon_names[] = {
	"kbd-scrolllock-off",
	"kbd-numlock-off",
	"kbd-capslock-off"
};

static void
g_strv_behead (gchar **arr)
{
	if (arr == NULL || *arr == NULL)
		return;

	g_free (*arr);
	memmove (arr, arr + 1, g_strv_length (arr) * sizeof (gchar *));
}

static void
activation_error (void)
{
	char const *vendor = ServerVendor (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()));
	int release = VendorRelease (CDK_DISPLAY_XDISPLAY (cdk_display_get_default ()));
	CtkWidget *dialog;

	/* VNC viewers will not work, do not barrage them with warnings */
	if (NULL != vendor && NULL != strstr (vendor, "VNC"))
		return;

	dialog = ctk_message_dialog_new_with_markup (NULL,
						     0,
						     CTK_MESSAGE_ERROR,
						     CTK_BUTTONS_CLOSE,
						     _
						     ("Error activating XKB configuration.\n"
						      "It can happen under various circumstances:\n"
						      " • a bug in libxklavier library\n"
						      " • a bug in X server (xkbcomp, xmodmap utilities)\n"
						      " • X server with incompatible libxkbfile implementation\n\n"
						      "X server version data:\n%s\n%d\n"
						      "If you report this situation as a bug, please include:\n"
						      " • The result of <b>%s</b>\n"
						      " • The result of <b>%s</b>"),
						     vendor,
						     release,
						     "xprop -root | grep XKB",
						     "gsettings list-keys org.cafe.peripherals-keyboard-xkb.kbd");
	g_signal_connect (dialog, "response",
			  G_CALLBACK (ctk_widget_destroy), NULL);
	csd_delayed_show_dialog (dialog);
}

static void
apply_desktop_settings (void)
{
	gboolean show_leds;
	int i;
	if (!inited_ok)
		return;

	csd_keyboard_manager_apply_settings (manager);
	cafekbd_desktop_config_load_from_gsettings (&current_desktop_config);
	/* again, probably it would be nice to compare things
	   before activating them */
	cafekbd_desktop_config_activate (&current_desktop_config);

	show_leds = g_settings_get_boolean (settings_desktop, DUPLICATE_LEDS_KEY);
	for (i = sizeof (indicator_icons) / sizeof (indicator_icons[0]);
	     --i >= 0;) {
		ctk_status_icon_set_visible (indicator_icons[i],
					     show_leds);
	}
}

static void
apply_desktop_settings_cb (GSettings *settings, gchar *key, gpointer   user_data)
{
    apply_desktop_settings ();
}

static void
popup_menu_launch_capplet ()
{
	GAppInfo *info;
	CdkAppLaunchContext *context;
	GError *error = NULL;

	info = g_app_info_create_from_commandline ("cafe-keyboard-properties", NULL, 0, &error);

	if (info != NULL) {
		context = cdk_display_get_app_launch_context (cdk_display_get_default ());
		g_app_info_launch (info, NULL,
				   G_APP_LAUNCH_CONTEXT (context), &error);

		g_object_unref (info);
		g_object_unref (context);
	}

	if (error != NULL) {
		g_warning
		    ("Could not execute keyboard properties capplet: [%s]\n",
		     error->message);
		g_error_free (error);
	}
}

static void
show_layout_destroy (CtkWidget * dialog, gint group)
{
	g_hash_table_remove (preview_dialogs, GINT_TO_POINTER (group));
}

static void
popup_menu_show_layout ()
{
	CtkWidget *dialog;
	XklEngine *engine = xkl_engine_get_instance (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()));
	XklState *xkl_state = xkl_engine_get_current_state (engine);
	gpointer p = g_hash_table_lookup (preview_dialogs,
					  GINT_TO_POINTER
					  (xkl_state->group));
	gchar **group_names = cafekbd_status_get_group_names ();

	if (xkl_state->group < 0
	    || xkl_state->group >= g_strv_length (group_names)) {
		return;
	}

	if (p != NULL) {
		/* existing window */
		ctk_window_present (CTK_WINDOW (p));
		return;
	}

	dialog =
	    cafekbd_keyboard_drawing_new_dialog (xkl_state->group,
					      group_names
					      [xkl_state->group]);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (show_layout_destroy),
			  GINT_TO_POINTER (xkl_state->group));
	g_hash_table_insert (preview_dialogs,
			     GINT_TO_POINTER (xkl_state->group), dialog);
}

static void
popup_menu_set_group (CtkMenuItem * item, gpointer param)
{
	gint group_number = GPOINTER_TO_INT (param);
	XklEngine *engine = cafekbd_status_get_xkl_engine ();
	XklState st;
	Window cur;

	st.group = group_number;
	xkl_engine_allow_one_switch_to_secondary_group (engine);
	cur = xkl_engine_get_current_window (engine);
	if (cur != (Window) NULL) {
		xkl_debug (150, "Enforcing the state %d for window %lx\n",
			   st.group, cur);
		xkl_engine_save_state (engine,
				       xkl_engine_get_current_window
				       (engine), &st);
/*    XSetInputFocus(CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), cur, RevertToNone, CurrentTime );*/
	} else {
		xkl_debug (150,
			   "??? Enforcing the state %d for unknown window\n",
			   st.group);
		/* strange situation - bad things can happen */
	}
	xkl_engine_lock_group (engine, st.group);
}

static void
status_icon_popup_menu_cb (CtkStatusIcon * icon, guint button, guint time)
{
	CtkWidget *toplevel;
	CdkScreen *screen;
	CdkVisual *visual;
	CtkStyleContext *context;
	CtkMenu *popup_menu = CTK_MENU (ctk_menu_new ());
	CtkMenu *groups_menu = CTK_MENU (ctk_menu_new ());
	/*Set up theme and transparency support*/
	toplevel = ctk_widget_get_toplevel (CTK_WIDGET(popup_menu));
	/* Fix any failures of compiz/other wm's to communicate with ctk for transparency */
	screen = ctk_widget_get_screen(CTK_WIDGET(toplevel));
	visual = cdk_screen_get_rgba_visual(screen);
	ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);
	/* Set menu and it's toplevel window to follow panel theme */
	context = ctk_widget_get_style_context (CTK_WIDGET(toplevel));
	ctk_style_context_add_class(context,"gnome-panel-menu-bar");
	ctk_style_context_add_class(context,"cafe-panel-menu-bar");
	int i = 0;
	gchar **current_name = cafekbd_status_get_group_names ();

	CtkWidget *item = ctk_menu_item_new_with_mnemonic (_("_Layouts"));
	ctk_widget_show (item);
	ctk_menu_shell_append (CTK_MENU_SHELL (popup_menu), item);
	ctk_menu_item_set_submenu (CTK_MENU_ITEM (item),
				   CTK_WIDGET (groups_menu));

	item =
	    ctk_menu_item_new_with_mnemonic (_("Keyboard _Preferences"));
	ctk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_launch_capplet,
			  NULL);
	ctk_menu_shell_append (CTK_MENU_SHELL (popup_menu), item);

	item = ctk_menu_item_new_with_mnemonic (_("Show _Current Layout"));
	ctk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_show_layout, NULL);
	ctk_menu_shell_append (CTK_MENU_SHELL (popup_menu), item);

	for (i = 0; *current_name; i++, current_name++) {
		gchar *image_file = cafekbd_status_get_image_filename (i);

		if (image_file == NULL) {
			item =
			    ctk_menu_item_new_with_label (*current_name);
		} else {
			GdkPixbuf *pixbuf =
			    gdk_pixbuf_new_from_file_at_size (image_file,
							      24, 24,
							      NULL);
			CtkWidget *img =
			    ctk_image_new_from_pixbuf (pixbuf);
			item =
			    ctk_image_menu_item_new_with_label
			    (*current_name);
			ctk_widget_show (img);
			ctk_image_menu_item_set_image (CTK_IMAGE_MENU_ITEM
						       (item), img);
			ctk_image_menu_item_set_always_show_image
			    (CTK_IMAGE_MENU_ITEM (item), TRUE);
			g_free (image_file);
		}
		ctk_widget_show (item);
		ctk_menu_shell_append (CTK_MENU_SHELL (groups_menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (popup_menu_set_group),
				  GINT_TO_POINTER (i));
	}

	ctk_menu_popup (popup_menu, NULL, NULL,
			ctk_status_icon_position_menu,
			(gpointer) icon, button, time);
}

static void
show_hide_icon ()
{
	if (g_strv_length (current_kbd_config.layouts_variants) > 1) {
		if (icon == NULL) {
			gboolean disable = g_settings_get_boolean (settings_desktop, DISABLE_INDICATOR_KEY);
			if (disable)
				return;

			xkl_debug (150, "Creating new icon\n");
			icon = cafekbd_status_new ();
                        /* commenting out fixes a Cdk-critical warning */
/*			ctk_status_icon_set_name (icon, "keyboard");*/
			g_signal_connect (icon, "popup-menu",
					  G_CALLBACK
					  (status_icon_popup_menu_cb),
					  NULL);

		}
	} else {
		if (icon != NULL) {
			xkl_debug (150, "Destroying icon\n");
			g_object_unref (icon);
			icon = NULL;
		}
	}
}

static gboolean
try_activating_xkb_config_if_new (CafekbdKeyboardConfig *
				  current_sys_kbd_config)
{
	/* Activate - only if different! */
	if (!cafekbd_keyboard_config_equals
	    (&current_kbd_config, current_sys_kbd_config)) {
		if (cafekbd_keyboard_config_activate (&current_kbd_config)) {
			if (pa_callback != NULL) {
				(*pa_callback) (pa_callback_user_data);
				return TRUE;
			}
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
filter_xkb_config (void)
{
	XklConfigItem *item;
	gchar *lname;
	gchar *vname;
	gchar **lv;
	gboolean any_change = FALSE;

	xkl_debug (100, "Filtering configuration against the registry\n");
	if (!xkl_registry) {
		xkl_registry =
		    xkl_config_registry_get_instance (xkl_engine);
		/* load all materials, unconditionally! */
		if (!xkl_config_registry_load (xkl_registry, TRUE)) {
			g_object_unref (xkl_registry);
			xkl_registry = NULL;
			return FALSE;
		}
	}
	lv = current_kbd_config.layouts_variants;
	item = xkl_config_item_new ();
	while (*lv) {
		xkl_debug (100, "Checking [%s]\n", *lv);
		if (cafekbd_keyboard_config_split_items (*lv, &lname, &vname)) {
			gboolean should_be_dropped = FALSE;
			g_snprintf (item->name, sizeof (item->name), "%s",
				    lname);
			if (!xkl_config_registry_find_layout
			    (xkl_registry, item)) {
				xkl_debug (100, "Bad layout [%s]\n",
					   lname);
				should_be_dropped = TRUE;
			} else if (vname) {
				g_snprintf (item->name,
					    sizeof (item->name), "%s",
					    vname);
				if (!xkl_config_registry_find_variant
				    (xkl_registry, lname, item)) {
					xkl_debug (100,
						   "Bad variant [%s(%s)]\n",
						   lname, vname);
					should_be_dropped = TRUE;
				}
			}
			if (should_be_dropped) {
				g_strv_behead (lv);
				any_change = TRUE;
				continue;
			}
		}
		lv++;
	}
	g_object_unref (item);
	return any_change;
}

static void
apply_xkb_settings (void)
{
	CafekbdKeyboardConfig current_sys_kbd_config;

	if (!inited_ok)
		return;

	cafekbd_keyboard_config_init (&current_sys_kbd_config, xkl_engine);

	cafekbd_keyboard_config_load_from_gsettings (&current_kbd_config,
					      &initial_sys_kbd_config);

	cafekbd_keyboard_config_load_from_x_current (&current_sys_kbd_config,
						  NULL);

	if (!try_activating_xkb_config_if_new (&current_sys_kbd_config)) {
		if (filter_xkb_config ()) {
			if (!try_activating_xkb_config_if_new
			    (&current_sys_kbd_config)) {
				g_warning
				    ("Could not activate the filtered XKB configuration");
				activation_error ();
			}
		} else {
			g_warning
			    ("Could not activate the XKB configuration");
			activation_error ();
		}
	} else
		xkl_debug (100,
			   "Actual KBD configuration was not changed: redundant notification\n");

	cafekbd_keyboard_config_term (&current_sys_kbd_config);
	show_hide_icon ();
}

static void
apply_xkb_settings_cb (GSettings *settings, gchar *key, gpointer   user_data)
{
    apply_xkb_settings ();
}

static void
csd_keyboard_xkb_analyze_sysconfig (void)
{
	if (!inited_ok)
		return;

	cafekbd_keyboard_config_init (&initial_sys_kbd_config, xkl_engine);
	cafekbd_keyboard_config_load_from_x_initial (&initial_sys_kbd_config,
						  NULL);
}

void
csd_keyboard_xkb_set_post_activation_callback (PostActivationCallback fun,
					       void *user_data)
{
	pa_callback = fun;
	pa_callback_user_data = user_data;
}

static CdkFilterReturn
csd_keyboard_xkb_evt_filter (CdkXEvent * xev, CdkEvent * event)
{
	XEvent *xevent = (XEvent *) xev;
	xkl_engine_filter_events (xkl_engine, xevent);
	return CDK_FILTER_CONTINUE;
}

/* When new Keyboard is plugged in - reload the settings */
static void
csd_keyboard_new_device (XklEngine * engine)
{
	apply_desktop_settings ();
	apply_xkb_settings ();
}

static void
csd_keyboard_update_indicator_icons ()
{
	Bool state;
	int new_state, i;
	Display *display = CDK_DISPLAY_XDISPLAY(cdk_display_get_default());
	XkbGetNamedIndicator (display, caps_lock, NULL, &state,
			      NULL, NULL);
	new_state = state ? 1 : 0;
	XkbGetNamedIndicator (display, num_lock, NULL, &state, NULL, NULL);
	new_state <<= 1;
	new_state |= (state ? 1 : 0);
	XkbGetNamedIndicator (display, scroll_lock, NULL, &state,
			      NULL, NULL);
	new_state <<= 1;
	new_state |= (state ? 1 : 0);
	xkl_debug (160, "Indicators state: %d\n", new_state);


	for (i = sizeof (indicator_icons) / sizeof (indicator_icons[0]);
	     --i >= 0;) {
		ctk_status_icon_set_from_icon_name (indicator_icons[i],
						    (new_state & (1 << i))
						    ?
						    indicator_on_icon_names
						    [i] :
						    indicator_off_icon_names
						    [i]);
	}
}

static void
csd_keyboard_state_changed (XklEngine * engine, XklEngineStateChange type,
			    gint new_group, gboolean restore)
{
	xkl_debug (160,
		   "State changed: type %d, new group: %d, restore: %d.\n",
		   type, new_group, restore);
	if (type == INDICATORS_CHANGED) {
		csd_keyboard_update_indicator_icons ();
	}
}

void
csd_keyboard_xkb_init (CsdKeyboardManager * kbd_manager)
{
	int i;
	Display *display = CDK_DISPLAY_XDISPLAY(cdk_display_get_default());
	cafe_settings_profile_start (NULL);

	ctk_icon_theme_append_search_path (ctk_icon_theme_get_default (),
					   DATADIR G_DIR_SEPARATOR_S
					   "icons");

	caps_lock = XInternAtom (display, "Caps Lock", False);
	num_lock = XInternAtom (display, "Num Lock", False);
	scroll_lock = XInternAtom (display, "Scroll Lock", False);

	for (i = sizeof (indicator_icons) / sizeof (indicator_icons[0]);
	     --i >= 0;) {
		indicator_icons[i] =
		    ctk_status_icon_new_from_icon_name
		    (indicator_off_icon_names[i]);
	}

	csd_keyboard_update_indicator_icons ();

	manager = kbd_manager;
	cafe_settings_profile_start ("xkl_engine_get_instance");
	xkl_engine = xkl_engine_get_instance (display);
	cafe_settings_profile_end ("xkl_engine_get_instance");
	if (xkl_engine) {
		inited_ok = TRUE;

		settings_desktop = g_settings_new (CAFEKBD_DESKTOP_SCHEMA);
		settings_kbd = g_settings_new (CAFEKBD_KBD_SCHEMA);

		cafekbd_desktop_config_init (&current_desktop_config,
		                             xkl_engine);
		cafekbd_keyboard_config_init (&current_kbd_config,
		                              xkl_engine);

		xkl_engine_backup_names_prop (xkl_engine);
		csd_keyboard_xkb_analyze_sysconfig ();

		cafekbd_desktop_config_start_listen (&current_desktop_config,
		                                     G_CALLBACK (apply_desktop_settings_cb),
		                                     NULL);

		cafekbd_keyboard_config_start_listen (&current_kbd_config,
		                                      G_CALLBACK (apply_xkb_settings_cb),
		                                      NULL);

		g_signal_connect (settings_desktop, "changed",
		                  G_CALLBACK (apply_desktop_settings_cb), NULL);
		g_signal_connect (settings_kbd, "changed",
		                  G_CALLBACK (apply_xkb_settings_cb), NULL);

		cdk_window_add_filter (NULL, (CdkFilterFunc)
				       csd_keyboard_xkb_evt_filter, NULL);

		if (xkl_engine_get_features (xkl_engine) &
		    XKLF_DEVICE_DISCOVERY)
			g_signal_connect (xkl_engine, "X-new-device",
					  G_CALLBACK
					  (csd_keyboard_new_device), NULL);
		g_signal_connect (xkl_engine, "X-state-changed",
				  G_CALLBACK
				  (csd_keyboard_state_changed), NULL);

		cafe_settings_profile_start ("xkl_engine_start_listen");
		xkl_engine_start_listen (xkl_engine,
					 XKLL_MANAGE_LAYOUTS |
					 XKLL_MANAGE_WINDOW_STATES);
		cafe_settings_profile_end ("xkl_engine_start_listen");

		cafe_settings_profile_start ("apply_desktop_settings");
		apply_desktop_settings ();
		cafe_settings_profile_end ("apply_desktop_settings");
		cafe_settings_profile_start ("apply_xkb_settings");
		apply_xkb_settings ();
		cafe_settings_profile_end ("apply_xkb_settings");
	}
	preview_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);

	cafe_settings_profile_end (NULL);
}

void
csd_keyboard_xkb_shutdown (void)
{
	int i;

	pa_callback = NULL;
	pa_callback_user_data = NULL;
	manager = NULL;

	for (i = sizeof (indicator_icons) / sizeof (indicator_icons[0]);
	     --i >= 0;) {
		g_object_unref (G_OBJECT (indicator_icons[i]));
		indicator_icons[i] = NULL;
	}

	g_hash_table_destroy (preview_dialogs);

	if (!inited_ok)
		return;

	xkl_engine_stop_listen (xkl_engine,
				XKLL_MANAGE_LAYOUTS |
				XKLL_MANAGE_WINDOW_STATES);

	cdk_window_remove_filter (NULL, (CdkFilterFunc)
				  csd_keyboard_xkb_evt_filter, NULL);

	if (settings_desktop != NULL) {
		g_object_unref (settings_desktop);
	}

	if (settings_kbd != NULL) {
		g_object_unref (settings_kbd);
	}

	if (xkl_registry) {
		g_object_unref (xkl_registry);
	}

	g_object_unref (xkl_engine);

	xkl_engine = NULL;
	inited_ok = FALSE;
}

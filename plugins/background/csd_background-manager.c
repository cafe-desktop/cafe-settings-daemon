/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2007 Red Hat, Inc.
 * Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <gio/gio.h>

#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-bg.h>
#include <X11/Xatom.h>

#include "cafe-settings-profile.h"
#include "csd_background-manager.h"

#define CAFE_SESSION_MANAGER_DBUS_NAME "org.gnome.SessionManager"
#define CAFE_SESSION_MANAGER_DBUS_PATH "/org/gnome/SessionManager"

struct CsdBackgroundManagerPrivate {
	GSettings       *settings;
	CafeBG          *bg;
	cairo_surface_t *surface;
	CafeBGCrossfade *fade;
	GList           *scr_sizes;

	gboolean         csd_can_draw;
	gboolean         baul_can_draw;
	gboolean         do_fade;
	gboolean         draw_in_progress;

	guint            timeout_id;

	GDBusProxy      *proxy;
	guint            proxy_signal_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (CsdBackgroundManager, csd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

/* Whether CSD is allowed to draw background */
static gboolean
csd_can_draw_bg (CsdBackgroundManager *manager)
{
	return g_settings_get_boolean (manager->priv->settings, CAFE_BG_KEY_DRAW_BACKGROUND);
}

/* Whether to change background with a fade effect */
static gboolean
can_fade_bg (CsdBackgroundManager *manager)
{
	return g_settings_get_boolean (manager->priv->settings, CAFE_BG_KEY_BACKGROUND_FADE);
}

/* Whether Baul is configured to draw desktop (show-desktop-icons) */
static gboolean
baul_can_draw_bg (CsdBackgroundManager *manager)
{
	return g_settings_get_boolean (manager->priv->settings, CAFE_BG_KEY_SHOW_DESKTOP);
}

static gboolean
baul_is_drawing_bg (CsdBackgroundManager *manager)
{
	Display       *display = cdk_x11_get_default_xdisplay ();
	Window         window = cdk_x11_get_default_root_xwindow ();
	Atom           baul_prop, wmclass_prop, type;
	Window         baul_window;
	int            format;
	unsigned long  nitems, after;
	unsigned char *data;
	CdkDisplay    *cdk_display;
	gboolean       running = FALSE;

	if (!manager->priv->baul_can_draw)
		return FALSE;

	baul_prop = XInternAtom (display, "BAUL_DESKTOP_WINDOW_ID", True);
	if (baul_prop == None)
		return FALSE;

	XGetWindowProperty (display, window, baul_prop, 0, 1, False,
			    XA_WINDOW, &type, &format, &nitems, &after, &data);

	if (data == NULL)
		return FALSE;

	baul_window = *(Window *) data;
	XFree (data);

	if (type != XA_WINDOW || format != 32)
		return FALSE;

	wmclass_prop = XInternAtom (display, "WM_CLASS", True);
	if (wmclass_prop == None)
		return FALSE;

	cdk_display = cdk_display_get_default ();
	cdk_x11_display_error_trap_push (cdk_display);

	XGetWindowProperty (display, baul_window, wmclass_prop, 0, 20, False,
			    XA_STRING, &type, &format, &nitems, &after, &data);

	XSync (display, False);

	if (cdk_x11_display_error_trap_pop (cdk_display) == BadWindow || data == NULL)
		return FALSE;

	/* See: baul_desktop_window_new(), in src/baul-desktop-window.c */
	if (nitems == 20 && after == 0 && format == 8 &&
	    !strcmp((char*) data, "desktop_window") &&
	    !strcmp((char*) data + strlen((char*) data) + 1, "Baul"))
	{
		running = TRUE;
	}
	XFree (data);

	return running;
}

static void
free_fade (CsdBackgroundManager *manager)
{
	if (manager->priv->fade != NULL) {
		g_object_unref (manager->priv->fade);
		manager->priv->fade = NULL;
	}
}

static void
free_bg_surface (CsdBackgroundManager *manager)
{
	if (manager->priv->surface != NULL) {
		cairo_surface_destroy (manager->priv->surface);
		manager->priv->surface = NULL;
	}
}

static void
free_scr_sizes (CsdBackgroundManager *manager)
{
	if (manager->priv->scr_sizes != NULL) {
		g_list_foreach (manager->priv->scr_sizes, (GFunc)g_free, NULL);
		g_list_free (manager->priv->scr_sizes);
		manager->priv->scr_sizes = NULL;
	}
}

static void
real_draw_bg (CsdBackgroundManager *manager,
	      CdkScreen		   *screen)
{
	CsdBackgroundManagerPrivate *p = manager->priv;
	CdkWindow *window = cdk_screen_get_root_window (screen);
	gint scale   = cdk_window_get_scale_factor (window);
	gint width   = WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;
	gint height  = HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;

	free_bg_surface (manager);
	p->surface = cafe_bg_create_surface_scale (p->bg, window, width, height, scale, TRUE);

	if (p->do_fade)
	{
		free_fade (manager);
		p->fade = cafe_bg_set_surface_as_root_with_crossfade (screen, p->surface);
		g_signal_connect_swapped (p->fade, "finished", G_CALLBACK (free_fade), manager);
	}
	else
	{
		cafe_bg_set_surface_as_root (screen, p->surface);
	}
	p->scr_sizes = g_list_prepend (p->scr_sizes, g_strdup_printf ("%dx%d", width, height));
}

static void
draw_background (CsdBackgroundManager *manager,
		 gboolean              may_fade)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	if (!p->csd_can_draw || p->draw_in_progress || baul_is_drawing_bg (manager))
		return;

	cafe_settings_profile_start (NULL);

	CdkDisplay *display   = cdk_display_get_default ();

	p->draw_in_progress = TRUE;
	p->do_fade = may_fade && can_fade_bg (manager);
	free_scr_sizes (manager);

	g_debug ("Drawing background on Screen");
	real_draw_bg (manager, cdk_display_get_default_screen (display));

	p->scr_sizes = g_list_reverse (p->scr_sizes);

	p->draw_in_progress = FALSE;
	cafe_settings_profile_end (NULL);
}

static void
on_bg_changed (CafeBG               *bg,
	       CsdBackgroundManager *manager)
{
	g_debug ("Background changed");
	draw_background (manager, TRUE);
}

static void
on_bg_transitioned (CafeBG               *bg,
		    CsdBackgroundManager *manager)
{
	g_debug ("Background transitioned");
	draw_background (manager, FALSE);
}

static void
on_screen_size_changed (CdkScreen            *screen,
			CsdBackgroundManager *manager)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	if (!p->csd_can_draw || p->draw_in_progress || baul_is_drawing_bg (manager))
		return;

	CdkWindow *window = cdk_screen_get_root_window (screen);
	gint scale = cdk_window_get_scale_factor (window);
	gint scr_num = cdk_x11_screen_get_screen_number (screen);
	gchar *old_size = g_list_nth_data (manager->priv->scr_sizes, scr_num);
	gchar *new_size = g_strdup_printf ("%dx%d", WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale,
						    HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale);
	if (g_strcmp0 (old_size, new_size) != 0)
	{
		g_debug ("Screen%d size changed: %s -> %s", scr_num, old_size, new_size);
		draw_background (manager, FALSE);
	} else {
		g_debug ("Screen%d size unchanged (%s). Ignoring.", scr_num, old_size);
	}
	g_free (new_size);
}

static void
disconnect_screen_signals (CsdBackgroundManager *manager)
{
	CdkDisplay *display   = cdk_display_get_default();

	g_signal_handlers_disconnect_by_func
		(cdk_display_get_default_screen (display),
		 G_CALLBACK (on_screen_size_changed), manager);
}

static void
connect_screen_signals (CsdBackgroundManager *manager)
{
	CdkDisplay *display   = cdk_display_get_default();

	CdkScreen *screen = cdk_display_get_default_screen (display);

	g_signal_connect (screen, "monitors-changed",
			  G_CALLBACK (on_screen_size_changed), manager);
	g_signal_connect (screen, "size-changed",
			  G_CALLBACK (on_screen_size_changed), manager);
}

static gboolean
settings_change_event_idle_cb (CsdBackgroundManager *manager)
{
	cafe_settings_profile_start ("settings_change_event_idle_cb");

	cafe_bg_load_from_preferences (manager->priv->bg);

	cafe_settings_profile_end ("settings_change_event_idle_cb");

	return FALSE;   /* remove from the list of event sources */
}

static gboolean
settings_change_event_cb (GSettings            *settings,
			  gpointer              keys,
			  gint                  n_keys,
			  CsdBackgroundManager *manager)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	/* Complements on_bg_handling_changed() */
	p->csd_can_draw = csd_can_draw_bg (manager);
	p->baul_can_draw = baul_can_draw_bg (manager);

	if (p->csd_can_draw && p->bg != NULL && !baul_is_drawing_bg (manager))
	{
		/* Defer signal processing to avoid making the dconf backend deadlock */
		g_idle_add ((GSourceFunc) settings_change_event_idle_cb, manager);
	}

	return FALSE;   /* let the event propagate further */
}

static void
setup_background (CsdBackgroundManager *manager)
{
	CsdBackgroundManagerPrivate *p = manager->priv;
	g_return_if_fail (p->bg == NULL);

	p->bg = cafe_bg_new();

	p->draw_in_progress = FALSE;

	g_signal_connect(p->bg, "changed", G_CALLBACK (on_bg_changed), manager);

	g_signal_connect(p->bg, "transitioned", G_CALLBACK (on_bg_transitioned), manager);

	cafe_bg_load_from_gsettings (p->bg, p->settings);

	connect_screen_signals (manager);

	g_signal_connect (p->settings, "change-event",
			  G_CALLBACK (settings_change_event_cb), manager);
}

static void
remove_background (CsdBackgroundManager *manager)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	disconnect_screen_signals (manager);

	g_signal_handlers_disconnect_by_func (p->settings, settings_change_event_cb, manager);

	if (p->settings != NULL) {
		g_object_unref (G_OBJECT (p->settings));
		p->settings = NULL;
	}

	if (p->bg != NULL) {
		g_object_unref (G_OBJECT (p->bg));
		p->bg = NULL;
	}

	free_scr_sizes (manager);
	free_bg_surface (manager);
	free_fade (manager);
}

static void
on_bg_handling_changed (GSettings            *settings,
			const char           *key,
			CsdBackgroundManager *manager)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	cafe_settings_profile_start (NULL);

	if (baul_is_drawing_bg (manager))
	{
		if (p->bg != NULL)
			remove_background (manager);
	}
	else if (p->csd_can_draw && p->bg == NULL)
	{
		setup_background (manager);
	}

	cafe_settings_profile_end (NULL);
}

static gboolean
queue_setup_background (CsdBackgroundManager *manager)
{
	manager->priv->timeout_id = 0;

	setup_background (manager);

	return FALSE;
}

static void
queue_timeout (CsdBackgroundManager *manager)
{
	if (manager->priv->timeout_id > 0)
		return;

	/* SessionRunning: now check if Baul is drawing background, and if not, set it.
	 *
	 * FIXME: We wait a few seconds after the session is up because Baul tells the
	 * session manager that its ready before it sets the background.
	 * https://bugzilla.gnome.org/show_bug.cgi?id=568588
	 */
	manager->priv->timeout_id =
		g_timeout_add_seconds (8, (GSourceFunc) queue_setup_background, manager);
}

static void
disconnect_session_manager_listener (CsdBackgroundManager* manager)
{
	if (manager->priv->proxy && manager->priv->proxy_signal_id) {
		g_signal_handler_disconnect (manager->priv->proxy,
					     manager->priv->proxy_signal_id);
		manager->priv->proxy_signal_id = 0;
	}
}

static void
on_session_manager_signal (GDBusProxy   *proxy,
			   const gchar  *sender_name,
			   const gchar  *signal_name,
			   GVariant     *parameters,
			   gpointer      user_data)
{
	CsdBackgroundManager *manager = CSD_BACKGROUND_MANAGER (user_data);

	if (g_strcmp0 (signal_name, "SessionRunning") == 0) {
		queue_timeout (manager);
		disconnect_session_manager_listener (manager);
	}
}

static void
draw_bg_after_session_loads (CsdBackgroundManager *manager)
{
	GError *error = NULL;
	GDBusProxyFlags flags;

	flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
	manager->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							      flags,
							      NULL, /* GDBusInterfaceInfo */
							      CAFE_SESSION_MANAGER_DBUS_NAME,
							      CAFE_SESSION_MANAGER_DBUS_PATH,
							      CAFE_SESSION_MANAGER_DBUS_NAME,
							      NULL, /* GCancellable */
							      &error);
	if (manager->priv->proxy == NULL) {
		g_warning ("Could not listen to session manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	manager->priv->proxy_signal_id = g_signal_connect (manager->priv->proxy,
							   "g-signal",
							   G_CALLBACK (on_session_manager_signal),
							   manager);
}

gboolean
csd_background_manager_start (CsdBackgroundManager  *manager,
			      GError               **error)
{
	CsdBackgroundManagerPrivate *p = manager->priv;

	g_debug ("Starting background manager");
	cafe_settings_profile_start (NULL);

	p->settings = g_settings_new (CAFE_BG_SCHEMA);

	p->csd_can_draw = csd_can_draw_bg (manager);
	p->baul_can_draw = baul_can_draw_bg (manager);

	g_signal_connect (p->settings, "changed::" CAFE_BG_KEY_DRAW_BACKGROUND,
			  G_CALLBACK (on_bg_handling_changed), manager);
	g_signal_connect (p->settings, "changed::" CAFE_BG_KEY_SHOW_DESKTOP,
			  G_CALLBACK (on_bg_handling_changed), manager);

	/* If Baul is set to draw the background, it is very likely in our session.
	 * But it might not be started yet, so baul_is_drawing_bg() would fail.
	 * In this case, we wait till the session is loaded, to avoid double-draws.
	 */
	if (p->csd_can_draw)
	{
		if (p->baul_can_draw)
		{
			draw_bg_after_session_loads (manager);
		}
		else
		{
			setup_background (manager);
		}
	}

	cafe_settings_profile_end (NULL);

	return TRUE;
}

void
csd_background_manager_stop (CsdBackgroundManager *manager)
{
	g_debug ("Stopping background manager");

	if (manager->priv->proxy)
	{
		disconnect_session_manager_listener (manager);
		g_object_unref (manager->priv->proxy);
	}

	if (manager->priv->timeout_id != 0) {
		g_source_remove (manager->priv->timeout_id);
		manager->priv->timeout_id = 0;
	}

	remove_background (manager);
}

static GObject*
csd_background_manager_constructor (GType                  type,
				    guint                  n_construct_properties,
				    GObjectConstructParam* construct_properties)
{
	CsdBackgroundManager *manager =
	   CSD_BACKGROUND_MANAGER (
	      G_OBJECT_CLASS (csd_background_manager_parent_class)->constructor (
				type, n_construct_properties, construct_properties));

	return G_OBJECT(manager);
}

static void
csd_background_manager_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (CSD_IS_BACKGROUND_MANAGER (object));

	CsdBackgroundManager *manager = CSD_BACKGROUND_MANAGER (object);

	g_return_if_fail (manager->priv != NULL);

	G_OBJECT_CLASS(csd_background_manager_parent_class)->finalize(object);
}

static void
csd_background_manager_init (CsdBackgroundManager* manager)
{
	manager->priv = csd_background_manager_get_instance_private(manager);
}

static void
csd_background_manager_class_init (CsdBackgroundManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->constructor = csd_background_manager_constructor;
	object_class->finalize = csd_background_manager_finalize;
}

CsdBackgroundManager*
csd_background_manager_new (void)
{
	if (manager_object != NULL)
	{
		g_object_ref(manager_object);
	}
	else
	{
		manager_object = g_object_new(CSD_TYPE_BACKGROUND_MANAGER, NULL);

		g_object_add_weak_pointer(manager_object, (gpointer*) &manager_object);
	}

	return CSD_BACKGROUND_MANAGER(manager_object);
}

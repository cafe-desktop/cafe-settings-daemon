/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <glib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>

#include <gio/gio.h>

#include "cafe-settings-plugin.h"
#include "csd-smartcard-plugin.h"
#include "csd-smartcard-manager.h"

struct CsdSmartcardPluginPrivate {
        CsdSmartcardManager *manager;
        DBusGConnection     *bus_connection;

        guint32              is_active : 1;
};

typedef enum
{
    MSD_SMARTCARD_REMOVE_ACTION_NONE,
    MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN,
    MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT,
} CsdSmartcardRemoveAction;

#define SCREENSAVER_DBUS_NAME      "org.cafe.ScreenSaver"
#define SCREENSAVER_DBUS_PATH      "/"
#define SCREENSAVER_DBUS_INTERFACE "org.cafe.ScreenSaver"

#define SM_DBUS_NAME      "org.gnome.SessionManager"
#define SM_DBUS_PATH      "/org/gnome/SessionManager"
#define SM_DBUS_INTERFACE "org.gnome.SessionManager"
#define SM_LOGOUT_MODE_FORCE 2

#define MSD_SMARTCARD_SCHEMA "org.cafe.peripherals-smartcard"
#define KEY_REMOVE_ACTION "removal-action"

CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (CsdSmartcardPlugin, csd_smartcard_plugin);

static void
simulate_user_activity (CsdSmartcardPlugin *plugin)
{
        DBusGProxy *screensaver_proxy;

        g_debug ("CsdSmartcardPlugin telling screensaver about smart card insertion");
        screensaver_proxy = dbus_g_proxy_new_for_name (plugin->priv->bus_connection,
                                                       SCREENSAVER_DBUS_NAME,
                                                       SCREENSAVER_DBUS_PATH,
                                                       SCREENSAVER_DBUS_INTERFACE);

        dbus_g_proxy_call_no_reply (screensaver_proxy,
                                    "SimulateUserActivity",
                                    G_TYPE_INVALID, G_TYPE_INVALID);

        g_object_unref (screensaver_proxy);
}

static void
lock_screen (CsdSmartcardPlugin *plugin)
{
        DBusGProxy *screensaver_proxy;

        g_debug ("CsdSmartcardPlugin telling screensaver to lock screen");
        screensaver_proxy = dbus_g_proxy_new_for_name (plugin->priv->bus_connection,
                                                       SCREENSAVER_DBUS_NAME,
                                                       SCREENSAVER_DBUS_PATH,
                                                       SCREENSAVER_DBUS_INTERFACE);

        dbus_g_proxy_call_no_reply (screensaver_proxy,
                                    "Lock",
                                    G_TYPE_INVALID, G_TYPE_INVALID);

        g_object_unref (screensaver_proxy);
}

static void
force_logout (CsdSmartcardPlugin *plugin)
{
        DBusGProxy *sm_proxy;
        GError     *error;
        gboolean    res;

        g_debug ("CsdSmartcardPlugin telling session manager to force logout");
        sm_proxy = dbus_g_proxy_new_for_name (plugin->priv->bus_connection,
                                              SM_DBUS_NAME,
                                              SM_DBUS_PATH,
                                              SM_DBUS_INTERFACE);

        error = NULL;
        res = dbus_g_proxy_call (sm_proxy,
                                 "Logout",
                                 &error,
                                 G_TYPE_UINT, SM_LOGOUT_MODE_FORCE,
                                 G_TYPE_INVALID, G_TYPE_INVALID);

        if (! res) {
                g_warning ("CsdSmartcardPlugin Unable to force logout: %s", error->message);
                g_error_free (error);
        }

        g_object_unref (sm_proxy);
}

static void
csd_smartcard_plugin_init (CsdSmartcardPlugin *plugin)
{
        plugin->priv = csd_smartcard_plugin_get_instance_private (plugin);

        g_debug ("CsdSmartcardPlugin initializing");

        plugin->priv->manager = csd_smartcard_manager_new (NULL);
}

static void
csd_smartcard_plugin_finalize (GObject *object)
{
        CsdSmartcardPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_SMARTCARD_PLUGIN (object));

        g_debug ("CsdSmartcardPlugin finalizing");

        plugin = MSD_SMARTCARD_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_smartcard_plugin_parent_class)->finalize (object);
}

static void
smartcard_inserted_cb (CsdSmartcardManager *card_monitor,
                       CsdSmartcard        *card,
                       CsdSmartcardPlugin  *plugin)
{
        char *name;

        name = csd_smartcard_get_name (card);
        g_debug ("CsdSmartcardPlugin smart card '%s' inserted", name);
        g_free (name);

        simulate_user_activity (plugin);
}

static gboolean
user_logged_in_with_smartcard (void)
{
    return g_getenv ("PKCS11_LOGIN_TOKEN_NAME") != NULL;
}

static CsdSmartcardRemoveAction
get_configured_remove_action (CsdSmartcardPlugin *plugin)
{
        GSettings *settings;
        char *remove_action_string;
        CsdSmartcardRemoveAction remove_action;

        settings = g_settings_new (MSD_SMARTCARD_SCHEMA);
        remove_action_string = g_settings_get_string (settings,
                                                      KEY_REMOVE_ACTION);

        if (remove_action_string == NULL) {
                g_warning ("CsdSmartcardPlugin unable to get smartcard remove action");
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "none") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "lock_screen") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN;
        } else if (strcmp (remove_action_string, "force_logout") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT;
        } else {
                g_warning ("CsdSmartcardPlugin unknown smartcard remove action");
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        }

        g_object_unref (settings);

        return remove_action;
}

static void
process_smartcard_removal (CsdSmartcardPlugin *plugin)
{
        CsdSmartcardRemoveAction remove_action;

        g_debug ("CsdSmartcardPlugin processing smartcard removal");
        remove_action = get_configured_remove_action (plugin);

        switch (remove_action)
        {
            case MSD_SMARTCARD_REMOVE_ACTION_NONE:
                return;
            case MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN:
                lock_screen (plugin);
                break;
            case MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT:
                force_logout (plugin);
                break;
        }
}

static void
smartcard_removed_cb (CsdSmartcardManager *card_monitor,
                      CsdSmartcard        *card,
                      CsdSmartcardPlugin  *plugin)
{

        char *name;

        name = csd_smartcard_get_name (card);
        g_debug ("CsdSmartcardPlugin smart card '%s' removed", name);
        g_free (name);

        if (!csd_smartcard_is_login_card (card)) {
                g_debug ("CsdSmartcardPlugin removed smart card was not used to login");
                return;
        }

        process_smartcard_removal (plugin);
}

static void
impl_activate (CafeSettingsPlugin *plugin)
{
        GError *error;
        CsdSmartcardPlugin *smartcard_plugin = MSD_SMARTCARD_PLUGIN (plugin);

        if (smartcard_plugin->priv->is_active) {
                g_debug ("CsdSmartcardPlugin Not activating smartcard plugin, because it's "
                         "already active");
                return;
        }

        if (!user_logged_in_with_smartcard ()) {
                g_debug ("CsdSmartcardPlugin Not activating smartcard plugin, because user didn't use "
                         " smartcard to log in");
                smartcard_plugin->priv->is_active = FALSE;
                return;
        }

        g_debug ("CsdSmartcardPlugin Activating smartcard plugin");

        error = NULL;
        smartcard_plugin->priv->bus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

        if (smartcard_plugin->priv->bus_connection == NULL) {
                g_warning ("CsdSmartcardPlugin Unable to connect to session bus: %s", error->message);
                return;
        }

        if (!csd_smartcard_manager_start (smartcard_plugin->priv->manager, &error)) {
                g_warning ("CsdSmartcardPlugin Unable to start smartcard manager: %s", error->message);
                g_error_free (error);
        }

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-removed",
                          G_CALLBACK (smartcard_removed_cb), smartcard_plugin);

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-inserted",
                          G_CALLBACK (smartcard_inserted_cb), smartcard_plugin);

        if (!csd_smartcard_manager_login_card_is_inserted (smartcard_plugin->priv->manager)) {
                g_debug ("CsdSmartcardPlugin processing smartcard removal immediately user logged in with smartcard "
                         "and it's not inserted");
                process_smartcard_removal (smartcard_plugin);
        }

        smartcard_plugin->priv->is_active = TRUE;
}

static void
impl_deactivate (CafeSettingsPlugin *plugin)
{
        CsdSmartcardPlugin *smartcard_plugin = MSD_SMARTCARD_PLUGIN (plugin);

        if (!smartcard_plugin->priv->is_active) {
                g_debug ("CsdSmartcardPlugin Not deactivating smartcard plugin, "
                         "because it's already inactive");
                return;
        }

        g_debug ("CsdSmartcardPlugin Deactivating smartcard plugin");

        csd_smartcard_manager_stop (smartcard_plugin->priv->manager);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_removed_cb, smartcard_plugin);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_inserted_cb, smartcard_plugin);
        smartcard_plugin->priv->bus_connection = NULL;
        smartcard_plugin->priv->is_active = FALSE;
}

static void
csd_smartcard_plugin_class_init (CsdSmartcardPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CafeSettingsPluginClass *plugin_class = CAFE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_smartcard_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
csd_smartcard_plugin_class_finalize (CsdSmartcardPluginClass *klass)
{
}


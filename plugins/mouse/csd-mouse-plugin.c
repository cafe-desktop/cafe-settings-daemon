/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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

#include "cafe-settings-plugin.h"
#include "csd-mouse-plugin.h"
#include "csd-mouse-manager.h"

struct MsdMousePluginPrivate {
        MsdMouseManager *manager;
};

CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (MsdMousePlugin, csd_mouse_plugin)

static void
csd_mouse_plugin_init (MsdMousePlugin *plugin)
{
        plugin->priv = csd_mouse_plugin_get_instance_private (plugin);

        g_debug ("MsdMousePlugin initializing");

        plugin->priv->manager = csd_mouse_manager_new ();
}

static void
csd_mouse_plugin_finalize (GObject *object)
{
        MsdMousePlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_MOUSE_PLUGIN (object));

        g_debug ("MsdMousePlugin finalizing");

        plugin = MSD_MOUSE_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_mouse_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CafeSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating mouse plugin");

        error = NULL;
        res = csd_mouse_manager_start (MSD_MOUSE_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start mouse manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CafeSettingsPlugin *plugin)
{
        g_debug ("Deactivating mouse plugin");
        csd_mouse_manager_stop (MSD_MOUSE_PLUGIN (plugin)->priv->manager);
}

static void
csd_mouse_plugin_class_init (MsdMousePluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        CafeSettingsPluginClass *plugin_class = CAFE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_mouse_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
csd_mouse_plugin_class_finalize (MsdMousePluginClass *klass)
{
}


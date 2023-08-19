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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "cafe-settings-plugin.h"
#include "csd_a11y-settings-plugin.h"
#include "csd_a11y-settings-manager.h"

struct CsdA11ySettingsPluginPrivate {
        CsdA11ySettingsManager *manager;
};

CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (CsdA11ySettingsPlugin, csd_a11y_settings_plugin)

static void
csd_a11y_settings_plugin_init (CsdA11ySettingsPlugin *plugin)
{
        plugin->priv = csd_a11y_settings_plugin_get_instance_private (plugin);

        g_debug ("CsdA11ySettingsPlugin initializing");

        plugin->priv->manager = csd_a11y_settings_manager_new ();
}

static void
csd_a11y_settings_plugin_finalize (GObject *object)
{
        CsdA11ySettingsPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_A11Y_SETTINGS_PLUGIN (object));

        g_debug ("CsdA11ySettingsPlugin finalizing");

        plugin = CSD_A11Y_SETTINGS_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_a11y_settings_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CafeSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating a11y-settings plugin");

        error = NULL;
        res = csd_a11y_settings_manager_start (CSD_A11Y_SETTINGS_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start a11y-settings manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CafeSettingsPlugin *plugin)
{
        g_debug ("Deactivating a11y-settings plugin");
        csd_a11y_settings_manager_stop (CSD_A11Y_SETTINGS_PLUGIN (plugin)->priv->manager);
}

static void
csd_a11y_settings_plugin_class_init (CsdA11ySettingsPluginClass *klass)
{
        GObjectClass             *object_class = G_OBJECT_CLASS (klass);
        CafeSettingsPluginClass *plugin_class = CAFE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_a11y_settings_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
csd_a11y_settings_plugin_class_finalize (CsdA11ySettingsPluginClass *klass)
{
}

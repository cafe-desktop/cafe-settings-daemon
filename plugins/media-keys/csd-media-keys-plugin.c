/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#ifdef HAVE_LIBCAFEMIXER
#include <libcafemixer/cafemixer.h>
#endif

#include "cafe-settings-plugin.h"
#include "csd-media-keys-plugin.h"
#include "csd-media-keys-manager.h"

struct _CsdMediaKeysPluginPrivate
{
        CsdMediaKeysManager *manager;
};

CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (CsdMediaKeysPlugin, csd_media_keys_plugin)

static void
csd_media_keys_plugin_init (CsdMediaKeysPlugin *plugin)
{
        plugin->priv = csd_media_keys_plugin_get_instance_private (plugin);

        g_debug ("CsdMediaKeysPlugin initializing");

        plugin->priv->manager = csd_media_keys_manager_new ();
}

static void
csd_media_keys_plugin_dispose (GObject *object)
{
        CsdMediaKeysPlugin *plugin;

        g_debug ("CsdMediaKeysPlugin disposing");

        plugin = MSD_MEDIA_KEYS_PLUGIN (object);

        g_clear_object (&plugin->priv->manager);

        G_OBJECT_CLASS (csd_media_keys_plugin_parent_class)->dispose (object);
}

static void
impl_activate (CafeSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error = NULL;

        g_debug ("Activating media_keys plugin");

#ifdef HAVE_LIBCAFEMIXER
        cafe_mixer_init ();
#endif
        res = csd_media_keys_manager_start (MSD_MEDIA_KEYS_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start media_keys manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CafeSettingsPlugin *plugin)
{
        g_debug ("Deactivating media_keys plugin");
        csd_media_keys_manager_stop (MSD_MEDIA_KEYS_PLUGIN (plugin)->priv->manager);
}

static void
csd_media_keys_plugin_class_init (CsdMediaKeysPluginClass *klass)
{
        GObjectClass            *object_class = G_OBJECT_CLASS (klass);
        CafeSettingsPluginClass *plugin_class = CAFE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->dispose = csd_media_keys_plugin_dispose;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
csd_media_keys_plugin_class_finalize (CsdMediaKeysPluginClass *klass)
{
}

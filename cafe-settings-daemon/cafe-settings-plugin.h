/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2007      William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __CAFE_SETTINGS_PLUGIN_H__
#define __CAFE_SETTINGS_PLUGIN_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define CAFE_TYPE_SETTINGS_PLUGIN              (cafe_settings_plugin_get_type())
#define CAFE_SETTINGS_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CAFE_TYPE_SETTINGS_PLUGIN, CafeSettingsPlugin))
#define CAFE_SETTINGS_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CAFE_TYPE_SETTINGS_PLUGIN, CafeSettingsPluginClass))
#define CAFE_IS_SETTINGS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CAFE_TYPE_SETTINGS_PLUGIN))
#define CAFE_IS_SETTINGS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_SETTINGS_PLUGIN))
#define CAFE_SETTINGS_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CAFE_TYPE_SETTINGS_PLUGIN, CafeSettingsPluginClass))

typedef struct
{
        GObject parent;
} CafeSettingsPlugin;

typedef struct
{
        GObjectClass parent_class;

        /* Virtual public methods */
        void            (*activate)                     (CafeSettingsPlugin *plugin);
        void            (*deactivate)                   (CafeSettingsPlugin *plugin);
} CafeSettingsPluginClass;

GType            cafe_settings_plugin_get_type           (void) G_GNUC_CONST;

void             cafe_settings_plugin_activate           (CafeSettingsPlugin *plugin);
void             cafe_settings_plugin_deactivate         (CafeSettingsPlugin *plugin);

/*
 * Utility macro used to register plugins
 *
 * use: CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (PluginName, plugin_name)
 */
#define _REGISTER_PLUGIN_FUNC(plugin_name)                                     \
G_MODULE_EXPORT GType                                                          \
register_cafe_settings_plugin (GTypeModule *type_module)                       \
{                                                                              \
        plugin_name##_register_type (type_module);                             \
                                                                               \
        return plugin_name##_get_type();                                       \
}

#define CAFE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE(PluginName, plugin_name)    \
        G_DEFINE_DYNAMIC_TYPE_EXTENDED (PluginName,                            \
                                        plugin_name,                           \
                                        CAFE_TYPE_SETTINGS_PLUGIN,             \
                                        0,                                     \
                                        G_ADD_PRIVATE_DYNAMIC(PluginName))     \
                                                                               \
_REGISTER_PLUGIN_FUNC(plugin_name)

G_END_DECLS

#endif  /* __CAFE_SETTINGS_PLUGIN_H__ */

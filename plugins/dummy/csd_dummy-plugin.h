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

#ifndef __CSD_DUMMY_PLUGIN_H__
#define __CSD_DUMMY_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "cafe-settings-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_DUMMY_PLUGIN                (csd_dummy_plugin_get_type ())
#define CSD_DUMMY_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_DUMMY_PLUGIN, CsdDummyPlugin))
#define CSD_DUMMY_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_DUMMY_PLUGIN, CsdDummyPluginClass))
#define CSD_IS_DUMMY_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_DUMMY_PLUGIN))
#define CSD_IS_DUMMY_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_DUMMY_PLUGIN))
#define CSD_DUMMY_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_DUMMY_PLUGIN, CsdDummyPluginClass))

typedef struct CsdDummyPluginPrivate CsdDummyPluginPrivate;

typedef struct
{
        CafeSettingsPlugin    parent;
        CsdDummyPluginPrivate *priv;
} CsdDummyPlugin;

typedef struct
{
        CafeSettingsPluginClass parent_class;
} CsdDummyPluginClass;

GType   csd_dummy_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_cafe_settings_plugin (GTypeModule *module);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_DUMMY_PLUGIN_H__ */

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

#ifndef __CSD_MEDIA_KEYS_PLUGIN_H__
#define __CSD_MEDIA_KEYS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "cafe-settings-plugin.h"

G_BEGIN_DECLS

#define CSD_TYPE_MEDIA_KEYS_PLUGIN                (csd_media_keys_plugin_get_type ())
#define CSD_MEDIA_KEYS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_MEDIA_KEYS_PLUGIN, CsdMediaKeysPlugin))
#define CSD_MEDIA_KEYS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_MEDIA_KEYS_PLUGIN, CsdMediaKeysPluginClass))
#define CSD_IS_MEDIA_KEYS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_MEDIA_KEYS_PLUGIN))
#define CSD_IS_MEDIA_KEYS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_MEDIA_KEYS_PLUGIN))
#define CSD_MEDIA_KEYS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_MEDIA_KEYS_PLUGIN, CsdMediaKeysPluginClass))

typedef struct _CsdMediaKeysPlugin         CsdMediaKeysPlugin;
typedef struct _CsdMediaKeysPluginClass    CsdMediaKeysPluginClass;
typedef struct _CsdMediaKeysPluginPrivate  CsdMediaKeysPluginPrivate;

struct _CsdMediaKeysPlugin
{
        CafeSettingsPlugin          parent;
        CsdMediaKeysPluginPrivate  *priv;
};

struct _CsdMediaKeysPluginClass
{
        CafeSettingsPluginClass     parent_class;
};

GType csd_media_keys_plugin_get_type (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_cafe_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __CSD_MEDIA_KEYS_PLUGIN_H__ */

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

#ifndef __MSD_KEYBINDINGS_PLUGIN_H__
#define __MSD_KEYBINDINGS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "cafe-settings-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_KEYBINDINGS_PLUGIN                (msd_keybindings_plugin_get_type ())
#define MSD_KEYBINDINGS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_KEYBINDINGS_PLUGIN, MsdKeybindingsPlugin))
#define MSD_KEYBINDINGS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_KEYBINDINGS_PLUGIN, MsdKeybindingsPluginClass))
#define MSD_IS_KEYBINDINGS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_KEYBINDINGS_PLUGIN))
#define MSD_IS_KEYBINDINGS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_KEYBINDINGS_PLUGIN))
#define MSD_KEYBINDINGS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_KEYBINDINGS_PLUGIN, MsdKeybindingsPluginClass))

typedef struct MsdKeybindingsPluginPrivate MsdKeybindingsPluginPrivate;

typedef struct
{
        CafeSettingsPlugin    parent;
        MsdKeybindingsPluginPrivate *priv;
} MsdKeybindingsPlugin;

typedef struct
{
        CafeSettingsPluginClass parent_class;
} MsdKeybindingsPluginClass;

GType   msd_keybindings_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_cafe_settings_plugin (GTypeModule *module);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_KEYBINDINGS_PLUGIN_H__ */

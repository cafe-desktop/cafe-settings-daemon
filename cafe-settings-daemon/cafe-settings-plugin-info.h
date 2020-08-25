/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __CAFE_SETTINGS_PLUGIN_INFO_H__
#define __CAFE_SETTINGS_PLUGIN_INFO_H__

#include <glib-object.h>
#include <gmodule.h>

#ifdef __cplusplus
extern "C" {
#endif
#define CAFE_TYPE_SETTINGS_PLUGIN_INFO              (cafe_settings_plugin_info_get_type())
#define CAFE_SETTINGS_PLUGIN_INFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CAFE_TYPE_SETTINGS_PLUGIN_INFO, CafeSettingsPluginInfo))
#define CAFE_SETTINGS_PLUGIN_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CAFE_TYPE_SETTINGS_PLUGIN_INFO, CafeSettingsPluginInfoClass))
#define CAFE_IS_SETTINGS_PLUGIN_INFO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CAFE_TYPE_SETTINGS_PLUGIN_INFO))
#define CAFE_IS_SETTINGS_PLUGIN_INFO_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CAFE_TYPE_SETTINGS_PLUGIN_INFO))
#define CAFE_SETTINGS_PLUGIN_INFO_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CAFE_TYPE_SETTINGS_PLUGIN_INFO, CafeSettingsPluginInfoClass))

typedef struct CafeSettingsPluginInfoPrivate CafeSettingsPluginInfoPrivate;

typedef struct
{
        GObject                         parent;
        CafeSettingsPluginInfoPrivate *priv;
} CafeSettingsPluginInfo;

typedef struct
{
        GObjectClass parent_class;

        void          (* activated)         (CafeSettingsPluginInfo *info);
        void          (* deactivated)       (CafeSettingsPluginInfo *info);
} CafeSettingsPluginInfoClass;

GType            cafe_settings_plugin_info_get_type           (void) G_GNUC_CONST;

CafeSettingsPluginInfo *cafe_settings_plugin_info_new_from_file (const char *filename);

gboolean         cafe_settings_plugin_info_activate        (CafeSettingsPluginInfo *info);
gboolean         cafe_settings_plugin_info_deactivate      (CafeSettingsPluginInfo *info);

gboolean         cafe_settings_plugin_info_is_active       (CafeSettingsPluginInfo *info);
gboolean         cafe_settings_plugin_info_get_enabled     (CafeSettingsPluginInfo *info);
gboolean         cafe_settings_plugin_info_is_available    (CafeSettingsPluginInfo *info);

const char      *cafe_settings_plugin_info_get_name        (CafeSettingsPluginInfo *info);
const char      *cafe_settings_plugin_info_get_description (CafeSettingsPluginInfo *info);
const char     **cafe_settings_plugin_info_get_authors     (CafeSettingsPluginInfo *info);
const char      *cafe_settings_plugin_info_get_website     (CafeSettingsPluginInfo *info);
const char      *cafe_settings_plugin_info_get_copyright   (CafeSettingsPluginInfo *info);
const char      *cafe_settings_plugin_info_get_location    (CafeSettingsPluginInfo *info);
int              cafe_settings_plugin_info_get_priority    (CafeSettingsPluginInfo *info);

void             cafe_settings_plugin_info_set_priority    (CafeSettingsPluginInfo *info,
                                                            int                     priority);
void             cafe_settings_plugin_info_set_schema      (CafeSettingsPluginInfo *info,
                                                            gchar                  *schema);
#ifdef __cplusplus
}
#endif

#endif  /* __CAFE_SETTINGS_PLUGIN_INFO_H__ */

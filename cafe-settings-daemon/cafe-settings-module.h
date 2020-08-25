/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef CAFE_SETTINGS_MODULE_H
#define CAFE_SETTINGS_MODULE_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAFE_TYPE_SETTINGS_MODULE               (cafe_settings_module_get_type ())
#define CAFE_SETTINGS_MODULE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAFE_TYPE_SETTINGS_MODULE, MateSettingsModule))
#define CAFE_SETTINGS_MODULE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CAFE_TYPE_SETTINGS_MODULE, MateSettingsModuleClass))
#define CAFE_IS_SETTINGS_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAFE_TYPE_SETTINGS_MODULE))
#define CAFE_IS_SETTINGS_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((obj), CAFE_TYPE_SETTINGS_MODULE))
#define CAFE_SETTINGS_MODULE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), CAFE_TYPE_SETTINGS_MODULE, MateSettingsModuleClass))

typedef struct _MateSettingsModule MateSettingsModule;

GType                    cafe_settings_module_get_type          (void) G_GNUC_CONST;

MateSettingsModule     *cafe_settings_module_new               (const gchar *path);

const char              *cafe_settings_module_get_path          (MateSettingsModule *module);

GObject                 *cafe_settings_module_new_object        (MateSettingsModule *module);

#ifdef __cplusplus
}
#endif

#endif

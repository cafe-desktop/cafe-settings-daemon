/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __CAFE_SETTINGS_MANAGER_H
#define __CAFE_SETTINGS_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAFE_TYPE_SETTINGS_MANAGER         (mate_settings_manager_get_type ())
#define CAFE_SETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAFE_TYPE_SETTINGS_MANAGER, MateSettingsManager))
#define CAFE_SETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAFE_TYPE_SETTINGS_MANAGER, MateSettingsManagerClass))
#define CAFE_IS_SETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAFE_TYPE_SETTINGS_MANAGER))
#define CAFE_IS_SETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAFE_TYPE_SETTINGS_MANAGER))
#define CAFE_SETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAFE_TYPE_SETTINGS_MANAGER, MateSettingsManagerClass))

typedef struct MateSettingsManagerPrivate MateSettingsManagerPrivate;

typedef struct
{
        GObject                      parent;
        MateSettingsManagerPrivate  *priv;
} MateSettingsManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* plugin_activated)         (MateSettingsManager *manager,
                                                    const char           *name);
        void          (* plugin_deactivated)       (MateSettingsManager *manager,
                                                    const char           *name);
} MateSettingsManagerClass;

typedef enum
{
        CAFE_SETTINGS_MANAGER_ERROR_GENERAL
} MateSettingsManagerError;

enum
{
        PLUGIN_LOAD_ALL,
        PLUGIN_LOAD_INIT,
        PLUGIN_LOAD_DEFER
};

#define CAFE_SETTINGS_MANAGER_ERROR mate_settings_manager_error_quark ()

GQuark                 mate_settings_manager_error_quark         (void);
GType                  mate_settings_manager_get_type   (void);

MateSettingsManager * mate_settings_manager_new        (void);

gboolean               mate_settings_manager_start      (MateSettingsManager *manager,
                                                          gint                load_init_flag,
                                                          GError            **error);
void                   mate_settings_manager_stop       (MateSettingsManager *manager);

gboolean               mate_settings_manager_awake      (MateSettingsManager *manager,
                                                          GError              **error);

#ifdef __cplusplus
}
#endif

#endif /* __CAFE_SETTINGS_MANAGER_H */

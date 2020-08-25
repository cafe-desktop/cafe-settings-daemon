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

#ifndef __CAFE_XSETTINGS_MANAGER_H
#define __CAFE_XSETTINGS_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAFE_TYPE_XSETTINGS_MANAGER         (cafe_xsettings_manager_get_type ())
#define CAFE_XSETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAFE_TYPE_XSETTINGS_MANAGER, MateXSettingsManager))
#define CAFE_XSETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAFE_TYPE_XSETTINGS_MANAGER, MateXSettingsManagerClass))
#define CAFE_IS_XSETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAFE_TYPE_XSETTINGS_MANAGER))
#define CAFE_IS_XSETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAFE_TYPE_XSETTINGS_MANAGER))
#define CAFE_XSETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAFE_TYPE_XSETTINGS_MANAGER, MateXSettingsManagerClass))

typedef struct MateXSettingsManagerPrivate MateXSettingsManagerPrivate;

typedef struct
{
        GObject                     parent;
        MateXSettingsManagerPrivate *priv;
} MateXSettingsManager;

typedef struct
{
        GObjectClass   parent_class;
} MateXSettingsManagerClass;

GType                   cafe_xsettings_manager_get_type            (void);

MateXSettingsManager * cafe_xsettings_manager_new                 (void);
gboolean                cafe_xsettings_manager_start               (MateXSettingsManager *manager,
                                                                     GError               **error);
void                    cafe_xsettings_manager_stop                (MateXSettingsManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CAFE_XSETTINGS_MANAGER_H */

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

#ifndef __CSD_KEYBOARD_MANAGER_H
#define __CSD_KEYBOARD_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_KEYBOARD_MANAGER         (csd_keyboard_manager_get_type ())
#define CSD_KEYBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_KEYBOARD_MANAGER, CsdKeyboardManager))
#define CSD_KEYBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_KEYBOARD_MANAGER, CsdKeyboardManagerClass))
#define CSD_IS_KEYBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_KEYBOARD_MANAGER))
#define CSD_IS_KEYBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_KEYBOARD_MANAGER))
#define CSD_KEYBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_KEYBOARD_MANAGER, CsdKeyboardManagerClass))

typedef struct CsdKeyboardManagerPrivate CsdKeyboardManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdKeyboardManagerPrivate *priv;
} CsdKeyboardManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdKeyboardManagerClass;

GType                   csd_keyboard_manager_get_type            (void);

CsdKeyboardManager *       csd_keyboard_manager_new                 (void);
gboolean                csd_keyboard_manager_start               (CsdKeyboardManager *manager,
                                                               GError         **error);
void                    csd_keyboard_manager_stop                (CsdKeyboardManager *manager);
void                    csd_keyboard_manager_apply_settings      (CsdKeyboardManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_KEYBOARD_MANAGER_H */

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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CSD_KEYBINDINGS_MANAGER_H
#define __CSD_KEYBINDINGS_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_KEYBINDINGS_MANAGER         (csd_keybindings_manager_get_type ())
#define CSD_KEYBINDINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_KEYBINDINGS_MANAGER, CsdKeybindingsManager))
#define CSD_KEYBINDINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_KEYBINDINGS_MANAGER, CsdKeybindingsManagerClass))
#define CSD_IS_KEYBINDINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_KEYBINDINGS_MANAGER))
#define CSD_IS_KEYBINDINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_KEYBINDINGS_MANAGER))
#define CSD_KEYBINDINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_KEYBINDINGS_MANAGER, CsdKeybindingsManagerClass))

typedef struct CsdKeybindingsManagerPrivate CsdKeybindingsManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdKeybindingsManagerPrivate *priv;
} CsdKeybindingsManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdKeybindingsManagerClass;

GType                   csd_keybindings_manager_get_type            (void);

CsdKeybindingsManager *       csd_keybindings_manager_new                 (void);
gboolean                csd_keybindings_manager_start               (CsdKeybindingsManager *manager,
                                                               GError         **error);
void                    csd_keybindings_manager_stop                (CsdKeybindingsManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_KEYBINDINGS_MANAGER_H */

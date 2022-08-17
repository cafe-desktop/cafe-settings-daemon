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

#ifndef __CSD_XRDB_MANAGER_H
#define __CSD_XRDB_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_XRDB_MANAGER         (csd_xrdb_manager_get_type ())
#define CSD_XRDB_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_XRDB_MANAGER, CsdXrdbManager))
#define CSD_XRDB_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_XRDB_MANAGER, CsdXrdbManagerClass))
#define CSD_IS_XRDB_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_XRDB_MANAGER))
#define CSD_IS_XRDB_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_XRDB_MANAGER))
#define CSD_XRDB_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_XRDB_MANAGER, CsdXrdbManagerClass))

typedef struct CsdXrdbManagerPrivate CsdXrdbManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdXrdbManagerPrivate *priv;
} CsdXrdbManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdXrdbManagerClass;

GType                   csd_xrdb_manager_get_type            (void);

CsdXrdbManager *        csd_xrdb_manager_new                 (void);
gboolean                csd_xrdb_manager_start               (CsdXrdbManager *manager,
                                                              GError        **error);
void                    csd_xrdb_manager_stop                (CsdXrdbManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_XRDB_MANAGER_H */

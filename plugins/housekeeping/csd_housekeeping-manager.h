/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Michael J. Chudobiak <mjc@avtechpulse.com>
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

#ifndef __CSD_HOUSEKEEPING_MANAGER_H
#define __CSD_HOUSEKEEPING_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_HOUSEKEEPING_MANAGER         (csd_housekeeping_manager_get_type ())
#define CSD_HOUSEKEEPING_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_HOUSEKEEPING_MANAGER, CsdHousekeepingManager))
#define CSD_HOUSEKEEPING_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_HOUSEKEEPING_MANAGER, CsdHousekeepingManagerClass))
#define CSD_IS_HOUSEKEEPING_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_HOUSEKEEPING_MANAGER))
#define CSD_IS_HOUSEKEEPING_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_HOUSEKEEPING_MANAGER))
#define CSD_HOUSEKEEPING_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_HOUSEKEEPING_MANAGER, CsdHousekeepingManagerClass))

typedef struct CsdHousekeepingManagerPrivate CsdHousekeepingManagerPrivate;

typedef struct {
        GObject                        parent;
        CsdHousekeepingManagerPrivate *priv;
} CsdHousekeepingManager;

typedef struct {
        GObjectClass   parent_class;
} CsdHousekeepingManagerClass;

GType                    csd_housekeeping_manager_get_type      (void);

CsdHousekeepingManager * csd_housekeeping_manager_new           (void);
gboolean                 csd_housekeeping_manager_start         (CsdHousekeepingManager  *manager,
                                                                 GError                 **error);
void                     csd_housekeeping_manager_stop          (CsdHousekeepingManager  *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_HOUSEKEEPING_MANAGER_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Stefano Karapetsas <stefano@karapetsas.com>
 *               2007 William Jon McCann <mccann@jhu.edu>
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
 * Authors:
 *      Stefano Karapetsas <stefano@karapetsas.com>
 */

#ifndef __CSD_MPRIS_MANAGER_H
#define __CSD_MPRIS_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSD_TYPE_MPRIS_MANAGER         (csd_mpris_manager_get_type ())
#define CSD_MPRIS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_MPRIS_MANAGER, CsdMprisManager))
#define CSD_MPRIS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_MPRIS_MANAGER, CsdMprisManagerClass))
#define CSD_IS_MPRIS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_MPRIS_MANAGER))
#define CSD_IS_MPRIS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_MPRIS_MANAGER))
#define CSD_MPRIS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_MPRIS_MANAGER, CsdMprisManagerClass))

typedef struct CsdMprisManagerPrivate CsdMprisManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdMprisManagerPrivate *priv;
} CsdMprisManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdMprisManagerClass;

GType                   csd_mpris_manager_get_type            (void);

CsdMprisManager *       csd_mpris_manager_new                 (void);
gboolean                csd_mpris_manager_start               (CsdMprisManager *manager,
                                                               GError         **error);
void                    csd_mpris_manager_stop                (CsdMprisManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __CSD_MPRIS_MANAGER_H */

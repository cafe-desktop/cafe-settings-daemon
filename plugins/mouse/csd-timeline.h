/* csdtimeline.c
 *
 * Copyright (C) 2008 Carlos Garnacho  <carlos@imendio.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __MSD_TIMELINE_H__
#define __MSD_TIMELINE_H__

#include <glib-object.h>
#include <cdk/cdk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_TIMELINE_DIRECTION       (csd_timeline_direction_get_type ())
#define MSD_TYPE_TIMELINE_PROGRESS_TYPE   (csd_timeline_progress_type_get_type ())
#define MSD_TYPE_TIMELINE                 (csd_timeline_get_type ())
#define MSD_TIMELINE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), MSD_TYPE_TIMELINE, CsdTimeline))
#define MSD_TIMELINE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass),  MSD_TYPE_TIMELINE, CsdTimelineClass))
#define MSD_IS_TIMELINE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MSD_TYPE_TIMELINE))
#define MSD_IS_TIMELINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass),  MSD_TYPE_TIMELINE))
#define MSD_TIMELINE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj),  MSD_TYPE_TIMELINE, CsdTimelineClass))

typedef enum {
  MSD_TIMELINE_DIRECTION_FORWARD,
  MSD_TIMELINE_DIRECTION_BACKWARD
} CsdTimelineDirection;

typedef enum {
  MSD_TIMELINE_PROGRESS_LINEAR,
  MSD_TIMELINE_PROGRESS_SINUSOIDAL,
  MSD_TIMELINE_PROGRESS_EXPONENTIAL
} CsdTimelineProgressType;

typedef struct CsdTimeline      CsdTimeline;
typedef struct CsdTimelineClass CsdTimelineClass;

struct CsdTimeline
{
  GObject parent_instance;
};

struct CsdTimelineClass
{
  GObjectClass parent_class;

  void (* started)           (CsdTimeline *timeline);
  void (* finished)          (CsdTimeline *timeline);
  void (* paused)            (CsdTimeline *timeline);

  void (* frame)             (CsdTimeline *timeline,
			      gdouble      progress);

  void (* __csd_reserved1) (void);
  void (* __csd_reserved2) (void);
  void (* __csd_reserved3) (void);
  void (* __csd_reserved4) (void);
};

typedef gdouble (*CsdTimelineProgressFunc) (gdouble progress);


GType                   csd_timeline_get_type           (void) G_GNUC_CONST;
GType                   csd_timeline_direction_get_type (void) G_GNUC_CONST;
GType                   csd_timeline_progress_type_get_type (void) G_GNUC_CONST;

CsdTimeline            *csd_timeline_new                (guint                    duration);
CsdTimeline            *csd_timeline_new_for_screen     (guint                    duration,
							 CdkScreen               *screen);

void                    csd_timeline_start              (CsdTimeline             *timeline);
void                    csd_timeline_pause              (CsdTimeline             *timeline);
void                    csd_timeline_rewind             (CsdTimeline             *timeline);

gboolean                csd_timeline_is_running         (CsdTimeline             *timeline);

guint                   csd_timeline_get_fps            (CsdTimeline             *timeline);
void                    csd_timeline_set_fps            (CsdTimeline             *timeline,
							 guint                    fps);

gboolean                csd_timeline_get_loop           (CsdTimeline             *timeline);
void                    csd_timeline_set_loop           (CsdTimeline             *timeline,
							 gboolean                 loop);

guint                   csd_timeline_get_duration       (CsdTimeline             *timeline);
void                    csd_timeline_set_duration       (CsdTimeline             *timeline,
							 guint                    duration);

CdkScreen              *csd_timeline_get_screen         (CsdTimeline             *timeline);
void                    csd_timeline_set_screen         (CsdTimeline             *timeline,
							 CdkScreen               *screen);

CsdTimelineDirection    csd_timeline_get_direction      (CsdTimeline             *timeline);
void                    csd_timeline_set_direction      (CsdTimeline             *timeline,
							 CsdTimelineDirection     direction);

CsdTimelineProgressType csd_timeline_get_progress_type  (CsdTimeline             *timeline);
void                    csd_timeline_set_progress_type  (CsdTimeline             *timeline,
							 CsdTimelineProgressType  type);
void                    csd_timeline_get_progress_func  (CsdTimeline             *timeline);

void                    csd_timeline_set_progress_func  (CsdTimeline             *timeline,
							 CsdTimelineProgressFunc  progress_func);

gdouble                 csd_timeline_get_progress       (CsdTimeline             *timeline);


#ifdef __cplusplus
}
#endif

#endif /* __MSD_TIMELINE_H__ */

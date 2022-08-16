/* msd-locate-pointer.c
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

#include <ctk/ctk.h>
#include "msd-timeline.h"
#include "msd-locate-pointer.h"

#include <cdk/cdkkeysyms.h>
#include <cdk/cdkx.h>
#include <X11/keysym.h>

#define ANIMATION_LENGTH 750
#define WINDOW_SIZE 101
#define N_CIRCLES 4

/* All circles are supposed to be moving when progress
 * reaches 0.5, and each of them are supposed to long
 * for half of the progress, hence the need of 0.5 to
 * get the circles interval, and the multiplication
 * by 2 to know a circle progress */
#define CIRCLES_PROGRESS_INTERVAL (0.5 / N_CIRCLES)
#define CIRCLE_PROGRESS(p) (MIN (1., ((gdouble) (p) * 2.)))

typedef struct MsdLocatePointerData MsdLocatePointerData;

struct MsdLocatePointerData
{
  MsdTimeline *timeline;
  CtkWindow *widget;
  CdkWindow *window;

  gdouble progress;
};

static MsdLocatePointerData *data = NULL;

static void
locate_pointer_paint (MsdLocatePointerData *data,
		      cairo_t              *cr,
		      gboolean              composited)
{
  CdkRGBA color;
  gdouble progress, circle_progress;
  gint width, height, i;
  CtkStyleContext *style;

  color.red = color.green = color.blue = 0.7;
  color.alpha = 0.;

  progress = data->progress;

  width = cdk_window_get_width (data->window);
  height = cdk_window_get_height (data->window);

  style = ctk_widget_get_style_context (CTK_WIDGET (data->widget));
  ctk_style_context_save (style);
  ctk_style_context_set_state (style, CTK_STATE_FLAG_SELECTED);
  ctk_style_context_add_class (style, CTK_STYLE_CLASS_VIEW);
  ctk_style_context_get_background_color (style,
                                          ctk_style_context_get_state (style),
                                          &color);
  if (color.alpha == 0.)
    {
      ctk_style_context_remove_class (style, CTK_STYLE_CLASS_VIEW);
      ctk_style_context_get_background_color (style,
                                              ctk_style_context_get_state (style),
                                              &color);
    }
  ctk_style_context_restore (style);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1., 1., 1., 0.);
  cairo_paint (cr);

  for (i = 0; i <= N_CIRCLES; i++)
    {
      if (progress < 0.)
	break;

      circle_progress = MIN (1., (progress * 2));
      progress -= CIRCLES_PROGRESS_INTERVAL;

      if (circle_progress >= 1.)
	continue;

      if (composited)
	{
	  cairo_set_source_rgba (cr,
				 color.red,
				 color.green,
				 color.blue,
				 1 - circle_progress);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);

	  cairo_fill (cr);
	  cairo_stroke (cr);
	}
      else
	{
	  cairo_set_source_rgb (cr, 0., 0., 0.);
	  cairo_set_line_width (cr, 3.);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);
	  cairo_stroke (cr);

	  cairo_set_source_rgb (cr, 1., 1., 1.);
	  cairo_set_line_width (cr, 1.);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);
	  cairo_stroke (cr);
	}
    }

  cairo_restore (cr);
}

static void
update_shape (MsdLocatePointerData *data)
{
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  mask = cdk_window_create_similar_image_surface (data->window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);
  locate_pointer_paint (data, cr, FALSE);

  region = cdk_cairo_region_create_from_surface (mask);

  cdk_window_shape_combine_region (data->window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
}

static void
timeline_frame_cb (MsdTimeline *timeline,
		   gdouble      progress,
		   gpointer     user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  CdkDisplay *display = cdk_window_get_display (data->window);
  CdkScreen *screen = cdk_display_get_default_screen (display);
  CdkSeat *seat;
  CdkDevice *pointer;
  gint cursor_x, cursor_y;

  if (cdk_screen_is_composited (screen))
    {
      ctk_widget_queue_draw (CTK_WIDGET (data->widget));
      data->progress = progress;
    }
  else if (progress >= data->progress + CIRCLES_PROGRESS_INTERVAL)
    {
      /* only invalidate window each circle interval */
      update_shape (data);
      ctk_widget_queue_draw (CTK_WIDGET (data->widget));
      data->progress += CIRCLES_PROGRESS_INTERVAL;
    }

  seat = cdk_display_get_default_seat (display);
  pointer = cdk_seat_get_pointer (seat);
  cdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);

  ctk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
}

static void
set_transparent_shape (CdkWindow *window)
{
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  mask = cdk_window_create_similar_image_surface (window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  cairo_set_source_rgba (cr, 1., 1., 1., 0.);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  region = cdk_cairo_region_create_from_surface (mask);

  cdk_window_shape_combine_region (window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
}

static void
unset_transparent_shape (CdkWindow *window)
{
  cdk_window_shape_combine_region (window, NULL, 0, 0);
}

static void
composited_changed (CdkScreen            *screen,
                    MsdLocatePointerData *data)
{
  if (cdk_screen_is_composited (screen))
    {
      unset_transparent_shape (data->window);
    }
  else
    {
      set_transparent_shape (data->window);
    }
}

static void
timeline_finished_cb (MsdTimeline *timeline,
		      gpointer     user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  CdkScreen *screen = cdk_window_get_screen (data->window);

  /* set transparent shape and hide window */
  if (!cdk_screen_is_composited (screen))
    {
      set_transparent_shape (data->window);
    }

  ctk_widget_hide (CTK_WIDGET (data->widget));
}

static void
locate_pointer_unrealize_cb (CtkWidget            *widget,
                             MsdLocatePointerData *data)
{
  if (data->window != NULL)
    {
      ctk_widget_unregister_window (CTK_WIDGET (data->widget),
                                    data->window);
      cdk_window_destroy (data->window);
    }
  data->window = NULL;
}

static void
locate_pointer_realize_cb (CtkWidget            *widget,
                           MsdLocatePointerData *data)
{
  CdkDisplay *display;
  CdkScreen *screen;
  CdkVisual *visual;
  CdkWindowAttr attributes;
  gint attributes_mask;

  display = ctk_widget_get_display (CTK_WIDGET (data->widget));
  screen = cdk_display_get_default_screen (display);
  visual = cdk_screen_get_rgba_visual (screen);

  if (visual == NULL)
    visual = cdk_screen_get_system_visual (screen);

  locate_pointer_unrealize_cb (CTK_WIDGET (data->widget), data);

  attributes_mask = CDK_WA_X | CDK_WA_Y;
  if (visual != NULL)
    {
      attributes_mask |= CDK_WA_VISUAL;
    }

  attributes.window_type = CDK_WINDOW_TEMP;
  attributes.wclass = CDK_INPUT_OUTPUT;
  attributes.visual = visual;
  attributes.event_mask = CDK_VISIBILITY_NOTIFY_MASK | CDK_EXPOSURE_MASK;
  attributes.width = 1;
  attributes.height = 1;

  data->window = cdk_window_new (cdk_screen_get_root_window (screen),
				 &attributes,
				 attributes_mask);

  ctk_widget_set_window (CTK_WIDGET (data->widget),
                         data->window);
  ctk_widget_register_window (CTK_WIDGET (data->widget),
                              data->window);
}

static gboolean
locate_pointer_draw_cb (CtkWidget      *widget,
                        cairo_t        *cr,
                        gpointer        user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  CdkScreen *screen = cdk_window_get_screen (data->window);

  if (ctk_cairo_should_draw_window (cr, data->window))
    {
      locate_pointer_paint (data, cr, cdk_screen_is_composited (screen));
    }

  return TRUE;
}

static MsdLocatePointerData *
msd_locate_pointer_data_new (void)
{
  MsdLocatePointerData *data;

  data = g_new0 (MsdLocatePointerData, 1);

  data->widget = CTK_WINDOW (ctk_window_new (CTK_WINDOW_POPUP));

  g_signal_connect (CTK_WIDGET (data->widget), "unrealize",
                    G_CALLBACK (locate_pointer_unrealize_cb),
                    data);
  g_signal_connect (CTK_WIDGET (data->widget), "realize",
                    G_CALLBACK (locate_pointer_realize_cb),
                    data);
  g_signal_connect (CTK_WIDGET (data->widget), "draw",
                    G_CALLBACK (locate_pointer_draw_cb),
                    data);

  ctk_widget_set_app_paintable (CTK_WIDGET (data->widget), TRUE);
  ctk_widget_realize (CTK_WIDGET (data->widget));

  data->timeline = msd_timeline_new (ANIMATION_LENGTH);
  g_signal_connect (data->timeline, "frame",
		    G_CALLBACK (timeline_frame_cb), data);
  g_signal_connect (data->timeline, "finished",
		    G_CALLBACK (timeline_finished_cb), data);

  return data;
}

static void
move_locate_pointer_window (MsdLocatePointerData *data,
			    CdkDisplay           *display)
{
  CdkSeat *seat;
  CdkDevice *pointer;
  gint cursor_x, cursor_y;
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  seat = cdk_display_get_default_seat (display);
  pointer = cdk_seat_get_pointer (seat);
  cdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);

  ctk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
  ctk_window_resize (data->widget,
                     WINDOW_SIZE, WINDOW_SIZE);

  mask = cdk_window_create_similar_image_surface (data->window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  cairo_set_source_rgba (cr, 0., 0., 0., 0.);
  cairo_paint (cr);

  region = cdk_cairo_region_create_from_surface (mask);

  cdk_window_input_shape_combine_region (data->window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
}

void
msd_locate_pointer (CdkDisplay *display)
{
  CdkScreen *screen = cdk_display_get_default_screen (display);

  if (data == NULL)
    {
      data = msd_locate_pointer_data_new ();
    }

  msd_timeline_pause (data->timeline);
  msd_timeline_rewind (data->timeline);

  data->progress = 0.;

  g_signal_connect (screen, "composited-changed",
                    G_CALLBACK (composited_changed), data);

  move_locate_pointer_window (data, display);
  composited_changed (screen, data);
  ctk_widget_show (CTK_WIDGET (data->widget));

  msd_timeline_start (data->timeline);
}


#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))

/* Owen magic */
static CdkFilterReturn
event_filter (CdkXEvent *cdkxevent,
              CdkEvent  *event,
              gpointer   user_data)
{
  XEvent *xevent = (XEvent *) cdkxevent;
  CdkDisplay *display = (CdkDisplay *) user_data;

  if (xevent->xany.type == KeyPress || xevent->xany.type == KeyRelease)
    {
      guint keyval;
      gint group;

      /* get the keysym */
      group = (xevent->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
      cdk_keymap_translate_keyboard_state (cdk_keymap_get_for_display (display),
                                           xevent->xkey.keycode,
                                           xevent->xkey.state,
                                           group,
                                           &keyval,
                                           NULL, NULL, NULL);

      if (keyval == CDK_KEY_Control_L || keyval == CDK_KEY_Control_R)
        {
          if (xevent->xany.type == KeyRelease)
            {
              XAllowEvents (xevent->xany.display,
                            AsyncKeyboard,
                            xevent->xkey.time);
              msd_locate_pointer (display);
            }
          else
            {
              XAllowEvents (xevent->xany.display,
                            SyncKeyboard,
                            xevent->xkey.time);
            }
        }
      else
        {
          XAllowEvents (xevent->xany.display,
                        ReplayKeyboard,
                        xevent->xkey.time);
          XUngrabButton (xevent->xany.display,
                         AnyButton,
                         AnyModifier,
                         xevent->xany.window);
          XUngrabKeyboard (xevent->xany.display,
                           xevent->xkey.time);
        }
    }
  else if (xevent->xany.type == ButtonPress)
    {
      XAllowEvents (xevent->xany.display,
                    ReplayPointer,
                    xevent->xbutton.time);
      XUngrabButton (xevent->xany.display,
                     AnyButton,
                     AnyModifier,
                     xevent->xany.window);
      XUngrabKeyboard (xevent->xany.display,
                       xevent->xbutton.time);
    }

  return CDK_FILTER_CONTINUE;
}

static void
set_locate_pointer (void)
{
  CdkKeymapKey *keys;
  CdkDisplay *display;
  CdkScreen *screen;
  int n_keys;
  gboolean has_entries = FALSE;
  static const guint keyvals[] = { CDK_KEY_Control_L, CDK_KEY_Control_R };
  unsigned int i, j;

  display = cdk_display_get_default ();
  screen = cdk_display_get_default_screen (display);

  for (i = 0; i < G_N_ELEMENTS (keyvals); ++i)
    {
      if (cdk_keymap_get_entries_for_keyval (cdk_keymap_get_for_display (display),
                                             keyvals[i],
                                             &keys,
                                             &n_keys))
        {
          has_entries = TRUE;
          for (j = 0; j < n_keys; ++j)
            {
              Window xroot;

              xroot = CDK_WINDOW_XID (cdk_screen_get_root_window (screen));

              XGrabKey (CDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        0,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (CDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        LockMask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (CDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        Mod2Mask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (CDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        Mod4Mask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
            }
          g_free (keys);
        }
    }

  if (has_entries)
    {
      cdk_window_add_filter (cdk_screen_get_root_window (screen),
                             (CdkFilterFunc) event_filter,
                             display);
    }
}


int
main (int argc, char *argv[])
{
  ctk_init (&argc, &argv);

  set_locate_pointer ();

  ctk_main ();

  return 0;
}


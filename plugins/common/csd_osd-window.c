/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * On-screen-display (OSD) window for cafe-settings-daemon's plugins
 *
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009 Novell, Inc
 *
 * Authors:
 *   William Jon McCann <mccann@jhu.edu>
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>

#include "csd_osd-window.h"

#define DIALOG_TIMEOUT 2000     /* dialog timeout in ms */
#define DIALOG_FADE_TIMEOUT 1500 /* timeout before fade starts */
#define FADE_TIMEOUT 10        /* timeout in ms between each frame of the fade */

#define BG_ALPHA 0.75

struct CsdOsdWindowPrivate
{
        guint                    is_composited : 1;
        guint                    hide_timeout_id;
        guint                    fade_timeout_id;
        double                   fade_out_alpha;
        gint                     scale_factor;
};

enum {
        DRAW_WHEN_COMPOSITED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (CsdOsdWindow, csd_osd_window, CTK_TYPE_WINDOW)

static gboolean
fade_timeout (CsdOsdWindow *window)
{
        if (window->priv->fade_out_alpha <= 0.0) {
                ctk_widget_hide (CTK_WIDGET (window));

                /* Reset it for the next time */
                window->priv->fade_out_alpha = 1.0;
                window->priv->fade_timeout_id = 0;

                return FALSE;
        } else {
                CdkRectangle rect;
                CtkWidget *win = CTK_WIDGET (window);
                CtkAllocation allocation;

                window->priv->fade_out_alpha -= 0.10;

                rect.x = 0;
                rect.y = 0;
                ctk_widget_get_allocation (win, &allocation);
                rect.width = allocation.width;
                rect.height = allocation.height;

                ctk_widget_realize (win);
                cdk_window_invalidate_rect (ctk_widget_get_window (win), &rect, FALSE);
        }

        return TRUE;
}

static gboolean
hide_timeout (CsdOsdWindow *window)
{
        if (window->priv->is_composited) {
                window->priv->hide_timeout_id = 0;
                window->priv->fade_timeout_id = g_timeout_add (FADE_TIMEOUT,
                                                               (GSourceFunc) fade_timeout,
                                                               window);
        } else {
                ctk_widget_hide (CTK_WIDGET (window));
        }

        return FALSE;
}

static void
remove_hide_timeout (CsdOsdWindow *window)
{
        if (window->priv->hide_timeout_id != 0) {
                g_source_remove (window->priv->hide_timeout_id);
                window->priv->hide_timeout_id = 0;
        }

        if (window->priv->fade_timeout_id != 0) {
                g_source_remove (window->priv->fade_timeout_id);
                window->priv->fade_timeout_id = 0;
                window->priv->fade_out_alpha = 1.0;
        }
}

static void
add_hide_timeout (CsdOsdWindow *window)
{
        int timeout;

        if (window->priv->is_composited) {
                timeout = DIALOG_FADE_TIMEOUT;
        } else {
                timeout = DIALOG_TIMEOUT;
        }
        window->priv->hide_timeout_id = g_timeout_add (timeout,
                                                       (GSourceFunc) hide_timeout,
                                                       window);
}

/* This is our draw-event handler when the window is in a compositing manager.
 * We draw everything by hand, using Cairo, so that we can have a nice
 * transparent/rounded look.
 */
static void
draw_when_composited (CtkWidget *widget, cairo_t *orig_cr)
{
        CsdOsdWindow    *window;
        cairo_t         *cr;
        cairo_surface_t *surface;
        int              width;
        int              height;
        CtkStyleContext *context;

        window = CSD_OSD_WINDOW (widget);

        context = ctk_widget_get_style_context (widget);
        cairo_set_operator (orig_cr, CAIRO_OPERATOR_SOURCE);
        ctk_window_get_size (CTK_WINDOW (widget), &width, &height);

        surface = cairo_surface_create_similar (cairo_get_target (orig_cr),
                                                CAIRO_CONTENT_COLOR_ALPHA,
                                                width,
                                                height);

        if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
                goto done;
        }

        cr = cairo_create (surface);
        if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
                goto done;
        }

        ctk_render_background (context, cr, 0, 0, width, height);
        ctk_render_frame (context, cr, 0, 0, width, height);

        g_signal_emit (window, signals[DRAW_WHEN_COMPOSITED], 0, cr);

        cairo_destroy (cr);

        /* Make sure we have a transparent background */
        cairo_rectangle (orig_cr, 0, 0, width, height);
        cairo_set_source_rgba (orig_cr, 0.0, 0.0, 0.0, 0.0);
        cairo_fill (orig_cr);

        cairo_set_source_surface (orig_cr, surface, 0, 0);
        cairo_paint_with_alpha (orig_cr, window->priv->fade_out_alpha);

done:
        if (surface != NULL) {
                cairo_surface_destroy (surface);
        }
}

/* This is our draw-event handler when the window is *not* in a compositing manager.
 * We just draw a rectangular frame by hand.  We do this with hardcoded drawing code,
 * instead of CtkFrame, to avoid changing the window's internal widget hierarchy:  in
 * either case (composited or non-composited), callers can assume that this works
 * identically to a CtkWindow without any intermediate widgetry.
 */
static void
draw_when_not_composited (CtkWidget *widget, cairo_t *cr)
{
        CtkStyleContext *context;
        int width;
        int height;

        ctk_window_get_size (CTK_WINDOW (widget), &width, &height);
        context = ctk_widget_get_style_context (widget);

        ctk_style_context_set_state (context, CTK_STATE_FLAG_ACTIVE);
        ctk_style_context_add_class(context,"csd_osd-window-solid");
        ctk_render_frame (context,
                          cr,
                          0,
                          0,
                          width,
                          height);
}

static gboolean
csd_osd_window_draw (CtkWidget *widget,
                     cairo_t   *cr)
{
	CsdOsdWindow *window;
	CtkWidget *child;

	window = CSD_OSD_WINDOW (widget);

	if (window->priv->is_composited)
		draw_when_composited (widget, cr);
	else
		draw_when_not_composited (widget, cr);

	child = ctk_bin_get_child (CTK_BIN (window));
	if (child)
		ctk_container_propagate_draw (CTK_CONTAINER (window), child, cr);

	return FALSE;
}

static void
csd_osd_window_real_show (CtkWidget *widget)
{
        CsdOsdWindow *window;

        if (CTK_WIDGET_CLASS (csd_osd_window_parent_class)->show) {
                CTK_WIDGET_CLASS (csd_osd_window_parent_class)->show (widget);
        }

        window = CSD_OSD_WINDOW (widget);
        remove_hide_timeout (window);
        add_hide_timeout (window);
}

static void
csd_osd_window_real_hide (CtkWidget *widget)
{
        CsdOsdWindow *window;

        if (CTK_WIDGET_CLASS (csd_osd_window_parent_class)->hide) {
                CTK_WIDGET_CLASS (csd_osd_window_parent_class)->hide (widget);
        }

        window = CSD_OSD_WINDOW (widget);
        remove_hide_timeout (window);
}

static void
csd_osd_window_real_realize (CtkWidget *widget)
{
        CdkScreen *screen;
        CdkVisual *visual;
        cairo_region_t *region;

        screen = ctk_widget_get_screen (widget);
        visual = cdk_screen_get_rgba_visual (screen);

        if (visual == NULL) {
                visual = cdk_screen_get_system_visual (screen);
        }

        ctk_widget_set_visual (widget, visual);

        if (CTK_WIDGET_CLASS (csd_osd_window_parent_class)->realize) {
                CTK_WIDGET_CLASS (csd_osd_window_parent_class)->realize (widget);
        }

        /* make the whole window ignore events */
        region = cairo_region_create ();
        ctk_widget_input_shape_combine_region (widget, region);
        cairo_region_destroy (region);
}

static void
csd_osd_window_style_updated (CtkWidget *widget)
{
        CtkStyleContext *context;
        CtkBorder padding;

        CTK_WIDGET_CLASS (csd_osd_window_parent_class)->style_updated (widget);

        /* We set our border width to 12 (per the CAFE standard), plus the
         * padding of the frame that we draw in our expose/draw handler.  This will
         * make our child be 12 pixels away from the frame.
         */

        context = ctk_widget_get_style_context (widget);
        ctk_style_context_get_padding (context, CTK_STATE_FLAG_NORMAL, &padding);
        ctk_container_set_border_width (CTK_CONTAINER (widget), 12 + MAX (padding.left, padding.top));
}

static void
csd_osd_window_get_preferred_width (CtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
        CtkStyleContext *context;
        CtkBorder padding;

        CTK_WIDGET_CLASS (csd_osd_window_parent_class)->get_preferred_width (widget, minimum, natural);

        /* See the comment in csd_osd_window_style_updated() for why we add the padding here */

        context = ctk_widget_get_style_context (widget);
        ctk_style_context_get_padding (context, CTK_STATE_FLAG_NORMAL, &padding);

        *minimum += padding.left;
        *natural += padding.left;
}

static void
csd_osd_window_get_preferred_height (CtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
        CtkStyleContext *context;
        CtkBorder padding;

        CTK_WIDGET_CLASS (csd_osd_window_parent_class)->get_preferred_height (widget, minimum, natural);

        /* See the comment in csd_osd_window_style_updated() for why we add the padding here */

        context = ctk_widget_get_style_context (widget);
        ctk_style_context_get_padding (context, CTK_STATE_FLAG_NORMAL, &padding);

        *minimum += padding.top;
        *natural += padding.top;
}

static GObject *
csd_osd_window_constructor (GType                  type,
                            guint                  n_construct_properties,
                            GObjectConstructParam *construct_params)
{
        GObject *object;

        object = G_OBJECT_CLASS (csd_osd_window_parent_class)->constructor (type, n_construct_properties, construct_params);

        g_object_set (object,
                      "type", CTK_WINDOW_POPUP,
                      "type-hint", CDK_WINDOW_TYPE_HINT_NOTIFICATION,
                      "skip-taskbar-hint", TRUE,
                      "skip-pager-hint", TRUE,
                      "focus-on-map", FALSE,
                      NULL);

        CtkWidget *widget = CTK_WIDGET (object);
        CtkStyleContext *style_context = ctk_widget_get_style_context (widget);
        ctk_style_context_add_class (style_context, "osd");

        return object;
}

static void
csd_osd_window_class_init (CsdOsdWindowClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (klass);

        gobject_class->constructor = csd_osd_window_constructor;

        widget_class->show = csd_osd_window_real_show;
        widget_class->hide = csd_osd_window_real_hide;
        widget_class->realize = csd_osd_window_real_realize;
        widget_class->style_updated = csd_osd_window_style_updated;
        widget_class->get_preferred_width = csd_osd_window_get_preferred_width;
        widget_class->get_preferred_height = csd_osd_window_get_preferred_height;
        widget_class->draw = csd_osd_window_draw;

        signals[DRAW_WHEN_COMPOSITED] = g_signal_new ("draw-when-composited",
                                                        G_TYPE_FROM_CLASS (gobject_class),
                                                        G_SIGNAL_RUN_FIRST,
                                                        G_STRUCT_OFFSET (CsdOsdWindowClass, draw_when_composited),
                                                        NULL, NULL,
                                                        g_cclosure_marshal_VOID__POINTER,
                                                        G_TYPE_NONE, 1,
                                                        G_TYPE_POINTER);

        ctk_widget_class_set_css_name (widget_class, "CsdOsdWindow");
}

/**
 * csd_osd_window_is_composited:
 * @window: a #CsdOsdWindow
 *
 * Return value: whether the window was created on a composited screen.
 */
gboolean
csd_osd_window_is_composited (CsdOsdWindow *window)
{
        return window->priv->is_composited;
}

/**
 * csd_osd_window_is_valid:
 * @window: a #CsdOsdWindow
 *
 * Return value: TRUE if the @window's idea of being composited matches whether
 * its current screen is actually composited, and whether the scale factor has
 * not changed since last draw.
 */
gboolean
csd_osd_window_is_valid (CsdOsdWindow *window)
{
        CdkScreen *screen = ctk_widget_get_screen (CTK_WIDGET (window));
        gint scale_factor = ctk_widget_get_scale_factor (CTK_WIDGET (window));
        return cdk_screen_is_composited (screen) == window->priv->is_composited
            && scale_factor == window->priv->scale_factor;
}

static void
csd_osd_window_init (CsdOsdWindow *window)
{
        CdkScreen *screen;

        window->priv = csd_osd_window_get_instance_private (window);

        screen = ctk_widget_get_screen (CTK_WIDGET (window));

        window->priv->is_composited = cdk_screen_is_composited (screen);
        window->priv->scale_factor = ctk_widget_get_scale_factor (CTK_WIDGET (window));

        if (window->priv->is_composited) {
                gdouble scalew, scaleh, scale;
                gint size;

                ctk_window_set_decorated (CTK_WINDOW (window), FALSE);
                ctk_widget_set_app_paintable (CTK_WIDGET (window), TRUE);

                CtkStyleContext *style = ctk_widget_get_style_context (CTK_WIDGET (window));
                ctk_style_context_add_class (style, "window-frame");

                /* assume 110x110 on a 640x480 display and scale from there */
                scalew = WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / (640.0 * window->priv->scale_factor);
                scaleh = HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / (480.0 * window->priv->scale_factor);
                scale = MIN (scalew, scaleh);
                size = 110 * MAX (1, scale);

                ctk_window_set_default_size (CTK_WINDOW (window), size, size);

                window->priv->fade_out_alpha = 1.0;
        } else {
                ctk_container_set_border_width (CTK_CONTAINER (window), 12);
        }
}

CtkWidget *
csd_osd_window_new (void)
{
        return g_object_new (CSD_TYPE_OSD_WINDOW, NULL);
}

/**
 * csd_osd_window_update_and_hide:
 * @window: a #CsdOsdWindow
 *
 * Queues the @window for immediate drawing, and queues a timer to hide the window.
 */
void
csd_osd_window_update_and_hide (CsdOsdWindow *window)
{
        remove_hide_timeout (window);
        add_hide_timeout (window);

        if (window->priv->is_composited) {
                ctk_widget_queue_draw (CTK_WIDGET (window));
        }
}

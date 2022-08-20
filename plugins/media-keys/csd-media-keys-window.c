/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 *
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

#include "csd-media-keys-window.h"

#define ICON_SCALE 0.55           /* size of the icon compared to the whole OSD */

struct CsdMediaKeysWindowPrivate
{
        CsdMediaKeysWindowAction action;
        char                    *icon_name;
        char                    *description;

        guint                    volume_muted : 1;
        guint                    mic_muted : 1;
        guint                    is_mic :1;
        int                      volume_level;

        CtkImage                *image;
        CtkWidget               *progress;
        CtkWidget               *label;
};

G_DEFINE_TYPE_WITH_PRIVATE (CsdMediaKeysWindow, csd_media_keys_window, CSD_TYPE_OSD_WINDOW)

static void
volume_controls_set_visible (CsdMediaKeysWindow *window,
                             gboolean            visible)
{
        if (window->priv->progress == NULL)
                return;

        if (visible) {
                ctk_widget_show (window->priv->progress);
        } else {
                ctk_widget_hide (window->priv->progress);
        }
}

static void
description_label_set_visible (CsdMediaKeysWindow *window,
                               gboolean            visible)
{
        if (visible) {
                ctk_label_set_text (CTK_LABEL (window->priv->label), window->priv->description);
                ctk_widget_show (window->priv->label);
        } else {
                ctk_widget_hide (window->priv->label);
        }
}

static void
window_set_icon_name (CsdMediaKeysWindow *window,
                      const char         *name)
{
        if (window->priv->image == NULL)
                return;

        ctk_image_set_from_icon_name (window->priv->image,
                                      name, CTK_ICON_SIZE_DIALOG);
}

static void
action_changed (CsdMediaKeysWindow *window)
{
        if (!csd_osd_window_is_composited (CSD_OSD_WINDOW (window))) {
                switch (window->priv->action) {
                case CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME:
                        volume_controls_set_visible (window, TRUE);
                        description_label_set_visible (window, FALSE);

                        if (window->priv->is_mic) {
                                if (window->priv->mic_muted)
                                        window_set_icon_name (window, "microphone-sensitivity-muted");
                                else
                                        window_set_icon_name (window, "microphone-sensitivity-high");
                        } else {
                                if (window->priv->volume_muted)
                                        window_set_icon_name (window, "audio-volume-muted");
                                else
                                        window_set_icon_name (window, "audio-volume-high");
                        }

                        break;
                case CSD_MEDIA_KEYS_WINDOW_ACTION_CUSTOM:
                        volume_controls_set_visible (window, FALSE);
                        description_label_set_visible (window, TRUE);
                        window_set_icon_name (window, window->priv->icon_name);
                        break;
                default:
                        g_assert_not_reached ();
                        break;
                }
        }

        csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));
}

static void
volume_level_changed (CsdMediaKeysWindow *window)
{
        csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));

        if (!csd_osd_window_is_composited (CSD_OSD_WINDOW (window)) && window->priv->progress != NULL) {
                double fraction;

                fraction = (double) window->priv->volume_level / 100.0;

                ctk_progress_bar_set_fraction (CTK_PROGRESS_BAR (window->priv->progress),
                                               fraction);
        }
}

static void
volume_muted_changed (CsdMediaKeysWindow *window)
{
        csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));

        if (!csd_osd_window_is_composited (CSD_OSD_WINDOW (window))) {
                if (window->priv->volume_muted) {
                        window_set_icon_name (window, "audio-volume-muted");
                } else {
                        window_set_icon_name (window, "audio-volume-high");
                }
        }
}

static void
mic_muted_changed (CsdMediaKeysWindow *window)
{
        csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));

        if (!csd_osd_window_is_composited (CSD_OSD_WINDOW (window))) {
                if (window->priv->mic_muted) {
                        window_set_icon_name (window, "microphone-sensitivity-muted");
                } else {
                        window_set_icon_name (window, "microphone-sensitivity-high");
                }
        }
}

void
csd_media_keys_window_set_action (CsdMediaKeysWindow      *window,
                                  CsdMediaKeysWindowAction action)
{
        g_return_if_fail (CSD_IS_MEDIA_KEYS_WINDOW (window));
        g_return_if_fail (action == CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

        if (window->priv->action != action) {
                window->priv->action = action;
                action_changed (window);
        } else {
                if (window->priv->is_mic) {
                        if (window->priv->mic_muted)
                                window_set_icon_name (window, "microphone-sensitivity-muted");
                        else
                                window_set_icon_name (window, "microphone-sensitivity-high");
                } else {
                        if (window->priv->volume_muted)
                                window_set_icon_name (window, "audio-volume-muted");
                        else
                                window_set_icon_name (window, "audio-volume-high");
                }
                csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));
        }
}

void
csd_media_keys_window_set_action_custom (CsdMediaKeysWindow      *window,
                                         const char              *icon_name,
                                         const char              *description)
{
        g_return_if_fail (CSD_IS_MEDIA_KEYS_WINDOW (window));
        g_return_if_fail (icon_name != NULL);

        if (window->priv->action != CSD_MEDIA_KEYS_WINDOW_ACTION_CUSTOM ||
            g_strcmp0 (window->priv->icon_name, icon_name) != 0 ||
            g_strcmp0 (window->priv->description, description) != 0) {
                window->priv->action = CSD_MEDIA_KEYS_WINDOW_ACTION_CUSTOM;
                g_free (window->priv->icon_name);
                window->priv->icon_name = g_strdup (icon_name);
                g_free (window->priv->description);
                window->priv->description = g_strdup (description);
                action_changed (window);
        } else {
                csd_osd_window_update_and_hide (CSD_OSD_WINDOW (window));
        }
}

void
csd_media_keys_window_set_volume_muted (CsdMediaKeysWindow *window,
                                        gboolean            muted)
{
        g_return_if_fail (CSD_IS_MEDIA_KEYS_WINDOW (window));

        if (window->priv->volume_muted != muted) {
                window->priv->volume_muted = muted;
                volume_muted_changed (window);
        }
        window->priv->is_mic = FALSE;
}

void
csd_media_keys_window_set_mic_muted (CsdMediaKeysWindow *window,
                                     gboolean            muted)
{
        g_return_if_fail (CSD_IS_MEDIA_KEYS_WINDOW (window));

        if (window->priv->mic_muted != muted) {
                window->priv->mic_muted = muted;
                mic_muted_changed (window);
        }
        window->priv->is_mic = TRUE;
}

void
csd_media_keys_window_set_volume_level (CsdMediaKeysWindow *window,
                                        int                 level)
{
        g_return_if_fail (CSD_IS_MEDIA_KEYS_WINDOW (window));

        if (window->priv->volume_level != level) {
                window->priv->volume_level = level;
                volume_level_changed (window);
        }
}

static GdkPixbuf *
load_pixbuf (CsdMediaKeysWindow *window,
             const char         *name,
             int                 icon_size)
{
        CtkIconTheme *theme;
        GdkPixbuf    *pixbuf;

        if (window != NULL && ctk_widget_has_screen (CTK_WIDGET (window))) {
                theme = ctk_icon_theme_get_for_screen (ctk_widget_get_screen (CTK_WIDGET (window)));
        } else {
                theme = ctk_icon_theme_get_default ();
        }

        pixbuf = ctk_icon_theme_load_icon (theme,
                                           name,
                                           icon_size,
                                           CTK_ICON_LOOKUP_FORCE_SIZE,
                                           NULL);

        return pixbuf;
}

static void
draw_eject (cairo_t *cr,
            double   _x0,
            double   _y0,
            double   width,
            double   height)
{
        int box_height;
        int tri_height;
        int separation;

        box_height = height * 0.2;
        separation = box_height / 3;
        tri_height = height - box_height - separation;

        cairo_rectangle (cr, _x0, _y0 + height - box_height, width, box_height);

        cairo_move_to (cr, _x0, _y0 + tri_height);
        cairo_rel_line_to (cr, width, 0);
        cairo_rel_line_to (cr, -width / 2, -tri_height);
        cairo_rel_line_to (cr, -width / 2, tri_height);
        cairo_close_path (cr);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, CSD_OSD_WINDOW_FG_ALPHA);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, CSD_OSD_WINDOW_FG_ALPHA / 2);
        cairo_set_line_width (cr, 2);
        cairo_stroke (cr);
}

static void
draw_waves (cairo_t *cr,
            double   cx,
            double   cy,
            double   max_radius,
            int      volume_level)
{
        const int n_waves = 3;
        int last_wave;
        int i;

        last_wave = n_waves * volume_level / 100;

        for (i = 0; i < n_waves; i++) {
                double angle1;
                double angle2;
                double radius;
                double alpha;

                angle1 = -M_PI / 4;
                angle2 = M_PI / 4;

                if (i < last_wave)
                        alpha = 1.0;
                else if (i > last_wave)
                        alpha = 0.1;
                else alpha = 0.1 + 0.9 * (n_waves * volume_level % 100) / 100.0;

                radius = (i + 1) * (max_radius / n_waves);
                cairo_arc (cr, cx, cy, radius, angle1, angle2);
                cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, alpha / 2);
                cairo_set_line_width (cr, 14);
                cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
                cairo_stroke_preserve (cr);

                cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha);
                cairo_set_line_width (cr, 10);
                cairo_set_line_cap  (cr, CAIRO_LINE_CAP_ROUND);
                cairo_stroke (cr);
        }
}

static void
draw_cross (cairo_t *cr,
            double   cx,
            double   cy,
            double   size)
{
        cairo_move_to (cr, cx, cy - size/2.0);
        cairo_rel_line_to (cr, size, size);

        cairo_move_to (cr, cx, cy + size/2.0);
        cairo_rel_line_to (cr, size, -size);

        cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, CSD_OSD_WINDOW_FG_ALPHA / 2);
        cairo_set_line_width (cr, 14);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        cairo_stroke_preserve (cr);

        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, CSD_OSD_WINDOW_FG_ALPHA);
        cairo_set_line_width (cr, 10);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        cairo_stroke (cr);
}

static void
draw_speaker (cairo_t *cr,
              double   cx,
              double   cy,
              double   width,
              double   height)
{
        double box_width;
        double box_height;
        double _x0;
        double _y0;

        box_width = width / 3;
        box_height = height / 3;

        _x0 = cx - (width / 2) + box_width;
        _y0 = cy - box_height / 2;

        cairo_move_to (cr, _x0, _y0);
        cairo_rel_line_to (cr, - box_width, 0);
        cairo_rel_line_to (cr, 0, box_height);
        cairo_rel_line_to (cr, box_width, 0);

        cairo_line_to (cr, cx + box_width, cy + height / 2);
        cairo_rel_line_to (cr, 0, -height);
        cairo_line_to (cr, _x0, _y0);
        cairo_close_path (cr);

        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, CSD_OSD_WINDOW_FG_ALPHA);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.6, 0.6, 0.6, CSD_OSD_WINDOW_FG_ALPHA / 2);
        cairo_set_line_width (cr, 2);
        cairo_stroke (cr);
}

static gboolean
render_speaker (CsdMediaKeysWindow *window,
                cairo_t            *cr,
                double              _x0,
                double              _y0,
                double              width,
                double              height)
{
        GdkPixbuf         *pixbuf;
        int                icon_size;
        int                n;
        static const char *icon_names[] = {
                "audio-volume-muted",
                "audio-volume-low",
                "audio-volume-medium",
                "audio-volume-high",
                "microphone-sensitivity-muted",
                "microphone-sensitivity-low",
                "microphone-sensitivity-medium",
                "microphone-sensitivity-high",
                NULL
        };
        if (!window->priv->is_mic) {
                if (window->priv->volume_muted) {
                        n = 0;
                } else {
                        /* select volume image */
                        n = 3 * window->priv->volume_level / 100 + 1;
                        if (n < 1) {
                                n = 1;
                        } else if (n > 3) {
                                n = 3;
                        }
                }
        } else {
                if (window->priv->mic_muted) {
                        n = 4;
                } else {
                        /* select microphone image */
                        n = 3 * window->priv->volume_level / 100 + 5;
                        if (n < 5) {
                                n = 5;
                        } else if (n > 7) {
                                n = 7;
                        }
                }
        }

        icon_size = (int)width;

        pixbuf = load_pixbuf (window, icon_names[n], icon_size);

        if (pixbuf == NULL) {
                return FALSE;
        }

        cdk_cairo_set_source_pixbuf (cr, pixbuf, _x0, _y0);
        cairo_paint_with_alpha (cr, CSD_OSD_WINDOW_FG_ALPHA);

        g_object_unref (pixbuf);

        return TRUE;
}

static void
draw_description_label (CsdMediaKeysWindow *window,
                        cairo_t            *cr,
                        double              _y0,
                        double              width)
{
        cairo_text_extents_t extents;

        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_set_font_size (cr, 14);

        /* centered text */
        cairo_text_extents (cr, window->priv->description, &extents);
        cairo_move_to (cr, width / 2 - extents.width / 2, _y0);
        cairo_show_text (cr, window->priv->description);
}

static void
draw_volume_boxes (CsdMediaKeysWindow *window,
                   cairo_t            *cr,
                   double              percentage,
                   double              _x0,
                   double              _y0,
                   double              width,
                   double              height)
{
        gdouble   x1;
        CtkStyleContext *context;

        height = round (height) - 1;
        width = round (width) - 1;
        x1 = round ((width - 1) * percentage);
        context = ctk_widget_get_style_context (CTK_WIDGET (window));

        /* bar background */
        ctk_style_context_save (context);
        ctk_style_context_add_class (context, CTK_STYLE_CLASS_TROUGH);

        ctk_render_background (context, cr, _x0, _y0, width, height);
        ctk_render_frame (context, cr, _x0, _y0, width, height);

        ctk_style_context_restore (context);

        /* bar progress */
        if (percentage < 0.01)
                return;

        ctk_style_context_save (context);
        ctk_style_context_add_class (context, CTK_STYLE_CLASS_PROGRESSBAR);

        ctk_render_background (context, cr, _x0 + 0.5, _y0 + 0.5, x1, height -1 );
        ctk_render_frame (context, cr, _x0 + 0.5, _y0 + 0.5, x1, height -1 );

        ctk_style_context_restore (context);
}

static void
draw_action_volume (CsdMediaKeysWindow *window,
                    cairo_t            *cr)
{
        int window_width;
        int window_height;
        double icon_box_width;
        double icon_box_height;
        double icon_box_x0;
        double icon_box_y0;
        double volume_box_x0;
        double volume_box_y0;
        double volume_box_width;
        double volume_box_height;
        gboolean res;

        ctk_window_get_size (CTK_WINDOW (window), &window_width, &window_height);

        icon_box_width = round (window_width * ICON_SCALE);
        icon_box_height = round (window_height * ICON_SCALE);
        volume_box_width = icon_box_width;
        volume_box_height = round (window_height * 0.05);

        icon_box_x0 = round ((window_width - icon_box_width) / 2);
        icon_box_y0 = round ((window_height - icon_box_height) / 2);
        volume_box_x0 = round (icon_box_x0);
        volume_box_y0 = round (window_height - icon_box_y0 / 2 - volume_box_height);

#if 0
        g_message ("icon box: w=%f h=%f _x0=%f _y0=%f",
                   icon_box_width,
                   icon_box_height,
                   icon_box_x0,
                   icon_box_y0);
        g_message ("volume box: w=%f h=%f _x0=%f _y0=%f",
                   volume_box_width,
                   volume_box_height,
                   volume_box_x0,
                   volume_box_y0);
#endif

        res = render_speaker (window,
                              cr,
                              icon_box_x0, icon_box_y0,
                              icon_box_width, icon_box_height);
        if (! res) {
                double speaker_width;
                double speaker_height;
                double speaker_cx;
                double speaker_cy;

                speaker_width = icon_box_width * 0.5;
                speaker_height = icon_box_height * 0.75;
                speaker_cx = icon_box_x0 + speaker_width / 2;
                speaker_cy = icon_box_y0 + speaker_height / 2;

#if 0
                g_message ("speaker box: w=%f h=%f cx=%f cy=%f",
                           speaker_width,
                           speaker_height,
                           speaker_cx,
                           speaker_cy);
#endif

                /* draw speaker symbol */
                draw_speaker (cr, speaker_cx, speaker_cy, speaker_width, speaker_height);

                if (! window->priv->volume_muted) {
                        /* draw sound waves */
                        double wave_x0;
                        double wave_y0;
                        double wave_radius;

                        wave_x0 = window_width / 2;
                        wave_y0 = speaker_cy;
                        wave_radius = icon_box_width / 2;

                        draw_waves (cr, wave_x0, wave_y0, wave_radius, window->priv->volume_level);
                } else {
                        /* draw 'mute' cross */
                        double cross_x0;
                        double cross_y0;
                        double cross_size;

                        cross_size = speaker_width * 3 / 4;
                        cross_x0 = icon_box_x0 + icon_box_width - cross_size;
                        cross_y0 = speaker_cy;

                        draw_cross (cr, cross_x0, cross_y0, cross_size);
                }
        }

        /* draw volume meter */
        draw_volume_boxes (window,
                           cr,
                           (double)window->priv->volume_level / 100.0,
                           volume_box_x0,
                           volume_box_y0,
                           volume_box_width,
                           volume_box_height);
}

static gboolean
render_custom (CsdMediaKeysWindow *window,
               cairo_t            *cr,
               double              _x0,
               double              _y0,
               double              width,
               double              height)
{
        GdkPixbuf         *pixbuf;
        int                icon_size;

        icon_size = (int)width;

        pixbuf = load_pixbuf (window, window->priv->icon_name, icon_size);

        if (pixbuf == NULL) {
                char *name;
                if (ctk_widget_get_direction (CTK_WIDGET (window)) == CTK_TEXT_DIR_RTL)
                        name = g_strdup_printf ("%s-rtl", window->priv->icon_name);
                else
                        name = g_strdup_printf ("%s-ltr", window->priv->icon_name);
                pixbuf = load_pixbuf (window, name, icon_size);
                g_free (name);
                if (pixbuf == NULL)
                        return FALSE;
        }

        cdk_cairo_set_source_pixbuf (cr, pixbuf, _x0, _y0);
        cairo_paint_with_alpha (cr, CSD_OSD_WINDOW_FG_ALPHA);

        g_object_unref (pixbuf);

        return TRUE;
}

static void
draw_action_custom (CsdMediaKeysWindow *window,
                    cairo_t            *cr)
{
        int window_width;
        int window_height;
        double icon_box_width;
        double icon_box_height;
        double icon_box_x0;
        double icon_box_y0;
        double label_box_y0;
        double label_box_width;
        double label_box_height;
        gboolean res;

        ctk_window_get_size (CTK_WINDOW (window), &window_width, &window_height);

        icon_box_width = round (window_width * ICON_SCALE);
        icon_box_height = round (window_height * ICON_SCALE);
        label_box_width = round (window_width);
        label_box_height = round (window_height * 0.175);

        icon_box_x0 = round ((window_width - icon_box_width) / 2);
        icon_box_y0 = round ((window_height - icon_box_height) / 2);
        label_box_y0 = round (window_height - label_box_height / 2);

#if 0
        g_message ("icon box: w=%f h=%f _x0=%f _y0=%f",
                   icon_box_width,
                   icon_box_height,
                   icon_box_x0,
                   icon_box_y0);
        g_message ("label box: w=%f h=%f _x0=0.0 _y0=%f",
                   label_box_width,
                   label_box_height,
                   label_box_y0);
#endif

        res = render_custom (window,
                             cr,
                             icon_box_x0, icon_box_y0,
                             icon_box_width, icon_box_height);
        if (! res && g_strcmp0 (window->priv->icon_name, "media-eject") == 0) {
                /* draw eject symbol */
                draw_eject (cr,
                            icon_box_x0, icon_box_y0,
                            icon_box_width, icon_box_height);
        }

        if (window->priv->description != NULL) {
                /* draw description label meter */
                draw_description_label (window,
                                        cr,
                                        label_box_y0,
                                        label_box_width);
        }
}

static void
csd_media_keys_window_draw_when_composited (CsdOsdWindow *osd_window,
                                              cairo_t      *cr)
{
        CsdMediaKeysWindow *window = CSD_MEDIA_KEYS_WINDOW (osd_window);

        switch (window->priv->action) {
        case CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME:
                draw_action_volume (window, cr);
                break;
        case CSD_MEDIA_KEYS_WINDOW_ACTION_CUSTOM:
                draw_action_custom (window, cr);
                break;
        default:
                break;
        }
}

static void
csd_media_keys_window_class_init (CsdMediaKeysWindowClass *klass)
{
        CsdOsdWindowClass *osd_window_class = CSD_OSD_WINDOW_CLASS (klass);

        osd_window_class->draw_when_composited = csd_media_keys_window_draw_when_composited;
}

static void
csd_media_keys_window_init (CsdMediaKeysWindow *window)
{
        window->priv = csd_media_keys_window_get_instance_private (window);

        if (!csd_osd_window_is_composited (CSD_OSD_WINDOW (window))) {
                CtkBuilder *builder;
                const gchar *objects[] = {"acme_box", NULL};
                CtkWidget *box;

                builder = ctk_builder_new ();
                ctk_builder_add_objects_from_file (builder,
                                                   CTKBUILDERDIR "/acme.ui",
                                                   (char **) objects,
                                                   NULL);

                window->priv->image = CTK_IMAGE (ctk_builder_get_object (builder, "acme_image"));
                window->priv->progress = CTK_WIDGET (ctk_builder_get_object (builder, "acme_volume_progressbar"));
                window->priv->label = CTK_WIDGET (ctk_builder_get_object (builder, "acme_label"));
                box = CTK_WIDGET (ctk_builder_get_object (builder, "acme_box"));

                if (box != NULL) {
                        ctk_container_add (CTK_CONTAINER (window), box);
                        ctk_widget_show_all (box);
                }

                /* The builder needs to stay alive until the window
                   takes ownership of the box (and its children)  */
                g_object_unref (builder);
        }
}

CtkWidget *
csd_media_keys_window_new (void)
{
        return g_object_new (CSD_TYPE_MEDIA_KEYS_WINDOW, NULL);
}

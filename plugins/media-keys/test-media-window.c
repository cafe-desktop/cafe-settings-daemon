/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include "csd-media-keys-window.h"

static gboolean
update_state (CtkWidget *window)
{
        static int count = 0;

        count++;

        switch (count) {
        case 1:
                csd_media_keys_window_set_volume_level (CSD_MEDIA_KEYS_WINDOW (window),
                                                        50);
                csd_media_keys_window_set_action (CSD_MEDIA_KEYS_WINDOW (window),
                                                  CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                ctk_widget_show (window);
                break;
        case 2:
                csd_media_keys_window_set_volume_level (CSD_MEDIA_KEYS_WINDOW (window),
                                                        100);
                csd_media_keys_window_set_action (CSD_MEDIA_KEYS_WINDOW (window),
                                                  CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                ctk_widget_show (window);
                break;
        case 3:
                csd_media_keys_window_set_volume_muted (CSD_MEDIA_KEYS_WINDOW (window),
                                                        TRUE);
                csd_media_keys_window_set_action (CSD_MEDIA_KEYS_WINDOW (window),
                                                  CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                ctk_widget_show (window);
                break;
        case 4:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "media-eject",
                                                         NULL);

                ctk_widget_show (window);
                break;
	case 5:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "touchpad-disabled",
                                                         _("Touchpad disabled"));

                ctk_widget_show (window);
                break;
        case 6:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "input-touchpad",
                                                         _("Touchpad enabled"));

                ctk_widget_show (window);
                break;
	case 7:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "bluetooth-disabled-symbolic",
                                                         _("Bluetooth disabled"));

                ctk_widget_show (window);
                break;
        case 8:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "bluetooth-active-symbolic",
                                                         _("Bluetooth enabled"));

                ctk_widget_show (window);
                break;
        case 9:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "airplane-mode-symbolic",
                                                         _("Airplane mode enabled"));

                ctk_widget_show (window);
                break;
        case 10:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "network-wireless-signal-excellent-symbolic",
                                                         _("Airplane mode disabled"));

                ctk_widget_show (window);
                break;
        case 11:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "video-single-display-symbolic",
                                                         _("No External Display"));

                ctk_widget_show (window);
                break;
        case 12:
                csd_media_keys_window_set_action_custom (CSD_MEDIA_KEYS_WINDOW (window),
                                                         "video-joined-displays-symbolic",
                                                         _("Changing Screen Layout"));

                ctk_widget_show (window);
                break;
        default:
                ctk_main_quit ();
                break;
        }

        return TRUE;
}

static void
test_window (void)
{
        CtkWidget *window;

        window = csd_media_keys_window_new ();
        ctk_window_set_position (CTK_WINDOW (window), CTK_WIN_POS_CENTER_ALWAYS);

        csd_media_keys_window_set_volume_level (CSD_MEDIA_KEYS_WINDOW (window),
                                                0);
        csd_media_keys_window_set_action (CSD_MEDIA_KEYS_WINDOW (window),
                                          CSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

        ctk_widget_show (window);

        g_timeout_add (3000, (GSourceFunc) update_state, window);
}

int
main (int    argc,
      char **argv)
{
        GError *error = NULL;

#ifdef ENABLE_NLS
        bindtextdomain (GETTEXT_PACKAGE, CAFE_SETTINGS_LOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
        textdomain (GETTEXT_PACKAGE);
#endif

        if (! ctk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
                fprintf (stderr, "%s", error->message);
                g_error_free (error);
                exit (1);
        }

        test_window ();

        ctk_main ();

        return 0;
}

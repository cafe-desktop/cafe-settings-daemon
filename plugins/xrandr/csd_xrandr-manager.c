/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2007, 2008 Red Hat, Inc
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

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <ctk/ctk.h>
#include <gio/gio.h>
#include <dbus/dbus-glib.h>

#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-rr-config.h>
#include <libcafe-desktop/cafe-rr.h>
#include <libcafe-desktop/cafe-rr-labeler.h>
#include <libcafe-desktop/cafe-desktop-utils.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#ifdef HAVE_RDA
#include <rda/rda.h>
#endif

#include "cafe-settings-profile.h"
#include "csd_xrandr-manager.h"

#define CONF_SCHEMA                                    "org.cafe.SettingsDaemon.plugins.xrandr"
#define CONF_KEY_SHOW_NOTIFICATION_ICON                "show-notification-icon"
#define CONF_KEY_USE_XORG_MONITOR_SETTINGS             "use-xorg-monitor-settings"
#define CONF_KEY_TURN_ON_EXTERNAL_MONITORS_AT_STARTUP  "turn-on-external-monitors-at-startup"
#define CONF_KEY_TURN_ON_LAPTOP_MONITOR_AT_STARTUP     "turn-on-laptop-monitor-at-startup"
#define CONF_KEY_DEFAULT_CONFIGURATION_FILE            "default-configuration-file"

#define VIDEO_KEYSYM    "XF86Display"
#define ROTATE_KEYSYM   "XF86RotateWindows"

/* Number of seconds that the confirmation dialog will last before it resets the
 * RANDR configuration to its old state.
 */
#define CONFIRMATION_DIALOG_SECONDS 30

/* name of the icon files (csd_xrandr.svg, etc.) */
#define CSD_XRANDR_ICON_NAME "csd_xrandr"

/* executable of the control center's display configuration capplet */
#define CSD_XRANDR_DISPLAY_CAPPLET "cafe-display-properties"

#define CSD_DBUS_PATH "/org/cafe/SettingsDaemon"
#define CSD_DBUS_NAME "org.cafe.SettingsDaemon"
#define CSD_XRANDR_DBUS_PATH CSD_DBUS_PATH "/XRANDR"
#define CSD_XRANDR_DBUS_NAME CSD_DBUS_NAME ".XRANDR"

struct CsdXrandrManagerPrivate
{
        DBusGConnection *dbus_connection;

        /* Key code of the XF86Display key (Fn-F7 on Thinkpads, Fn-F4 on HP machines, etc.) */
        guint switch_video_mode_keycode;

        /* Key code of the XF86RotateWindows key (present on some tablets) */
        guint rotate_windows_keycode;

        CafeRRScreen *rw_screen;
        gboolean running;

        CtkStatusIcon *status_icon;
        CtkWidget *popup_menu;
        CafeRRConfig *configuration;
        CafeRRLabeler *labeler;
        GSettings *settings;

        /* fn-F7 status */
        int             current_fn_f7_config;             /* -1 if no configs */
        CafeRRConfig **fn_f7_configs;  /* NULL terminated, NULL if there are no configs */

        /* Last time at which we got a "screen got reconfigured" event; see on_randr_event() */
        guint32 last_config_timestamp;
};

static const CafeRRRotation possible_rotations[] = {
        CAFE_RR_ROTATION_0,
        CAFE_RR_ROTATION_90,
        CAFE_RR_ROTATION_180,
        CAFE_RR_ROTATION_270
        /* We don't allow REFLECT_X or REFLECT_Y for now, as cafe-display-properties doesn't allow them, either */
};

static void csd_xrandr_manager_finalize (GObject *object);

static void error_message (CsdXrandrManager *mgr, const char *primary_text, GError *error_to_display, const char *secondary_text);

static void status_icon_popup_menu (CsdXrandrManager *manager, guint button, guint32 timestamp);
static void run_display_capplet (CtkWidget *widget);
static void get_allowed_rotations_for_output (CafeRRConfig *config,
                                              CafeRRScreen *rr_screen,
                                              CafeRROutputInfo *output,
                                              int *out_num_rotations,
                                              CafeRRRotation *out_rotations);

G_DEFINE_TYPE_WITH_PRIVATE (CsdXrandrManager, csd_xrandr_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static FILE *log_file;

static void
log_open (void)
{
        char *toggle_filename;
        char *log_filename;
        struct stat st;

        if (log_file)
                return;

        toggle_filename = g_build_filename (g_get_home_dir (), "csd_debug-randr", NULL);
        log_filename = g_build_filename (g_get_home_dir (), "csd_debug-randr.log", NULL);

        if (stat (toggle_filename, &st) != 0)
                goto out;

        log_file = fopen (log_filename, "a");

        if (log_file && ftell (log_file) == 0)
                fprintf (log_file, "To keep this log from being created, please rm ~/csd_debug-randr\n");

out:
        g_free (toggle_filename);
        g_free (log_filename);
}

static void
log_close (void)
{
        if (log_file) {
                fclose (log_file);
                log_file = NULL;
        }
}

static void
log_msg (const char *format, ...)
{
        if (log_file) {
                va_list args;

                va_start (args, format);
                vfprintf (log_file, format, args);
                va_end (args);
        }
}

static void
log_output (CafeRROutputInfo *output)
{
        gchar *name = cafe_rr_output_info_get_name (output);
        gchar *display_name = cafe_rr_output_info_get_display_name (output);

        log_msg ("        %s: ", name ? name : "unknown");

        if (cafe_rr_output_info_is_connected (output)) {
                if (cafe_rr_output_info_is_active (output)) {
                        int x, y, width, height;
                        cafe_rr_output_info_get_geometry (output, &x, &y, &width, &height);
                        log_msg ("%dx%d@%d +%d+%d",
                                 width,
                                 height,
                                 cafe_rr_output_info_get_refresh_rate (output),
                                 x,
                                 y);
                } else
                        log_msg ("off");
        } else
                log_msg ("disconnected");

        if (display_name)
                log_msg (" (%s)", display_name);

        if (cafe_rr_output_info_get_primary (output))
                log_msg (" (primary output)");

        log_msg ("\n");
}

static void
log_configuration (CafeRRConfig *config)
{
        int i;
        CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (config);

        log_msg ("        cloned: %s\n", cafe_rr_config_get_clone (config) ? "yes" : "no");

        for (i = 0; outputs[i] != NULL; i++)
                log_output (outputs[i]);

        if (i == 0)
                log_msg ("        no outputs!\n");
}

static char
timestamp_relationship (guint32 a, guint32 b)
{
        if (a < b)
                return '<';
        else if (a > b)
                return '>';
        else
                return '=';
}

static void
log_screen (CafeRRScreen *screen)
{
        CafeRRConfig *config;
        int min_w, min_h, max_w, max_h;
        guint32 change_timestamp, config_timestamp;

        if (!log_file)
                return;

        config = cafe_rr_config_new_current (screen, NULL);

        cafe_rr_screen_get_ranges (screen, &min_w, &max_w, &min_h, &max_h);
        cafe_rr_screen_get_timestamps (screen, &change_timestamp, &config_timestamp);

        log_msg ("        Screen min(%d, %d), max(%d, %d), change=%u %c config=%u\n",
                 min_w, min_h,
                 max_w, max_h,
                 change_timestamp,
                 timestamp_relationship (change_timestamp, config_timestamp),
                 config_timestamp);

        log_configuration (config);
        g_object_unref (config);
}

static void
log_configurations (CafeRRConfig **configs)
{
        int i;

        if (!configs) {
                log_msg ("    No configurations\n");
                return;
        }

        for (i = 0; configs[i]; i++) {
                log_msg ("    Configuration %d\n", i);
                log_configuration (configs[i]);
        }
}

static void
show_timestamps_dialog (CsdXrandrManager *manager, const char *msg)
{
#if 1
        return;
#else
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *dialog;
        guint32 change_timestamp, config_timestamp;
        static int serial;

        cafe_rr_screen_get_timestamps (priv->rw_screen, &change_timestamp, &config_timestamp);

        dialog = ctk_message_dialog_new (NULL,
                                         0,
                                         CTK_MESSAGE_INFO,
                                         CTK_BUTTONS_CLOSE,
                                         "RANDR timestamps (%d):\n%s\nchange: %u\nconfig: %u",
                                         serial++,
                                         msg,
                                         change_timestamp,
                                         config_timestamp);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (ctk_widget_destroy), NULL);
        ctk_widget_show (dialog);
#endif
}

/* This function centralizes the use of cafe_rr_config_apply_from_filename_with_time().
 *
 * Optionally filters out CAFE_RR_ERROR_NO_MATCHING_CONFIG from
 * cafe_rr_config_apply_from_filename_with_time(), since that is not usually an error.
 */
static gboolean
apply_configuration_from_filename (CsdXrandrManager *manager,
                                   const char       *filename,
                                   gboolean          no_matching_config_is_an_error,
                                   guint32           timestamp,
                                   GError          **error)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        GError *my_error;
        gboolean success;
        char *str;

        str = g_strdup_printf ("Applying %s with timestamp %d", filename, timestamp);
        show_timestamps_dialog (manager, str);
        g_free (str);

        my_error = NULL;
        success = cafe_rr_config_apply_from_filename_with_time (priv->rw_screen, filename, timestamp, &my_error);
        if (success)
                return TRUE;

        if (g_error_matches (my_error, CAFE_RR_ERROR, CAFE_RR_ERROR_NO_MATCHING_CONFIG)) {
                if (no_matching_config_is_an_error)
                        goto fail;

                /* This is not an error; the user probably changed his monitors
                 * and so they don't match any of the stored configurations.
                 */
                g_error_free (my_error);
                return TRUE;
        }

fail:
        g_propagate_error (error, my_error);
        return FALSE;
}

/* This function centralizes the use of cafe_rr_config_apply_with_time().
 *
 * Applies a configuration and displays an error message if an error happens.
 * We just return whether setting the configuration succeeded.
 */
static gboolean
apply_configuration_and_display_error (CsdXrandrManager *manager, CafeRRConfig *config, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = manager->priv;
        GError *error;
        gboolean success;

        error = NULL;
        success = cafe_rr_config_apply_with_time (config, priv->rw_screen, timestamp, &error);
        if (!success) {
                log_msg ("Could not switch to the following configuration (timestamp %u): %s\n", timestamp, error->message);
                log_configuration (config);
                error_message (manager, _("Could not switch the monitor configuration"), error, NULL);
                g_error_free (error);
        }

        return success;
}

static void
restore_backup_configuration_without_messages (const char *backup_filename, const char *intended_filename)
{
        backup_filename = cafe_rr_config_get_backup_filename ();
        rename (backup_filename, intended_filename);
}

static void
restore_backup_configuration (CsdXrandrManager *manager, const char *backup_filename, const char *intended_filename, guint32 timestamp)
{
        int saved_errno;

        if (rename (backup_filename, intended_filename) == 0) {
                GError *error;

                error = NULL;
                if (!apply_configuration_from_filename (manager, intended_filename, FALSE, timestamp, &error)) {
                        error_message (manager, _("Could not restore the display's configuration"), error, NULL);

                        if (error)
                                g_error_free (error);
                }

                return;
        }

        saved_errno = errno;

        /* ENOENT means the original file didn't exist.  That is *not* an error;
         * the backup was not created because there wasn't even an original
         * monitors.xml (such as on a first-time login).  Note that *here* there
         * is a "didn't work" monitors.xml, so we must delete that one.
         */
        if (saved_errno == ENOENT)
                unlink (intended_filename);
        else {
                char *msg;

                msg = g_strdup_printf ("Could not rename %s to %s: %s",
                                       backup_filename, intended_filename,
                                       g_strerror (saved_errno));
                error_message (manager,
                               _("Could not restore the display's configuration from a backup"),
                               NULL,
                               msg);
                g_free (msg);
        }

        unlink (backup_filename);
}

typedef struct {
        CsdXrandrManager *manager;
        CtkWidget *dialog;

        int countdown;
        int response_id;
} TimeoutDialog;

static void
print_countdown_text (TimeoutDialog *timeout)
{
        ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (timeout->dialog),
                                                  ngettext ("The display will be reset to its previous configuration in %d second",
                                                            "The display will be reset to its previous configuration in %d seconds",
                                                            timeout->countdown),
                                                  timeout->countdown);
}

static gboolean
timeout_cb (gpointer data)
{
        TimeoutDialog *timeout = data;

        timeout->countdown--;

        if (timeout->countdown == 0) {
                timeout->response_id = CTK_RESPONSE_CANCEL;
                ctk_main_quit ();
        } else {
                print_countdown_text (timeout);
        }

        return TRUE;
}

static void
timeout_response_cb (CtkDialog *dialog, int response_id, gpointer data)
{
        TimeoutDialog *timeout = data;

        if (response_id == CTK_RESPONSE_DELETE_EVENT) {
                /* The user closed the dialog or pressed ESC, revert */
                timeout->response_id = CTK_RESPONSE_CANCEL;
        } else
                timeout->response_id = response_id;

        ctk_main_quit ();
}

static gboolean
user_says_things_are_ok (CsdXrandrManager *manager, CdkWindow *parent_window)
{
        TimeoutDialog timeout;
        guint timeout_id;

        timeout.manager = manager;

        timeout.dialog = ctk_message_dialog_new (NULL,
                                                 CTK_DIALOG_MODAL,
                                                 CTK_MESSAGE_QUESTION,
                                                 CTK_BUTTONS_NONE,
                                                 _("Does the display look OK?"));

        timeout.countdown = CONFIRMATION_DIALOG_SECONDS;

        print_countdown_text (&timeout);

        ctk_window_set_icon_name (CTK_WINDOW (timeout.dialog), "preferences-desktop-display");
        ctk_dialog_add_button (CTK_DIALOG (timeout.dialog), _("_Restore Previous Configuration"), CTK_RESPONSE_CANCEL);
        ctk_dialog_add_button (CTK_DIALOG (timeout.dialog), _("_Keep This Configuration"), CTK_RESPONSE_ACCEPT);
        ctk_dialog_set_default_response (CTK_DIALOG (timeout.dialog), CTK_RESPONSE_ACCEPT); /* ah, the optimism */

        g_signal_connect (timeout.dialog, "response",
                          G_CALLBACK (timeout_response_cb),
                          &timeout);

        ctk_widget_realize (timeout.dialog);

        if (parent_window)
                cdk_window_set_transient_for (ctk_widget_get_window (timeout.dialog), parent_window);

        ctk_widget_show_all (timeout.dialog);
        /* We don't use g_timeout_add_seconds() since we actually care that the user sees "real" second ticks in the dialog */
        timeout_id = g_timeout_add (1000,
                                    timeout_cb,
                                    &timeout);
        ctk_main ();

        ctk_widget_destroy (timeout.dialog);
        g_source_remove (timeout_id);

        if (timeout.response_id == CTK_RESPONSE_ACCEPT)
                return TRUE;
        else
                return FALSE;
}

struct confirmation {
        CsdXrandrManager *manager;
        CdkWindow *parent_window;
        guint32 timestamp;
};

static gboolean
confirm_with_user_idle_cb (gpointer data)
{
        struct confirmation *confirmation = data;
        char *backup_filename;
        char *intended_filename;

        backup_filename = cafe_rr_config_get_backup_filename ();
        intended_filename = cafe_rr_config_get_intended_filename ();

        if (user_says_things_are_ok (confirmation->manager, confirmation->parent_window))
                unlink (backup_filename);
        else
                restore_backup_configuration (confirmation->manager, backup_filename, intended_filename, confirmation->timestamp);

        g_free (confirmation);

        return FALSE;
}

static void
queue_confirmation_by_user (CsdXrandrManager *manager, CdkWindow *parent_window, guint32 timestamp)
{
        struct confirmation *confirmation;

        confirmation = g_new (struct confirmation, 1);
        confirmation->manager = manager;
        confirmation->parent_window = parent_window;
        confirmation->timestamp = timestamp;

        g_idle_add (confirm_with_user_idle_cb, confirmation);
}

static gboolean
try_to_apply_intended_configuration (CsdXrandrManager *manager, CdkWindow *parent_window, guint32 timestamp, GError **error)
{
        char *backup_filename;
        char *intended_filename;
        gboolean result;

        /* Try to apply the intended configuration */

        backup_filename = cafe_rr_config_get_backup_filename ();
        intended_filename = cafe_rr_config_get_intended_filename ();

        result = apply_configuration_from_filename (manager, intended_filename, FALSE, timestamp, error);
        if (!result) {
                error_message (manager, _("The selected configuration for displays could not be applied"), error ? *error : NULL, NULL);
                restore_backup_configuration_without_messages (backup_filename, intended_filename);
                goto out;
        } else {
                /* We need to return as quickly as possible, so instead of
                 * confirming with the user right here, we do it in an idle
                 * handler.  The caller only expects a status for "could you
                 * change the RANDR configuration?", not "is the user OK with it
                 * as well?".
                 */
                queue_confirmation_by_user (manager, parent_window, timestamp);
        }

out:
        g_free (backup_filename);
        g_free (intended_filename);

        return result;
}

/* DBus method for org.cafe.SettingsDaemon.XRANDR ApplyConfiguration; see csd_xrandr-manager.xml for the interface definition */
static gboolean
csd_xrandr_manager_apply_configuration (CsdXrandrManager *manager,
                                        GError          **error)
{
        return try_to_apply_intended_configuration (manager, NULL, CDK_CURRENT_TIME, error);
}

/* DBus method for org.cafe.SettingsDaemon.XRANDR_2 ApplyConfiguration; see csd_xrandr-manager.xml for the interface definition */
static gboolean
csd_xrandr_manager_2_apply_configuration (CsdXrandrManager *manager,
                                          gint64            parent_window_id,
                                          gint64            timestamp,
                                          GError          **error)
{
        CdkWindow *parent_window;
        gboolean result;

        if (parent_window_id != 0)
                parent_window = cdk_x11_window_foreign_new_for_display (cdk_display_get_default (), (Window) parent_window_id);
        else
                parent_window = NULL;

        result = try_to_apply_intended_configuration (manager, parent_window, (guint32) timestamp, error);

        if (parent_window)
                g_object_unref (parent_window);

        return result;
}

/* We include this after the definition of csd_xrandr_manager_apply_configuration() so the prototype will already exist */
#include "csd_xrandr-manager-glue.h"

static gboolean
is_laptop (CafeRRScreen *screen, CafeRROutputInfo *output)
{
        CafeRROutput *rr_output;

        rr_output = cafe_rr_screen_get_output_by_name (screen, cafe_rr_output_info_get_name (output));
        return cafe_rr_output_is_laptop (rr_output);
}

static gboolean
get_clone_size (CafeRRScreen *screen, int *width, int *height)
{
        CafeRRMode **modes = cafe_rr_screen_list_clone_modes (screen);
        int best_w, best_h;
        int i;

        best_w = 0;
        best_h = 0;

        for (i = 0; modes[i] != NULL; ++i) {
                CafeRRMode *mode = modes[i];
                int w, h;

                w = cafe_rr_mode_get_width (mode);
                h = cafe_rr_mode_get_height (mode);

                if (w * h > best_w * best_h) {
                        best_w = w;
                        best_h = h;
                }
        }

        if (best_w > 0 && best_h > 0) {
                if (width)
                        *width = best_w;
                if (height)
                        *height = best_h;

                return TRUE;
        }

        return FALSE;
}

static void
print_output (CafeRROutputInfo *info)
{
        int x, y, width, height;

        g_print ("  Output: %s attached to %s\n", cafe_rr_output_info_get_display_name (info), cafe_rr_output_info_get_name (info));
        g_print ("     status: %s\n", cafe_rr_output_info_is_active (info) ? "on" : "off");

        cafe_rr_output_info_get_geometry (info, &x, &y, &width, &height);
        g_print ("     width: %d\n", width);
        g_print ("     height: %d\n", height);
        g_print ("     rate: %d\n", cafe_rr_output_info_get_refresh_rate (info));
        g_print ("     position: %d %d\n", x, y);
}

static void
print_configuration (CafeRRConfig *config, const char *header)
{
        int i;
        CafeRROutputInfo **outputs;

        g_print ("=== %s Configuration ===\n", header);
        if (!config) {
                g_print ("  none\n");
                return;
        }

        outputs = cafe_rr_config_get_outputs (config);
        for (i = 0; outputs[i] != NULL; ++i)
                print_output (outputs[i]);
}

static gboolean
config_is_all_off (CafeRRConfig *config)
{
        int j;
        CafeRROutputInfo **outputs;

        outputs = cafe_rr_config_get_outputs (config);

        for (j = 0; outputs[j] != NULL; ++j) {
                if (cafe_rr_output_info_is_active (outputs[j])) {
                        return FALSE;
                }
        }

        return TRUE;
}

static CafeRRConfig *
make_clone_setup (CafeRRScreen *screen)
{
        CafeRRConfig *result;
        CafeRROutputInfo **outputs;
        int width, height;
        int i;

        if (!get_clone_size (screen, &width, &height))
                return NULL;

        result = cafe_rr_config_new_current (screen, NULL);
        outputs = cafe_rr_config_get_outputs (result);

        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];

                cafe_rr_output_info_set_active (info, FALSE);
                if (cafe_rr_output_info_is_connected (info)) {
                        CafeRROutput *output =
                                cafe_rr_screen_get_output_by_name (screen, cafe_rr_output_info_get_name (info));
                        CafeRRMode **modes = cafe_rr_output_list_modes (output);
                        int j;
                        int best_rate = 0;

                        for (j = 0; modes[j] != NULL; ++j) {
                                CafeRRMode *mode = modes[j];
                                int w, h;

                                w = cafe_rr_mode_get_width (mode);
                                h = cafe_rr_mode_get_height (mode);

                                if (w == width && h == height) {
                                        best_rate = cafe_rr_mode_get_freq (mode);
                                }
                        }

                        if (best_rate > 0) {
                                cafe_rr_output_info_set_active (info, TRUE);
                                cafe_rr_output_info_set_rotation (info, CAFE_RR_ROTATION_0);
                                cafe_rr_output_info_set_refresh_rate (info, best_rate);
                                cafe_rr_output_info_set_geometry (info, 0, 0, width, height);
                        }
                }
        }

        if (config_is_all_off (result)) {
                g_object_unref (result);
                result = NULL;
        }

        print_configuration (result, "clone setup");

        return result;
}

static CafeRRMode *
find_best_mode (CafeRROutput *output)
{
        CafeRRMode *preferred;
        CafeRRMode **modes;
        int best_size;
        int best_width, best_height, best_rate;
        int i;
        CafeRRMode *best_mode;

        preferred = cafe_rr_output_get_preferred_mode (output);
        if (preferred)
                return preferred;

        modes = cafe_rr_output_list_modes (output);
        if (!modes)
                return NULL;

        best_size = best_width = best_height = best_rate = 0;
        best_mode = NULL;

        for (i = 0; modes[i] != NULL; i++) {
                int w, h, r;
                int size;

                w = cafe_rr_mode_get_width (modes[i]);
                h = cafe_rr_mode_get_height (modes[i]);
                r = cafe_rr_mode_get_freq  (modes[i]);

                size = w * h;

                if (size > best_size) {
                        best_size   = size;
                        best_width  = w;
                        best_height = h;
                        best_rate   = r;
                        best_mode   = modes[i];
                } else if (size == best_size) {
                        if (r > best_rate) {
                                best_rate = r;
                                best_mode = modes[i];
                        }
                }
        }

        return best_mode;
}

static gboolean
turn_on (CafeRRScreen *screen,
         CafeRROutputInfo *info,
         int x, int y)
{
        CafeRROutput *output = cafe_rr_screen_get_output_by_name (screen, cafe_rr_output_info_get_name (info));
        CafeRRMode *mode = find_best_mode (output);

        if (mode) {
                cafe_rr_output_info_set_active (info, TRUE);
                cafe_rr_output_info_set_geometry (info, x, y, cafe_rr_mode_get_width (mode), cafe_rr_mode_get_height (mode));
                cafe_rr_output_info_set_rotation (info, CAFE_RR_ROTATION_0);
                cafe_rr_output_info_set_refresh_rate (info, cafe_rr_mode_get_freq (mode));

                return TRUE;
        }

        return FALSE;
}

static CafeRRConfig *
make_primary_only_setup (CafeRRScreen *screen)
{
        /*Leave all of the monitors turned on, just change from mirror to xinerama layout*/
        CafeRRConfig *result = cafe_rr_config_new_current (screen, NULL);
        CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (result);
        int i, x, width,height;
        x = 0;

        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];
                width = cafe_rr_output_info_get_preferred_width (info);
                height = cafe_rr_output_info_get_preferred_height (info);
                cafe_rr_output_info_set_geometry (info, x, 0, width, height);
                cafe_rr_output_info_set_active (info, TRUE);
                x = x + width;
        }

        if (result && config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        cafe_rr_config_set_clone (result, FALSE);
        print_configuration (result, "Primary only setup");

        return result;
}

static CafeRRConfig *
make_laptop_setup (CafeRRScreen *screen)
{
        /* Turn on the laptop, disable everything else */
        CafeRRConfig *result = cafe_rr_config_new_current (screen, NULL);
        CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (result);
        int i;

        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info)) {
                        if (!turn_on (screen, info, 0, 0)) {
                                g_object_unref (G_OBJECT (result));
                                result = NULL;
                                break;
                        }
                }
                else {
                        cafe_rr_output_info_set_active (info, FALSE);
                }
        }

        if (result && config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "Laptop setup");

        /* FIXME - Maybe we should return NULL if there is more than
         * one connected "laptop" screen?
         */
        return result;
}

static int
turn_on_and_get_rightmost_offset (CafeRRScreen *screen, CafeRROutputInfo *info, int x)
{
        if (turn_on (screen, info, x, 0)) {
                int width;
                cafe_rr_output_info_get_geometry (info, NULL, NULL, &width, NULL);
                x += width;
        }

        return x;
}

static CafeRRConfig *
make_xinerama_setup (CafeRRScreen *screen)
{
        /* Turn on everything that has a preferred mode, and
         * position it from left to right
         */
        CafeRRConfig *result = cafe_rr_config_new_current (screen, NULL);
	CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (result);
        int i;
        int x;

        x = 0;
        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info))
                        x = turn_on_and_get_rightmost_offset (screen, info, x);
        }

        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];

                if (cafe_rr_output_info_is_connected (info) && !is_laptop (screen, info))
                        x = turn_on_and_get_rightmost_offset (screen, info, x);
        }

        if (config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "xinerama setup");

        return result;
}

static CafeRRConfig *
make_other_setup (CafeRRScreen *screen)
{
        /* Turn off all laptops, and make all external monitors clone
         * from (0, 0)
         */

        CafeRRConfig *result = cafe_rr_config_new_current (screen, NULL);
        CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (result);
        int i;

        for (i = 0; outputs[i] != NULL; ++i) {
                CafeRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info)) {
                        cafe_rr_output_info_set_active (info, FALSE);
                }
                else {
                        if (cafe_rr_output_info_is_connected (info))
                                turn_on (screen, info, 0, 0);
               }
        }

        if (config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "other setup");

        return result;
}

static GPtrArray *
sanitize (CsdXrandrManager *manager, GPtrArray *array)
{
        int i;
        GPtrArray *new;

        g_debug ("before sanitizing");

        for (i = 0; i < array->len; ++i) {
                if (array->pdata[i]) {
                        print_configuration (array->pdata[i], "before");
                }
        }


        /* Remove configurations that are duplicates of
         * configurations earlier in the cycle
         */
        for (i = 0; i < array->len; i++) {
                int j;

                for (j = i + 1; j < array->len; j++) {
                        CafeRRConfig *this = array->pdata[j];
                        CafeRRConfig *other = array->pdata[i];

                        if (this && other && cafe_rr_config_equal (this, other)) {
                                g_debug ("removing duplicate configuration");
                                g_object_unref (this);
                                array->pdata[j] = NULL;
                                break;
                        }
                }
        }

        for (i = 0; i < array->len; ++i) {
                CafeRRConfig *config = array->pdata[i];

                if (config && config_is_all_off (config)) {
                        g_debug ("removing configuration as all outputs are off");
                        g_object_unref (array->pdata[i]);
                        array->pdata[i] = NULL;
                }
        }

        /* Do a final sanitization pass.  This will remove configurations that
         * don't fit in the framebuffer's Virtual size.
         */

        for (i = 0; i < array->len; i++) {
                CafeRRConfig *config = array->pdata[i];

                if (config) {
                        GError *error;

                        error = NULL;
                        if (!cafe_rr_config_applicable (config, manager->priv->rw_screen, &error)) { /* NULL-GError */
                                g_debug ("removing configuration which is not applicable because %s", error->message);
                                g_error_free (error);

                                g_object_unref (config);
                                array->pdata[i] = NULL;
                        }
                }
        }

        /* Remove NULL configurations */
        new = g_ptr_array_new ();

        for (i = 0; i < array->len; ++i) {
                if (array->pdata[i]) {
                        g_ptr_array_add (new, array->pdata[i]);
                        print_configuration (array->pdata[i], "Final");
                }
        }

        if (new->len > 0) {
                g_ptr_array_add (new, NULL);
        } else {
                g_ptr_array_free (new, TRUE);
                new = NULL;
        }

        g_ptr_array_free (array, TRUE);

        return new;
}

static void
generate_fn_f7_configs (CsdXrandrManager *mgr)
{
        GPtrArray *array = g_ptr_array_new ();
        CafeRRScreen *screen = mgr->priv->rw_screen;

        g_debug ("Generating configurations");

        /* Free any existing list of configurations */
        if (mgr->priv->fn_f7_configs) {
                int i;

                for (i = 0; mgr->priv->fn_f7_configs[i] != NULL; ++i)
                        g_object_unref (mgr->priv->fn_f7_configs[i]);
                g_free (mgr->priv->fn_f7_configs);

                mgr->priv->fn_f7_configs = NULL;
                mgr->priv->current_fn_f7_config = -1;
        }

        g_ptr_array_add (array, cafe_rr_config_new_current (screen, NULL));
        g_ptr_array_add (array, make_clone_setup (screen));
        g_ptr_array_add (array, make_xinerama_setup (screen));
        g_ptr_array_add (array, make_laptop_setup (screen));
        g_ptr_array_add (array, make_other_setup (screen));

        array = sanitize (mgr, array);

        if (array) {
                mgr->priv->fn_f7_configs = (CafeRRConfig **)g_ptr_array_free (array, FALSE);
                mgr->priv->current_fn_f7_config = 0;
        }
}

static void
error_message (CsdXrandrManager *mgr, const char *primary_text, GError *error_to_display, const char *secondary_text)
{
#ifdef HAVE_LIBNOTIFY
        CsdXrandrManagerPrivate *priv = mgr->priv;
        NotifyNotification *notification;

        g_assert (error_to_display == NULL || secondary_text == NULL);

        if (priv->status_icon)
                notification = notify_notification_new (primary_text,
                                                        error_to_display ? error_to_display->message : secondary_text,
                                                        ctk_status_icon_get_icon_name(priv->status_icon));
        else
                notification = notify_notification_new (primary_text,
                                                        error_to_display ? error_to_display->message : secondary_text,
                                                        CSD_XRANDR_ICON_NAME);

        notify_notification_show (notification, NULL); /* NULL-GError */
#else
        CtkWidget *dialog;

        dialog = ctk_message_dialog_new (NULL, CTK_DIALOG_MODAL, CTK_MESSAGE_ERROR, CTK_BUTTONS_CLOSE,
                                         "%s", primary_text);
        ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), "%s",
                                                  error_to_display ? error_to_display->message : secondary_text);

        ctk_dialog_run (CTK_DIALOG (dialog));
        ctk_widget_destroy (dialog);
#endif /* HAVE_LIBNOTIFY */
}

static void
handle_fn_f7 (CsdXrandrManager *mgr, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = mgr->priv;
        CafeRRScreen *screen = priv->rw_screen;
        CafeRRConfig *current;
        GError *error;

        /* Theory of fn-F7 operation
         *
         * We maintain a datastructure "fn_f7_status", that contains
         * a list of CafeRRConfig's. Each of the CafeRRConfigs has a
         * mode (or "off") for each connected output.
         *
         * When the user hits fn-F7, we cycle to the next CafeRRConfig
         * in the data structure. If the data structure does not exist, it
         * is generated. If the configs in the data structure do not match
         * the current hardware reality, it is regenerated.
         *
         */
        g_debug ("Handling fn-f7");

        log_open ();
        log_msg ("Handling XF86Display hotkey - timestamp %u\n", timestamp);

        error = NULL;
        if (!cafe_rr_screen_refresh (screen, &error) && error) {
                char *str;

                str = g_strdup_printf (_("Could not refresh the screen information: %s"), error->message);
                g_error_free (error);

                log_msg ("%s\n", str);
                error_message (mgr, str, NULL, _("Trying to switch the monitor configuration anyway."));
                g_free (str);
        }

        if (!priv->fn_f7_configs) {
                log_msg ("Generating stock configurations:\n");
                generate_fn_f7_configs (mgr);
                log_configurations (priv->fn_f7_configs);
        }

        current = cafe_rr_config_new_current (screen, NULL);

        if (priv->fn_f7_configs &&
            (!cafe_rr_config_match (current, priv->fn_f7_configs[0]) ||
             !cafe_rr_config_equal (current, priv->fn_f7_configs[mgr->priv->current_fn_f7_config]))) {
                    /* Our view of the world is incorrect, so regenerate the
                     * configurations
                     */
                    generate_fn_f7_configs (mgr);
                    log_msg ("Regenerated stock configurations:\n");
                    log_configurations (priv->fn_f7_configs);
            }

        g_object_unref (current);

        if (priv->fn_f7_configs) {
                guint32 server_timestamp;
                gboolean success;

                mgr->priv->current_fn_f7_config++;

                if (priv->fn_f7_configs[mgr->priv->current_fn_f7_config] == NULL)
                        mgr->priv->current_fn_f7_config = 0;

                g_debug ("cycling to next configuration (%d)", mgr->priv->current_fn_f7_config);

                print_configuration (priv->fn_f7_configs[mgr->priv->current_fn_f7_config], "new config");

                g_debug ("applying");

                /* See https://bugzilla.gnome.org/show_bug.cgi?id=610482
                 *
                 * Sometimes we'll get two rapid XF86Display keypress events,
                 * but their timestamps will be out of order with respect to the
                 * RANDR timestamps.  This *may* be due to stupid BIOSes sending
                 * out display-switch keystrokes "to make Windows work".
                 *
                 * The X server will error out if the timestamp provided is
                 * older than a previous change configuration timestamp. We
                 * assume here that we do want this event to go through still,
                 * since kernel timestamps may be skewed wrt the X server.
                 */
                cafe_rr_screen_get_timestamps (screen, NULL, &server_timestamp);
                if (timestamp < server_timestamp)
                        timestamp = server_timestamp;

                success = apply_configuration_and_display_error (mgr, priv->fn_f7_configs[mgr->priv->current_fn_f7_config], timestamp);

                if (success) {
                        log_msg ("Successfully switched to configuration (timestamp %u):\n", timestamp);
                        log_configuration (priv->fn_f7_configs[mgr->priv->current_fn_f7_config]);
                }
        }
        else {
                g_debug ("no configurations generated");
        }

        log_close ();

        g_debug ("done handling fn-f7");
}

static CafeRROutputInfo *
get_laptop_output_info (CafeRRScreen *screen, CafeRRConfig *config)
{
        int i;
        CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (config);

        for (i = 0; outputs[i] != NULL; i++) {
                if (is_laptop (screen, outputs[i]))
                        return outputs[i];
        }

        return NULL;

}

static CafeRRRotation
get_next_rotation (CafeRRRotation allowed_rotations, CafeRRRotation current_rotation)
{
        int i;
        int current_index;

        /* First, find the index of the current rotation */

        current_index = -1;

        for (i = 0; i < G_N_ELEMENTS (possible_rotations); i++) {
                CafeRRRotation r;

                r = possible_rotations[i];
                if (r == current_rotation) {
                        current_index = i;
                        break;
                }
        }

        if (current_index == -1) {
                /* Huh, the current_rotation was not one of the supported rotations.  Bail out. */
                return current_rotation;
        }

        /* Then, find the next rotation that is allowed */

        i = (current_index + 1) % G_N_ELEMENTS (possible_rotations);

        while (1) {
                CafeRRRotation r;

                r = possible_rotations[i];
                if (r == current_rotation) {
                        /* We wrapped around and no other rotation is suported.  Bummer. */
                        return current_rotation;
                } else if (r & allowed_rotations)
                        return r;

                i = (i + 1) % G_N_ELEMENTS (possible_rotations);
        }
}

/* We use this when the XF86RotateWindows key is pressed.  That key is present
 * on some tablet PCs; they use it so that the user can rotate the tablet
 * easily.
 */
static void
handle_rotate_windows (CsdXrandrManager *mgr, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = mgr->priv;
        CafeRRScreen *screen = priv->rw_screen;
        CafeRRConfig *current;
        CafeRROutputInfo *rotatable_output_info;
        int num_allowed_rotations;
        CafeRRRotation allowed_rotations;
        CafeRRRotation next_rotation;

        g_debug ("Handling XF86RotateWindows");

        /* Which output? */

        current = cafe_rr_config_new_current (screen, NULL);

        rotatable_output_info = get_laptop_output_info (screen, current);
        if (rotatable_output_info == NULL) {
                g_debug ("No laptop outputs found to rotate; XF86RotateWindows key will do nothing");
                goto out;
        }

        /* Which rotation? */

        get_allowed_rotations_for_output (current, priv->rw_screen, rotatable_output_info, &num_allowed_rotations, &allowed_rotations);
        next_rotation = get_next_rotation (allowed_rotations, cafe_rr_output_info_get_rotation (rotatable_output_info));

        if (next_rotation == cafe_rr_output_info_get_rotation (rotatable_output_info)) {
                g_debug ("No rotations are supported other than the current one; XF86RotateWindows key will do nothing");
                goto out;
        }

        /* Rotate */

        cafe_rr_output_info_set_rotation (rotatable_output_info, next_rotation);

        apply_configuration_and_display_error (mgr, current, timestamp);

out:
        g_object_unref (current);
}

static CdkFilterReturn
event_filter (CdkXEvent           *xevent,
              CdkEvent            *event,
              gpointer             data)
{
        CsdXrandrManager *manager = data;
        XEvent *xev = (XEvent *) xevent;

        if (!manager->priv->running)
                return CDK_FILTER_CONTINUE;

        /* verify we have a key event */
        if (xev->xany.type != KeyPress && xev->xany.type != KeyRelease)
                return CDK_FILTER_CONTINUE;

        if (xev->xany.type == KeyPress) {
                if (xev->xkey.keycode == manager->priv->switch_video_mode_keycode)
                        handle_fn_f7 (manager, xev->xkey.time);
                else if (xev->xkey.keycode == manager->priv->rotate_windows_keycode)
                        handle_rotate_windows (manager, xev->xkey.time);

                return CDK_FILTER_CONTINUE;
        }

        return CDK_FILTER_CONTINUE;
}

static void
refresh_tray_icon_menu_if_active (CsdXrandrManager *manager, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = manager->priv;

        if (priv->popup_menu) {
                ctk_menu_shell_cancel (CTK_MENU_SHELL (priv->popup_menu)); /* status_icon_popup_menu_selection_done_cb() will free everything */
                status_icon_popup_menu (manager, 0, timestamp);
        }
}

static void
auto_configure_outputs (CsdXrandrManager *manager, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = manager->priv;
        CafeRRConfig *config;
        CafeRROutputInfo **outputs;
        int i;
        GList *just_turned_on;
        GList *l;
        int x;
        GError *error;
        gboolean applicable;

        config = cafe_rr_config_new_current (priv->rw_screen, NULL);

        /* For outputs that are connected and on (i.e. they have a CRTC assigned
         * to them, so they are getting a signal), we leave them as they are
         * with their current modes.
         *
         * For other outputs, we will turn on connected-but-off outputs and turn
         * off disconnected-but-on outputs.
         *
         * FIXME: If an output remained connected+on, it would be nice to ensure
         * that the output's CRTCs still has a reasonable mode (think of
         * changing one monitor for another with different capabilities).
         */

        just_turned_on = NULL;
        outputs = cafe_rr_config_get_outputs (config);

        for (i = 0; outputs[i] != NULL; i++) {
                CafeRROutputInfo *output = outputs[i];

                if (cafe_rr_output_info_is_connected (output) && !cafe_rr_output_info_is_active (output)) {
                        cafe_rr_output_info_set_active (output, TRUE);
                        cafe_rr_output_info_set_rotation (output, CAFE_RR_ROTATION_0);
                        just_turned_on = g_list_prepend (just_turned_on, GINT_TO_POINTER (i));
                } else if (!cafe_rr_output_info_is_connected (output) && cafe_rr_output_info_is_active (output))
                        cafe_rr_output_info_set_active (output, FALSE);
        }

        /* Now, lay out the outputs from left to right.  Put first the outputs
         * which remained on; put last the outputs that were newly turned on.
         */

        x = 0;

        /* First, outputs that remained on */

        for (i = 0; outputs[i] != NULL; i++) {
                CafeRROutputInfo *output = outputs[i];

                if (g_list_find (just_turned_on, GINT_TO_POINTER (i)))
                        continue;

                if (cafe_rr_output_info_is_active (output)) {
                        int width, height;
                        g_assert (cafe_rr_output_info_is_connected (output));

                        cafe_rr_output_info_get_geometry (output, NULL, NULL, &width, &height);
			cafe_rr_output_info_set_geometry (output, x, 0, width, height);

                        x += width;
                }
        }

        /* Second, outputs that were newly-turned on */

        for (l = just_turned_on; l; l = l->next) {
                CafeRROutputInfo *output;
		int width;

                i = GPOINTER_TO_INT (l->data);
                output = outputs[i];

                g_assert (cafe_rr_output_info_is_active (output) && cafe_rr_output_info_is_connected (output));

                /* since the output was off, use its preferred width/height (it doesn't have a real width/height yet) */
                width = cafe_rr_output_info_get_preferred_width (output);
                cafe_rr_output_info_set_geometry (output, x, 0, width, cafe_rr_output_info_get_preferred_height (output));

                x += width;
        }

        /* Check if we have a large enough framebuffer size.  If not, turn off
         * outputs from right to left until we reach a usable size.
         */

        just_turned_on = g_list_reverse (just_turned_on); /* now the outputs here are from right to left */

        l = just_turned_on;
        while (1) {
                CafeRROutputInfo *output;
                gboolean is_bounds_error;

                error = NULL;
                applicable = cafe_rr_config_applicable (config, priv->rw_screen, &error);

                if (applicable)
                        break;

                is_bounds_error = g_error_matches (error, CAFE_RR_ERROR, CAFE_RR_ERROR_BOUNDS_ERROR);
                g_error_free (error);

                if (!is_bounds_error)
                        break;

                if (l) {
                        i = GPOINTER_TO_INT (l->data);
                        l = l->next;

                        output = outputs[i];
                        cafe_rr_output_info_set_active (output, FALSE);
                } else
                        break;
        }

        /* Apply the configuration! */

        if (applicable)
                apply_configuration_and_display_error (manager, config, timestamp);

        g_list_free (just_turned_on);
        g_object_unref (config);

        /* Finally, even though we did a best-effort job in sanitizing the
         * outputs, we don't know the physical layout of the monitors.  We'll
         * start the display capplet so that the user can tweak things to his
         * liking.
         */

#if 0
        /* FIXME: This is disabled for now.  The capplet is not a single-instance application.
         * If you do this:
         *
         *   1. Start the display capplet
         *
         *   2. Plug an extra monitor
         *
         *   3. Hit the "Detect displays" button
         *
         * Then we will get a RANDR event because X re-probes the outputs.  We don't want to
         * start up a second display capplet right there!
         */

        run_display_capplet (NULL);
#endif
}

static void
apply_color_profiles (void)
{
        gboolean ret;
        GError *error = NULL;

        /* run the cafe-color-manager apply program */
        ret = g_spawn_command_line_async (BINDIR "/gcm-apply", &error);
        if (!ret) {
                /* only print the warning if the binary is installed */
                if (error->code != G_SPAWN_ERROR_NOENT) {
                        g_warning ("failed to apply color profiles: %s", error->message);
                }
                g_error_free (error);
        }
}

static void
on_randr_event (CafeRRScreen *screen, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);
        CsdXrandrManagerPrivate *priv = manager->priv;
        guint32 change_timestamp, config_timestamp;

        if (!priv->running)
                return;

        cafe_rr_screen_get_timestamps (screen, &change_timestamp, &config_timestamp);

        log_open ();
        log_msg ("Got RANDR event with timestamps change=%u %c config=%u\n",
                 change_timestamp,
                 timestamp_relationship (change_timestamp, config_timestamp),
                 config_timestamp);

        if (change_timestamp >= config_timestamp) {
                /* The event is due to an explicit configuration change.
                 *
                 * If the change was performed by us, then we need to do nothing.
                 *
                 * If the change was done by some other X client, we don't need
                 * to do anything, either; the screen is already configured.
                 */
                show_timestamps_dialog (manager, "ignoring since change > config");
                log_msg ("  Ignoring event since change >= config\n");
        } else {
                /* Here, config_timestamp > change_timestamp.  This means that
                 * the screen got reconfigured because of hotplug/unplug; the X
                 * server is just notifying us, and we need to configure the
                 * outputs in a sane way.
                 */

                char *intended_filename;
                GError *error;
                gboolean success;

                show_timestamps_dialog (manager, "need to deal with reconfiguration, as config > change");

                intended_filename = cafe_rr_config_get_intended_filename ();

                error = NULL;
                success = apply_configuration_from_filename (manager, intended_filename, TRUE, config_timestamp, &error);
                g_free (intended_filename);

                if (!success) {
                        /* We don't bother checking the error type.
                         *
                         * Both G_FILE_ERROR_NOENT and
                         * CAFE_RR_ERROR_NO_MATCHING_CONFIG would mean, "there
                         * was no configuration to apply, or none that matched
                         * the current outputs", and in that case we need to run
                         * our fallback.
                         *
                         * Any other error means "we couldn't do the smart thing
                         * of using a previously- saved configuration, anyway,
                         * for some other reason.  In that case, we also need to
                         * run our fallback to avoid leaving the user with a
                         * bogus configuration.
                         */

                        if (error)
                                g_error_free (error);

                        if (config_timestamp != priv->last_config_timestamp) {
                                priv->last_config_timestamp = config_timestamp;
                                auto_configure_outputs (manager, config_timestamp);
                                log_msg ("  Automatically configured outputs to deal with event\n");
                        } else
                                log_msg ("  Ignored event as old and new config timestamps are the same\n");
                } else
                        log_msg ("Applied stored configuration to deal with event\n");
        }

        /* poke cafe-color-manager */
        apply_color_profiles ();

        refresh_tray_icon_menu_if_active (manager, MAX (change_timestamp, config_timestamp));

        log_close ();
}

static void
run_display_capplet (CtkWidget *widget)
{
        CdkScreen *screen;
        GError *error;

        if (widget)
                screen = ctk_widget_get_screen (widget);
        else
                screen = cdk_screen_get_default ();

        error = NULL;
        if (!cafe_cdk_spawn_command_line_on_screen (screen, CSD_XRANDR_DISPLAY_CAPPLET, &error)) {
                CtkWidget *dialog;

                dialog = ctk_message_dialog_new_with_markup (NULL, 0, CTK_MESSAGE_ERROR, CTK_BUTTONS_OK,
                                                             "<span weight=\"bold\" size=\"larger\">"
                                                             "Display configuration could not be run"
                                                             "</span>\n\n"
                                                             "%s", error->message);
                ctk_dialog_run (CTK_DIALOG (dialog));
                ctk_widget_destroy (dialog);

                g_error_free (error);
        }
}

static void
popup_menu_configure_display_cb (CtkMenuItem *item, gpointer data)
{
        run_display_capplet (CTK_WIDGET (item));
}

static void
status_icon_popup_menu_selection_done_cb (CtkMenuShell *menu_shell, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);
        struct CsdXrandrManagerPrivate *priv = manager->priv;

        ctk_widget_destroy (priv->popup_menu);
        priv->popup_menu = NULL;

        cafe_rr_labeler_hide (priv->labeler);
        g_object_unref (priv->labeler);
        priv->labeler = NULL;

        g_object_unref (priv->configuration);
        priv->configuration = NULL;
}

#define OUTPUT_TITLE_ITEM_BORDER 2
#define OUTPUT_TITLE_ITEM_PADDING 4

static void
title_item_size_allocate_cb (CtkWidget *widget, CtkAllocation *allocation, gpointer data)
{
        /* When CtkMenu does size_request on its items, it asks them for their "toggle size",
         * which will be non-zero when there are check/radio items.  CtkMenu remembers
         * the largest of those sizes.  During the size_allocate pass, CtkMenu calls
         * ctk_menu_item_toggle_size_allocate() with that value, to tell the menu item
         * that it should later paint its child a bit to the right of its edge.
         *
         * However, we want the "title" menu items for each RANDR output to span the *whole*
         * allocation of the menu item, not just the "allocation minus toggle" area.
         *
         * So, we let the menu item size_allocate itself as usual, but this
         * callback gets run afterward.  Here we hack a toggle size of 0 into
         * the menu item, and size_allocate it by hand *again*.  We also need to
         * avoid recursing into this function.
         */

        g_assert (CTK_IS_MENU_ITEM (widget));

        ctk_menu_item_toggle_size_allocate (CTK_MENU_ITEM (widget), 0);

        g_signal_handlers_block_by_func (widget, title_item_size_allocate_cb, NULL);

        /* Sigh. There is no way to turn on CTK_ALLOC_NEEDED outside of CTK+
         * itself; also, since calling size_allocate on a widget with the same
         * allcation is a no-op, we need to allocate with a "different" size
         * first.
         */

        allocation->width++;
        ctk_widget_size_allocate (widget, allocation);

        allocation->width--;
        ctk_widget_size_allocate (widget, allocation);

        g_signal_handlers_unblock_by_func (widget, title_item_size_allocate_cb, NULL);
}

static CtkWidget *
make_menu_item_for_output_title (CsdXrandrManager *manager, CafeRROutputInfo *output)
{
        CtkWidget       *item;
        CtkStyleContext *context;
        CtkCssProvider  *provider, *provider2;
        CtkWidget       *label;
        CtkWidget       *image;
        CtkWidget *box;
        char *str;
        GString *string;
        CdkRGBA color;
        gchar *css, *color_string, *theme_name;
        CtkSettings *settings;
        GSettings *icon_settings;

        struct CsdXrandrManagerPrivate *priv = manager->priv;

        item = ctk_menu_item_new ();
        box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
        image = ctk_image_new_from_icon_name ("computer", CTK_ICON_SIZE_MENU);
        context = ctk_widget_get_style_context (item);
        ctk_style_context_add_class (context, "xrandr-applet");

        g_signal_connect (item, "size-allocate",
                          G_CALLBACK (title_item_size_allocate_cb), NULL);

        str = g_markup_printf_escaped ("<b>%s</b>", cafe_rr_output_info_get_display_name (output));
        label = ctk_label_new (NULL);
        ctk_label_set_markup (CTK_LABEL (label), str);
        g_free (str);

        /* Add padding around the label to fit the box that we'll draw for color-coding */
        ctk_label_set_xalign (CTK_LABEL (label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (label), 0.5);
        ctk_widget_set_margin_start (label, OUTPUT_TITLE_ITEM_BORDER + OUTPUT_TITLE_ITEM_PADDING);
        ctk_widget_set_margin_end (label, OUTPUT_TITLE_ITEM_BORDER + OUTPUT_TITLE_ITEM_PADDING);
        ctk_widget_set_margin_top (label, OUTPUT_TITLE_ITEM_BORDER + OUTPUT_TITLE_ITEM_PADDING);
        ctk_widget_set_margin_bottom (label, OUTPUT_TITLE_ITEM_BORDER + OUTPUT_TITLE_ITEM_PADDING);

        /*Load the icon unless the user has icons in menus turned off*/
        icon_settings = g_settings_new ("org.cafe.interface");
        if (g_settings_get_boolean (icon_settings, "menus-have-icons")){
            ctk_container_add (CTK_CONTAINER (box), image);
            }
        ctk_container_add (CTK_CONTAINER (box), label);
        ctk_container_add (CTK_CONTAINER (item), box);

        cafe_rr_labeler_get_rgba_for_output (priv->labeler, output, &color);

        color_string = cdk_rgba_to_string (&color);

        /*This can be overriden by themes, check all label:insensitive entries if it does not show up*/
        string = g_string_new(NULL);
        g_string_append (string, ".cafe-panel-menu-bar menuitem.xrandr-applet:disabled>box>label{\n");
        /*g_string_append (string, "color: black;"); Does not work-overridden in all themes*/
        g_string_append (string, "padding-left: 4px; padding-right: 4px;");
        g_string_append (string, "border-color: gray;");
        g_string_append (string, "border-width: 1px;");
        g_string_append (string, "border-style: inset;");
        g_string_append (string, "background-image: none;");
        /*Bright color for active monitor, dimmed for inactive monitor*/
        if (cafe_rr_output_info_is_active (output)){
                g_string_append (string, "background-color:");
                g_string_append (string, color_string);
                g_string_append (string, ";");
                g_string_append (string," }");
        }
        else{
                g_string_append (string, "background-color: alpha(");
                g_string_append (string, color_string);
                g_string_append (string, ", 0.4);");
                g_string_append (string," }");
                ctk_style_context_add_class (context, "monitor-off");
        }

        css = g_string_free (string, FALSE);

        context = ctk_widget_get_style_context (label);
        provider = ctk_css_provider_new ();
        ctk_css_provider_load_from_data (provider,css, -1, NULL);

        ctk_style_context_add_provider (context,
					CTK_STYLE_PROVIDER (provider),
					CTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

        g_object_unref (provider);
        g_free (color_string);
        g_free (css);

        /*This is NOT overrridden by themes as FALLBACK won't work here
         *Disable dim/opacity effects applied to icons in an insensitive menu item
         */

        context = ctk_widget_get_style_context (image);
        provider = ctk_css_provider_new ();

        ctk_css_provider_load_from_data (provider,
            ".cafe-panel-menu-bar menuitem.xrandr-applet:disabled>box>image{\n"
             "opacity: 1.0; \n"
             "-ctk-icon-style:regular; \n" /* symbolic icons would get the disabled color*/
             "-ctk-icon-effect: none; \n"
             "}",
             -1, NULL);
        ctk_style_context_add_provider (context,
					CTK_STYLE_PROVIDER (provider),
					CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        /*Deal with the GNOME and *bird themes, match display capplet theme */
        provider2 = ctk_css_provider_new ();
        settings = ctk_settings_get_default();
        context = ctk_widget_get_style_context (label);
        g_object_get (settings, "ctk-theme-name", &theme_name, NULL);
        if (g_strcmp0 (theme_name, "Adwaita") == 0 ||
                      g_strcmp0 (theme_name, "Adwaita-dark") == 0 ||
                      g_strcmp0 (theme_name, "Raleigh") == 0 ||
                      g_strcmp0 (theme_name, "win32") == 0 ||
                      g_strcmp0 (theme_name, "HighContrast") == 0 ||
                      g_strcmp0 (theme_name, "HighContrastInverse") == 0 ||
                      g_strcmp0 (theme_name, "Blackbird") == 0 ||
                      g_strcmp0 (theme_name, "Bluebird") == 0 ||
                      g_strcmp0 (theme_name, "Greybird") == 0){
            ctk_css_provider_load_from_data (provider2,
                    ".cafe-panel-menu-bar menuitem.xrandr-applet:disabled>box>label{\n"
                    "color: black;\n"
                    "}"
                    ".cafe-panel-menu-bar menuitem.xrandr-applet.monitor-off:disabled>box>label{\n"
                     "color: alpha (black, 0.6);\n"
                    "}",
                    -1, NULL);
            ctk_style_context_add_provider(context,
					    CTK_STYLE_PROVIDER (provider2),
					    CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
        /*Keep or take this off all other themes as soon as the theme changes*/
        else{
            ctk_style_context_remove_provider(context, CTK_STYLE_PROVIDER (provider2));
        }

        g_object_unref (provider);
        g_object_unref (provider2);

        ctk_widget_set_sensitive (item, FALSE); /* the title is not selectable */

        ctk_widget_show_all (item);

        return item;
}

static void
get_allowed_rotations_for_output (CafeRRConfig *config,
                                  CafeRRScreen *rr_screen,
                                  CafeRROutputInfo *output,
                                  int *out_num_rotations,
                                  CafeRRRotation *out_rotations)
{
        CafeRRRotation current_rotation;
        int i;

        *out_num_rotations = 0;
        *out_rotations = 0;

        current_rotation = cafe_rr_output_info_get_rotation (output);

        /* Yay for brute force */

        for (i = 0; i < G_N_ELEMENTS (possible_rotations); i++) {
                CafeRRRotation rotation_to_test;

                rotation_to_test = possible_rotations[i];

                cafe_rr_output_info_set_rotation (output, rotation_to_test);

                if (cafe_rr_config_applicable (config, rr_screen, NULL)) { /* NULL-GError */
                        (*out_num_rotations)++;
                        (*out_rotations) |= rotation_to_test;
                }
        }

        cafe_rr_output_info_set_rotation (output, current_rotation);

        if (*out_num_rotations == 0 || *out_rotations == 0) {
                g_warning ("Huh, output %p says it doesn't support any rotations, and yet it has a current rotation?", output);
                *out_num_rotations = 1;
                *out_rotations = cafe_rr_output_info_get_rotation (output);
        }
}

static void
add_unsupported_rotation_item (CsdXrandrManager *manager)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *item;
        CtkWidget *label;
        gchar *markup;

        item = ctk_menu_item_new ();

        label = ctk_label_new (NULL);
        markup = g_strdup_printf ("<i>%s</i>", _("Rotation not supported"));
        ctk_label_set_markup (CTK_LABEL (label), markup);
        g_free (markup);
        ctk_container_add (CTK_CONTAINER (item), label);

        ctk_widget_show_all (item);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);
}

static void
ensure_current_configuration_is_saved (void)
{
        CafeRRScreen *rr_screen;
        CafeRRConfig *rr_config;

        /* Normally, cafe_rr_config_save() creates a backup file based on the
         * old monitors.xml.  However, if *that* file didn't exist, there is
         * nothing from which to create a backup.  So, here we'll save the
         * current/unchanged configuration and then let our caller call
         * cafe_rr_config_save() again with the new/changed configuration, so
         * that there *will* be a backup file in the end.
         */

        rr_screen = cafe_rr_screen_new (cdk_screen_get_default (), NULL); /* NULL-GError */
        if (!rr_screen)
                return;

        rr_config = cafe_rr_config_new_current (rr_screen, NULL);
        cafe_rr_config_save (rr_config, NULL); /* NULL-GError */

        g_object_unref (rr_config);
        g_object_unref (rr_screen);
}

static void
monitor_activate_cb (CtkCheckMenuItem *item, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CafeRROutputInfo *output;
        GError *error;

        ensure_current_configuration_is_saved ();

        output = g_object_get_data (G_OBJECT (item), "output");

        /*This is borrowed from the capplet in cafe-control-center
         *And shares the same limitations concerning monitors
         *which have been turned off and back on without being reconfigured
         */
        if (ctk_check_menu_item_get_active (item)){
                int x, y, width, height;
                cafe_rr_output_info_get_geometry (output, &x, &y, NULL, NULL);
                width = cafe_rr_output_info_get_preferred_width (output);
                height = cafe_rr_output_info_get_preferred_height (output);
                cafe_rr_output_info_set_geometry (output, x, y, width, height);
                cafe_rr_output_info_set_active (output, TRUE);

        }
        else{
                cafe_rr_output_info_set_active (output, FALSE);
        }

        error = NULL;
        if (!cafe_rr_config_save (priv->configuration, &error)) {
                error_message (manager, _("Could not save monitor configuration"), error, NULL);
                if (error)
                        g_error_free (error);

                return;
        }

        try_to_apply_intended_configuration (manager, NULL, ctk_get_current_event_time (), NULL);
}

static void
output_rotation_item_activate_cb (CtkCheckMenuItem *item, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CafeRROutputInfo *output;
        CafeRRRotation rotation;
        GError *error;

        /* Not interested in deselected items */
        if (!ctk_check_menu_item_get_active (item))
                return;

        ensure_current_configuration_is_saved ();

        output = g_object_get_data (G_OBJECT (item), "output");
        rotation = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "rotation"));

        cafe_rr_output_info_set_rotation (output, rotation);

        error = NULL;
        if (!cafe_rr_config_save (priv->configuration, &error)) {
                error_message (manager, _("Could not save monitor configuration"), error, NULL);
                if (error)
                        g_error_free (error);

                return;
        }

        try_to_apply_intended_configuration (manager, NULL, ctk_get_current_event_time (), NULL); /* NULL-GError */
}

static void
mirror_outputs_cb(CtkCheckMenuItem *item, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CafeRRScreen *screen = priv->rw_screen;

        if (ctk_check_menu_item_get_active(item)){

                CafeRRConfig *config;
                config = make_clone_setup (screen);
                if (!config || config == NULL){
                        error_message (manager, _("Mirroring outputs not supported"), NULL, NULL);
                }

                cafe_rr_config_save (config, NULL);
                try_to_apply_intended_configuration (manager, NULL, ctk_get_current_event_time (), NULL);

                g_object_unref (config);

        }
        else{

                CafeRRConfig *config;
                config = make_primary_only_setup (screen);
                /*If nothing worked, bring up the display capplet so the user can reconfigure*/
                if (config == NULL)
                         run_display_capplet(CTK_WIDGET(item));
                cafe_rr_config_save (config, NULL);
                try_to_apply_intended_configuration (manager, NULL, ctk_get_current_event_time (), NULL);

                g_object_unref (config);

        }
}
static void
add_items_for_rotations (CsdXrandrManager *manager, CafeRROutputInfo *output, CafeRRRotation allowed_rotations)
{
        typedef struct {
                CafeRRRotation	rotation;
                const char *	name;
        } RotationInfo;
        static const RotationInfo rotations[] = {
                { CAFE_RR_ROTATION_0, N_("Normal") },
                { CAFE_RR_ROTATION_90, N_("Left") },
                { CAFE_RR_ROTATION_270, N_("Right") },
                { CAFE_RR_ROTATION_180, N_("Upside Down") },
                /* We don't allow REFLECT_X or REFLECT_Y for now, as cafe-display-properties doesn't allow them, either */
        };

        struct CsdXrandrManagerPrivate *priv = manager->priv;
        int i;
        GSList *group;
        CtkWidget *active_item;
        gulong active_item_activate_id;

        group = NULL;
        active_item = NULL;
        active_item_activate_id = 0;

        for (i = 0; i < G_N_ELEMENTS (rotations); i++) {
                CafeRRRotation rot;
                CtkWidget *item;
                gulong activate_id;

                rot = rotations[i].rotation;

                if ((allowed_rotations & rot) == 0) {
                        /* don't display items for rotations which are
                         * unavailable.  Their availability is not under the
                         * user's control, anyway.
                         */
                        continue;
                }

                item = ctk_radio_menu_item_new_with_label (group, _(rotations[i].name));
                /*HERE*/
                if (!(cafe_rr_output_info_is_active (output))){
                    ctk_widget_set_sensitive (item, FALSE); /*Rotation can't be set from the OFF state*/
                }
                ctk_widget_show_all (item);
                ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);

                g_object_set_data (G_OBJECT (item), "output", output);
                g_object_set_data (G_OBJECT (item), "rotation", GINT_TO_POINTER (rot));

                activate_id = g_signal_connect (item, "activate",
                                                G_CALLBACK (output_rotation_item_activate_cb), manager);

                group = ctk_radio_menu_item_get_group (CTK_RADIO_MENU_ITEM (item));

                if (rot == cafe_rr_output_info_get_rotation (output)) {
                        active_item = item;
                        active_item_activate_id = activate_id;
                }
        }

        if (active_item) {
                /* Block the signal temporarily so our callback won't be called;
                 * we are just setting up the UI.
                 */
                g_signal_handler_block (active_item, active_item_activate_id);

                ctk_check_menu_item_set_active (CTK_CHECK_MENU_ITEM (active_item), TRUE);

                g_signal_handler_unblock (active_item, active_item_activate_id);
        }

}

static void
add_rotation_items_for_output (CsdXrandrManager *manager, CafeRROutputInfo *output)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        int num_rotations;
        CafeRRRotation rotations;

        get_allowed_rotations_for_output (priv->configuration, priv->rw_screen, output, &num_rotations, &rotations);

        if (num_rotations == 1)
                add_unsupported_rotation_item (manager);
        else
                add_items_for_rotations (manager, output, rotations);
}

static void
add_enable_option_for_output (CsdXrandrManager *manager, CafeRROutputInfo *output)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *item;
        gulong activate_id;

        item = ctk_check_menu_item_new();

        if (cafe_rr_output_info_is_active (output)){
                ctk_menu_item_set_label (CTK_MENU_ITEM(item), _("ON"));
                ctk_widget_set_tooltip_text(item, _("Turn this monitor off"));
        }
        else {
                ctk_menu_item_set_label (CTK_MENU_ITEM(item), _("OFF"));
                ctk_widget_set_tooltip_text(item, _("Turn this monitor on"));
        }

        ctk_widget_show_all (item);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);

        g_object_set_data (G_OBJECT (item), "output", output);

        activate_id = g_signal_connect (item, "activate",
                                G_CALLBACK (monitor_activate_cb), manager);

        /* Block the signal temporarily so our callback won't be called;
        * we are just setting up the UI.
        */
        g_signal_handler_block (item, activate_id);

        if (cafe_rr_output_info_is_active (output)){
            ctk_check_menu_item_set_active(CTK_CHECK_MENU_ITEM(item), TRUE);
        }
        else{
            ctk_check_menu_item_set_active(CTK_CHECK_MENU_ITEM(item), FALSE);
        }

        g_signal_handler_unblock (item, activate_id);
}

static void
add_menu_items_for_output (CsdXrandrManager *manager, CafeRROutputInfo *output)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *item;

        item = make_menu_item_for_output_title (manager, output);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);

        add_enable_option_for_output (manager, output);
        add_rotation_items_for_output (manager, output);
}

static void
add_menu_items_for_outputs (CsdXrandrManager *manager)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        int i;
        CafeRROutputInfo **outputs;

        outputs = cafe_rr_config_get_outputs (priv->configuration);
        for (i = 0; outputs[i] != NULL; i++) {
                if (cafe_rr_output_info_is_connected (outputs[i]))
                        add_menu_items_for_output (manager, outputs[i]);
        }
}

static void
add_menu_items_for_clone (CsdXrandrManager *manager)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *item;
        gulong activate_id;

        item = ctk_check_menu_item_new_with_label(_("Same output all monitors"));
        ctk_widget_set_tooltip_text(item, _("Mirror same output to all monitors and turn them on"));
        ctk_widget_show_all (item);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);
        activate_id =  g_signal_connect (item, "activate",
                                G_CALLBACK (mirror_outputs_cb), manager);
        /*Block the handler until the GUI is set up no matter what the monitor state*/
        g_signal_handler_block (item, activate_id);

        if (cafe_rr_config_get_clone(priv->configuration)){
            ctk_check_menu_item_set_active(CTK_CHECK_MENU_ITEM(item), TRUE);
        }
        else{
            ctk_check_menu_item_set_active(CTK_CHECK_MENU_ITEM(item), FALSE);
        }
        g_signal_handler_unblock (item, activate_id);
}

static void
status_icon_popup_menu (CsdXrandrManager *manager, guint button, guint32 timestamp)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;
        CtkWidget *item;
        CtkWidget *image;
        CtkWidget *label;
        CtkWidget *box;
        GSettings *icon_settings;

        g_assert (priv->configuration == NULL);
        priv->configuration = cafe_rr_config_new_current (priv->rw_screen, NULL);

        g_assert (priv->labeler == NULL);
        priv->labeler = cafe_rr_labeler_new (priv->configuration);

        g_assert (priv->popup_menu == NULL);
        priv->popup_menu = ctk_menu_new ();

        add_menu_items_for_outputs (manager);

        item = ctk_separator_menu_item_new ();
        ctk_widget_show (item);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);

        add_menu_items_for_clone (manager);

        item = ctk_menu_item_new();
        box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 10);
        image = ctk_image_new_from_icon_name ("preferences-system", CTK_ICON_SIZE_MENU);
        label = ctk_label_new_with_mnemonic(_("_Configure Display Settings…"));
        /*Load the icon unless the user has icons in menus turned off*/
        icon_settings = g_settings_new ("org.cafe.interface");
        if (g_settings_get_boolean (icon_settings, "menus-have-icons")){
            ctk_container_add (CTK_CONTAINER (box), image);
            g_signal_connect (item, "size-allocate",
                          G_CALLBACK (title_item_size_allocate_cb), NULL);
            }
        ctk_container_add (CTK_CONTAINER (box), label);
        ctk_container_add (CTK_CONTAINER (item), box);
        ctk_widget_set_tooltip_text(item, _("Open the display configuration dialog (all settings)"));
        g_signal_connect (item, "activate",
                          G_CALLBACK (popup_menu_configure_display_cb), manager);
        ctk_widget_show_all (item);
        ctk_menu_shell_append (CTK_MENU_SHELL (priv->popup_menu), item);

        g_signal_connect (priv->popup_menu, "selection-done",
                          G_CALLBACK (status_icon_popup_menu_selection_done_cb), manager);

        /*Set up custom theming and forced transparency support*/
        CtkWidget *toplevel = ctk_widget_get_toplevel (priv->popup_menu);
        /*Fix any failures of compiz/other wm's to communicate with ctk for transparency */
        CdkScreen *screen = ctk_widget_get_screen(CTK_WIDGET(toplevel));
        CdkVisual *visual = cdk_screen_get_rgba_visual(screen);
        ctk_widget_set_visual(CTK_WIDGET(toplevel), visual);
        /*Set up the ctk theme class from cafe-panel*/
        CtkStyleContext *context;
        context = ctk_widget_get_style_context (CTK_WIDGET(toplevel));
        ctk_style_context_add_class(context,"gnome-panel-menu-bar");
        ctk_style_context_add_class(context,"cafe-panel-menu-bar");

        ctk_menu_popup (CTK_MENU (priv->popup_menu), NULL, NULL,
                        ctk_status_icon_position_menu,
                        priv->status_icon, button, timestamp);
}

static void
status_icon_activate_cb (CtkStatusIcon *status_icon, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);

        /* Suck; we don't get a proper button/timestamp */
        status_icon_popup_menu (manager, 0, ctk_get_current_event_time ());
}

static void
status_icon_popup_menu_cb (CtkStatusIcon *status_icon, guint button, guint32 timestamp, gpointer data)
{
        CsdXrandrManager *manager = CSD_XRANDR_MANAGER (data);

        status_icon_popup_menu (manager, button, timestamp);
}

static void
status_icon_start (CsdXrandrManager *manager)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;

        /* Ideally, we should detect if we are on a tablet and only display
         * the icon in that case.
         */
        if (!priv->status_icon) {
                priv->status_icon = ctk_status_icon_new_from_icon_name (CSD_XRANDR_ICON_NAME);
                ctk_status_icon_set_tooltip_text (priv->status_icon, _("Configure display settings"));

                g_signal_connect (priv->status_icon, "activate",
                                  G_CALLBACK (status_icon_activate_cb), manager);
                g_signal_connect (priv->status_icon, "popup-menu",
                                  G_CALLBACK (status_icon_popup_menu_cb), manager);
        }
}

static void
status_icon_stop (CsdXrandrManager *manager)
{
        struct CsdXrandrManagerPrivate *priv = manager->priv;

        if (priv->status_icon) {
                g_signal_handlers_disconnect_by_func (
                        priv->status_icon, G_CALLBACK (status_icon_activate_cb), manager);
                g_signal_handlers_disconnect_by_func (
                        priv->status_icon, G_CALLBACK (status_icon_popup_menu_cb), manager);

                /* hide the icon before unreffing it; otherwise we will leak
                   whitespace in the notification area due to a bug in there */
                ctk_status_icon_set_visible (priv->status_icon, FALSE);
                g_object_unref (priv->status_icon);
                priv->status_icon = NULL;
        }
}

static void
start_or_stop_icon (CsdXrandrManager *manager)
{
        if (g_settings_get_boolean (manager->priv->settings, CONF_KEY_SHOW_NOTIFICATION_ICON)) {
                status_icon_start (manager);
        }
        else {
                status_icon_stop (manager);
        }
}

static void
on_config_changed (GSettings        *settings,
                   gchar            *key,
                   CsdXrandrManager *manager)
{
        if (g_strcmp0 (key, CONF_KEY_SHOW_NOTIFICATION_ICON) == 0)
                start_or_stop_icon (manager);
}

static gboolean
apply_intended_configuration (CsdXrandrManager *manager, const char *intended_filename, guint32 timestamp)
{
        GError *my_error;
        gboolean result;

#ifdef HAVE_RDA
	if (rda_session_is_remote()) {
		return;
	}
#endif

        my_error = NULL;
        result = apply_configuration_from_filename (manager, intended_filename, TRUE, timestamp, &my_error);
        if (!result) {
                if (my_error) {
                        if (!g_error_matches (my_error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
                            !g_error_matches (my_error, CAFE_RR_ERROR, CAFE_RR_ERROR_NO_MATCHING_CONFIG))
                                error_message (manager, _("Could not apply the stored configuration for monitors"), my_error, NULL);

                        g_error_free (my_error);
                }
        }

        return result;
}

static void
apply_default_boot_configuration (CsdXrandrManager *mgr, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = mgr->priv;
        CafeRRScreen *screen = priv->rw_screen;
        CafeRRConfig *config;
        gboolean turn_on_external, turn_on_laptop;

        turn_on_external =
                g_settings_get_boolean (mgr->priv->settings, CONF_KEY_TURN_ON_EXTERNAL_MONITORS_AT_STARTUP);
        turn_on_laptop =
                g_settings_get_boolean (mgr->priv->settings, CONF_KEY_TURN_ON_LAPTOP_MONITOR_AT_STARTUP);

        if (turn_on_external && turn_on_laptop)
                config = make_clone_setup (screen);
        else if (!turn_on_external && turn_on_laptop)
                config = make_laptop_setup (screen);
        else if (turn_on_external && !turn_on_laptop)
                config = make_other_setup (screen);
        else
                config = make_laptop_setup (screen);

        if (config) {
                apply_configuration_and_display_error (mgr, config, timestamp);
                g_object_unref (config);
        }
}

static gboolean
apply_stored_configuration_at_startup (CsdXrandrManager *manager, guint32 timestamp)
{
        GError *my_error;
        gboolean success;
        char *backup_filename;
        char *intended_filename;

        backup_filename = cafe_rr_config_get_backup_filename ();
        intended_filename = cafe_rr_config_get_intended_filename ();

        /* 1. See if there was a "saved" configuration.  If there is one, it means
         * that the user had selected to change the display configuration, but the
         * machine crashed.  In that case, we'll apply *that* configuration and save it on top of the
         * "intended" one.
         */

        my_error = NULL;

        success = apply_configuration_from_filename (manager, backup_filename, FALSE, timestamp, &my_error);
        if (success) {
                /* The backup configuration existed, and could be applied
                 * successfully, so we must restore it on top of the
                 * failed/intended one.
                 */
                restore_backup_configuration (manager, backup_filename, intended_filename, timestamp);
                goto out;
        }

        if (!g_error_matches (my_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
                /* Epic fail:  there (probably) was a backup configuration, but
                 * we could not apply it.  The only thing we can do is delete
                 * the backup configuration.  Let's hope that the user doesn't
                 * get left with an unusable display...
                 */

                unlink (backup_filename);
                goto out;
        }

        /* 2. There was no backup configuration!  This means we are
         * good.  Apply the intended configuration instead.
         */

        success = apply_intended_configuration (manager, intended_filename, timestamp);

out:
        if (my_error)
                g_error_free (my_error);

        g_free (backup_filename);
        g_free (intended_filename);

        return success;
}

static gboolean
apply_default_configuration_from_file (CsdXrandrManager *manager, guint32 timestamp)
{
        CsdXrandrManagerPrivate *priv = manager->priv;
        char *default_config_filename;
        gboolean result;

        default_config_filename = g_settings_get_string (priv->settings, CONF_KEY_DEFAULT_CONFIGURATION_FILE);
        if (!default_config_filename)
                return FALSE;

        result = apply_configuration_from_filename (manager, default_config_filename, TRUE, timestamp, NULL);

        g_free (default_config_filename);
        return result;
}

gboolean
csd_xrandr_manager_start (CsdXrandrManager *manager,
                          GError          **error)
{
        CdkDisplay      *display;

        g_debug ("Starting xrandr manager");
        cafe_settings_profile_start (NULL);

        log_open ();
        log_msg ("------------------------------------------------------------\nSTARTING XRANDR PLUGIN\n");

        manager->priv->rw_screen = cafe_rr_screen_new (cdk_screen_get_default (), error);

        if (manager->priv->rw_screen == NULL) {
                log_msg ("Could not initialize the RANDR plugin%s%s\n",
                         (error && *error) ? ": " : "",
                         (error && *error) ? (*error)->message : "");
                log_close ();
                return FALSE;
        }

        g_signal_connect (manager->priv->rw_screen, "changed", G_CALLBACK (on_randr_event), manager);

        log_msg ("State of screen at startup:\n");
        log_screen (manager->priv->rw_screen);

        manager->priv->running = TRUE;
        manager->priv->settings = g_settings_new (CONF_SCHEMA);

        g_signal_connect (manager->priv->settings,
                          "changed::" CONF_KEY_SHOW_NOTIFICATION_ICON,
                          G_CALLBACK (on_config_changed),
                          manager);

        display = cdk_display_get_default ();

        if (manager->priv->switch_video_mode_keycode) {
                cdk_x11_display_error_trap_push (display);

                XGrabKey (cdk_x11_get_default_xdisplay(),
                          manager->priv->switch_video_mode_keycode, AnyModifier,
                          cdk_x11_get_default_root_xwindow(),
                          True, GrabModeAsync, GrabModeAsync);

                cdk_display_flush (display);
                cdk_x11_display_error_trap_pop_ignored (display);
        }

        if (manager->priv->rotate_windows_keycode) {
                cdk_x11_display_error_trap_push (display);

                XGrabKey (cdk_x11_get_default_xdisplay(),
                          manager->priv->rotate_windows_keycode, AnyModifier,
                          cdk_x11_get_default_root_xwindow(),
                          True, GrabModeAsync, GrabModeAsync);

                cdk_display_flush (display);
                cdk_x11_display_error_trap_pop_ignored (display);
        }

        show_timestamps_dialog (manager, "Startup");
        if (!apply_stored_configuration_at_startup (manager, CDK_CURRENT_TIME)) /* we don't have a real timestamp at startup anyway */
                if (!apply_default_configuration_from_file (manager, CDK_CURRENT_TIME))
                        if (!g_settings_get_boolean (manager->priv->settings, CONF_KEY_USE_XORG_MONITOR_SETTINGS))
                                apply_default_boot_configuration (manager, CDK_CURRENT_TIME);

        log_msg ("State of screen after initial configuration:\n");
        log_screen (manager->priv->rw_screen);

        cdk_window_add_filter (cdk_get_default_root_window(),
                               (CdkFilterFunc)event_filter,
                               manager);

        start_or_stop_icon (manager);

        log_close ();

        cafe_settings_profile_end (NULL);

        return TRUE;
}

void
csd_xrandr_manager_stop (CsdXrandrManager *manager)
{
        CdkDisplay      *display;

        g_debug ("Stopping xrandr manager");

        manager->priv->running = FALSE;

        display = cdk_display_get_default ();

        if (manager->priv->switch_video_mode_keycode) {
                cdk_x11_display_error_trap_push (display);

                XUngrabKey (cdk_x11_get_default_xdisplay(),
                            manager->priv->switch_video_mode_keycode, AnyModifier,
                            cdk_x11_get_default_root_xwindow());

                cdk_x11_display_error_trap_pop_ignored (display);
        }

        if (manager->priv->rotate_windows_keycode) {
                cdk_x11_display_error_trap_push (display);

                XUngrabKey (cdk_x11_get_default_xdisplay(),
                            manager->priv->rotate_windows_keycode, AnyModifier,
                            cdk_x11_get_default_root_xwindow());

                cdk_x11_display_error_trap_pop_ignored (display);
        }

        cdk_window_remove_filter (cdk_get_default_root_window (),
                                  (CdkFilterFunc) event_filter,
                                  manager);

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->rw_screen != NULL) {
                g_object_unref (manager->priv->rw_screen);
                manager->priv->rw_screen = NULL;
        }

        if (manager->priv->dbus_connection != NULL) {
                dbus_g_connection_unref (manager->priv->dbus_connection);
                manager->priv->dbus_connection = NULL;
        }

        status_icon_stop (manager);

        log_open ();
        log_msg ("STOPPING XRANDR PLUGIN\n------------------------------------------------------------\n");
        log_close ();
}

static void
csd_xrandr_manager_class_init (CsdXrandrManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_xrandr_manager_finalize;

        dbus_g_object_type_install_info (CSD_TYPE_XRANDR_MANAGER, &dbus_glib_csd_xrandr_manager_object_info);
}

static guint
get_keycode_for_keysym_name (const char *name)
{
        Display *dpy;
        guint keyval;

        dpy = cdk_x11_get_default_xdisplay ();

        keyval = cdk_keyval_from_name (name);
        return XKeysymToKeycode (dpy, keyval);
}

static void
csd_xrandr_manager_init (CsdXrandrManager *manager)
{
        manager->priv = csd_xrandr_manager_get_instance_private (manager);

        manager->priv->switch_video_mode_keycode = get_keycode_for_keysym_name (VIDEO_KEYSYM);
        manager->priv->rotate_windows_keycode = get_keycode_for_keysym_name (ROTATE_KEYSYM);

        manager->priv->current_fn_f7_config = -1;
        manager->priv->fn_f7_configs = NULL;
}

static void
csd_xrandr_manager_finalize (GObject *object)
{
        CsdXrandrManager *xrandr_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_XRANDR_MANAGER (object));

        xrandr_manager = CSD_XRANDR_MANAGER (object);

        g_return_if_fail (xrandr_manager->priv != NULL);

        G_OBJECT_CLASS (csd_xrandr_manager_parent_class)->finalize (object);
}

static gboolean
register_manager_dbus (CsdXrandrManager *manager)
{
        GError *error = NULL;

        manager->priv->dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (manager->priv->dbus_connection == NULL) {
                if (error != NULL) {
                        g_warning ("Error getting session bus: %s", error->message);
                        g_error_free (error);
                }
                return FALSE;
        }

        /* Hmm, should we do this in csd_xrandr_manager_start()? */
        dbus_g_connection_register_g_object (manager->priv->dbus_connection, CSD_XRANDR_DBUS_PATH, G_OBJECT (manager));

        return TRUE;
}

CsdXrandrManager *
csd_xrandr_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_XRANDR_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);

                if (!register_manager_dbus (manager_object)) {
                        g_object_unref (manager_object);
                        return NULL;
                }
        }

        return CSD_XRANDR_MANAGER (manager_object);
}

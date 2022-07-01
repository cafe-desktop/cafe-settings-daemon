/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>

#include <dbus/dbus-glib.h>

#include <gio/gio.h>

#include "msd-a11y-preferences-dialog.h"

#define SM_DBUS_NAME      "org.gnome.SessionManager"
#define SM_DBUS_PATH      "/org/gnome/SessionManager"
#define SM_DBUS_INTERFACE "org.gnome.SessionManager"


#define CTKBUILDER_UI_FILE "msd-a11y-preferences-dialog.ui"

#define KEY_A11Y_SCHEMA              "org.cafe.accessibility-keyboard"
#define KEY_STICKY_KEYS_ENABLED      "stickykeys-enable"
#define KEY_BOUNCE_KEYS_ENABLED      "bouncekeys-enable"
#define KEY_SLOW_KEYS_ENABLED        "slowkeys-enable"
#define KEY_MOUSE_KEYS_ENABLED       "mousekeys-enable"

#define KEY_AT_SCHEMA                   "org.cafe.applications-at"
#define KEY_AT_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_AT_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_AT_SCREEN_READER_ENABLED    "screen-reader-enabled"

#define FONT_RENDER_SCHEMA        "org.cafe.font-rendering"
#define KEY_FONT_DPI              "dpi"

/* X servers sometimes lie about the screen's physical dimensions, so we cannot
 * compute an accurate DPI value.  When this happens, the user gets fonts that
 * are too huge or too tiny.  So, we see what the server returns:  if it reports
 * something outside of the range [DPI_LOW_REASONABLE_VALUE,
 * DPI_HIGH_REASONABLE_VALUE], then we assume that it is lying and we use
 * DPI_FALLBACK instead.
 *
 * See get_dpi_from_gsettings_or_server() below, and also
 * https://bugzilla.novell.com/show_bug.cgi?id=217790
 */
#define DPI_LOW_REASONABLE_VALUE 50
#define DPI_HIGH_REASONABLE_VALUE 500

#define DPI_FACTOR_LARGE   1.25
#define DPI_FACTOR_LARGER  1.5
#define DPI_FACTOR_LARGEST 2.0
#define DPI_DEFAULT        96

#define KEY_INTERFACE_SCHEMA   "org.cafe.interface"
#define KEY_CTK_THEME          "ctk-theme"
#define KEY_COLOR_SCHEME       "ctk-color-scheme"
#define KEY_ICON_THEME         "icon-theme"

#define KEY_MARCO_SCHEMA    "org.cafe.Marco"
#define KEY_MARCO_THEME     "theme"

#define HIGH_CONTRAST_THEME    "HighContrast"

struct MsdA11yPreferencesDialogPrivate
{
        CtkWidget *sticky_keys_checkbutton;
        CtkWidget *slow_keys_checkbutton;
        CtkWidget *bounce_keys_checkbutton;

        CtkWidget *large_print_checkbutton;
        CtkWidget *high_contrast_checkbutton;

        CtkWidget *screen_reader_checkbutton;
        CtkWidget *screen_keyboard_checkbutton;
        CtkWidget *screen_magnifier_checkbutton;

        GSettings *settings_a11y;
        GSettings *settings_at;
        GSettings *settings_interface;
        GSettings *settings_marco;
};

enum {
        PROP_0,
};

static void     msd_a11y_preferences_dialog_finalize    (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (MsdA11yPreferencesDialog, msd_a11y_preferences_dialog, CTK_TYPE_DIALOG)

static void
msd_a11y_preferences_dialog_class_init (MsdA11yPreferencesDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = msd_a11y_preferences_dialog_finalize;
}

static void
on_response (MsdA11yPreferencesDialog *dialog,
             gint                      response_id)
{
        switch (response_id) {
        default:
                break;
        }
}

static gboolean
config_get_bool (GSettings  *settings,
                 const char *key,
                 gboolean   *is_writable)
{
        int          enabled;

        if (is_writable) {
                *is_writable = g_settings_is_writable (settings, key);
        }

        enabled = g_settings_get_boolean (settings, key);

        return enabled;
}

static double
dpi_from_pixels_and_mm (int pixels,
                        int mm)
{
        double dpi;

        if (mm >= 1) {
                dpi = pixels / (mm / 25.4);
        } else {
                dpi = 0;
        }

        return dpi;
}

static double
get_dpi_from_x_server (void)
{
        CdkScreen *screen;
        double     dpi;
        int        scale;

        screen = cdk_screen_get_default ();
        if (screen != NULL) {
                double width_dpi;
                double height_dpi;

                Screen *xscreen = cdk_x11_screen_get_xscreen (screen);

                scale = cdk_window_get_scale_factor (cdk_screen_get_root_window (screen));
                width_dpi = dpi_from_pixels_and_mm (WidthOfScreen (xscreen), WidthMMOfScreen (xscreen));
                height_dpi = dpi_from_pixels_and_mm (HeightOfScreen (xscreen), HeightMMOfScreen (xscreen));

                if (width_dpi < DPI_LOW_REASONABLE_VALUE
                    || width_dpi > DPI_HIGH_REASONABLE_VALUE
                    || height_dpi < DPI_LOW_REASONABLE_VALUE
                    || height_dpi > DPI_HIGH_REASONABLE_VALUE) {
                        dpi = DPI_DEFAULT;
                } else {
                        dpi = (width_dpi + height_dpi) / 2.0;
                }

                dpi *= scale;

        } else {
                /* Huh!?  No screen? */
                dpi = DPI_DEFAULT;
        }

        return dpi;
}

static gboolean
config_get_large_print (gboolean *is_writable)
{
        GSettings   *settings;
        gboolean     ret;
        gdouble      x_dpi;
        gdouble      u_dpi;
        gdouble      gs_dpi;

        settings = g_settings_new (FONT_RENDER_SCHEMA);

        gs_dpi = g_settings_get_double (settings, KEY_FONT_DPI);

        if (gs_dpi != 0) {
                u_dpi = gs_dpi;
        } else {
                u_dpi = DPI_DEFAULT;
        }

        x_dpi = get_dpi_from_x_server ();

        g_object_unref (settings);

        g_debug ("MsdA11yPreferences: got x-dpi=%f user-dpi=%f", x_dpi, u_dpi);

        ret = (((double)DPI_FACTOR_LARGE * x_dpi) < u_dpi);

        return ret;
}

static void
config_set_large_print (gboolean enabled)
{
        GSettings *settings;

        settings = g_settings_new (FONT_RENDER_SCHEMA);

        if (enabled) {
                gdouble x_dpi;
                gdouble u_dpi;

                x_dpi = get_dpi_from_x_server ();
                u_dpi = (double)DPI_FACTOR_LARGER * x_dpi;

                g_debug ("MsdA11yPreferences: setting x-dpi=%f user-dpi=%f", x_dpi, u_dpi);

                g_settings_set_double (settings, KEY_FONT_DPI, u_dpi);
        } else {
                g_settings_reset (settings, KEY_FONT_DPI);
        }

        g_object_unref (settings);
}

static gboolean
config_get_high_contrast (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        gboolean ret;
        char    *ctk_theme;

        ret = FALSE;

        *is_writable = g_settings_is_writable (dialog->priv->settings_interface, KEY_CTK_THEME);
        ctk_theme = g_settings_get_string (dialog->priv->settings_interface, KEY_CTK_THEME);

        if (ctk_theme != NULL && strcmp (ctk_theme, HIGH_CONTRAST_THEME) == 0) {
                ret = TRUE;
        }
        g_free (ctk_theme);

        return ret;
}

static void
config_set_high_contrast (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        if (enabled) {
                g_settings_set_string (dialog->priv->settings_interface, KEY_CTK_THEME, HIGH_CONTRAST_THEME);
                g_settings_set_string (dialog->priv->settings_interface, KEY_ICON_THEME, HIGH_CONTRAST_THEME);
                /* there isn't a high contrast marco theme afaik */
        } else {
                g_settings_reset (dialog->priv->settings_interface, KEY_CTK_THEME);
                g_settings_reset (dialog->priv->settings_interface, KEY_ICON_THEME);
                g_settings_reset (dialog->priv->settings_marco, KEY_MARCO_THEME);
        }
}

static gboolean
config_get_sticky_keys (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_a11y, KEY_STICKY_KEYS_ENABLED, is_writable);
}

static void
config_set_sticky_keys (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_a11y, KEY_STICKY_KEYS_ENABLED, enabled);
}

static gboolean
config_get_bounce_keys (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_a11y, KEY_BOUNCE_KEYS_ENABLED, is_writable);
}

static void
config_set_bounce_keys (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_a11y, KEY_BOUNCE_KEYS_ENABLED, enabled);
}

static gboolean
config_get_slow_keys (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_a11y, KEY_SLOW_KEYS_ENABLED, is_writable);
}

static void
config_set_slow_keys (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_a11y, KEY_SLOW_KEYS_ENABLED, enabled);
}

static gboolean
config_have_at_gsettings_condition (const char *condition)
{
        DBusGProxy      *sm_proxy;
        DBusGConnection *connection;
        GError          *error;
        gboolean         res;
        gboolean         is_handled;

        error = NULL;
        connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (connection == NULL) {
                g_warning ("Unable to connect to session bus: %s", error->message);
                return FALSE;
        }
        sm_proxy = dbus_g_proxy_new_for_name (connection,
                                              SM_DBUS_NAME,
                                              SM_DBUS_PATH,
                                              SM_DBUS_INTERFACE);
        if (sm_proxy == NULL) {
                return FALSE;
        }

        is_handled = FALSE;
        res = dbus_g_proxy_call (sm_proxy,
                                 "IsAutostartConditionHandled",
                                 &error,
                                 G_TYPE_STRING, condition,
                                 G_TYPE_INVALID,
                                 G_TYPE_BOOLEAN, &is_handled,
                                 G_TYPE_INVALID);
        if (! res) {
                g_warning ("Unable to call IsAutostartConditionHandled (%s): %s",
                           condition,
                           error->message);
        }

        g_object_unref (sm_proxy);

        return is_handled;
}

static gboolean
config_get_at_screen_reader (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_at, KEY_AT_SCREEN_READER_ENABLED, is_writable);
}

static gboolean
config_get_at_screen_keyboard (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_at, KEY_AT_SCREEN_KEYBOARD_ENABLED, is_writable);
}

static gboolean
config_get_at_screen_magnifier (MsdA11yPreferencesDialog *dialog, gboolean *is_writable)
{
        return config_get_bool (dialog->priv->settings_at, KEY_AT_SCREEN_MAGNIFIER_ENABLED, is_writable);
}

static void
config_set_at_screen_reader (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_at, KEY_AT_SCREEN_READER_ENABLED, enabled);
}

static void
config_set_at_screen_keyboard (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_at, KEY_AT_SCREEN_KEYBOARD_ENABLED, enabled);
}

static void
config_set_at_screen_magnifier (MsdA11yPreferencesDialog *dialog, gboolean enabled)
{
        g_settings_set_boolean (dialog->priv->settings_at, KEY_AT_SCREEN_MAGNIFIER_ENABLED, enabled);
}

static void
on_sticky_keys_checkbutton_toggled (CtkToggleButton          *button,
                                    MsdA11yPreferencesDialog *dialog)
{
        config_set_sticky_keys (dialog, ctk_toggle_button_get_active (button));
}

static void
on_bounce_keys_checkbutton_toggled (CtkToggleButton          *button,
                                    MsdA11yPreferencesDialog *dialog)
{
        config_set_bounce_keys (dialog, ctk_toggle_button_get_active (button));
}

static void
on_slow_keys_checkbutton_toggled (CtkToggleButton          *button,
                                  MsdA11yPreferencesDialog *dialog)
{
        config_set_slow_keys (dialog, ctk_toggle_button_get_active (button));
}

static void
on_high_contrast_checkbutton_toggled (CtkToggleButton          *button,
                                      MsdA11yPreferencesDialog *dialog)
{
        config_set_high_contrast (dialog, ctk_toggle_button_get_active (button));
}

static void
on_at_screen_reader_checkbutton_toggled (CtkToggleButton          *button,
                                         MsdA11yPreferencesDialog *dialog)
{
        config_set_at_screen_reader (dialog, ctk_toggle_button_get_active (button));
}

static void
on_at_screen_keyboard_checkbutton_toggled (CtkToggleButton          *button,
                                           MsdA11yPreferencesDialog *dialog)
{
        config_set_at_screen_keyboard (dialog, ctk_toggle_button_get_active (button));
}

static void
on_at_screen_magnifier_checkbutton_toggled (CtkToggleButton          *button,
                                            MsdA11yPreferencesDialog *dialog)
{
        config_set_at_screen_magnifier (dialog, ctk_toggle_button_get_active (button));
}

static void
on_large_print_checkbutton_toggled (CtkToggleButton          *button,
                                    MsdA11yPreferencesDialog *dialog)
{
        config_set_large_print (ctk_toggle_button_get_active (button));
}

static void
ui_set_sticky_keys (MsdA11yPreferencesDialog *dialog,
                    gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->sticky_keys_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->sticky_keys_checkbutton), enabled);
        }
}

static void
ui_set_bounce_keys (MsdA11yPreferencesDialog *dialog,
                    gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->bounce_keys_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->bounce_keys_checkbutton), enabled);
        }
}

static void
ui_set_slow_keys (MsdA11yPreferencesDialog *dialog,
                  gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->slow_keys_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->slow_keys_checkbutton), enabled);
        }
}

static void
ui_set_high_contrast (MsdA11yPreferencesDialog *dialog,
                      gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->high_contrast_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->high_contrast_checkbutton), enabled);
        }
}

static void
ui_set_at_screen_reader (MsdA11yPreferencesDialog *dialog,
                         gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_reader_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_reader_checkbutton), enabled);
        }
}

static void
ui_set_at_screen_keyboard (MsdA11yPreferencesDialog *dialog,
                           gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_keyboard_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_keyboard_checkbutton), enabled);
        }
}

static void
ui_set_at_screen_magnifier (MsdA11yPreferencesDialog *dialog,
                            gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_magnifier_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->screen_magnifier_checkbutton), enabled);
        }
}

static void
ui_set_large_print (MsdA11yPreferencesDialog *dialog,
                    gboolean                  enabled)
{
        gboolean active;

        active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (dialog->priv->large_print_checkbutton));
        if (active != enabled) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->large_print_checkbutton), enabled);
        }
}

static void
key_changed_cb (GSettings                *settings,
                gchar                    *key,
                MsdA11yPreferencesDialog *dialog)
{
        if (g_strcmp0 (key, KEY_STICKY_KEYS_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_sticky_keys (dialog, enabled);
        } else if (g_strcmp0 (key, KEY_BOUNCE_KEYS_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_bounce_keys (dialog, enabled);
        } else if (g_strcmp0 (key, KEY_SLOW_KEYS_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_slow_keys (dialog, enabled);
        } else if (g_strcmp0 (key, KEY_AT_SCREEN_READER_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_at_screen_reader (dialog, enabled);
        } else if (g_strcmp0 (key, KEY_AT_SCREEN_KEYBOARD_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_at_screen_keyboard (dialog, enabled);
        } else if (strcmp (key, KEY_AT_SCREEN_MAGNIFIER_ENABLED) == 0) {
                gboolean enabled;
                enabled = g_settings_get_boolean (settings, key);
                ui_set_at_screen_magnifier (dialog, enabled);
        } else {
                g_debug ("Config key not handled: %s", key);
        }
}

static void
setup_dialog (MsdA11yPreferencesDialog *dialog,
              CtkBuilder               *builder)
{
        CtkWidget   *widget;
        gboolean     enabled;
        gboolean     is_writable;

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "sticky_keys_checkbutton"));
        dialog->priv->sticky_keys_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_sticky_keys_checkbutton_toggled),
                          dialog);
        enabled = config_get_sticky_keys (dialog, &is_writable);
        ui_set_sticky_keys (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "bounce_keys_checkbutton"));
        dialog->priv->bounce_keys_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_bounce_keys_checkbutton_toggled),
                          dialog);
        enabled = config_get_bounce_keys (dialog, &is_writable);
        ui_set_bounce_keys (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "slow_keys_checkbutton"));
        dialog->priv->slow_keys_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_slow_keys_checkbutton_toggled),
                          dialog);
        enabled = config_get_slow_keys (dialog, &is_writable);
        ui_set_slow_keys (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "high_contrast_checkbutton"));
        dialog->priv->high_contrast_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_high_contrast_checkbutton_toggled),
                          dialog);
        enabled = config_get_high_contrast (dialog, &is_writable);
        ui_set_high_contrast (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "at_screen_keyboard_checkbutton"));
        dialog->priv->screen_keyboard_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_at_screen_keyboard_checkbutton_toggled),
                          dialog);
        enabled = config_get_at_screen_keyboard (dialog, &is_writable);
        ui_set_at_screen_keyboard (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }
        ctk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("CAFE " KEY_AT_SCHEMA " " KEY_AT_SCREEN_KEYBOARD_ENABLED)) {
                ctk_widget_show_all (widget);
        } else {
                ctk_widget_hide (widget);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "at_screen_reader_checkbutton"));
        dialog->priv->screen_reader_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_at_screen_reader_checkbutton_toggled),
                          dialog);
        enabled = config_get_at_screen_reader (dialog, &is_writable);
        ui_set_at_screen_reader (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }
        ctk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("CAFE " KEY_AT_SCHEMA " " KEY_AT_SCREEN_READER_ENABLED)) {
                ctk_widget_show_all (widget);
        } else {
                ctk_widget_hide (widget);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "at_screen_magnifier_checkbutton"));
        dialog->priv->screen_magnifier_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_at_screen_magnifier_checkbutton_toggled),
                          dialog);
        enabled = config_get_at_screen_magnifier (dialog, &is_writable);
        ui_set_at_screen_magnifier (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }
        ctk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("CAFE " KEY_AT_SCHEMA " " KEY_AT_SCREEN_MAGNIFIER_ENABLED)) {
                ctk_widget_show_all (widget);
        } else {
                ctk_widget_hide (widget);
        }

        widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                     "large_print_checkbutton"));
        dialog->priv->large_print_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_large_print_checkbutton_toggled),
                          dialog);
        enabled = config_get_large_print (&is_writable);
        ui_set_large_print (dialog, enabled);
        if (! is_writable) {
                ctk_widget_set_sensitive (widget, FALSE);
        }

        g_signal_connect (dialog->priv->settings_a11y,
                          "changed",
                          G_CALLBACK (key_changed_cb),
                          dialog);
        g_signal_connect (dialog->priv->settings_at,
                          "changed",
                          G_CALLBACK (key_changed_cb),
                          dialog);
}

static void
msd_a11y_preferences_dialog_init (MsdA11yPreferencesDialog *dialog)
{
        static const gchar *ui_file_path = CTKBUILDERDIR "/" CTKBUILDER_UI_FILE;
        gchar *objects[] = {"main_box", NULL};
        GError *error = NULL;
        CtkBuilder  *builder;

        dialog->priv = msd_a11y_preferences_dialog_get_instance_private (dialog);

        dialog->priv->settings_a11y = g_settings_new (KEY_A11Y_SCHEMA);
        dialog->priv->settings_at = g_settings_new (KEY_AT_SCHEMA);
        dialog->priv->settings_interface = g_settings_new (KEY_INTERFACE_SCHEMA);
        dialog->priv->settings_marco = g_settings_new (KEY_MARCO_SCHEMA);

        builder = ctk_builder_new ();
        ctk_builder_set_translation_domain (builder, PACKAGE);
        if (ctk_builder_add_objects_from_file (builder, ui_file_path, objects,
                                               &error) == 0) {
                g_warning ("Could not load A11Y-UI: %s", error->message);
                g_error_free (error);
        } else {
                CtkWidget *widget;

                widget = CTK_WIDGET (ctk_builder_get_object (builder,
                                                             "main_box"));
                ctk_container_add (CTK_CONTAINER (ctk_dialog_get_content_area (CTK_DIALOG (dialog))),
                                   widget);
                ctk_container_set_border_width (CTK_CONTAINER (widget), 12);
                setup_dialog (dialog, builder);
       }

        g_object_unref (builder);

        ctk_container_set_border_width (CTK_CONTAINER (dialog), 12);
        ctk_window_set_title (CTK_WINDOW (dialog), _("Universal Access Preferences"));
        ctk_window_set_icon_name (CTK_WINDOW (dialog), "preferences-desktop-accessibility");
        g_object_set (dialog,
                      "resizable", FALSE,
                      NULL);

        ctk_dialog_add_buttons (CTK_DIALOG (dialog),
                                CTK_STOCK_CLOSE, CTK_RESPONSE_CLOSE,
                                NULL);
        g_signal_connect (dialog,
                          "response",
                          G_CALLBACK (on_response),
                          dialog);


        ctk_widget_show_all (CTK_WIDGET (dialog));
}

static void
msd_a11y_preferences_dialog_finalize (GObject *object)
{
        MsdA11yPreferencesDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_A11Y_PREFERENCES_DIALOG (object));

        dialog = MSD_A11Y_PREFERENCES_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        g_object_unref (dialog->priv->settings_a11y);
        g_object_unref (dialog->priv->settings_at);
        g_object_unref (dialog->priv->settings_interface);
        g_object_unref (dialog->priv->settings_marco);

        G_OBJECT_CLASS (msd_a11y_preferences_dialog_parent_class)->finalize (object);
}

CtkWidget *
msd_a11y_preferences_dialog_new (void)
{
        GObject *object;

        object = g_object_new (MSD_TYPE_A11Y_PREFERENCES_DIALOG,
                               NULL);

        return CTK_WIDGET (object);
}

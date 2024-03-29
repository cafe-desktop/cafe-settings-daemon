/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
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

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
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

#include <X11/XKBlib.h>
#include <X11/extensions/XKBstr.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif /* HAVE_LIBNOTIFY */

#include "cafe-settings-profile.h"
#include "csd_a11y-keyboard-manager.h"
#include "csd_a11y-preferences-dialog.h"

#define CONFIG_SCHEMA "org.cafe.accessibility-keyboard"
#define NOTIFICATION_TIMEOUT 30

struct CsdA11yKeyboardManagerPrivate
{
        int        xkbEventBase;
        gboolean   stickykeys_shortcut_val;
        gboolean   slowkeys_shortcut_val;
        CtkWidget *stickykeys_alert;
        CtkWidget *slowkeys_alert;
        CtkWidget *preferences_dialog;
        CtkStatusIcon *status_icon;
        XkbDescRec *original_xkb_desc;

        GSettings  *settings;

#ifdef HAVE_LIBNOTIFY
        NotifyNotification *notification;
#endif /* HAVE_LIBNOTIFY */
};

static void     csd_a11y_keyboard_manager_finalize (GObject *object);
static void     csd_a11y_keyboard_manager_ensure_status_icon (CsdA11yKeyboardManager *manager);
static void     set_server_from_settings (CsdA11yKeyboardManager *manager);

G_DEFINE_TYPE_WITH_PRIVATE (CsdA11yKeyboardManager, csd_a11y_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

#undef DEBUG_ACCESSIBILITY
#ifdef DEBUG_ACCESSIBILITY
#define d(str)          g_debug (str)
#else
#define d(str)          do { } while (0)
#endif

static CdkFilterReturn
devicepresence_filter (CdkXEvent *xevent,
                       CdkEvent  *event,
                       gpointer   data)
{
        XEvent *xev = (XEvent *) xevent;
        G_GNUC_UNUSED XEventClass class_presence;
        int xi_presence;

        DevicePresence (cdk_x11_get_default_xdisplay (), xi_presence, class_presence);

        if (xev->type == xi_presence)
        {
            XDevicePresenceNotifyEvent *dpn = (XDevicePresenceNotifyEvent *) xev;
            if (dpn->devchange == DeviceEnabled) {
                set_server_from_settings (data);
	    }
        }
        return CDK_FILTER_CONTINUE;
}

static gboolean
supports_xinput_devices (void)
{
        gint op_code, event, error;

        return XQueryExtension (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()),
                                "XInputExtension",
                                &op_code,
                                &event,
                                &error);
}

static void
set_devicepresence_handler (CsdA11yKeyboardManager *manager)
{
        Display *display;
        CdkDisplay *cdk_display;
        XEventClass class_presence;
        G_GNUC_UNUSED int xi_presence;

        if (!supports_xinput_devices ())
                return;

        display = cdk_x11_get_default_xdisplay ();

        cdk_display = cdk_display_get_default ();

        cdk_x11_display_error_trap_push (cdk_display);
        DevicePresence (display, xi_presence, class_presence);
        /* FIXME:
         * Note that this might overwrite other events, see:
         * https://bugzilla.gnome.org/show_bug.cgi?id=610245#c2
         **/
        XSelectExtensionEvent (display,
                               RootWindow (display, DefaultScreen (display)),
                               &class_presence, 1);

        cdk_display_flush (cdk_display);
        if (!cdk_x11_display_error_trap_pop (cdk_display))
                cdk_window_add_filter (NULL, devicepresence_filter, manager);
}

static gboolean
xkb_enabled (CsdA11yKeyboardManager *manager)
{
        gboolean have_xkb;
        int opcode, errorBase, major, minor;

        have_xkb = XkbQueryExtension (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()),
                                      &opcode,
                                      &manager->priv->xkbEventBase,
                                      &errorBase,
                                      &major,
                                      &minor)
                && XkbUseExtension (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), &major, &minor);

        return have_xkb;
}

static XkbDescRec *
get_xkb_desc_rec (CsdA11yKeyboardManager *manager)
{
        CdkDisplay *display;
        XkbDescRec *desc;
        Status      status = Success;

        display = cdk_display_get_default ();

        cdk_x11_display_error_trap_push (display);
        desc = XkbGetMap (CDK_DISPLAY_XDISPLAY(display), XkbAllMapComponentsMask, XkbUseCoreKbd);
        if (desc != NULL) {
                desc->ctrls = NULL;
                status = XkbGetControls (CDK_DISPLAY_XDISPLAY(display), XkbAllControlsMask, desc);
        }
        cdk_x11_display_error_trap_pop_ignored (display);

        g_return_val_if_fail (desc != NULL, NULL);
        g_return_val_if_fail (desc->ctrls != NULL, NULL);
        g_return_val_if_fail (status == Success, NULL);

        return desc;
}

static int
get_int (GSettings  *settings,
         char const *key)
{
        int res = g_settings_get_int (settings, key);
        if (res <= 0) {
                res = 1;
        }
        return res;
}

static gboolean
set_int (GSettings      *settings,
         char const     *key,
         int             val)
{
        int pre_val = g_settings_get_int (settings, key);
        g_settings_set_int (settings, key, val);
#ifdef DEBUG_ACCESSIBILITY
        if (val != pre_val) {
                g_warning ("%s changed", key);
        }
#endif
        return val != pre_val;
}

static gboolean
set_bool (GSettings      *settings,
          char const     *key,
          int             val)
{
        gboolean bval = (val != 0);
        gboolean pre_val = g_settings_get_boolean (settings, key);

        g_settings_set_boolean (settings, key, bval ? TRUE : FALSE);
#ifdef DEBUG_ACCESSIBILITY
        if (bval != pre_val) {
                d ("%s changed", key);
                return TRUE;
        }
#endif
        return (bval != pre_val);
}

static unsigned long
set_clear (gboolean      flag,
           unsigned long value,
           unsigned long mask)
{
        if (flag) {
                return value | mask;
        }
        return value & ~mask;
}

static gboolean
set_ctrl_from_settings (XkbDescRec   *desc,
                     GSettings  *settings,
                     char const   *key,
                     unsigned long mask)
{
        gboolean result = g_settings_get_boolean (settings, key);
        desc->ctrls->enabled_ctrls = set_clear (result, desc->ctrls->enabled_ctrls, mask);
        return result;
}

static void
set_server_from_settings (CsdA11yKeyboardManager *manager)
{
        XkbDescRec      *desc;
        gboolean         enable_accessX;
        CdkDisplay      *display;

        cafe_settings_profile_start (NULL);

        desc = get_xkb_desc_rec (manager);
        if (!desc) {
                return;
        }

        /* general */
        enable_accessX = g_settings_get_boolean (manager->priv->settings, "enable");

        desc->ctrls->enabled_ctrls = set_clear (enable_accessX,
                                                desc->ctrls->enabled_ctrls,
                                                XkbAccessXKeysMask);

        if (set_ctrl_from_settings (desc, manager->priv->settings, "timeout-enable",
                                 XkbAccessXTimeoutMask)) {
                desc->ctrls->ax_timeout = get_int (manager->priv->settings, "timeout");
                /* disable only the master flag via the server we will disable
                 * the rest on the rebound without affecting gsettings state
                 * don't change the option flags at all.
                 */
                desc->ctrls->axt_ctrls_mask = XkbAccessXKeysMask | XkbAccessXFeedbackMask;
                desc->ctrls->axt_ctrls_values = 0;
                desc->ctrls->axt_opts_mask = 0;
        }

        desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "feature-state-change-beep"),
                                             desc->ctrls->ax_options,
                                             XkbAccessXFeedbackMask | XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask);

        /* bounce keys */
        if (set_ctrl_from_settings (desc,
                                 manager->priv->settings,
                                 "bouncekeys-enable",
                                 XkbBounceKeysMask)) {
                desc->ctrls->debounce_delay  = get_int (manager->priv->settings,
                                                        "bouncekeys-delay");
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "bouncekeys-beep-reject"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_BKRejectFBMask);
        }

        /* mouse keys */
        if (set_ctrl_from_settings (desc,
                                 manager->priv->settings,
                                 "mousekeys-enable",
                                 XkbMouseKeysMask | XkbMouseKeysAccelMask)) {
                desc->ctrls->mk_interval     = 100;     /* msec between mousekey events */
                desc->ctrls->mk_curve        = 50;

                /* We store pixels / sec, XKB wants pixels / event */
                desc->ctrls->mk_max_speed    = get_int (manager->priv->settings,
                        "mousekeys-max-speed") / (1000 / desc->ctrls->mk_interval);
                if (desc->ctrls->mk_max_speed <= 0)
                        desc->ctrls->mk_max_speed = 1;

                desc->ctrls->mk_time_to_max = get_int (manager->priv->settings, /* events before max */
                                                       "mousekeys-accel-time") / desc->ctrls->mk_interval;
                if (desc->ctrls->mk_time_to_max <= 0)
                        desc->ctrls->mk_time_to_max = 1;

                desc->ctrls->mk_delay = get_int (manager->priv->settings, /* ms before 1st event */
                                                 "mousekeys-init-delay");
        }

        /* slow keys */
        if (set_ctrl_from_settings (desc,
                                 manager->priv->settings,
                                 "slowkeys-enable",
                                 XkbSlowKeysMask)) {
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "slowkeys-beep-press"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKPressFBMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "slowkeys-beep-accept"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKAcceptFBMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "slowkeys-beep-reject"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKRejectFBMask);
                desc->ctrls->slow_keys_delay = get_int (manager->priv->settings,
                                                        "slowkeys-delay");
                /* anything larger than 500 seems to loose all keyboard input */
                if (desc->ctrls->slow_keys_delay > 500)
                        desc->ctrls->slow_keys_delay = 500;
        }

        /* sticky keys */
        if (set_ctrl_from_settings (desc,
                                 manager->priv->settings,
                                 "stickykeys-enable",
                                 XkbStickyKeysMask)) {
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "stickykeys-latch-to-lock"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_LatchToLockMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "stickykeys-two-key-off"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_TwoKeysMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "stickykeys-modifier-beep"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_StickyKeysFBMask);
        }

        /* toggle keys */
        desc->ctrls->ax_options = set_clear (g_settings_get_boolean (manager->priv->settings, "togglekeys-enable"),
                                             desc->ctrls->ax_options,
                                             XkbAccessXFeedbackMask | XkbAX_IndicatorFBMask);

        /*
        g_debug ("CHANGE to : 0x%x", desc->ctrls->enabled_ctrls);
        g_debug ("CHANGE to : 0x%x (2)", desc->ctrls->ax_options);
        */

        display = cdk_display_get_default ();

        cdk_x11_display_error_trap_push (display);
        XkbSetControls (CDK_DISPLAY_XDISPLAY(display),
                        XkbSlowKeysMask         |
                        XkbBounceKeysMask       |
                        XkbStickyKeysMask       |
                        XkbMouseKeysMask        |
                        XkbMouseKeysAccelMask   |
                        XkbAccessXKeysMask      |
                        XkbAccessXTimeoutMask   |
                        XkbAccessXFeedbackMask  |
                        XkbControlsEnabledMask,
                        desc);

        XkbFreeKeyboard (desc, XkbAllComponentsMask, True);

        XSync (CDK_DISPLAY_XDISPLAY(display), FALSE);
        cdk_x11_display_error_trap_pop_ignored (display);

        cafe_settings_profile_end (NULL);
}

static gboolean
ax_response_callback (CsdA11yKeyboardManager *manager,
                      CtkWindow              *parent,
                      gint                    response_id,
                      guint                   revert_controls_mask,
                      gboolean                enabled)
{
        GError *err;

        switch (response_id) {
        case CTK_RESPONSE_DELETE_EVENT:
        case CTK_RESPONSE_REJECT:
        case CTK_RESPONSE_CANCEL:

                /* we're reverting, so we invert sense of 'enabled' flag */
                d ("cancelling AccessX request");
                if (revert_controls_mask == XkbStickyKeysMask) {
                        g_settings_set_boolean (manager->priv->settings,
                                               "stickykeys-enable",
                                               !enabled);
                }
                else if (revert_controls_mask == XkbSlowKeysMask) {
                        g_settings_set_boolean (manager->priv->settings,
                                               "slowkeys-enable",
                                               !enabled);
                }
                set_server_from_settings (manager);

                break;

        case CTK_RESPONSE_HELP:
                err = NULL;
                if (!ctk_show_uri_on_window (parent,
                                   "help:cafe-user-guide/goscustaccess-6",
                                   ctk_get_current_event_time(),
                                   &err)) {
                        CtkWidget *error_dialog = ctk_message_dialog_new (parent,
                                                                          0,
                                                                          CTK_MESSAGE_ERROR,
                                                                          CTK_BUTTONS_CLOSE,
                                                                          _("There was an error displaying help: %s"),
                                                                          err->message);
                        g_signal_connect (error_dialog, "response",
                                          G_CALLBACK (ctk_widget_destroy), NULL);
                        ctk_window_set_resizable (CTK_WINDOW (error_dialog), FALSE);
                        ctk_widget_show (error_dialog);
                        g_error_free (err);
                }
                return FALSE;
        default:
                break;
        }
        return TRUE;
}

static void
ax_stickykeys_response (CtkDialog              *dialog,
                        gint                    response_id,
                        CsdA11yKeyboardManager *manager)
{
        if (ax_response_callback (manager, CTK_WINDOW (dialog),
                                  response_id, XkbStickyKeysMask,
                                  manager->priv->stickykeys_shortcut_val)) {
                ctk_widget_destroy (CTK_WIDGET (dialog));
        }
}

static void
ax_slowkeys_response (CtkDialog              *dialog,
                      gint                    response_id,
                      CsdA11yKeyboardManager *manager)
{
        if (ax_response_callback (manager, CTK_WINDOW (dialog),
                                  response_id, XkbSlowKeysMask,
                                  manager->priv->slowkeys_shortcut_val)) {
                ctk_widget_destroy (CTK_WIDGET (dialog));
        }
}

static void
maybe_show_status_icon (CsdA11yKeyboardManager *manager)
{
        gboolean     show;

        /* for now, show if accessx is enabled */
        show = g_settings_get_boolean (manager->priv->settings, "enable");

        if (!show && manager->priv->status_icon == NULL)
                return;

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        ctk_status_icon_set_visible (manager->priv->status_icon, show);
}

#ifdef HAVE_LIBNOTIFY
static void
on_notification_closed (NotifyNotification     *notification,
                        CsdA11yKeyboardManager *manager)
{
        g_object_unref (manager->priv->notification);
        manager->priv->notification = NULL;
}

static void
on_slow_keys_action (NotifyNotification     *notification,
                     const char             *action,
                     CsdA11yKeyboardManager *manager)
{
        gboolean res;
        int      response_id;

        g_assert (action != NULL);

        if (strcmp (action, "accept") == 0) {
                response_id = CTK_RESPONSE_ACCEPT;
        } else if (strcmp (action, "reject") == 0) {
                response_id = CTK_RESPONSE_REJECT;
        } else {
                return;
        }

        res = ax_response_callback (manager, NULL,
                                    response_id, XkbSlowKeysMask,
                                    manager->priv->slowkeys_shortcut_val);
        if (res) {
                notify_notification_close (manager->priv->notification, NULL);
        }
}

static void
on_sticky_keys_action (NotifyNotification     *notification,
                       const char             *action,
                       CsdA11yKeyboardManager *manager)
{
        gboolean res;
        int      response_id;

        g_assert (action != NULL);

        if (strcmp (action, "accept") == 0) {
                response_id = CTK_RESPONSE_ACCEPT;
        } else if (strcmp (action, "reject") == 0) {
                response_id = CTK_RESPONSE_REJECT;
        } else {
                return;
        }

        res = ax_response_callback (manager, NULL,
                                    response_id, XkbStickyKeysMask,
                                    manager->priv->stickykeys_shortcut_val);
        if (res) {
                notify_notification_close (manager->priv->notification, NULL);
        }
}

#endif /* HAVE_LIBNOTIFY */

static gboolean
ax_slowkeys_warning_post_bubble (CsdA11yKeyboardManager *manager,
                                 gboolean                enabled)
{
#ifdef HAVE_LIBNOTIFY
        gboolean    res;
        const char *title;
        const char *message;
        GError     *error;

        title = enabled ?
                _("Do you want to activate Slow Keys?") :
                _("Do you want to deactivate Slow Keys?");
        message = _("You just held down the Shift key for 8 seconds.  This is the shortcut "
                    "for the Slow Keys feature, which affects the way your keyboard works.");

        if (manager->priv->status_icon == NULL || ! ctk_status_icon_is_embedded (manager->priv->status_icon)) {
                return FALSE;
        }

        if (manager->priv->slowkeys_alert != NULL) {
                ctk_widget_destroy (manager->priv->slowkeys_alert);
        }

        if (manager->priv->notification != NULL) {
                notify_notification_close (manager->priv->notification, NULL);
        }

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        manager->priv->notification = notify_notification_new (title,
                                                               message,
                                                               "preferences-desktop-accessibility");
        notify_notification_set_timeout (manager->priv->notification, NOTIFICATION_TIMEOUT * 1000);

        notify_notification_add_action (manager->priv->notification,
                                        "reject",
                                        enabled ? _("Don't activate") : _("Don't deactivate"),
                                        (NotifyActionCallback) on_slow_keys_action,
                                        manager,
                                        NULL);
        notify_notification_add_action (manager->priv->notification,
                                        "accept",
                                        enabled ? _("Activate") : _("Deactivate"),
                                        (NotifyActionCallback) on_slow_keys_action,
                                        manager,
                                        NULL);

        g_signal_connect (manager->priv->notification,
                          "closed",
                          G_CALLBACK (on_notification_closed),
                          manager);

        error = NULL;
        res = notify_notification_show (manager->priv->notification, &error);
        if (! res) {
                g_warning ("CsdA11yKeyboardManager: unable to show notification: %s", error->message);
                g_error_free (error);
                notify_notification_close (manager->priv->notification, NULL);
        }

        return res;
#else
        return FALSE;
#endif /* HAVE_LIBNOTIFY */
}


static void
ax_slowkeys_warning_post_dialog (CsdA11yKeyboardManager *manager,
                                 gboolean                enabled)
{
        const char *title;
        const char *message;

        title = enabled ?
                _("Do you want to activate Slow Keys?") :
                _("Do you want to deactivate Slow Keys?");
        message = _("You just held down the Shift key for 8 seconds.  This is the shortcut "
                    "for the Slow Keys feature, which affects the way your keyboard works.");

        if (manager->priv->slowkeys_alert != NULL) {
                ctk_widget_show (manager->priv->slowkeys_alert);
                return;
        }

        manager->priv->slowkeys_alert = ctk_message_dialog_new (NULL,
                                                                0,
                                                                CTK_MESSAGE_WARNING,
                                                                CTK_BUTTONS_NONE,
                                                                "%s", title);

        ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (manager->priv->slowkeys_alert),
                                                  "%s", message);

        ctk_dialog_add_button (CTK_DIALOG (manager->priv->slowkeys_alert),
                               CTK_STOCK_HELP,
                               CTK_RESPONSE_HELP);
        ctk_dialog_add_button (CTK_DIALOG (manager->priv->slowkeys_alert),
                               enabled ? _("Do_n't activate") : _("Do_n't deactivate"),
                               CTK_RESPONSE_REJECT);
        ctk_dialog_add_button (CTK_DIALOG (manager->priv->slowkeys_alert),
                               enabled ? _("_Activate") : _("_Deactivate"),
                               CTK_RESPONSE_ACCEPT);

        ctk_window_set_title (CTK_WINDOW (manager->priv->slowkeys_alert),
                              _("Slow Keys Alert"));
        ctk_window_set_icon_name (CTK_WINDOW (manager->priv->slowkeys_alert),
                                  "input-keyboard");
        ctk_dialog_set_default_response (CTK_DIALOG (manager->priv->slowkeys_alert),
                                         CTK_RESPONSE_ACCEPT);

        g_signal_connect (manager->priv->slowkeys_alert,
                          "response",
                          G_CALLBACK (ax_slowkeys_response),
                          manager);
        ctk_widget_show (manager->priv->slowkeys_alert);

        g_object_add_weak_pointer (G_OBJECT (manager->priv->slowkeys_alert),
                                   (gpointer*) &manager->priv->slowkeys_alert);
}

static void
ax_slowkeys_warning_post (CsdA11yKeyboardManager *manager,
                          gboolean                enabled)
{

        manager->priv->slowkeys_shortcut_val = enabled;

        /* alway try to show something */
        if (! ax_slowkeys_warning_post_bubble (manager, enabled)) {
                ax_slowkeys_warning_post_dialog (manager, enabled);
        }
}

static gboolean
ax_stickykeys_warning_post_bubble (CsdA11yKeyboardManager *manager,
                                   gboolean                enabled)
{
#ifdef HAVE_LIBNOTIFY
        gboolean    res;
        const char *title;
        const char *message;
        GError     *error;

        title = enabled ?
                _("Do you want to activate Sticky Keys?") :
                _("Do you want to deactivate Sticky Keys?");
        message = enabled ?
                _("You just pressed the Shift key 5 times in a row.  This is the shortcut "
                  "for the Sticky Keys feature, which affects the way your keyboard works.") :
                _("You just pressed two keys at once, or pressed the Shift key 5 times in a row.  "
                  "This turns off the Sticky Keys feature, which affects the way your keyboard works.");

        if (manager->priv->status_icon == NULL || ! ctk_status_icon_is_embedded (manager->priv->status_icon)) {
                return FALSE;
        }

        if (manager->priv->slowkeys_alert != NULL) {
                ctk_widget_destroy (manager->priv->slowkeys_alert);
        }

        if (manager->priv->notification != NULL) {
                notify_notification_close (manager->priv->notification, NULL);
        }

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        manager->priv->notification = notify_notification_new (title,
                                                               message,
                                                               "preferences-desktop-accessibility");
        notify_notification_set_timeout (manager->priv->notification, NOTIFICATION_TIMEOUT * 1000);

        notify_notification_add_action (manager->priv->notification,
                                        "reject",
                                        enabled ? _("Don't activate") : _("Don't deactivate"),
                                        (NotifyActionCallback) on_sticky_keys_action,
                                        manager,
                                        NULL);
        notify_notification_add_action (manager->priv->notification,
                                        "accept",
                                        enabled ? _("Activate") : _("Deactivate"),
                                        (NotifyActionCallback) on_sticky_keys_action,
                                        manager,
                                        NULL);

        g_signal_connect (manager->priv->notification,
                          "closed",
                          G_CALLBACK (on_notification_closed),
                          manager);

        error = NULL;
        res = notify_notification_show (manager->priv->notification, &error);
        if (! res) {
                g_warning ("CsdA11yKeyboardManager: unable to show notification: %s", error->message);
                g_error_free (error);
                notify_notification_close (manager->priv->notification, NULL);
        }

        return res;
#else
        return FALSE;
#endif /* HAVE_LIBNOTIFY */
}

static void
ax_stickykeys_warning_post_dialog (CsdA11yKeyboardManager *manager,
                                   gboolean                enabled)
{
        const char *title;
        const char *message;

        title = enabled ?
                _("Do you want to activate Sticky Keys?") :
                _("Do you want to deactivate Sticky Keys?");
        message = enabled ?
                _("You just pressed the Shift key 5 times in a row.  This is the shortcut "
                  "for the Sticky Keys feature, which affects the way your keyboard works.") :
                _("You just pressed two keys at once, or pressed the Shift key 5 times in a row.  "
                  "This turns off the Sticky Keys feature, which affects the way your keyboard works.");

        if (manager->priv->stickykeys_alert != NULL) {
                ctk_widget_show (manager->priv->stickykeys_alert);
                return;
        }

        manager->priv->stickykeys_alert = ctk_message_dialog_new (NULL,
                                                                  0,
                                                                  CTK_MESSAGE_WARNING,
                                                                  CTK_BUTTONS_NONE,
                                                                  "%s", title);

        ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (manager->priv->stickykeys_alert),
                                                  "%s", message);

        ctk_dialog_add_button (CTK_DIALOG (manager->priv->stickykeys_alert),
                               CTK_STOCK_HELP,
                               CTK_RESPONSE_HELP);
        ctk_dialog_add_button (CTK_DIALOG (manager->priv->stickykeys_alert),
                               enabled ? _("Do_n't activate") : _("Do_n't deactivate"),
                               CTK_RESPONSE_REJECT);
        ctk_dialog_add_button (CTK_DIALOG (manager->priv->stickykeys_alert),
                               enabled ? _("_Activate") : _("_Deactivate"),
                               CTK_RESPONSE_ACCEPT);

        ctk_window_set_title (CTK_WINDOW (manager->priv->stickykeys_alert),
                              _("Sticky Keys Alert"));
        ctk_window_set_icon_name (CTK_WINDOW (manager->priv->stickykeys_alert),
                                  "input-keyboard");
        ctk_dialog_set_default_response (CTK_DIALOG (manager->priv->stickykeys_alert),
                                         CTK_RESPONSE_ACCEPT);

        g_signal_connect (manager->priv->stickykeys_alert,
                          "response",
                          G_CALLBACK (ax_stickykeys_response),
                          manager);
        ctk_widget_show (manager->priv->stickykeys_alert);

        g_object_add_weak_pointer (G_OBJECT (manager->priv->stickykeys_alert),
                                   (gpointer*) &manager->priv->stickykeys_alert);
}

static void
ax_stickykeys_warning_post (CsdA11yKeyboardManager *manager,
                            gboolean                enabled)
{

        manager->priv->stickykeys_shortcut_val = enabled;

        /* alway try to show something */
        if (! ax_stickykeys_warning_post_bubble (manager, enabled)) {
                ax_stickykeys_warning_post_dialog (manager, enabled);
        }
}

static void
set_settings_from_server (CsdA11yKeyboardManager *manager)
{
        GSettings      *settings;
        XkbDescRec     *desc;
        gboolean        changed = FALSE;
        gboolean        slowkeys_changed;
        gboolean        stickykeys_changed;

        desc = get_xkb_desc_rec (manager);
        if (! desc) {
                return;
        }

        settings = g_settings_new (CONFIG_SCHEMA);
        g_settings_delay(settings);

        /*
          fprintf (stderr, "changed to : 0x%x\n", desc->ctrls->enabled_ctrls);
          fprintf (stderr, "changed to : 0x%x (2)\n", desc->ctrls->ax_options);
        */

        changed |= set_bool (settings,
                             "enable",
                             desc->ctrls->enabled_ctrls & XkbAccessXKeysMask);

        changed |= set_bool (settings,
                             "feature-state-change-beep",
                             desc->ctrls->ax_options & (XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask));
        changed |= set_bool (settings,
                             "timeout-enable",
                             desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask);
        changed |= set_int (settings,
                            "timeout",
                            desc->ctrls->ax_timeout);

        changed |= set_bool (settings,
                             "bouncekeys-enable",
                             desc->ctrls->enabled_ctrls & XkbBounceKeysMask);
        changed |= set_int (settings,
                            "bouncekeys-delay",
                            desc->ctrls->debounce_delay);
        changed |= set_bool (settings,
                             "bouncekeys-beep-reject",
                             desc->ctrls->ax_options & XkbAX_BKRejectFBMask);

        changed |= set_bool (settings,
                             "mousekeys-enable",
                             desc->ctrls->enabled_ctrls & XkbMouseKeysMask);
        changed |= set_int (settings,
                            "mousekeys-max-speed",
                            desc->ctrls->mk_max_speed * (1000 / desc->ctrls->mk_interval));
        /* NOTE : mk_time_to_max is measured in events not time */
        changed |= set_int (settings,
                            "mousekeys-accel-time",
                            desc->ctrls->mk_time_to_max * desc->ctrls->mk_interval);
        changed |= set_int (settings,
                            "mousekeys-init-delay",
                            desc->ctrls->mk_delay);

        slowkeys_changed = set_bool (settings,
                                     "slowkeys-enable",
                                     desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-press",
                             desc->ctrls->ax_options & XkbAX_SKPressFBMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-accept",
                             desc->ctrls->ax_options & XkbAX_SKAcceptFBMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-reject",
                             desc->ctrls->ax_options & XkbAX_SKRejectFBMask);
        changed |= set_int (settings,
                            "slowkeys-delay",
                            desc->ctrls->slow_keys_delay);

        stickykeys_changed = set_bool (settings,
                                       "stickykeys-enable",
                                       desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
        changed |= set_bool (settings,
                             "stickykeys-latch-to-lock",
                             desc->ctrls->ax_options & XkbAX_LatchToLockMask);
        changed |= set_bool (settings,
                             "stickykeys-two-key-off",
                             desc->ctrls->ax_options & XkbAX_TwoKeysMask);
        changed |= set_bool (settings,
                             "stickykeys-modifier-beep",
                             desc->ctrls->ax_options & XkbAX_StickyKeysFBMask);

        changed |= set_bool (settings,
                             "togglekeys-enable",
                             desc->ctrls->ax_options & XkbAX_IndicatorFBMask);

        if (!changed && stickykeys_changed ^ slowkeys_changed) {
                /*
                 * sticky or slowkeys has changed, singly, without our intervention.
                 * 99% chance this is due to a keyboard shortcut being used.
                 * we need to detect via this hack until we get
                 *  XkbAXN_AXKWarning notifications working (probable XKB bug),
                 *  at which time we can directly intercept such shortcuts instead.
                 * See cb_xkb_event_filter () below.
                 */

                /* sanity check: are keyboard shortcuts available? */
                if (desc->ctrls->enabled_ctrls & XkbAccessXKeysMask) {
                        if (slowkeys_changed) {
                                ax_slowkeys_warning_post (manager,
                                                          desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
                        } else {
                                ax_stickykeys_warning_post (manager,
                                                            desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
                        }
                }
        }

        XkbFreeKeyboard (desc, XkbAllComponentsMask, True);

        changed |= (stickykeys_changed | slowkeys_changed);

        if (changed) {
                g_settings_apply (settings);
        }

        g_object_unref (settings);
}

static CdkFilterReturn
cb_xkb_event_filter (CdkXEvent              *xevent,
                     CdkEvent               *ignored1,
                     CsdA11yKeyboardManager *manager)
{
        XEvent   *xev   = (XEvent *) xevent;
        XkbEvent *xkbEv = (XkbEvent *) xevent;

        if (xev->xany.type == (manager->priv->xkbEventBase + XkbEventCode) &&
            xkbEv->any.xkb_type == XkbControlsNotify) {
                d ("XKB state changed");
                set_settings_from_server (manager);
        } else if (xev->xany.type == (manager->priv->xkbEventBase + XkbEventCode) &&
                   xkbEv->any.xkb_type == XkbAccessXNotify) {
                if (xkbEv->accessx.detail == XkbAXN_AXKWarning) {
                        d ("About to turn on an AccessX feature from the keyboard!");
                        /*
                         * TODO: when XkbAXN_AXKWarnings start working, we need to
                         * invoke ax_keys_warning_dialog_run here instead of in
                         * set_settings_from_server().
                         */
                }
        }

        return CDK_FILTER_CONTINUE;
}

static void
keyboard_callback (GSettings              *settings,
                   gchar                  *key,
                   CsdA11yKeyboardManager *manager)
{
        set_server_from_settings (manager);
        maybe_show_status_icon (manager);
}

static gboolean
start_a11y_keyboard_idle_cb (CsdA11yKeyboardManager *manager)
{
        guint        event_mask;

        g_debug ("Starting a11y_keyboard manager");
        cafe_settings_profile_start (NULL);

        if (!xkb_enabled (manager))
                goto out;

        manager->priv->settings = g_settings_new (CONFIG_SCHEMA);
        g_signal_connect (manager->priv->settings, "changed", G_CALLBACK (keyboard_callback), manager);

        set_devicepresence_handler (manager);

        /* Save current xkb state so we can restore it on exit
         */
        manager->priv->original_xkb_desc = get_xkb_desc_rec (manager);

        event_mask = XkbControlsNotifyMask;
#ifdef DEBUG_ACCESSIBILITY
        event_mask |= XkbAccessXNotifyMask; /* make default when AXN_AXKWarning works */
#endif

        /* be sure to init before starting to monitor the server */
        set_server_from_settings (manager);

        XkbSelectEvents (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()),
                         XkbUseCoreKbd,
                         event_mask,
                         event_mask);

        cdk_window_add_filter (NULL,
                               (CdkFilterFunc) cb_xkb_event_filter,
                               manager);

        maybe_show_status_icon (manager);

 out:
        cafe_settings_profile_end (NULL);

        return FALSE;
}


gboolean
csd_a11y_keyboard_manager_start (CsdA11yKeyboardManager *manager,
                                 GError                **error)
{
        cafe_settings_profile_start (NULL);

        g_idle_add ((GSourceFunc) start_a11y_keyboard_idle_cb, manager);

        cafe_settings_profile_end (NULL);

        return TRUE;
}

static void
restore_server_xkb_config (CsdA11yKeyboardManager *manager)
{
        CdkDisplay      *display;

        display = cdk_display_get_default ();
        cdk_x11_display_error_trap_push (display);
        XkbSetControls (CDK_DISPLAY_XDISPLAY(display),
                        XkbSlowKeysMask         |
                        XkbBounceKeysMask       |
                        XkbStickyKeysMask       |
                        XkbMouseKeysMask        |
                        XkbMouseKeysAccelMask   |
                        XkbAccessXKeysMask      |
                        XkbAccessXTimeoutMask   |
                        XkbAccessXFeedbackMask  |
                        XkbControlsEnabledMask,
                        manager->priv->original_xkb_desc);

        XkbFreeKeyboard (manager->priv->original_xkb_desc,
                         XkbAllComponentsMask, True);

        XSync (CDK_DISPLAY_XDISPLAY(display), FALSE);
        cdk_x11_display_error_trap_pop_ignored (display);

        manager->priv->original_xkb_desc = NULL;
}

void
csd_a11y_keyboard_manager_stop (CsdA11yKeyboardManager *manager)
{
        CsdA11yKeyboardManagerPrivate *p = manager->priv;

        g_debug ("Stopping a11y_keyboard manager");

        cdk_window_remove_filter (NULL, devicepresence_filter, manager);

        if (p->status_icon)
                ctk_status_icon_set_visible (p->status_icon, FALSE);

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        cdk_window_remove_filter (NULL,
                                  (CdkFilterFunc) cb_xkb_event_filter,
                                  manager);

        /* Disable all the AccessX bits
         */
        restore_server_xkb_config (manager);

        if (p->slowkeys_alert != NULL)
                ctk_widget_destroy (p->slowkeys_alert);

        if (p->stickykeys_alert != NULL)
                ctk_widget_destroy (p->stickykeys_alert);

        p->slowkeys_shortcut_val = FALSE;
        p->stickykeys_shortcut_val = FALSE;
}

static void
csd_a11y_keyboard_manager_class_init (CsdA11yKeyboardManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_a11y_keyboard_manager_finalize;
}

static void
on_preferences_dialog_response (CtkDialog              *dialog,
                                int                     response,
                                CsdA11yKeyboardManager *manager)
{
        g_signal_handlers_disconnect_by_func (dialog,
                                              on_preferences_dialog_response,
                                              manager);

        ctk_widget_destroy (CTK_WIDGET (dialog));
        manager->priv->preferences_dialog = NULL;
}

static void
on_status_icon_activate (CtkStatusIcon          *status_icon,
                         CsdA11yKeyboardManager *manager)
{
        if (manager->priv->preferences_dialog == NULL) {
                manager->priv->preferences_dialog = csd_a11y_preferences_dialog_new ();
                g_signal_connect (manager->priv->preferences_dialog,
                                  "response",
                                  G_CALLBACK (on_preferences_dialog_response),
                                  manager);

                ctk_window_present (CTK_WINDOW (manager->priv->preferences_dialog));
        } else {
                g_signal_handlers_disconnect_by_func (manager->priv->preferences_dialog,
                                                      on_preferences_dialog_response,
                                                      manager);
                ctk_widget_destroy (CTK_WIDGET (manager->priv->preferences_dialog));
                manager->priv->preferences_dialog = NULL;
        }
}

static void
csd_a11y_keyboard_manager_ensure_status_icon (CsdA11yKeyboardManager *manager)
{
        cafe_settings_profile_start (NULL);

        if (!manager->priv->status_icon) {

                manager->priv->status_icon = ctk_status_icon_new_from_icon_name ("preferences-desktop-accessibility");
                g_signal_connect (manager->priv->status_icon,
                                  "activate",
                                  G_CALLBACK (on_status_icon_activate),
                                  manager);
        }

        cafe_settings_profile_end (NULL);
}

static void
csd_a11y_keyboard_manager_init (CsdA11yKeyboardManager *manager)
{
        manager->priv = csd_a11y_keyboard_manager_get_instance_private (manager);
}

static void
csd_a11y_keyboard_manager_finalize (GObject *object)
{
        CsdA11yKeyboardManager *a11y_keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_A11Y_KEYBOARD_MANAGER (object));

        a11y_keyboard_manager = CSD_A11Y_KEYBOARD_MANAGER (object);

        g_return_if_fail (a11y_keyboard_manager->priv != NULL);

        G_OBJECT_CLASS (csd_a11y_keyboard_manager_parent_class)->finalize (object);
}

CsdA11yKeyboardManager *
csd_a11y_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_A11Y_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_A11Y_KEYBOARD_MANAGER (manager_object);
}

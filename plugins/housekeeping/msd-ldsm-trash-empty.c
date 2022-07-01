/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * msd-ldsm-trash-empty.c
 * Copyright (C) Chris Coulson 2009 <chrisccoulson@googlemail.com>
 *	     (C) Ryan Lortie 2008
 *
 * msd-ldsm-trash-empty.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * msd-ldsm-trash-empty.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "msd-ldsm-trash-empty.h"

#define CAJA_PREFS_SCHEMA "org.cafe.caja.preferences"
#define CAJA_CONFIRM_TRASH_KEY "confirm-trash"

/* Some of this code has been borrowed from the trash-applet, courtesy of Ryan Lortie */

static CtkWidget *trash_empty_confirm_dialog = NULL;
static CtkWidget *trash_empty_dialog = NULL;
static CtkWidget *location_label;
static CtkWidget *file_label;
static CtkWidget *progressbar;

static gsize trash_empty_total_files;
static gboolean trash_empty_update_pending = FALSE;
static GFile *trash_empty_current_file = NULL;
static gsize trash_empty_deleted_files;
static GTimer *timer = NULL;
static gboolean trash_empty_actually_deleting;

static gboolean
trash_empty_done (gpointer data)
{
        ctk_widget_destroy (trash_empty_dialog);
        trash_empty_dialog = NULL;
        if (timer) {
                g_timer_destroy (timer);
                timer = NULL;
        }

        return FALSE;
}

static gboolean
trash_empty_update_dialog (gpointer user_data)
{
        gsize deleted, total;
        GFile *file;
        gboolean actually_deleting;

        g_assert (trash_empty_update_pending);

        deleted = trash_empty_deleted_files;
        total = trash_empty_total_files;
        file = trash_empty_current_file;
        actually_deleting = trash_empty_actually_deleting;

        /* maybe the done() got processed first. */
        if (!trash_empty_dialog)
                goto out;

        if (!actually_deleting) {
                /* If we havent finished counting yet, then pulse the progressbar every 100ms.
                 * This stops the user from thinking the dialog has frozen if there are
                 * a lot of files to delete. We don't pulse it every time we are called from the
                 * worker thread, otherwise it moves to fast and looks hideous
                 */
                if (timer) {
                        if (g_timer_elapsed (timer, NULL) > 0.1) {
                                ctk_progress_bar_pulse (CTK_PROGRESS_BAR (progressbar));
                                g_timer_start (timer);
                        }
                } else {
                        timer = g_timer_new ();
                        g_timer_start (timer);
                        ctk_progress_bar_pulse (CTK_PROGRESS_BAR (progressbar));
                }
        } else {
                gchar *text;
                gchar *tmp;
                gchar *markup;
                GFile *parent;

                text = g_strdup_printf (_("Removing item %lu of %lu"),
                                        deleted, total);
                ctk_progress_bar_set_text (CTK_PROGRESS_BAR (progressbar), text);

                g_free (text);

                if (deleted > total)
                        ctk_progress_bar_set_fraction (CTK_PROGRESS_BAR (progressbar), 1.0);
                else
                        ctk_progress_bar_set_fraction (CTK_PROGRESS_BAR (progressbar),
                                                       (gdouble) deleted / (gdouble) total);

                parent = g_file_get_parent (file);
                text = g_file_get_uri (parent);
                g_object_unref (parent);

                ctk_label_set_text (CTK_LABEL (location_label), text);
                g_free (text);

                tmp = g_file_get_basename (file);
                text = g_markup_printf_escaped (_("Removing: %s"), tmp);
                markup = g_strdup_printf ("<i>%s</i>", text);
                ctk_label_set_markup (CTK_LABEL (file_label), markup);
                g_free (markup);
                g_free (text);
                g_free (tmp);

                /* unhide the labels */
                ctk_widget_show_all (CTK_WIDGET (trash_empty_dialog));
        }

out:
        trash_empty_current_file = NULL;
        g_object_unref (file);

        trash_empty_update_pending = FALSE;

        return FALSE;
}

/* Worker thread begin */

static void
trash_empty_maybe_schedule_update (GIOSchedulerJob *job,
                                   GFile           *file,
                                   gsize            deleted,
                                   gboolean         actually_deleting)
{
        if (!trash_empty_update_pending) {
                g_assert (trash_empty_current_file == NULL);

                trash_empty_current_file = g_object_ref (file);
                trash_empty_deleted_files = deleted;
                trash_empty_actually_deleting = actually_deleting;

                trash_empty_update_pending = TRUE;
                g_io_scheduler_job_send_to_mainloop_async (job,
                                                           trash_empty_update_dialog,
                                                           NULL, NULL);
        }
}

static void
trash_empty_delete_contents (GIOSchedulerJob *job,
                             GCancellable *cancellable,
                             GFile *file,
                             gboolean actually_delete,
                             gsize *deleted)
{
        GFileEnumerator *enumerator;
        GFileInfo *info;
        GFile *child;

        if (g_cancellable_is_cancelled (cancellable))
                return;

        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                cancellable, NULL);

        if (enumerator) {
                while ((info = g_file_enumerator_next_file (enumerator,
                                                            cancellable, NULL)) != NULL) {
                        child = g_file_get_child (file, g_file_info_get_name (info));

                        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
                                trash_empty_delete_contents (job, cancellable, child,
                                                             actually_delete, deleted);

                        trash_empty_maybe_schedule_update (job, child, *deleted, actually_delete);
                        if (actually_delete)
                                g_file_delete (child, cancellable, NULL);

                        (*deleted)++;

                        g_object_unref (child);
                        g_object_unref (info);

                        if (g_cancellable_is_cancelled (cancellable))
                                break;
                }

                g_object_unref (enumerator);
        }
}

static gboolean
trash_empty_job (GIOSchedulerJob *job,
                 GCancellable *cancellable,
                 gpointer user_data)
{
        gsize deleted;
        GFile *trash;

        trash = g_file_new_for_uri ("trash:///");

        /* first do a dry run to count the number of files */
        deleted = 0;
        trash_empty_delete_contents (job, cancellable, trash, FALSE, &deleted);
        trash_empty_total_files = deleted;

        /* now do the real thing */
        deleted = 0;
        trash_empty_delete_contents (job, cancellable, trash, TRUE, &deleted);

        /* done */
        g_object_unref (trash);
        g_io_scheduler_job_send_to_mainloop_async (job,
                                                   trash_empty_done,
                                                   NULL, NULL);

        return FALSE;
}

/* Worker thread end */

static void
trash_empty_start ()
{
        CtkWidget *vbox1, *vbox2, *hbox;
        CtkWidget *label1, *label3;
        gchar *markup;
        GCancellable *cancellable;

        trash_empty_dialog = ctk_dialog_new ();
        ctk_window_set_default_size (CTK_WINDOW (trash_empty_dialog), 400, -1);
        ctk_window_set_icon_name (CTK_WINDOW (trash_empty_dialog), "user-trash");
        ctk_window_set_title (CTK_WINDOW (trash_empty_dialog),
                              _("Emptying the trash"));

        vbox1 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);
        vbox2 = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
        hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);

        label1 = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (label1), TRUE);
        ctk_label_set_xalign (CTK_LABEL (label1), 0.0);
        ctk_label_set_yalign (CTK_LABEL (label1), 0.5);

        label3 = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (label3), TRUE);
        ctk_label_set_xalign (CTK_LABEL (label3), 0.0);
        ctk_label_set_yalign (CTK_LABEL (label3), 0.5);
        ctk_widget_hide (label3);

        location_label = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (location_label), TRUE);
        ctk_label_set_xalign (CTK_LABEL (location_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (location_label), 0.5);

        file_label = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (file_label), TRUE);
        ctk_label_set_xalign (CTK_LABEL (file_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (file_label), 0.5);

        progressbar = ctk_progress_bar_new ();
        ctk_progress_bar_set_pulse_step (CTK_PROGRESS_BAR (progressbar), 0.1);
        ctk_progress_bar_set_text (CTK_PROGRESS_BAR (progressbar), _("Preparing to empty trash…"));

        ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (trash_empty_dialog))), vbox1, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (vbox1), label1, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (hbox), label3, FALSE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (hbox), location_label, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (vbox1), hbox, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (vbox2), progressbar, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (vbox2), file_label, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

        ctk_widget_show (label1);
        ctk_widget_show (vbox1);
        ctk_widget_show_all (vbox2);
        ctk_widget_show (hbox);
        ctk_widget_show (location_label);

        ctk_container_set_border_width (CTK_CONTAINER (ctk_dialog_get_content_area (CTK_DIALOG (trash_empty_dialog))), 6);
        ctk_container_set_border_width (CTK_CONTAINER (vbox1), 6);

        ctk_dialog_add_button (CTK_DIALOG (trash_empty_dialog),
                               CTK_STOCK_CANCEL,
                               CTK_RESPONSE_CANCEL);

        markup = g_markup_printf_escaped ("<big><b>%s</b></big>", _("Emptying the trash"));
        ctk_label_set_markup (CTK_LABEL (label1), markup);
        /* Translators: "Emptying trash from <device>" */
        ctk_label_set_text (CTK_LABEL (label3), _("From: "));

        cancellable = g_cancellable_new ();
        g_signal_connect_object (trash_empty_dialog, "response",
                                 G_CALLBACK (g_cancellable_cancel),
                                 cancellable, G_CONNECT_SWAPPED);
        g_io_scheduler_push_job (trash_empty_job, NULL, NULL, 0, cancellable);

        ctk_widget_show (trash_empty_dialog);

        g_free (markup);
        g_object_unref (cancellable);
}

static void
trash_empty_confirmation_response (CtkDialog *dialog,
                                   gint       response_id,
                                   gpointer   user_data)
{
        if (response_id == CTK_RESPONSE_YES)
                trash_empty_start ();

        ctk_widget_destroy (CTK_WIDGET (dialog));
        trash_empty_confirm_dialog = NULL;
}

static gboolean
trash_empty_require_confirmation (void)
{
        GSettings *settings;
        gboolean require_confirmation = TRUE;

        settings = g_settings_new (CAJA_PREFS_SCHEMA);
        require_confirmation = g_settings_get_boolean (settings, CAJA_CONFIRM_TRASH_KEY);
        g_object_unref (settings);

        return require_confirmation;
}

static void
trash_empty_show_confirmation_dialog ()
{
        CtkWidget *button;

        if (!trash_empty_require_confirmation ()) {
                trash_empty_start ();
                return;
        }

        trash_empty_confirm_dialog = ctk_message_dialog_new (NULL, 0,
                                                             CTK_MESSAGE_WARNING,
                                                             CTK_BUTTONS_NONE,
                                                             _("Empty all of the items from the trash?"));

        ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (trash_empty_confirm_dialog),
                                                  _("If you choose to empty the trash, all items in "
                                                  "it will be permanently lost. Please note that "
                                                  "you can also delete them separately."));

        ctk_dialog_add_button (CTK_DIALOG (trash_empty_confirm_dialog), CTK_STOCK_CANCEL,
                               CTK_RESPONSE_CANCEL);

        button = ctk_button_new_with_mnemonic (_("_Empty Trash"));
        ctk_widget_show (button);
        ctk_widget_set_can_default (button, TRUE);

        ctk_dialog_add_action_widget (CTK_DIALOG (trash_empty_confirm_dialog),
                                      button, CTK_RESPONSE_YES);

        ctk_dialog_set_default_response (CTK_DIALOG (trash_empty_confirm_dialog),
                                         CTK_RESPONSE_YES);

        ctk_window_set_icon_name (CTK_WINDOW (trash_empty_confirm_dialog),
                                  "user-trash");

        ctk_widget_show (trash_empty_confirm_dialog);

        g_signal_connect (trash_empty_confirm_dialog, "response",
                          G_CALLBACK (trash_empty_confirmation_response), NULL);
}

void
msd_ldsm_trash_empty (void)
{
        if (trash_empty_confirm_dialog)
                ctk_window_present (CTK_WINDOW (trash_empty_confirm_dialog));
        else if (trash_empty_dialog)
                ctk_window_present (CTK_WINDOW (trash_empty_dialog));
        else
                trash_empty_show_confirmation_dialog ();
}

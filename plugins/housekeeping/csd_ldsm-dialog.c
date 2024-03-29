/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * csd_ldsm-dialog.c
 * Copyright (C) Chris Coulson 2009 <chrisccoulson@googlemail.com>
 *
 * csd_ldsm-dialog.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * csd_ldsm-dialog.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "csd_ldsm-dialog.h"

#define SETTINGS_SCHEMA "org.cafe.SettingsDaemon.plugins.housekeeping"
#define SETTINGS_IGNORE_PATHS "ignore-paths"

enum
{
        PROP_0,
        PROP_OTHER_USABLE_PARTITIONS,
        PROP_OTHER_PARTITIONS,
        PROP_HAS_TRASH,
        PROP_SPACE_REMAINING,
        PROP_PARTITION_NAME,
        PROP_MOUNT_PATH
};

struct CsdLdsmDialogPrivate
{
        CtkWidget *primary_label;
        CtkWidget *secondary_label;
        CtkWidget *ignore_check_button;
        gboolean other_usable_partitions;
        gboolean other_partitions;
        gboolean has_trash;
        gint64 space_remaining;
        gchar *partition_name;
        gchar *mount_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (CsdLdsmDialog, csd_ldsm_dialog, CTK_TYPE_DIALOG);

static const gchar*
csd_ldsm_dialog_get_checkbutton_text (CsdLdsmDialog *dialog)
{
        g_return_val_if_fail (CSD_IS_LDSM_DIALOG (dialog), NULL);

        if (dialog->priv->other_partitions)
                return _("Don't show any warnings again for this file system");
        else
                return _("Don't show any warnings again");
}

static gchar*
csd_ldsm_dialog_get_primary_text (CsdLdsmDialog *dialog)
{
        gchar *primary_text, *free_space;

        g_return_val_if_fail (CSD_IS_LDSM_DIALOG (dialog), NULL);

        free_space = g_format_size (dialog->priv->space_remaining);

        if (dialog->priv->other_partitions) {
                primary_text = g_strdup_printf (_("The volume \"%s\" has only %s disk space remaining."),
                                                dialog->priv->partition_name, free_space);
        } else {
                primary_text = g_strdup_printf (_("This computer has only %s disk space remaining."),
                                                free_space);
        }

        g_free (free_space);

        return primary_text;
}

static const gchar*
csd_ldsm_dialog_get_secondary_text (CsdLdsmDialog *dialog)
{
        g_return_val_if_fail (CSD_IS_LDSM_DIALOG (dialog), NULL);

        if (dialog->priv->other_usable_partitions) {
                if (dialog->priv->has_trash) {
                        return _("You can free up disk space by emptying the Trash, removing " \
                                 "unused programs or files, or moving files to another disk or partition.");
                } else {
                        return _("You can free up disk space by removing unused programs or files, " \
                                 "or by moving files to another disk or partition.");
                }
        } else {
                if (dialog->priv->has_trash) {
                        return _("You can free up disk space by emptying the Trash, removing unused " \
                                 "programs or files, or moving files to an external disk.");
                } else {
                        return _("You can free up disk space by removing unused programs or files, " \
                                 "or by moving files to an external disk.");
                }
        }
}

static gint
ignore_path_compare (gconstpointer a,
                     gconstpointer b)
{
        return g_strcmp0 ((const gchar *)a, (const gchar *)b);
}

static gboolean
update_ignore_paths (GSList **ignore_paths,
                     const gchar *mount_path,
                     gboolean ignore)
{
        GSList *found;
        gchar *path_to_remove;

        found = g_slist_find_custom (*ignore_paths, mount_path, (GCompareFunc) ignore_path_compare);

        if (ignore && (found == NULL)) {
                *ignore_paths = g_slist_prepend (*ignore_paths, g_strdup (mount_path));
                return TRUE;
        }

        if (!ignore && (found != NULL)) {
                path_to_remove = found->data;
                *ignore_paths = g_slist_remove (*ignore_paths, path_to_remove);
                g_free (path_to_remove);
                return TRUE;
        }

        return FALSE;
}

static void
ignore_check_button_toggled_cb (CtkToggleButton *button,
                                gpointer user_data)
{
        CsdLdsmDialog *dialog = (CsdLdsmDialog *)user_data;
        GSettings *settings;
        gchar **settings_list;
        GSList *ignore_paths = NULL;
        gboolean ignore, updated;
        gint i;

        settings = g_settings_new (SETTINGS_SCHEMA);

        settings_list = g_settings_get_strv (settings, SETTINGS_IGNORE_PATHS);

        for (i = 0; i < g_strv_length (settings_list); i++) {
                ignore_paths = g_slist_prepend (ignore_paths, g_strdup (settings_list[i]));
        }
        g_strfreev (settings_list);

        if (i > 0)
                ignore_paths = g_slist_reverse (ignore_paths);

        ignore = ctk_toggle_button_get_active (button);
        updated = update_ignore_paths (&ignore_paths, dialog->priv->mount_path, ignore);

        if (updated) {
            GSList *l;
            GPtrArray *array = g_ptr_array_new ();

            for (l = ignore_paths; l != NULL; l = l->next)
                    g_ptr_array_add (array, l->data);

            g_ptr_array_add (array, NULL);

            if (!g_settings_set_strv (settings, SETTINGS_IGNORE_PATHS, (const gchar **) array->pdata)) {
                    g_warning ("Cannot change ignore preference - failed to commit changes");
            }

            g_ptr_array_free (array, FALSE);
        }

        g_slist_foreach (ignore_paths, (GFunc) g_free, NULL);
        g_slist_free (ignore_paths);
        g_object_unref (settings);
}

static void
csd_ldsm_dialog_init (CsdLdsmDialog *dialog)
{
        CtkWidget *main_vbox, *text_vbox, *hbox;
        CtkWidget *image;

        dialog->priv = csd_ldsm_dialog_get_instance_private (dialog);

        main_vbox = ctk_dialog_get_content_area (CTK_DIALOG (dialog));

        /* Set up all the window stuff here */
        ctk_window_set_title (CTK_WINDOW (dialog), _("Low Disk Space"));
        ctk_window_set_icon_name (CTK_WINDOW (dialog),
                                  "dialog-warning");
        ctk_window_set_resizable (CTK_WINDOW (dialog), FALSE);
        ctk_window_set_position (CTK_WINDOW (dialog), CTK_WIN_POS_CENTER);
        ctk_window_set_urgency_hint (CTK_WINDOW (dialog), TRUE);
        ctk_window_set_focus_on_map (CTK_WINDOW (dialog), FALSE);
        ctk_container_set_border_width (CTK_CONTAINER (dialog), 5);

        /* Create the image */
        image = ctk_image_new_from_icon_name ("dialog-warning", CTK_ICON_SIZE_DIALOG);
        ctk_widget_set_halign (image, CTK_ALIGN_START);
        ctk_widget_set_valign (image, CTK_ALIGN_END);

        /* Create the labels */
        dialog->priv->primary_label = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (dialog->priv->primary_label), TRUE);
        ctk_label_set_single_line_mode (CTK_LABEL (dialog->priv->primary_label), FALSE);
        ctk_label_set_max_width_chars (CTK_LABEL (dialog->priv->primary_label), 70);
        ctk_label_set_xalign (CTK_LABEL (dialog->priv->primary_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (dialog->priv->primary_label), 0.0);

        dialog->priv->secondary_label = ctk_label_new (NULL);
        ctk_label_set_line_wrap (CTK_LABEL (dialog->priv->secondary_label), TRUE);
        ctk_label_set_single_line_mode (CTK_LABEL (dialog->priv->secondary_label), FALSE);
        ctk_label_set_max_width_chars (CTK_LABEL (dialog->priv->secondary_label), 70);
        ctk_label_set_xalign (CTK_LABEL (dialog->priv->secondary_label), 0.0);
        ctk_label_set_yalign (CTK_LABEL (dialog->priv->secondary_label), 0.0);

        /* Create the check button to ignore future warnings */
        dialog->priv->ignore_check_button = ctk_check_button_new ();
        /* The button should be inactive if the dialog was just called.
         * I suppose it could be possible for the user to manually edit the GSettings key between
         * the mount being checked and the dialog appearing, but I don't think it matters
         * too much */
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (dialog->priv->ignore_check_button), FALSE);
        g_signal_connect (dialog->priv->ignore_check_button, "toggled",
                          G_CALLBACK (ignore_check_button_toggled_cb), dialog);

        /* Now set up the dialog's CtkBox's' */
        ctk_box_set_spacing (CTK_BOX (main_vbox), 14);

        hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 12);
        ctk_container_set_border_width (CTK_CONTAINER (hbox), 5);

        text_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 12);

        ctk_box_pack_start (CTK_BOX (text_vbox), dialog->priv->primary_label, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (text_vbox), dialog->priv->secondary_label, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (text_vbox), dialog->priv->ignore_check_button, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (hbox), image, FALSE, FALSE, 0);
        ctk_box_pack_start (CTK_BOX (hbox), text_vbox, TRUE, TRUE, 0);
        ctk_box_pack_start (CTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

        /* Set up the action area */
        ctk_box_set_spacing (CTK_BOX (ctk_dialog_get_action_area (CTK_DIALOG (dialog))), 6);
        ctk_container_set_border_width (CTK_CONTAINER (ctk_dialog_get_action_area (CTK_DIALOG (dialog))), 5);

        ctk_widget_show_all (hbox);
}

static void
csd_ldsm_dialog_finalize (GObject *object)
{
        CsdLdsmDialog *self;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_LDSM_DIALOG (object));

        self = CSD_LDSM_DIALOG (object);

        if (self->priv->partition_name)
                g_free (self->priv->partition_name);

        if (self->priv->mount_path)
                g_free (self->priv->mount_path);

        G_OBJECT_CLASS (csd_ldsm_dialog_parent_class)->finalize (object);
}

static void
csd_ldsm_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        CsdLdsmDialog *self;

        g_return_if_fail (CSD_IS_LDSM_DIALOG (object));

        self = CSD_LDSM_DIALOG (object);

        switch (prop_id)
        {
        case PROP_OTHER_USABLE_PARTITIONS:
                self->priv->other_usable_partitions = g_value_get_boolean (value);
                break;
        case PROP_OTHER_PARTITIONS:
                self->priv->other_partitions = g_value_get_boolean (value);
                break;
        case PROP_HAS_TRASH:
                self->priv->has_trash = g_value_get_boolean (value);
                break;
        case PROP_SPACE_REMAINING:
                self->priv->space_remaining = g_value_get_int64 (value);
                break;
        case PROP_PARTITION_NAME:
                self->priv->partition_name = g_value_dup_string (value);
                break;
        case PROP_MOUNT_PATH:
                self->priv->mount_path = g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
csd_ldsm_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
        CsdLdsmDialog *self;

        g_return_if_fail (CSD_IS_LDSM_DIALOG (object));

        self = CSD_LDSM_DIALOG (object);

        switch (prop_id)
        {
        case PROP_OTHER_USABLE_PARTITIONS:
                g_value_set_boolean (value, self->priv->other_usable_partitions);
                break;
        case PROP_OTHER_PARTITIONS:
                g_value_set_boolean (value, self->priv->other_partitions);
                break;
        case PROP_HAS_TRASH:
                g_value_set_boolean (value, self->priv->has_trash);
                break;
        case PROP_SPACE_REMAINING:
                g_value_set_int64 (value, self->priv->space_remaining);
                break;
        case PROP_PARTITION_NAME:
                g_value_set_string (value, self->priv->partition_name);
                break;
        case PROP_MOUNT_PATH:
                g_value_set_string (value, self->priv->mount_path);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
csd_ldsm_dialog_class_init (CsdLdsmDialogClass *klass)
{
        GObjectClass* object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_ldsm_dialog_finalize;
        object_class->set_property = csd_ldsm_dialog_set_property;
        object_class->get_property = csd_ldsm_dialog_get_property;

        g_object_class_install_property (object_class,
                                         PROP_OTHER_USABLE_PARTITIONS,
                                         g_param_spec_boolean ("other-usable-partitions",
                                                               "other-usable-partitions",
                                                               "Set to TRUE if there are other usable partitions on the system",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_OTHER_PARTITIONS,
                                         g_param_spec_boolean ("other-partitions",
                                                               "other-partitions",
                                                               "Set to TRUE if there are other partitions on the system",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_HAS_TRASH,
                                         g_param_spec_boolean ("has-trash",
                                                               "has-trash",
                                                               "Set to TRUE if the partition has files in it's trash folder that can be deleted",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_SPACE_REMAINING,
                                         g_param_spec_int64 ("space-remaining",
                                                             "space-remaining",
                                                             "Specify how much space is remaining in bytes",
                                                             G_MININT64, G_MAXINT64, 0,
                                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_PARTITION_NAME,
                                         g_param_spec_string ("partition-name",
                                                              "partition-name",
                                                              "Specify the name of the partition",
                                                              "Unknown",
                                                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_MOUNT_PATH,
                                         g_param_spec_string ("mount-path",
                                                              "mount-path",
                                                              "Specify the mount path for the partition",
                                                              "Unknown",
                                                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

CsdLdsmDialog*
csd_ldsm_dialog_new (gboolean     other_usable_partitions,
                     gboolean     other_partitions,
                     gboolean     display_baobab,
                     gboolean     display_empty_trash,
                     gint64       space_remaining,
                     const gchar *partition_name,
                     const gchar *mount_path)
{
        CsdLdsmDialog *dialog;
        CtkWidget *button_empty_trash, *button_ignore, *button_analyze;
        CtkWidget *empty_trash_image, *analyze_image, *ignore_image;
        gchar *primary_text, *primary_text_markup;
        const gchar *secondary_text, *checkbutton_text;

        dialog = CSD_LDSM_DIALOG (g_object_new (CSD_TYPE_LDSM_DIALOG,
                                                "other-usable-partitions", other_usable_partitions,
                                                "other-partitions", other_partitions,
                                                "has-trash", display_empty_trash,
                                                "space-remaining", space_remaining,
                                                "partition-name", partition_name,
                                                "mount-path", mount_path,
                                                NULL));

        /* Add some buttons */
        if (dialog->priv->has_trash) {
                button_empty_trash = ctk_dialog_add_button (CTK_DIALOG (dialog),
                                                            _("Empty Trash"),
                                                            CSD_LDSM_DIALOG_RESPONSE_EMPTY_TRASH);
                empty_trash_image = ctk_image_new_from_icon_name ("edit-clear", CTK_ICON_SIZE_BUTTON);
                ctk_button_set_image (CTK_BUTTON (button_empty_trash), empty_trash_image);
        }

        if (display_baobab) {
                button_analyze = ctk_dialog_add_button (CTK_DIALOG (dialog),
                                                        _("Examine…"),
                                                        CSD_LDSM_DIALOG_RESPONSE_ANALYZE);
                analyze_image = ctk_image_new_from_icon_name ("cafe-disk-usage-analyzer", CTK_ICON_SIZE_BUTTON);
                ctk_button_set_image (CTK_BUTTON (button_analyze), analyze_image);
        }

        button_ignore = ctk_dialog_add_button (CTK_DIALOG (dialog),
                                               _("Ignore"),
                                               CTK_RESPONSE_CANCEL);
        ignore_image = ctk_image_new_from_stock (CTK_STOCK_CANCEL, CTK_ICON_SIZE_BUTTON);
        ctk_button_set_image (CTK_BUTTON (button_ignore), ignore_image);

        ctk_widget_grab_default (button_ignore);

        /* Set the label text */
        primary_text = csd_ldsm_dialog_get_primary_text (dialog);
        primary_text_markup = g_markup_printf_escaped ("<big><b>%s</b></big>", primary_text);
        ctk_label_set_markup (CTK_LABEL (dialog->priv->primary_label), primary_text_markup);

        secondary_text = csd_ldsm_dialog_get_secondary_text (dialog);
        ctk_label_set_text (CTK_LABEL (dialog->priv->secondary_label), secondary_text);

        checkbutton_text = csd_ldsm_dialog_get_checkbutton_text (dialog);
        ctk_button_set_label (CTK_BUTTON (dialog->priv->ignore_check_button), checkbutton_text);

        g_free (primary_text);
        g_free (primary_text_markup);

        return dialog;
}

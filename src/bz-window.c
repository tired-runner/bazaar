/* bz-window.c
 *
 * Copyright 2025 Adam Masciola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "bz-backend.h"
#include "bz-background.h"
#include "bz-browse-widget.h"
#include "bz-entry-group.h"
#include "bz-flatpak-instance.h"
#include "bz-search-widget.h"
#include "bz-transaction-manager.h"
#include "bz-update-dialog.h"
#include "bz-window.h"

struct _BzWindow
{
  AdwApplicationWindow parent_instance;

  BzFlatpakInstance *flatpak;
  GListStore        *remote_groups;
  GHashTable        *id_to_entry_group_hash;
  GHashTable        *unique_id_to_entry_hash;
  GHashTable        *installed_unique_ids_set;

  GListStore   *bg_entries;
  BzEntryGroup *pending_group;

  /* Template widgets */
  AdwStatusPage        *status;
  BzBackground         *background;
  BzBrowseWidget       *browse;
  GtkButton            *refresh;
  GtkButton            *search;
  AdwToastOverlay      *toasts;
  AdwSpinner           *spinner;
  AdwBottomSheet       *sheet;
  BzTransactionManager *transaction_mgr;
};

G_DEFINE_FINAL_TYPE (BzWindow, bz_window, ADW_TYPE_APPLICATION_WINDOW)

static void
last_success_changed (BzTransactionManager *manager,
                      GParamSpec           *pspec,
                      BzWindow             *self);

static void
refresh_clicked (GtkButton *button,
                 BzWindow  *self);
static void
search_clicked (GtkButton *button,
                BzWindow  *self);

static void
gather_entries_progress (BzEntry  *entry,
                         BzWindow *self);

static DexFuture *
refresh_then (DexFuture *future,
              BzWindow  *self);
static DexFuture *
fetch_refs_then (DexFuture *future,
                 BzWindow  *self);
static DexFuture *
fetch_installs_then (DexFuture *future,
                     BzWindow  *self);
static DexFuture *
fetch_updates_then (DexFuture *future,
                    BzWindow  *self);
static DexFuture *
refresh_catch (DexFuture *future,
               BzWindow  *self);
static DexFuture *
refresh_finally (DexFuture *future,
                 BzWindow  *self);

static void
search_selected_changed (BzSearchWidget *search,
                         GParamSpec     *pspec,
                         BzWindow       *self);

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self);

static void
update_dialog_response (BzUpdateDialog *dialog,
                        const char     *response,
                        BzWindow       *self);

static void
refresh (BzWindow *self);

static void
browse (BzWindow *self);

static void
transact (BzWindow *self,
          BzEntry  *entry,
          gboolean  remove);

static void
try_transact (BzWindow     *self,
              BzEntryGroup *entry,
              gboolean      remove);

static void
update (BzWindow *self,
        BzEntry **updates,
        guint     n_updates);

static void
search (BzWindow   *self,
        const char *text);

static void
bz_window_dispose (GObject *object)
{
  BzWindow *self = BZ_WINDOW (object);

  g_clear_pointer (&self->id_to_entry_group_hash, g_hash_table_unref);
  g_clear_pointer (&self->unique_id_to_entry_hash, g_hash_table_unref);
  g_clear_pointer (&self->installed_unique_ids_set, g_hash_table_unref);
  g_clear_object (&self->remote_groups);
  g_clear_object (&self->flatpak);
  g_clear_object (&self->pending_group);
  g_clear_object (&self->bg_entries);

  G_OBJECT_CLASS (bz_window_parent_class)->dispose (object);
}

static void
bz_window_class_init (BzWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_window_dispose;

  g_type_ensure (BZ_TYPE_BACKGROUND);
  g_type_ensure (BZ_TYPE_TRANSACTION_MANAGER);
  g_type_ensure (BZ_TYPE_BROWSE_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-window.ui");
  gtk_widget_class_bind_template_child (widget_class, BzWindow, background);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, browse);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, spinner);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, status);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toasts);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, sheet);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, refresh);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transaction_mgr);
}

static void
bz_window_init (BzWindow *self)
{
  GtkEventController *motion_controller = NULL;

  self->remote_groups          = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->id_to_entry_group_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->unique_id_to_entry_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->installed_unique_ids_set = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, NULL);

  gtk_widget_init_template (GTK_WIDGET (self));
  // bz_browse_widget_set_model (self->browse, G_LIST_MODEL (self->remote));

  g_signal_connect (self->transaction_mgr, "notify::last-success",
                    G_CALLBACK (last_success_changed), self);

  g_signal_connect (self->refresh, "clicked", G_CALLBACK (refresh_clicked), self);
  g_signal_connect (self->search, "clicked", G_CALLBACK (search_clicked), self);

  motion_controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_limit (motion_controller, GTK_LIMIT_NONE);
  bz_background_set_motion_controller (
      self->background,
      GTK_EVENT_CONTROLLER_MOTION (motion_controller));
  gtk_widget_add_controller (GTK_WIDGET (self), motion_controller);

  refresh (self);
}

static void
last_success_changed (BzTransactionManager *manager,
                      GParamSpec           *pspec,
                      BzWindow             *self)
{
  BzTransaction *transaction = NULL;

  transaction = bz_transaction_manager_get_last_success (manager);
  if (transaction != NULL)
    {
      GListModel *installs   = NULL;
      GListModel *removals   = NULL;
      guint       n_installs = 0;
      guint       n_removals = 0;

      installs = bz_transaction_get_installs (transaction);
      removals = bz_transaction_get_removals (transaction);

      if (installs != NULL)
        n_installs = g_list_model_get_n_items (installs);
      if (removals != NULL)
        n_removals = g_list_model_get_n_items (removals);

      for (guint i = 0; i < n_installs; i++)
        {
          g_autoptr (BzEntry) entry = NULL;
          const char   *unique_id   = NULL;
          const char   *id          = NULL;
          BzEntryGroup *group       = NULL;

          entry     = g_list_model_get_item (installs, i);
          unique_id = bz_entry_get_unique_id (entry);
          g_hash_table_add (self->installed_unique_ids_set, g_strdup (unique_id));

          id    = bz_entry_get_id (entry);
          group = g_hash_table_lookup (self->id_to_entry_group_hash, id);

          if (group != NULL)
            {
              int installable = 0;
              int removable   = 0;

              g_object_get (
                  group,
                  "installable", &installable,
                  "removable", &removable,
                  NULL);
              g_object_set (
                  group,
                  "installable", installable - 1,
                  "removable", removable + 1,
                  NULL);
            }
        }

      for (guint i = 0; i < n_removals; i++)
        {
          g_autoptr (BzEntry) entry = NULL;
          const char   *unique_id   = NULL;
          const char   *id          = NULL;
          BzEntryGroup *group       = NULL;

          entry     = g_list_model_get_item (removals, i);
          unique_id = bz_entry_get_unique_id (entry);
          g_hash_table_remove (self->installed_unique_ids_set, unique_id);

          id    = bz_entry_get_id (entry);
          group = g_hash_table_lookup (self->id_to_entry_group_hash, id);

          if (group != NULL)
            {
              int installable = 0;
              int removable   = 0;

              g_object_get (
                  group,
                  "installable", &installable,
                  "removable", &removable,
                  NULL);
              g_object_set (
                  group,
                  "installable", installable + 1,
                  "removable", removable - 1,
                  NULL);
            }
        }
    }
}

static void
refresh_clicked (GtkButton *button,
                 BzWindow  *self)
{
  refresh (self);
}

static void
search_clicked (GtkButton *button,
                BzWindow  *self)
{
  search (self, NULL);
}

static void
gather_entries_progress (BzEntry  *entry,
                         BzWindow *self)
{

  const char   *id        = NULL;
  BzEntryGroup *group     = NULL;
  const char   *unique_id = NULL;

  id    = bz_entry_get_id (entry);
  group = g_hash_table_lookup (self->id_to_entry_group_hash, id);

  if (group == NULL)
    {
      group = bz_entry_group_new ();
      g_hash_table_replace (
          self->id_to_entry_group_hash,
          g_strdup (id),
          group);
      g_list_store_append (
          self->remote_groups, group);
    }
  bz_entry_group_add (group, entry, FALSE, FALSE, FALSE);

  unique_id = bz_entry_get_unique_id (entry);
  g_hash_table_replace (
      self->unique_id_to_entry_hash,
      g_strdup (unique_id),
      g_object_ref (entry));
}

static DexFuture *
refresh_then (DexFuture *future,
              BzWindow  *self)
{
  g_autoptr (GError) local_error    = NULL;
  const GValue   *value             = NULL;
  GtkApplication *application       = NULL;
  g_autoptr (GListModel) blocklists = NULL;
  DexFuture *ref_remote_future      = NULL;

  value         = dex_future_get_value (future, &local_error);
  self->flatpak = g_value_dup_object (value);
  bz_transaction_manager_set_backend (self->transaction_mgr, BZ_BACKEND (self->flatpak));

  gtk_widget_set_sensitive (GTK_WIDGET (self->search), self->remote_groups != NULL);

  application = gtk_window_get_application (GTK_WINDOW (self));
  g_object_get (application, "blocklists", &blocklists, NULL);

  ref_remote_future = bz_backend_retrieve_remote_entries_with_blocklists (
      BZ_BACKEND (self->flatpak),
      NULL,
      blocklists,
      (BzBackendGatherEntriesFunc) gather_entries_progress,
      g_object_ref (self), g_object_unref);
  ref_remote_future = dex_future_then (
      ref_remote_future, (DexFutureCallback) fetch_refs_then,
      g_object_ref (self), g_object_unref);
  ref_remote_future = dex_future_then (
      ref_remote_future, (DexFutureCallback) fetch_installs_then,
      g_object_ref (self), g_object_unref);
  ref_remote_future = dex_future_then (
      ref_remote_future, (DexFutureCallback) fetch_updates_then,
      g_object_ref (self), g_object_unref);

  return ref_remote_future;
}

static DexFuture *
fetch_refs_then (DexFuture *future,
                 BzWindow  *self)
{
  guint n_groups             = 0;
  g_autoptr (GHashTable) set = NULL;

  n_groups = g_list_model_get_n_items (G_LIST_MODEL (self->remote_groups));

  g_clear_object (&self->bg_entries);
  self->bg_entries = g_list_store_new (BZ_TYPE_ENTRY);
  set              = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (guint safe = 0, found = 0;
       safe < 1000 && found < MIN (20, n_groups);
       safe++)
    {
      guint i                        = 0;
      g_autoptr (BzEntryGroup) group = NULL;
      BzEntry *entry                 = NULL;

      i = g_random_int_range (0, n_groups);
      if (g_hash_table_contains (set, GUINT_TO_POINTER (i)))
        continue;

      group = g_list_model_get_item (G_LIST_MODEL (self->remote_groups), i);
      entry = bz_entry_group_get_ui_entry (group);

      if (bz_entry_get_icon_paintable (entry) == NULL)
        continue;
      if (!g_str_has_prefix (bz_entry_get_id (entry), "org.gnome."))
        continue;

      g_list_store_append (self->bg_entries, entry);
      g_hash_table_add (set, GUINT_TO_POINTER (i));
      found++;
    }

  bz_background_set_entries (self->background, G_LIST_MODEL (self->bg_entries));

  adw_toast_overlay_add_toast (
      self->toasts,
      adw_toast_new_format ("Discovered %d Apps", n_groups));

  return bz_backend_retrieve_install_ids (BZ_BACKEND (self->flatpak));
}

static DexFuture *
fetch_installs_then (DexFuture *future,
                     BzWindow  *self)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;
  guint         n_groups         = 0;

  g_clear_pointer (&self->installed_unique_ids_set, g_hash_table_unref);

  value                          = dex_future_get_value (future, NULL);
  self->installed_unique_ids_set = g_value_dup_boxed (value);

  n_groups = g_list_model_get_n_items (G_LIST_MODEL (self->remote_groups));

  for (guint i = 0; i < n_groups; i++)
    {
      g_autoptr (BzEntryGroup) group = NULL;
      GListModel *entries            = NULL;
      guint       n_entries          = 0;
      int         installable        = 0;
      int         removable          = 0;

      group     = g_list_model_get_item (G_LIST_MODEL (self->remote_groups), i);
      entries   = bz_entry_group_get_model (group);
      n_entries = g_list_model_get_n_items (entries);

      for (guint j = 0; j < n_entries; j++)
        {
          g_autoptr (BzEntry) entry = NULL;
          const char *unique_id     = NULL;

          entry     = g_list_model_get_item (entries, j);
          unique_id = bz_entry_get_unique_id (entry);

          if (g_hash_table_contains (
                  self->installed_unique_ids_set,
                  unique_id))
            removable++;
          else
            installable++;
        }

      g_object_set (
          group,
          "installable", installable,
          "removable", removable,
          NULL);
    }

  return bz_backend_retrieve_update_ids (BZ_BACKEND (self->flatpak));
}

static DexFuture *
fetch_updates_then (DexFuture *future,
                    BzWindow  *self)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;
  g_autoptr (GPtrArray) names    = NULL;

  value = dex_future_get_value (future, NULL);
  names = g_value_dup_boxed (value);

  if (names->len > 0)
    {
      g_autoptr (GListStore) updates = NULL;

      updates = g_list_store_new (BZ_TYPE_ENTRY);
      for (guint i = 0; i < names->len; i++)
        {
          const char *unique_id = NULL;
          BzEntry    *entry     = NULL;

          unique_id = g_ptr_array_index (names, i);
          entry     = g_hash_table_lookup (
              self->unique_id_to_entry_hash, unique_id);

          /* FIXME address all refs */
          if (entry != NULL)
            g_list_store_append (updates, entry);
        }

      if (g_list_model_get_n_items (G_LIST_MODEL (updates)) > 0)
        {
          AdwDialog *update_dialog = NULL;

          update_dialog = bz_update_dialog_new (G_LIST_MODEL (updates));
          g_signal_connect (update_dialog, "response", G_CALLBACK (update_dialog_response), self);

          adw_dialog_present (update_dialog, GTK_WIDGET (self));
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
refresh_catch (DexFuture *future,
               BzWindow  *self)
{
  g_autoptr (GError) local_error = NULL;

  dex_future_get_value (future, &local_error);
  adw_toast_overlay_add_toast (self->toasts, adw_toast_new_format ("Failed! %s", local_error->message));

  return dex_future_new_true ();
}

static DexFuture *
refresh_finally (DexFuture *future,
                 BzWindow  *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), TRUE);

  return dex_future_new_true ();
}

static void
search_selected_changed (BzSearchWidget *search,
                         GParamSpec     *pspec,
                         BzWindow       *self)
{
  gboolean      remove = FALSE;
  BzEntryGroup *group  = NULL;
  GtkWidget    *dialog = NULL;

  group = bz_search_widget_get_selected (search, &remove);
  if (group != NULL)
    try_transact (self, group, remove);

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (search), ADW_TYPE_DIALOG);
  if (dialog != NULL)
    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self)
{
  gboolean should_install = FALSE;
  gboolean should_remove  = FALSE;

  should_install = g_strcmp0 (response, "install") == 0;
  should_remove  = g_strcmp0 (response, "remove") == 0;

  if (self->pending_group != NULL &&
      (should_install || should_remove))
    {
      BzEntryGroup *group  = self->pending_group;
      GPtrArray    *checks = NULL;

      checks = g_object_get_data (G_OBJECT (alert), "checks");
      if (checks != NULL)
        {
          GListModel *entries       = NULL;
          guint       n_entries     = 0;
          g_autoptr (BzEntry) entry = NULL;

          entries   = bz_entry_group_get_model (group);
          n_entries = g_list_model_get_n_items (entries);

          for (guint i = 0; i < n_entries; i++)
            {
              GtkCheckButton *check = NULL;

              check = g_ptr_array_index (checks, i);
              if (check != NULL && gtk_check_button_get_active (check))
                {
                  entry = g_list_model_get_item (entries, i);
                  break;
                }
            }

          if (entry != NULL)
            transact (self, entry, should_remove);
        }
      else
        transact (self, bz_entry_group_get_ui_entry (group), should_remove);
    }
  else
    g_clear_object (&self->pending_group);
}

static void
update_dialog_response (BzUpdateDialog *dialog,
                        const char     *response,
                        BzWindow       *self)
{
  g_autoptr (GListModel) updates = NULL;

  updates = bz_update_dialog_was_accepted (dialog);

  if (updates != NULL)
    {
      guint                n_updates   = 0;
      g_autofree BzEntry **updates_buf = NULL;

      n_updates   = g_list_model_get_n_items (updates);
      updates_buf = g_malloc_n (n_updates, sizeof (*updates_buf));

      for (guint i = 0; i < n_updates; i++)
        updates_buf[i] = g_list_model_get_item (updates, i);

      update (self, updates_buf, n_updates);

      for (guint i = 0; i < n_updates; i++)
        g_object_unref (updates_buf[i]);
    }
}

void
bz_window_refresh (BzWindow *self)
{
  g_return_if_fail (BZ_IS_WINDOW (self));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->refresh)))
    refresh (self);
  else
    adw_toast_overlay_add_toast (
        self->toasts,
        adw_toast_new_format ("Can't refresh right now!"));
}

void
bz_window_browse (BzWindow *self)
{
  g_return_if_fail (BZ_IS_WINDOW (self));

  browse (self);
}

void
bz_window_search (BzWindow   *self,
                  const char *text)
{
  g_return_if_fail (BZ_IS_WINDOW (self));

  search (self, text);
}

static void
refresh (BzWindow *self)
{
  DexFuture *future = NULL;

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), FALSE);

  g_hash_table_remove_all (self->id_to_entry_group_hash);
  g_hash_table_remove_all (self->unique_id_to_entry_hash);
  g_list_store_remove_all (self->remote_groups);
  g_clear_object (&self->flatpak);

  future = bz_flatpak_instance_new ();
  future = dex_future_then (
      future, (DexFutureCallback) refresh_then,
      g_object_ref (self), g_object_unref);
  future = dex_future_catch (
      future, (DexFutureCallback) refresh_catch,
      g_object_ref (self), g_object_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) refresh_finally,
      g_object_ref (self), g_object_unref);
  dex_future_disown (future);
}

static void
browse (BzWindow *self)
{
  bz_background_set_entries (self->background, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), TRUE);
}

static void
transact (BzWindow *self,
          BzEntry  *entry,
          gboolean  remove)
{
  g_autoptr (BzTransaction) transaction = NULL;

  if (remove)
    transaction = bz_transaction_new_full (
        NULL, 0,
        NULL, 0,
        &entry, 1);
  else
    transaction = bz_transaction_new_full (
        &entry, 1,
        NULL, 0,
        NULL, 0);

  bz_transaction_manager_add (self->transaction_mgr, transaction);
  adw_bottom_sheet_set_open (self->sheet, TRUE);
}

static void
try_transact (BzWindow     *self,
              BzEntryGroup *group,
              gboolean      remove)
{
  BzEntry    *ui_entry  = NULL;
  AdwDialog  *alert     = NULL;
  GListModel *entries   = NULL;
  guint       n_entries = 0;

  g_clear_object (&self->pending_group);
  ui_entry = bz_entry_group_get_ui_entry (group);
  if (ui_entry == NULL)
    return;

  self->pending_group = g_object_ref (group);

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), "Confirm Transaction");

  if (remove)
    {
      adw_alert_dialog_format_body_markup (
          ADW_ALERT_DIALOG (alert),
          "You are about to remove the following Flatpak:\n\n"
          "<b>%s</b>\n"
          "<tt>%s</tt>\n\n"
          "Are you sure?",
          bz_entry_get_title (ui_entry),
          bz_entry_get_id (ui_entry));
      adw_alert_dialog_add_responses (
          ADW_ALERT_DIALOG (alert),
          "cancel", "Cancel",
          "remove", "Remove",
          NULL);
      adw_alert_dialog_set_response_appearance (
          ADW_ALERT_DIALOG (alert), "remove", ADW_RESPONSE_DESTRUCTIVE);
    }
  else
    {
      adw_alert_dialog_format_body_markup (
          ADW_ALERT_DIALOG (alert),
          "You are about to install the following Flatpak:\n\n"
          "<b>%s</b>\n"
          "<tt>%s</tt>\n\n"
          "Are you sure?",
          bz_entry_get_title (ui_entry),
          bz_entry_get_id (ui_entry));
      adw_alert_dialog_add_responses (
          ADW_ALERT_DIALOG (alert),
          "cancel", "Cancel",
          "install", "Install",
          NULL);
      adw_alert_dialog_set_response_appearance (
          ADW_ALERT_DIALOG (alert), "install", ADW_RESPONSE_SUGGESTED);
    }

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "cancel");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");

  entries   = bz_entry_group_get_model (group);
  n_entries = g_list_model_get_n_items (entries);

  if (n_entries > 0)
    {
      GPtrArray      *checks            = NULL;
      GtkCheckButton *first_valid_check = NULL;
      int             n_valid_checks    = FALSE;
      GtkWidget      *box               = NULL;

      checks = g_ptr_array_new ();
      box    = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      for (guint i = 0; i < n_entries; i++)
        {
          g_autoptr (BzEntry) variant   = NULL;
          const char      *unique_id    = NULL;
          gboolean         is_installed = FALSE;
          g_autofree char *label        = NULL;
          GtkWidget       *check        = NULL;
          GtkWidget       *check_label  = NULL;

          variant      = g_list_model_get_item (entries, i);
          unique_id    = bz_entry_get_unique_id (variant);
          is_installed = g_hash_table_contains (self->installed_unique_ids_set, unique_id);

          if ((!remove && is_installed) ||
              (remove && !is_installed))
            {
              g_ptr_array_add (checks, NULL);
              continue;
            }

          label = g_strdup_printf (
              "%s: %s",
              bz_entry_get_remote_repo_name (variant),
              bz_entry_get_unique_id (variant));

          check = gtk_check_button_new ();
          g_ptr_array_add (checks, check);

          gtk_widget_set_has_tooltip (check, TRUE);
          gtk_widget_set_tooltip_text (check, label);

          check_label = gtk_label_new (label);
          gtk_widget_add_css_class (check_label, "monospace");
          gtk_label_set_wrap (GTK_LABEL (check_label), TRUE);
          gtk_label_set_wrap_mode (GTK_LABEL (check_label), PANGO_WRAP_WORD_CHAR);
          gtk_check_button_set_child (GTK_CHECK_BUTTON (check), check_label);

          if (first_valid_check != NULL)
            gtk_check_button_set_group (GTK_CHECK_BUTTON (check), first_valid_check);
          else
            {
              gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
              first_valid_check = GTK_CHECK_BUTTON (check);
            }

          gtk_box_append (GTK_BOX (box), check);
          n_valid_checks++;
        }

      adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (alert), box);

      g_object_set_data_full (
          G_OBJECT (alert),
          "checks",
          checks,
          (GDestroyNotify) g_ptr_array_unref);

      if (n_valid_checks == 1)
        gtk_widget_set_sensitive (GTK_WIDGET (first_valid_check), FALSE);
    }

  g_signal_connect (alert, "response", G_CALLBACK (install_confirmation_response), self);
  adw_dialog_present (alert, GTK_WIDGET (self));
}

static void
update (BzWindow *self,
        BzEntry **updates,
        guint     n_updates)
{
  g_autoptr (BzTransaction) transaction = NULL;

  transaction = bz_transaction_new_full (
      NULL, 0,
      updates, n_updates,
      NULL, 0);
  bz_transaction_manager_add (self->transaction_mgr, transaction);
  adw_bottom_sheet_set_open (self->sheet, TRUE);
}

static void
search (BzWindow   *self,
        const char *initial)
{
  GtkWidget *search_widget = NULL;
  AdwDialog *dialog        = NULL;

  /* prevent stacking issue */
  if (adw_application_window_get_visible_dialog (
          ADW_APPLICATION_WINDOW (self)) != NULL)
    return;

  search_widget = bz_search_widget_new (
      G_LIST_MODEL (self->remote_groups),
      initial);
  dialog = adw_dialog_new ();

  g_signal_connect (search_widget, "notify::selected",
                    G_CALLBACK (search_selected_changed), self);

  adw_dialog_set_child (dialog, search_widget);
  adw_dialog_set_content_width (dialog, 1500);
  adw_dialog_set_content_height (dialog, 1200);

  adw_dialog_present (dialog, GTK_WIDGET (self));
}

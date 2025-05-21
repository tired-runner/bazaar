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
#include "bz-flatpak-instance.h"
#include "bz-search-widget.h"
#include "bz-transaction-manager.h"
#include "bz-update-dialog.h"
#include "bz-window.h"

struct _BzWindow
{
  AdwApplicationWindow parent_instance;

  BzFlatpakInstance *flatpak;
  GListStore        *remote_entries;
  GHashTable        *id_to_entry_hash;
  GHashTable        *unique_id_to_entry_hash;

  GListStore *bg_entries;
  BzEntry    *pending_installation;

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
install (BzWindow *self,
         BzEntry  *entry);

static void
try_install (BzWindow *self,
             BzEntry  *entry);

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

  g_clear_pointer (&self->id_to_entry_hash, g_hash_table_unref);
  g_clear_pointer (&self->unique_id_to_entry_hash, g_hash_table_unref);
  g_clear_object (&self->remote_entries);
  g_clear_object (&self->flatpak);
  g_clear_object (&self->pending_installation);
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

  self->remote_entries   = g_list_store_new (BZ_TYPE_ENTRY);
  self->id_to_entry_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->unique_id_to_entry_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);

  gtk_widget_init_template (GTK_WIDGET (self));
  // bz_browse_widget_set_model (self->browse, G_LIST_MODEL (self->remote));

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

  const char *id        = NULL;
  BzEntry    *match     = NULL;
  const char *unique_id = NULL;

  id    = bz_entry_get_id (entry);
  match = g_hash_table_lookup (self->id_to_entry_hash, id);

  if (match != NULL)
    {
      GPtrArray *extra = NULL;

      extra = g_object_steal_data (G_OBJECT (match), "extra-matches");
      g_ptr_array_add (extra, g_object_ref (match));

      g_object_set_data_full (
          G_OBJECT (entry),
          "extra-matches",
          extra,
          (GDestroyNotify) g_ptr_array_unref);
    }
  else
    g_object_set_data_full (
        G_OBJECT (entry),
        "extra-matches",
        g_ptr_array_new_with_free_func (g_object_unref),
        (GDestroyNotify) g_ptr_array_unref);

  g_hash_table_replace (
      self->id_to_entry_hash,
      g_strdup (id),
      g_object_ref (entry));

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

  gtk_widget_set_sensitive (GTK_WIDGET (self->search), self->remote_entries != NULL);

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
      ref_remote_future, (DexFutureCallback) fetch_updates_then,
      g_object_ref (self), g_object_unref);

  return ref_remote_future;
}

static DexFuture *
fetch_refs_then (DexFuture *future,
                 BzWindow  *self)
{
  GHashTableIter iter        = { 0 };
  guint          n_entries   = 0;
  g_autoptr (GHashTable) set = NULL;

  /* TODO: lots of computation, perhaps put in fiber */
  g_hash_table_iter_init (&iter, self->id_to_entry_hash);
  for (;;)
    {
      const char *id    = NULL;
      BzEntry    *entry = NULL;
      GPtrArray  *extra = NULL;

      if (!g_hash_table_iter_next (
              &iter, (gpointer *) &id, (gpointer *) &entry))
        break;

      /* Don't really like this */
      extra = g_object_get_data (G_OBJECT (entry), "extra-matches");
      if (extra != NULL)
        {
          g_autoptr (GPtrArray) remote_names = NULL;
          GPtrArray  *search_tokens          = NULL;
          const char *remote_name            = NULL;

          remote_names  = g_ptr_array_new ();
          search_tokens = bz_entry_get_search_tokens (entry);

          remote_name = bz_entry_get_remote_repo_name (entry);
          if (remote_name != NULL)
            g_ptr_array_add (remote_names, (gpointer) remote_name);

          for (guint i = 0; i < extra->len; i++)
            {
              BzEntry    *extra_entry       = NULL;
              const char *extra_remote_name = NULL;

              extra_entry       = g_ptr_array_index (extra, i);
              extra_remote_name = bz_entry_get_remote_repo_name (extra_entry);

              if (extra_remote_name != NULL &&
                  !g_ptr_array_find_with_equal_func (
                      remote_names, extra_remote_name, g_str_equal, NULL))
                {
                  g_ptr_array_add (remote_names, (gpointer) extra_remote_name);
                  g_ptr_array_add (search_tokens, g_strdup (extra_remote_name));
                }
            }
          g_ptr_array_sort (remote_names, (GCompareFunc) g_strcmp0);

          if (remote_names->len > 0)
            {
              g_autoptr (GString) merged_remote_name = NULL;

              merged_remote_name = g_string_new (g_ptr_array_index (remote_names, 0));
              for (guint i = 1; i < remote_names->len; i++)
                g_string_append_printf (
                    merged_remote_name,
                    ", %s",
                    (const char *) g_ptr_array_index (remote_names, i));

              g_object_set (entry, "remote-repo-name", merged_remote_name->str, NULL);
            }
        }

      g_list_store_append (self->remote_entries, entry);
    }
  n_entries = g_list_model_get_n_items (G_LIST_MODEL (self->remote_entries));

  g_clear_object (&self->bg_entries);
  self->bg_entries = g_list_store_new (BZ_TYPE_ENTRY);
  set              = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (guint safe = 0, found = 0;
       safe < 1000 && found < MIN (20, n_entries);
       safe++)
    {
      guint i                   = 0;
      g_autoptr (BzEntry) entry = NULL;

      i = g_random_int_range (0, n_entries);
      if (g_hash_table_contains (set, GUINT_TO_POINTER (i)))
        continue;

      entry = g_list_model_get_item (G_LIST_MODEL (self->remote_entries), i);
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
      adw_toast_new_format ("Discovered %d Apps", n_entries));

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
  BzEntry   *entry  = NULL;
  GtkWidget *dialog = NULL;

  entry = bz_search_widget_get_selected (search);
  if (entry != NULL && BZ_IS_FLATPAK_ENTRY (entry))
    try_install (self, entry);

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (search), ADW_TYPE_DIALOG);
  if (dialog != NULL)
    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self)
{
  if (self->pending_installation != NULL &&
      g_strcmp0 (response, "install") == 0)
    {
      BzEntry   *entry  = self->pending_installation;
      GPtrArray *checks = NULL;

      checks = g_object_get_data (G_OBJECT (alert), "checks");
      if (checks != NULL)
        {
          GPtrArray *extra = NULL;

          extra = g_object_get_data (
              G_OBJECT (self->pending_installation),
              "extra-matches");

          for (guint i = 0; i < checks->len; i++)
            {
              GtkCheckButton *check = NULL;

              check = g_ptr_array_index (checks, i);
              if (gtk_check_button_get_active (check))
                {
                  if (i > 0)
                    entry = g_ptr_array_index (extra, i - 1);
                  break;
                }
            }
        }

      install (self, entry);
    }
  else
    g_clear_object (&self->pending_installation);
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

  g_hash_table_remove_all (self->id_to_entry_hash);
  g_hash_table_remove_all (self->unique_id_to_entry_hash);
  g_list_store_remove_all (self->remote_entries);
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
install (BzWindow *self,
         BzEntry  *entry)
{
  g_autoptr (BzTransaction) transaction = NULL;

  transaction = bz_transaction_new_full (
      &entry, 1,
      NULL, 0,
      NULL, 0);
  bz_transaction_manager_add (self->transaction_mgr, transaction);
  adw_bottom_sheet_set_open (self->sheet, TRUE);
}

static void
try_install (BzWindow *self,
             BzEntry  *entry)
{
  AdwDialog *alert = NULL;
  GPtrArray *extra = NULL;

  g_clear_object (&self->pending_installation);
  self->pending_installation = g_object_ref (entry);

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), "Confirm Transaction");
  adw_alert_dialog_format_body_markup (
      ADW_ALERT_DIALOG (alert),
      "You are about to install the following Flatpak:\n\n<b>%s</b>\n<tt>%s</tt>\n\nAre you sure?",
      bz_entry_get_title (entry),
      bz_entry_get_id (entry));
  adw_alert_dialog_add_responses (
      ADW_ALERT_DIALOG (alert),
      "cancel", "Cancel",
      "install", "Install",
      NULL);
  adw_alert_dialog_set_response_appearance (
      ADW_ALERT_DIALOG (alert), "install", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "cancel");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");

  /* TODO: revisit this, use GtkBuilder */
  extra = g_object_get_data (G_OBJECT (entry), "extra-matches");
  if (extra != NULL && extra->len > 0)
    {
      GPtrArray *checks            = NULL;
      GtkWidget *box               = NULL;
      GtkWidget *first_check       = NULL;
      GtkWidget *first_check_label = NULL;

      checks = g_ptr_array_new ();
      box    = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

      first_check = gtk_check_button_new ();
      g_ptr_array_add (checks, first_check);
      first_check_label = gtk_label_new (bz_entry_get_unique_id (entry));
      gtk_widget_add_css_class (first_check_label, "monospace");

      gtk_widget_set_has_tooltip (first_check_label, TRUE);
      gtk_widget_set_tooltip_text (first_check_label, bz_entry_get_unique_id (entry));
      gtk_label_set_ellipsize (GTK_LABEL (first_check_label), PANGO_ELLIPSIZE_MIDDLE);
      gtk_check_button_set_child (GTK_CHECK_BUTTON (first_check), first_check_label);

      for (guint i = 0; i < extra->len; i++)
        {
          BzEntry   *variant     = NULL;
          GtkWidget *check       = NULL;
          GtkWidget *check_label = NULL;

          variant = g_ptr_array_index (extra, i);

          check = gtk_check_button_new_with_label (
              bz_entry_get_unique_id (variant));
          g_ptr_array_add (checks, check);
          check_label = gtk_label_new (bz_entry_get_unique_id (variant));
          gtk_widget_add_css_class (check_label, "monospace");

          gtk_widget_set_has_tooltip (check_label, TRUE);
          gtk_widget_set_tooltip_text (check_label, bz_entry_get_unique_id (variant));
          gtk_label_set_ellipsize (GTK_LABEL (check_label), PANGO_ELLIPSIZE_MIDDLE);
          gtk_check_button_set_child (GTK_CHECK_BUTTON (check), check_label);

          gtk_check_button_set_group (
              GTK_CHECK_BUTTON (check),
              GTK_CHECK_BUTTON (first_check));

          gtk_box_append (GTK_BOX (box), check);
        }

      gtk_check_button_set_active (GTK_CHECK_BUTTON (first_check), TRUE);
      gtk_box_prepend (GTK_BOX (box), first_check);

      adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (alert), box);

      g_object_set_data_full (
          G_OBJECT (alert),
          "checks",
          checks,
          (GDestroyNotify) g_ptr_array_unref);
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
      G_LIST_MODEL (self->remote_entries),
      initial);
  dialog = adw_dialog_new ();

  g_signal_connect (search_widget, "notify::selected",
                    G_CALLBACK (search_selected_changed), self);

  adw_dialog_set_child (dialog, search_widget);
  adw_dialog_set_content_width (dialog, 1500);
  adw_dialog_set_content_height (dialog, 1200);

  adw_dialog_present (dialog, GTK_WIDGET (self));
}

/* ga-window.c
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

#include "ga-background.h"
#include "ga-browse-widget.h"
#include "ga-flatpak-instance.h"
#include "ga-search-widget.h"
#include "ga-update-page.h"
#include "ga-window.h"

struct _GaWindow
{
  AdwApplicationWindow parent_instance;

  GaFlatpakInstance *flatpak;
  GListStore        *remote;
  GHashTable        *id_to_entry_hash;

  GListStore *bg_entries;
  GaEntry    *pending_installation;

  /* Template widgets */
  GaBackground    *background;
  GaBrowseWidget  *browse;
  GtkButton       *refresh;
  GtkButton       *search;
  AdwToastOverlay *toasts;
  AdwSpinner      *spinner;
  AdwStatusPage   *status;
  GtkLabel        *progress_label;
  GtkProgressBar  *progress_bar;
  AdwSpinner      *progress_spinner;
};

G_DEFINE_FINAL_TYPE (GaWindow, ga_window, ADW_TYPE_APPLICATION_WINDOW)

static void
refresh_clicked (GtkButton *button,
                 GaWindow  *self);
static void
search_clicked (GtkButton *button,
                GaWindow  *self);

static void
gather_entries_progress (GaEntry  *entry,
                         GaWindow *self);

static DexFuture *
refresh_then (DexFuture *future,
              GaWindow  *self);
static DexFuture *
fetch_refs_then (DexFuture *future,
                 GaWindow  *self);
static DexFuture *
fetch_updates_then (DexFuture *future,
                    GaWindow  *self);
static DexFuture *
refresh_catch (DexFuture *future,
               GaWindow  *self);
static DexFuture *
refresh_finally (DexFuture *future,
                 GaWindow  *self);

static void
install_progress (GaFlatpakEntry *entry,
                  const char     *status,
                  gboolean        is_estimating,
                  int             progress_num,
                  guint64         bytes_transferred,
                  guint64         start_time,
                  GaWindow       *self);
static DexFuture *
install_finally (DexFuture *future,
                 GaWindow  *self);

static void
search_selected_changed (GaSearchWidget *search,
                         GParamSpec     *pspec,
                         GaWindow       *self);

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               GaWindow       *self);

static void
install_success_toast_button_clicked (AdwToast *toast,
                                      GaWindow *self);

static void
install_error_toast_button_clicked (AdwToast *toast,
                                    GaWindow *window);

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GaWindow       *self);

static void
update_dialog_closed (AdwDialog *dialog,
                      GaWindow  *self);

static void
refresh (GaWindow *self);

static void
browse (GaWindow *self);

static void
install (GaWindow *self,
         GaEntry  *entry);

static void
try_install (GaWindow *self,
             GaEntry  *entry);

static void
update (GaWindow *self,
        GaEntry **updates,
        guint     n_updates);

static void
search (GaWindow *self);

static void
show_error (GaWindow *self,
            char     *error_text);

static void
ga_window_dispose (GObject *object)
{
  GaWindow *self = GA_WINDOW (object);

  g_clear_pointer (&self->id_to_entry_hash, g_hash_table_unref);
  g_clear_object (&self->remote);
  g_clear_object (&self->flatpak);
  g_clear_object (&self->pending_installation);
  g_clear_object (&self->bg_entries);

  G_OBJECT_CLASS (ga_window_parent_class)->dispose (object);
}

static void
ga_window_class_init (GaWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ga_window_dispose;

  g_type_ensure (GA_TYPE_BACKGROUND);
  g_type_ensure (GA_TYPE_BROWSE_WIDGET);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/ga-window.ui");
  gtk_widget_class_bind_template_child (widget_class, GaWindow, background);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, browse);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, spinner);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, status);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, toasts);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, refresh);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, search);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, progress_label);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, GaWindow, progress_spinner);
}

static void
ga_window_init (GaWindow *self)
{
  GtkEventController *motion_controller = NULL;

  self->remote = g_list_store_new (GA_TYPE_ENTRY);

  gtk_widget_init_template (GTK_WIDGET (self));
  // ga_browse_widget_set_model (self->browse, G_LIST_MODEL (self->remote));

  g_signal_connect (self->refresh, "clicked", G_CALLBACK (refresh_clicked), self);
  g_signal_connect (self->search, "clicked", G_CALLBACK (search_clicked), self);

  motion_controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_limit (motion_controller, GTK_LIMIT_NONE);
  ga_background_set_motion_controller (
      self->background,
      GTK_EVENT_CONTROLLER_MOTION (motion_controller));
  gtk_widget_add_controller (GTK_WIDGET (self), motion_controller);

  refresh (self);
}

static void
refresh_clicked (GtkButton *button,
                 GaWindow  *self)
{
  refresh (self);
}

static void
search_clicked (GtkButton *button,
                GaWindow  *self)
{
  search (self);
}

static void
gather_entries_progress (GaEntry  *entry,
                         GaWindow *self)
{
  g_list_store_append (self->remote, entry);
}

static DexFuture *
refresh_then (DexFuture *future,
              GaWindow  *self)
{
  g_autoptr (GError) local_error  = NULL;
  const GValue *value             = NULL;
  DexFuture    *ref_remote_future = NULL;

  value         = dex_future_get_value (future, &local_error);
  self->flatpak = g_value_dup_object (value);

  ref_remote_future = ga_flatpak_instance_ref_remote_apps (
      self->flatpak,
      (GaFlatpakGatherEntriesFunc) gather_entries_progress,
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
                 GaWindow  *self)
{
  guint n_entries            = 0;
  g_autoptr (GHashTable) set = NULL;

  n_entries = g_list_model_get_n_items (G_LIST_MODEL (self->remote));

  self->id_to_entry_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  for (guint i = 0; i < n_entries; i++)
    {
      g_autoptr (GaFlatpakEntry) entry = NULL;
      const char *name                 = NULL;

      entry = g_list_model_get_item (G_LIST_MODEL (self->remote), i);
      name  = ga_flatpak_entry_get_name (entry);

      g_hash_table_replace (
          self->id_to_entry_hash,
          g_strdup (name),
          g_steal_pointer (&entry));
    }

  g_clear_object (&self->bg_entries);
  self->bg_entries = g_list_store_new (GA_TYPE_ENTRY);
  set              = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (guint safe = 0, found = 0;
       safe < 1000 && found < MIN (20, n_entries);
       safe++)
    {
      guint i                   = 0;
      g_autoptr (GaEntry) entry = NULL;

      i = g_random_int_range (0, n_entries);
      if (g_hash_table_contains (set, GUINT_TO_POINTER (i)))
        continue;

      entry = g_list_model_get_item (G_LIST_MODEL (self->remote), i);
      if (ga_entry_get_icon_paintable (entry) == NULL)
        continue;
      if (!g_str_has_prefix (ga_flatpak_entry_get_name (GA_FLATPAK_ENTRY (entry)), "org.gnome."))
        continue;

      g_list_store_append (self->bg_entries, entry);
      g_hash_table_add (set, GUINT_TO_POINTER (i));
      found++;
    }

  ga_background_set_entries (self->background, G_LIST_MODEL (self->bg_entries));

  adw_toast_overlay_add_toast (
      self->toasts,
      adw_toast_new_format ("Discovered %d Apps", n_entries));

  return ga_flatpak_instance_ref_updates (self->flatpak);
}

static DexFuture *
fetch_updates_then (DexFuture *future,
                    GaWindow  *self)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;
  g_autoptr (GPtrArray) names    = NULL;

  value = dex_future_get_value (future, NULL);
  names = g_value_dup_boxed (value);

  if (names->len > 0)
    {
      g_autoptr (GListStore) updates = NULL;

      updates = g_list_store_new (GA_TYPE_ENTRY);
      for (guint i = 0; i < names->len; i++)
        {
          const char *name  = NULL;
          GaEntry    *entry = NULL;

          name  = g_ptr_array_index (names, i);
          entry = g_hash_table_lookup (self->id_to_entry_hash, name);

          /* FIXME address all refs */
          if (entry != NULL)
            g_list_store_append (updates, entry);
        }

      if (g_list_model_get_n_items (G_LIST_MODEL (updates)) > 0)
        {
          GtkWidget *update_page = NULL;
          AdwDialog *dialog      = NULL;

          update_page = ga_update_page_new (G_LIST_MODEL (updates));
          dialog      = adw_dialog_new ();

          g_signal_connect (dialog, "closed", G_CALLBACK (update_dialog_closed), self);

          adw_dialog_set_child (dialog, update_page);
          adw_dialog_set_content_width (dialog, 500);
          adw_dialog_set_content_height (dialog, 300);

          adw_dialog_present (dialog, GTK_WIDGET (self));
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
refresh_catch (DexFuture *future,
               GaWindow  *self)
{
  g_autoptr (GError) local_error = NULL;

  dex_future_get_value (future, &local_error);

  adw_toast_overlay_add_toast (self->toasts, adw_toast_new_format ("Failed! %s", local_error->message));
  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), TRUE);

  return dex_future_new_true ();
}

static DexFuture *
refresh_finally (DexFuture *future,
                 GaWindow  *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), self->remote != NULL);

  return dex_future_new_true ();
}

static void
install_progress (GaFlatpakEntry *entry,
                  const char     *status,
                  gboolean        is_estimating,
                  int             progress_num,
                  guint64         bytes_transferred,
                  guint64         start_time,
                  GaWindow       *self)
{
  gboolean         show_bar = FALSE;
  const char      *title    = NULL;
  g_autofree char *text     = NULL;

  show_bar = status != NULL && status[0] != '\0';
  gtk_widget_set_visible (GTK_WIDGET (self->progress_bar), show_bar);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_spinner), !show_bar);

  if (is_estimating)
    gtk_progress_bar_pulse (self->progress_bar);
  else
    gtk_progress_bar_set_fraction (self->progress_bar, (double) progress_num / 100.0);

  title = ga_entry_get_title (GA_ENTRY (entry));
  text  = g_strdup_printf ("Installing: %s (%s)", title, status);

  gtk_label_set_text (self->progress_label, text);
}

static DexFuture *
install_finally (DexFuture *future,
                 GaWindow  *self)
{
  if (self->pending_installation != NULL)
    {
      const char *entry_title        = NULL;
      g_autoptr (GError) local_error = NULL;
      const GValue *value            = NULL;

      entry_title = ga_entry_get_title (self->pending_installation);
      value       = dex_future_get_value (future, &local_error);

      if (value != NULL)
        {
          AdwToast *toast = NULL;

          toast = adw_toast_new_format ("Successfully installed %s", entry_title);
          if (GA_IS_FLATPAK_ENTRY (self->pending_installation))
            {
              adw_toast_set_button_label (toast, "Launch");

              g_object_set_data_full (
                  G_OBJECT (toast), "to-launch",
                  g_object_ref (self->pending_installation),
                  g_object_unref);
              g_signal_connect (
                  toast, "button-clicked",
                  G_CALLBACK (install_success_toast_button_clicked),
                  self);
            }

          adw_toast_overlay_add_toast (self->toasts, toast);
        }
      else
        {
          AdwToast *toast = NULL;

          toast = adw_toast_new_format ("Failed to install %s", entry_title);
          adw_toast_set_button_label (toast, "View Error");

          g_object_set_data_full (
              G_OBJECT (toast), "error-text",
              g_strdup (local_error->message), g_free);
          g_signal_connect (
              toast, "button-clicked",
              G_CALLBACK (install_error_toast_button_clicked),
              self);

          adw_toast_overlay_add_toast (self->toasts, toast);
        }
    }

  g_clear_object (&self->pending_installation);

  gtk_label_set_text (self->progress_label, NULL);
  gtk_progress_bar_set_fraction (self->progress_bar, 0);

  gtk_widget_set_visible (GTK_WIDGET (self->progress_label), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_bar), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_spinner), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), self->remote != NULL);

  return dex_future_new_true ();
}

static void
search_selected_changed (GaSearchWidget *search,
                         GParamSpec     *pspec,
                         GaWindow       *self)
{
  GaEntry   *entry  = NULL;
  GtkWidget *dialog = NULL;

  entry = ga_search_widget_get_selected (search);
  if (entry != NULL && GA_IS_FLATPAK_ENTRY (entry))
    try_install (self, entry);

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (search), ADW_TYPE_DIALOG);
  if (dialog != NULL)
    adw_dialog_close (ADW_DIALOG (dialog));
}

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               GaWindow       *self)
{
  if (self->pending_installation != NULL &&
      g_strcmp0 (response, "install") == 0)
    install (self, self->pending_installation);
  else
    g_clear_object (&self->pending_installation);
}

static void
install_success_toast_button_clicked (AdwToast *toast,
                                      GaWindow *self)
{
  g_autoptr (GaFlatpakEntry) entry = NULL;
  g_autoptr (GError) local_error   = NULL;
  gboolean result                  = FALSE;

  entry = g_object_steal_data (G_OBJECT (toast), "to-launch");
  g_assert (entry != NULL);

  result = ga_flatpak_entry_launch (entry, &local_error);
  if (!result)
    show_error (self, g_strdup (local_error->message));
}

static void
install_error_toast_button_clicked (AdwToast *toast,
                                    GaWindow *self)
{
  g_autofree char *error_text = NULL;

  error_text = g_object_steal_data (G_OBJECT (toast), "error-text");
  g_assert (error_text != NULL);

  show_error (self, error_text);
}

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GaWindow       *self)
{
  if (g_strcmp0 (response, "copy") == 0)
    {
      const char   *body = NULL;
      GdkClipboard *clipboard;

      body      = adw_alert_dialog_get_body (alert);
      clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

      gdk_clipboard_set_text (clipboard, body);

      adw_toast_overlay_add_toast (
          self->toasts,
          adw_toast_new_format ("Error copied to clipboard"));
    }
}

static void
update_dialog_closed (AdwDialog *dialog,
                      GaWindow  *self)
{
  GtkWidget *page                = NULL;
  g_autoptr (GListModel) updates = NULL;

  page    = adw_dialog_get_child (dialog);
  updates = ga_updated_page_was_accepted (GA_UPDATE_PAGE (page));

  if (updates != NULL)
    {
      guint                n_updates   = 0;
      g_autofree GaEntry **updates_buf = NULL;

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
ga_window_refresh (GaWindow *self)
{
  g_return_if_fail (GA_IS_WINDOW (self));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->refresh)))
    refresh (self);
  else
    adw_toast_overlay_add_toast (
        self->toasts,
        adw_toast_new_format ("Can't refresh right now!"));
}

void
ga_window_browse (GaWindow *self)
{
  g_return_if_fail (GA_IS_WINDOW (self));

  browse (self);
}

void
ga_window_search (GaWindow *self)
{
  g_return_if_fail (GA_IS_WINDOW (self));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->search)))
    search (self);
  else
    adw_toast_overlay_add_toast (
        self->toasts,
        adw_toast_new_format ("Can't search right now!"));
}

static void
refresh (GaWindow *self)
{
  DexFuture *future = NULL;

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), FALSE);

  g_clear_pointer (&self->id_to_entry_hash, g_hash_table_unref);
  g_list_store_remove_all (self->remote);
  g_clear_object (&self->flatpak);

  future = ga_flatpak_instance_new ();
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
browse (GaWindow *self)
{
  ga_background_set_entries (self->background, NULL);

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->browse), TRUE);
}

static void
install (GaWindow *self,
         GaEntry  *entry)
{
  DexFuture *future = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), FALSE);

  gtk_widget_set_visible (GTK_WIDGET (self->progress_label), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_bar), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_spinner), TRUE);

  future = ga_flatpak_instance_schedule_transaction (
      self->flatpak,
      (GaFlatpakEntry **) &entry,
      1,
      NULL,
      0,
      (GaFlatpakTransactionProgressFunc) install_progress,
      g_object_ref (self),
      g_object_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) install_finally,
      g_object_ref (self), g_object_unref);
  dex_future_disown (future);
}

static void
try_install (GaWindow *self,
             GaEntry  *entry)
{
  AdwDialog *alert = NULL;

  g_clear_object (&self->pending_installation);
  self->pending_installation = g_object_ref (entry);

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), "Confirm Transaction");
  adw_alert_dialog_format_body_markup (
      ADW_ALERT_DIALOG (alert),
      "You are about to install the following Flatpak:\n\n<b>%s</b>\n<tt>%s</tt>\n\nAre you sure?",
      ga_entry_get_title (entry),
      ga_flatpak_entry_get_name (GA_FLATPAK_ENTRY (entry)));
  adw_alert_dialog_add_responses (
      ADW_ALERT_DIALOG (alert),
      "cancel", "Cancel",
      "install", "Install",
      NULL);
  adw_alert_dialog_set_response_appearance (
      ADW_ALERT_DIALOG (alert), "install", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "cancel");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");

  g_signal_connect (alert, "response", G_CALLBACK (install_confirmation_response), self);
  adw_dialog_present (alert, GTK_WIDGET (self));
}

static void
update (GaWindow *self,
        GaEntry **updates,
        guint     n_updates)
{
  DexFuture *future = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (self->refresh), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->search), FALSE);

  gtk_widget_set_visible (GTK_WIDGET (self->progress_label), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_bar), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->progress_spinner), TRUE);

  future = ga_flatpak_instance_schedule_transaction (
      self->flatpak,
      NULL,
      0,
      (GaFlatpakEntry **) updates,
      n_updates,
      (GaFlatpakTransactionProgressFunc) install_progress,
      g_object_ref (self),
      g_object_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) install_finally,
      g_object_ref (self), g_object_unref);
  dex_future_disown (future);
}

static void
search (GaWindow *self)
{
  GtkWidget *search_widget = NULL;
  AdwDialog *dialog        = NULL;

  search_widget = ga_search_widget_new (G_LIST_MODEL (self->remote));
  dialog        = adw_dialog_new ();

  g_signal_connect (search_widget, "notify::selected",
                    G_CALLBACK (search_selected_changed), self);

  adw_dialog_set_child (dialog, search_widget);
  adw_dialog_set_content_width (dialog, 1500);
  adw_dialog_set_content_height (dialog, 1200);

  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
show_error (GaWindow *self,
            char     *error_text)
{
  AdwDialog *alert = NULL;

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), "An Error Occured");
  adw_alert_dialog_format_body (
      ADW_ALERT_DIALOG (alert),
      "%s", error_text);
  adw_alert_dialog_add_responses (
      ADW_ALERT_DIALOG (alert),
      "close", "Close",
      "copy", "Copy and Close",
      NULL);
  adw_alert_dialog_set_response_appearance (
      ADW_ALERT_DIALOG (alert), "copy", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "close");

  g_signal_connect (alert, "response", G_CALLBACK (error_alert_response), self);
  adw_dialog_present (alert, GTK_WIDGET (self));
}

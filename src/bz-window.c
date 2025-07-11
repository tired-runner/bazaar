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

// This file is an utter mess

#include "config.h"

#include <glib/gi18n.h>

#include "bz-browse-widget.h"
#include "bz-comet-overlay.h"
#include "bz-entry-group.h"
#include "bz-error.h"
#include "bz-flathub-page.h"
#include "bz-full-view.h"
#include "bz-global-progress.h"
#include "bz-installed-page.h"
#include "bz-progress-bar.h"
#include "bz-search-widget.h"
#include "bz-transaction-manager.h"
#include "bz-update-dialog.h"
#include "bz-window.h"

struct _BzWindow
{
  AdwApplicationWindow parent_instance;

  GSettings            *settings;
  BzContentProvider    *content_provider;
  BzFlathubState       *flathub;
  BzTransactionManager *transaction_manager;
  GListModel           *applications;
  GListModel           *installed;
  GListStore           *updates;
  gboolean              busy;
  double                busy_progress;
  gboolean              checking_updates;
  gboolean              online;

  GBinding *search_to_view_binding;
  gboolean  breakpoint_applied;
  guint     after_update_check_timeout;

  /* Template widgets */
  BzCometOverlay      *comet_overlay;
  AdwOverlaySplitView *split_view;
  AdwOverlaySplitView *search_split;
  AdwViewStack        *transactions_stack;
  AdwViewStack        *main_stack;
  BzFullView          *full_view;
  GtkToggleButton     *toggle_transactions;
  GtkButton           *go_back;
  GtkButton           *refresh;
  GtkButton           *search;
  BzSearchWidget      *search_widget;
  GtkButton           *update_button;
  GtkRevealer         *title_revealer;
  GtkRevealer         *up_to_date_revealer;
  AdwToggleGroup      *title_toggle_group;
  GtkToggleButton     *transactions_pause;
  GtkButton           *transactions_stop;
  GtkButton           *transactions_clear;
  AdwToastOverlay     *toasts;
  AdwToolbarView      *toolbar_view;
  AdwHeaderBar        *top_header_bar;
  AdwHeaderBar        *bottom_header_bar;
};

G_DEFINE_FINAL_TYPE (BzWindow, bz_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,

  PROP_SETTINGS,
  PROP_TRANSACTION_MANAGER,
  PROP_CONTENT_PROVIDER,
  PROP_FLATHUB,
  PROP_APPLICATIONS,
  PROP_INSTALLED,
  PROP_UPDATES,
  PROP_BUSY,
  PROP_BUSY_PROGRESS,
  PROP_CHECKING_UPDATES,
  PROP_ONLINE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void setting_changed (GSettings   *settings,
                             const gchar *pkey,
                             BzWindow    *self);

static void
transactions_active_changed (BzTransactionManager *manager,
                             GParamSpec           *pspec,
                             BzWindow             *self);

static void
has_transactions_changed (BzTransactionManager *manager,
                          GParamSpec           *pspec,
                          BzWindow             *self);

static void
updates_changed (GListModel *model,
                 guint       position,
                 guint       removed,
                 guint       added,
                 BzWindow   *self);

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self);

static void
update_dialog_response (BzUpdateDialog *dialog,
                        const char     *response,
                        BzWindow       *self);

static void
transact (BzWindow *self,
          BzEntry  *entry,
          gboolean  remove);

static void
try_transact (BzWindow     *self,
              BzEntry      *entry,
              BzEntryGroup *group,
              gboolean      remove);

static void
update (BzWindow *self,
        BzEntry **updates,
        guint     n_updates);

static void
search (BzWindow   *self,
        const char *text);

static void
check_transactions (BzWindow *self);

static void
set_page (BzWindow *self);

static void
set_bottom_bar (BzWindow *self);

static void
hide_up_to_date_notif (BzWindow *self);

static void
bz_window_dispose (GObject *object)
{
  BzWindow *self = BZ_WINDOW (object);

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_func (
        self->settings, setting_changed, self);
  if (self->transaction_manager != NULL)
    {
      g_signal_handlers_disconnect_by_func (
          self->transaction_manager, transactions_active_changed, self);
      g_signal_handlers_disconnect_by_func (
          self->transaction_manager, has_transactions_changed, self);
    }
  if (self->updates != NULL)
    g_signal_handlers_disconnect_by_func (
        self->updates, updates_changed, self);

  g_clear_object (&self->settings);
  g_clear_object (&self->content_provider);
  g_clear_object (&self->flathub);
  g_clear_object (&self->transaction_manager);
  g_clear_object (&self->applications);
  g_clear_object (&self->installed);
  g_clear_object (&self->updates);

  g_clear_object (&self->search_to_view_binding);
  g_clear_handle_id (&self->after_update_check_timeout, g_source_remove);

  G_OBJECT_CLASS (bz_window_parent_class)->dispose (object);
}

static void
bz_window_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BzWindow *self = BZ_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;
    case PROP_TRANSACTION_MANAGER:
      g_value_set_object (value, self->transaction_manager);
      break;
    case PROP_CONTENT_PROVIDER:
      g_value_set_object (value, self->content_provider);
      break;
    case PROP_FLATHUB:
      g_value_set_object (value, self->flathub);
      break;
    case PROP_APPLICATIONS:
      g_value_set_object (value, self->applications);
      break;
    case PROP_INSTALLED:
      g_value_set_object (value, self->installed);
      break;
    case PROP_UPDATES:
      g_value_set_object (value, self->updates);
      break;
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;
    case PROP_BUSY_PROGRESS:
      g_value_set_double (value, self->busy_progress);
      break;
    case PROP_CHECKING_UPDATES:
      g_value_set_boolean (value, self->checking_updates);
      break;
    case PROP_ONLINE:
      g_value_set_boolean (value, self->online);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_window_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BzWindow *self = BZ_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      if (self->settings != NULL)
        g_signal_handlers_disconnect_by_func (
            self->settings, setting_changed, self);
      g_clear_object (&self->settings);
      self->settings = g_value_dup_object (value);
      if (self->settings != NULL)
        {
          g_signal_connect (
              self->settings, "changed",
              G_CALLBACK (setting_changed), self);
          setting_changed (self->settings, "show-animated-background", self);
        }
      break;
    case PROP_TRANSACTION_MANAGER:
      if (self->transaction_manager != NULL)
        {
          g_signal_handlers_disconnect_by_func (
              self->transaction_manager, transactions_active_changed, self);
          g_signal_handlers_disconnect_by_func (
              self->transaction_manager, has_transactions_changed, self);
        }
      g_clear_object (&self->transaction_manager);
      self->transaction_manager = g_value_dup_object (value);
      if (self->transaction_manager != NULL)
        {
          g_signal_connect (
              self->transaction_manager, "notify::active",
              G_CALLBACK (transactions_active_changed), self);
          g_signal_connect (
              self->transaction_manager, "notify::has-transactions",
              G_CALLBACK (has_transactions_changed), self);
        }
      check_transactions (self);
      break;
    case PROP_CONTENT_PROVIDER:
      g_clear_object (&self->content_provider);
      self->content_provider = g_value_dup_object (value);
      if (!gtk_window_is_active (GTK_WINDOW (self)) &&
          (self->content_provider == NULL ||
           !bz_content_provider_get_has_inputs (self->content_provider)))
        adw_toggle_group_set_active_name (self->title_toggle_group, "flathub");
      break;
    case PROP_FLATHUB:
      g_clear_object (&self->flathub);
      self->flathub = g_value_dup_object (value);
      break;
    case PROP_APPLICATIONS:
      g_clear_object (&self->applications);
      self->applications = g_value_dup_object (value);
      break;
    case PROP_INSTALLED:
      g_clear_object (&self->installed);
      self->installed = g_value_dup_object (value);
      break;
    case PROP_UPDATES:
      if (self->updates != NULL)
        g_signal_handlers_disconnect_by_func (
            self->updates, updates_changed, self);
      g_clear_object (&self->updates);
      self->updates = g_value_dup_object (value);
      if (self->updates != NULL)
        {
          g_signal_connect (
              self->updates, "items-changed",
              G_CALLBACK (updates_changed), self);
          gtk_widget_set_visible (
              GTK_WIDGET (self->update_button),
              g_list_model_get_n_items (G_LIST_MODEL (self->updates)) > 0);
        }
      else
        gtk_widget_set_visible (GTK_WIDGET (self->update_button), FALSE);
      break;
    case PROP_BUSY:
      self->busy = g_value_get_boolean (value);
      set_page (self);
      break;
    case PROP_BUSY_PROGRESS:
      self->busy_progress = g_value_get_double (value);
      break;
    case PROP_CHECKING_UPDATES:
      {
        gboolean last_value = FALSE;

        g_clear_handle_id (&self->after_update_check_timeout, g_source_remove);
        last_value             = self->checking_updates;
        self->checking_updates = g_value_get_boolean (value);
        if (last_value && !self->checking_updates &&
            g_list_model_get_n_items (G_LIST_MODEL (self->updates)) == 0)
          {
            gtk_revealer_set_reveal_child (self->up_to_date_revealer, TRUE);
            self->after_update_check_timeout =
                g_timeout_add_once (4000 /* 5 seconds */,
                                    (GSourceOnceFunc) hide_up_to_date_notif, self);
          }
        set_bottom_bar (self);
      }
      break;
    case PROP_ONLINE:
      self->online = g_value_get_boolean (value);
      set_page (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
browser_group_selected_cb (BzWindow     *self,
                           BzEntryGroup *group,
                           gpointer      browser)
{
  bz_window_show_group (self, group);
}

static void
search_split_open_changed_cb (BzWindow            *self,
                              GParamSpec          *pspec,
                              AdwOverlaySplitView *view)
{
  gboolean show_sidebar = FALSE;

  g_clear_object (&self->search_to_view_binding);
  show_sidebar = adw_overlay_split_view_get_show_sidebar (view);

  if (show_sidebar)
    self->search_to_view_binding = g_object_bind_property (
        self->search_widget, "previewing",
        self->full_view, "entry-group",
        G_BINDING_SYNC_CREATE);

  set_page (self);
}

static void
search_widget_select_cb (BzWindow       *self,
                         BzEntryGroup   *group,
                         BzSearchWidget *search)
{
  int      installable = 0;
  int      removable   = 0;
  gboolean remove      = FALSE;

  g_object_get (
      group,
      "installable", &installable,
      "removable", &removable,
      NULL);

  remove = installable == 0 && removable > 0;
  try_transact (self, NULL, group, remove);
}

static void
full_view_install_cb (BzWindow   *self,
                      BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), FALSE);
}

static void
full_view_remove_cb (BzWindow   *self,
                     BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), TRUE);
}

static void
installed_page_install_cb (BzWindow   *self,
                           BzEntry    *entry,
                           BzFullView *view)
{
  try_transact (self, entry, NULL, FALSE);
}

static void
installed_page_remove_cb (BzWindow   *self,
                          BzEntry    *entry,
                          BzFullView *view)
{
  try_transact (self, entry, NULL, TRUE);
}

static void
page_toggled_cb (BzWindow       *self,
                 GParamSpec     *pspec,
                 AdwToggleGroup *toggles)
{
  set_page (self);
}

static void
breakpoint_apply_cb (BzWindow      *self,
                     AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = TRUE;

  adw_header_bar_set_title_widget (self->top_header_bar, NULL);
  adw_header_bar_set_title_widget (self->bottom_header_bar, NULL);

  adw_header_bar_set_title_widget (self->top_header_bar, GTK_WIDGET (self->up_to_date_revealer));
  adw_header_bar_set_title_widget (self->bottom_header_bar, GTK_WIDGET (self->title_revealer));

  set_bottom_bar (self);
}

static void
breakpoint_unapply_cb (BzWindow      *self,
                       AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = FALSE;

  adw_header_bar_set_title_widget (self->top_header_bar, NULL);
  adw_header_bar_set_title_widget (self->bottom_header_bar, NULL);

  adw_header_bar_set_title_widget (self->top_header_bar, GTK_WIDGET (self->title_revealer));
  adw_header_bar_set_title_widget (self->bottom_header_bar, GTK_WIDGET (self->up_to_date_revealer));

  set_bottom_bar (self);
}

static void
pause_transactions_cb (BzWindow        *self,
                       GtkToggleButton *toggle)
{
  gboolean paused = FALSE;

  paused = gtk_toggle_button_get_active (toggle);
  bz_transaction_manager_set_paused (self->transaction_manager, paused);
  check_transactions (self);
}

static void
stop_transactions_cb (BzWindow  *self,
                      GtkButton *button)
{
  bz_transaction_manager_set_paused (self->transaction_manager, TRUE);
  bz_transaction_manager_cancel_current (self->transaction_manager);
}

static void
go_back_cb (BzWindow  *self,
            GtkButton *button)
{
  set_page (self);
}

static void
refresh_cb (BzWindow  *self,
            GtkButton *button)
{
  gtk_widget_activate_action (GTK_WIDGET (self), "app.refresh", NULL);
}

static void
update_cb (BzWindow  *self,
           GtkButton *button)
{
  /* if the button is clickable, there have to be updates */
  bz_window_push_update_dialog (self, self->updates);
}

static void
transactions_clear_cb (BzWindow  *self,
                       GtkButton *button)
{
  bz_transaction_manager_clear_finished (self->transaction_manager);
}

static void
bz_window_class_init (BzWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_window_dispose;
  object_class->get_property = bz_window_get_property;
  object_class->set_property = bz_window_set_property;

  props[PROP_SETTINGS] =
      g_param_spec_object (
          "settings",
          NULL, NULL,
          G_TYPE_SETTINGS,
          G_PARAM_READWRITE);

  props[PROP_TRANSACTION_MANAGER] =
      g_param_spec_object (
          "transaction-manager",
          NULL, NULL,
          BZ_TYPE_TRANSACTION_MANAGER,
          G_PARAM_READWRITE);

  props[PROP_CONTENT_PROVIDER] =
      g_param_spec_object (
          "content-provider",
          NULL, NULL,
          BZ_TYPE_CONTENT_PROVIDER,
          G_PARAM_READWRITE);

  props[PROP_FLATHUB] =
      g_param_spec_object (
          "flathub",
          NULL, NULL,
          BZ_TYPE_FLATHUB_STATE,
          G_PARAM_READWRITE);

  props[PROP_APPLICATIONS] =
      g_param_spec_object (
          "applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_INSTALLED] =
      g_param_spec_object (
          "installed",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_UPDATES] =
      g_param_spec_object (
          "updates",
          NULL, NULL,
          G_TYPE_LIST_STORE,
          G_PARAM_READWRITE);

  props[PROP_BUSY] =
      g_param_spec_boolean (
          "busy",
          NULL, NULL,
          FALSE,
          G_PARAM_READWRITE);

  props[PROP_BUSY_PROGRESS] =
      g_param_spec_double (
          "busy-progress",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE);

  props[PROP_CHECKING_UPDATES] =
      g_param_spec_boolean (
          "checking_updates",
          NULL, NULL,
          FALSE,
          G_PARAM_READWRITE);

  props[PROP_ONLINE] =
      g_param_spec_boolean (
          "online",
          NULL, NULL,
          FALSE,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_COMET_OVERLAY);
  g_type_ensure (BZ_TYPE_SEARCH_WIDGET);
  g_type_ensure (BZ_TYPE_GLOBAL_PROGRESS);
  g_type_ensure (BZ_TYPE_PROGRESS_BAR);
  g_type_ensure (BZ_TYPE_BROWSE_WIDGET);
  g_type_ensure (BZ_TYPE_FULL_VIEW);
  g_type_ensure (BZ_TYPE_INSTALLED_PAGE);
  g_type_ensure (BZ_TYPE_FLATHUB_PAGE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-window.ui");
  gtk_widget_class_bind_template_child (widget_class, BzWindow, comet_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search_split);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, full_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toasts);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toggle_transactions);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, go_back);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, refresh);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search_widget);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, update_button);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, title_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, up_to_date_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, title_toggle_group);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_pause);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stop);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_clear);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toolbar_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, top_header_bar);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, bottom_header_bar);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, browser_group_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_widget_select_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_install_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_install_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, page_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_unapply_cb);
  gtk_widget_class_bind_template_callback (widget_class, pause_transactions_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_transactions_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_split_open_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, go_back_cb);
  gtk_widget_class_bind_template_callback (widget_class, refresh_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_cb);
  gtk_widget_class_bind_template_callback (widget_class, transactions_clear_cb);
}

static void
bz_window_init (BzWindow *self)
{
  // const char *desktop = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  // desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  // if (desktop != NULL)
  //   {
  //     if (g_strcmp0 (desktop, "GNOME") == 0)
  //       gtk_widget_set_visible (GTK_WIDGET (self->support_gnome), TRUE);
  //     else if (g_strcmp0 (desktop, "KDE") == 0)
  //       gtk_widget_set_visible (GTK_WIDGET (self->support_kde), TRUE);
  //   }

  adw_toggle_group_set_active_name (self->title_toggle_group, "curated");
}

static void
setting_changed (GSettings   *settings,
                 const gchar *pkey,
                 BzWindow    *self)
{
}

static void
transactions_active_changed (BzTransactionManager *manager,
                             GParamSpec           *pspec,
                             BzWindow             *self)
{
  check_transactions (self);
}

static void
has_transactions_changed (BzTransactionManager *manager,
                          GParamSpec           *pspec,
                          BzWindow             *self)
{
  check_transactions (self);
}

static void
updates_changed (GListModel *model,
                 guint       position,
                 guint       removed,
                 guint       added,
                 BzWindow   *self)
{
  gtk_widget_set_visible (
      GTK_WIDGET (self->update_button),
      g_list_model_get_n_items (model) > 0);
}

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self)
{
  gboolean should_install           = FALSE;
  gboolean should_remove            = FALSE;
  g_autoptr (BzEntry) cb_entry      = NULL;
  g_autoptr (BzEntryGroup) cb_group = NULL;

  should_install = g_strcmp0 (response, "install") == 0;
  should_remove  = g_strcmp0 (response, "remove") == 0;

  cb_entry = g_object_steal_data (G_OBJECT (alert), "entry");
  cb_group = g_object_steal_data (G_OBJECT (alert), "group");

  if (should_install || should_remove)
    {
      if (cb_group != NULL)
        {
          GPtrArray *checks = NULL;

          checks = g_object_get_data (G_OBJECT (alert), "checks");
          if (checks != NULL)
            {
              GListModel *entries       = NULL;
              guint       n_entries     = 0;
              g_autoptr (BzEntry) entry = NULL;

              entries   = bz_entry_group_get_model (cb_group);
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
            transact (self, bz_entry_group_get_ui_entry (cb_group), should_remove);
        }
      else if (cb_entry != NULL)
        transact (self, cb_entry, should_remove);
    }
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

      g_list_store_remove_all (G_LIST_STORE (updates));
    }
}

void
bz_window_search (BzWindow   *self,
                  const char *text)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  search (self, text);
}

void
bz_window_toggle_transactions (BzWindow *self)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  gtk_toggle_button_set_active (
      self->toggle_transactions,
      !gtk_toggle_button_get_active (
          self->toggle_transactions));
}

void
bz_window_push_update_dialog (BzWindow   *self,
                              GListStore *updates)
{
  AdwDialog *update_dialog = NULL;

  g_return_if_fail (BZ_IS_WINDOW (self));

  update_dialog = bz_update_dialog_new (G_LIST_MODEL (updates));
  adw_dialog_set_content_width (update_dialog, 500);
  g_signal_connect (update_dialog, "response", G_CALLBACK (update_dialog_response), self);

  adw_dialog_present (update_dialog, GTK_WIDGET (self));
}

void
bz_window_show_entry (BzWindow *self,
                      BzEntry  *entry)
{
  /* TODO: IMPLEMENT ME! */
  bz_show_error_for_widget (
      GTK_WIDGET (self),
      _ ("The ability to inspect and install local .flatpak bundle files is coming soon! "
         "In the meantime, try running\n\n"
         "flatpak install --bundle your-bundle.flatpak\n\n"
         "on the command line."));
}

void
bz_window_show_group (BzWindow     *self,
                      BzEntryGroup *group)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  g_return_if_fail (BZ_IS_ENTRY_GROUP (group));

  bz_full_view_set_entry_group (self->full_view, group);
  adw_view_stack_set_visible_child_name (self->main_stack, "view");
  gtk_widget_set_visible (GTK_WIDGET (self->go_back), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->search), FALSE);
  gtk_revealer_set_reveal_child (self->title_revealer, FALSE);
}

static void
transact (BzWindow *self,
          BzEntry  *entry,
          gboolean  remove)
{
  g_autoptr (BzTransaction) transaction = NULL;
  GdkPaintable *icon                    = NULL;

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

  bz_transaction_manager_add (self->transaction_manager, transaction);

  icon = bz_entry_get_icon_paintable (entry);
  if (icon != NULL)
    {
      g_autoptr (BzComet) comet = NULL;

      comet = g_object_new (
          BZ_TYPE_COMET,
          "from", self->main_stack,
          "to", self->toggle_transactions,
          "paintable", icon,
          NULL);
      bz_comet_overlay_spawn (self->comet_overlay, comet);
    }

  // adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
}

static void
try_transact (BzWindow     *self,
              BzEntry      *entry,
              BzEntryGroup *group,
              gboolean      remove)
{
  BzEntry   *ui_entry = NULL;
  AdwDialog *alert    = NULL;

  if (self->busy)
    {
      adw_toast_overlay_add_toast (
          self->toasts,
          adw_toast_new_format (_ ("Can't do that right now!")));
      return;
    }

  if (entry != NULL)
    ui_entry = entry;
  else if (group != NULL)
    ui_entry = bz_entry_group_get_ui_entry (group);
  if (ui_entry == NULL)
    return;

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), _ ("Confirm Action"));

  if (entry != NULL)
    g_object_set_data_full (
        G_OBJECT (alert),
        "entry", g_object_ref (entry),
        g_object_unref);

  if (group != NULL)
    g_object_set_data_full (
        G_OBJECT (alert),
        "group", g_object_ref (group),
        g_object_unref);

  if (remove)
    {
      adw_alert_dialog_format_body_markup (
          ADW_ALERT_DIALOG (alert),
          _ ("You are about to remove the following Flatpak:\n\n"
             "<b>%s</b>\n"
             "<tt>%s</tt>\n\n"
             "Are you sure?"),
          bz_entry_get_title (ui_entry),
          bz_entry_get_id (ui_entry));
      adw_alert_dialog_add_responses (
          ADW_ALERT_DIALOG (alert),
          "cancel", _ ("Cancel"),
          "remove", _ ("Remove"),
          NULL);
      adw_alert_dialog_set_response_appearance (
          ADW_ALERT_DIALOG (alert), "remove", ADW_RESPONSE_DESTRUCTIVE);
      adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "remove");
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");
    }
  else
    {
      adw_alert_dialog_format_body_markup (
          ADW_ALERT_DIALOG (alert),
          _ ("You are about to install the following Flatpak:\n\n"
             "<b>%s</b>\n"
             "<tt>%s</tt>\n\n"
             "Are you sure?"),
          bz_entry_get_title (ui_entry),
          bz_entry_get_id (ui_entry));
      adw_alert_dialog_add_responses (
          ADW_ALERT_DIALOG (alert),
          "cancel", _ ("Cancel"),
          "install", _ ("Install"),
          NULL);
      adw_alert_dialog_set_response_appearance (
          ADW_ALERT_DIALOG (alert), "install", ADW_RESPONSE_SUGGESTED);
      adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "install");
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");
    }

  if (group != NULL)
    {
      GListModel *entries   = NULL;
      guint       n_entries = 0;

      entries   = bz_entry_group_get_model (group);
      n_entries = g_list_model_get_n_items (entries);

      if (n_entries > 0)
        {
          GtkWidget      *box               = NULL;
          GtkWidget      *expander          = NULL;
          GPtrArray      *checks            = NULL;
          GtkCheckButton *first_valid_check = NULL;
          int             n_valid_checks    = FALSE;

          box      = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
          expander = gtk_expander_new (_ ("More details"));

          gtk_expander_set_child (GTK_EXPANDER (expander), box);
          adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (alert), expander);

          checks = g_ptr_array_new ();

          for (guint i = 0; i < n_entries; i++)
            {
              g_autoptr (BzEntry) variant   = NULL;
              gboolean         is_installed = FALSE;
              g_autofree char *label        = NULL;
              GtkWidget       *check        = NULL;
              GtkWidget       *check_label  = NULL;

              variant      = g_list_model_get_item (entries, i);
              is_installed = bz_entry_group_query_removable (group, variant);

              if (bz_entry_is_holding (variant) ||
                  (!remove && is_installed) ||
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

          g_object_set_data_full (
              G_OBJECT (alert),
              "checks",
              checks,
              (GDestroyNotify) g_ptr_array_unref);

          if (n_valid_checks == 1)
            gtk_widget_set_sensitive (GTK_WIDGET (first_valid_check), FALSE);
        }
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
  bz_transaction_manager_add (self->transaction_manager, transaction);
}

static void
search (BzWindow   *self,
        const char *initial)
{
  gboolean open_sidebar = FALSE;

  if (initial != NULL && *initial != '\0')
    {
      bz_search_widget_set_text (self->search_widget, initial);
      open_sidebar = TRUE;
    }
  else
    open_sidebar = !adw_overlay_split_view_get_show_sidebar (self->search_split);

  adw_overlay_split_view_set_show_sidebar (self->search_split, open_sidebar);
}

static void
check_transactions (BzWindow *self)
{
  gboolean has_transactions = FALSE;
  gboolean paused           = FALSE;
  gboolean active           = FALSE;

  if (self->transaction_manager != NULL)
    has_transactions = bz_transaction_manager_get_has_transactions (self->transaction_manager);
  adw_view_stack_set_visible_child_name (
      self->transactions_stack,
      has_transactions
          ? "content"
          : "empty");

  if (self->transaction_manager == NULL)
    return;

  paused = gtk_toggle_button_get_active (self->transactions_pause);
  active = bz_transaction_manager_get_active (self->transaction_manager);

  // ugh, this is so messy

  if (paused)
    {
      gtk_button_set_icon_name (GTK_BUTTON (self->transactions_pause), "media-playback-start-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Resume the execution of transactions"));
      gtk_widget_add_css_class (GTK_WIDGET (self->transactions_pause), "suggested-action");
    }
  else
    {
      gtk_button_set_icon_name (GTK_BUTTON (self->transactions_pause), "media-playback-pause-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Pause the execution of transactions"));
      gtk_widget_remove_css_class (GTK_WIDGET (self->transactions_pause), "suggested-action");
    }

  if (active)
    gtk_widget_add_css_class (GTK_WIDGET (self->transactions_stop), "destructive-action");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->transactions_stop), "destructive-action");
}

static void
set_page (BzWindow *self)
{
  const char *active_name   = NULL;
  gboolean    show_search   = FALSE;
  const char *visible_child = NULL;

  active_name = adw_toggle_group_get_active_name (self->title_toggle_group);
  show_search = adw_overlay_split_view_get_show_sidebar (self->search_split);

  if (self->busy)
    visible_child = "loading";
  else if (show_search)
    visible_child = "view";
  else if (g_strcmp0 (active_name, "installed") == 0)
    visible_child = "installed";
  else if (g_strcmp0 (active_name, "curated") == 0)
    visible_child = self->online ? "browse" : "offline";
  else if (g_strcmp0 (active_name, "flathub") == 0)
    visible_child = self->online ? "flathub" : "offline";

  adw_view_stack_set_visible_child_name (self->main_stack, visible_child);
  gtk_widget_set_sensitive (GTK_WIDGET (self->title_toggle_group), !self->busy);
  gtk_revealer_set_reveal_child (self->title_revealer, !show_search);
  set_bottom_bar (self);

  gtk_widget_set_visible (GTK_WIDGET (self->go_back), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->search), TRUE);

  if (show_search)
    gtk_widget_grab_focus (GTK_WIDGET (self->search_widget));
  else
    bz_full_view_set_entry_group (self->full_view, NULL);
}

static void
set_bottom_bar (BzWindow *self)
{
  gboolean show_search     = FALSE;
  gboolean show_bottom_bar = FALSE;

  show_search     = adw_overlay_split_view_get_show_sidebar (self->search_split);
  show_bottom_bar = (self->breakpoint_applied && !show_search) ||
                    (!self->breakpoint_applied && self->after_update_check_timeout > 0);

  adw_toolbar_view_set_reveal_bottom_bars (self->toolbar_view, show_bottom_bar);
}

static void
hide_up_to_date_notif (BzWindow *self)
{
  self->after_update_check_timeout = 0;
  gtk_revealer_set_reveal_child (self->up_to_date_revealer, FALSE);
  set_bottom_bar (self);
}

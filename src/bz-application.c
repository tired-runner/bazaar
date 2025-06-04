/* bz-application.c
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

#include <glib/gi18n.h>

#include "bz-application.h"
#include "bz-content-provider.h"
#include "bz-entry-group.h"
#include "bz-flatpak-instance.h"
#include "bz-preferences-dialog.h"
#include "bz-transaction-manager.h"
#include "bz-window.h"

struct _BzApplication
{
  AdwApplication parent_instance;

  GSettings  *settings;
  GListModel *blocklists;
  GListModel *content_configs;

  BzTransactionManager *transactions;
  BzContentProvider    *content_provider;

  gboolean running;
  gboolean busy;

  GtkCssProvider  *css;
  GtkMapListModel *content_configs_to_files;

  BzFlatpakInstance *flatpak;
  GListStore        *remote_groups;
  GHashTable        *id_to_entry_group_hash;
  GHashTable        *unique_id_to_entry_hash;
  GHashTable        *installed_unique_ids_set;
};

G_DEFINE_FINAL_TYPE (BzApplication, bz_application, ADW_TYPE_APPLICATION)

enum
{
  PROP_0,

  PROP_SETTINGS,
  PROP_BLOCKLISTS,
  PROP_CONTENT_CONFIGS,
  PROP_TRANSACTION_MANAGER,
  PROP_CONTENT_PROVIDER,
  PROP_BUSY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
gather_entries_progress (BzEntry       *entry,
                         BzApplication *self);

static void
last_success_changed (BzTransactionManager *manager,
                      GParamSpec           *pspec,
                      BzApplication        *self);

static DexFuture *
refresh_then (DexFuture     *future,
              BzApplication *self);
static DexFuture *
fetch_refs_then (DexFuture     *future,
                 BzApplication *self);
static DexFuture *
fetch_installs_then (DexFuture     *future,
                     BzApplication *self);
static DexFuture *
fetch_updates_then (DexFuture     *future,
                    BzApplication *self);
static DexFuture *
refresh_finally (DexFuture     *future,
                 BzApplication *self);

static void
refresh (BzApplication *self);

static void
bz_application_dispose (GObject *object)
{
  BzApplication *self = BZ_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->blocklists);
  g_clear_object (&self->content_configs);
  g_clear_object (&self->transactions);
  g_clear_object (&self->content_provider);
  g_clear_object (&self->content_configs_to_files);
  g_clear_object (&self->css);
  g_clear_pointer (&self->id_to_entry_group_hash, g_hash_table_unref);
  g_clear_pointer (&self->unique_id_to_entry_hash, g_hash_table_unref);
  g_clear_pointer (&self->installed_unique_ids_set, g_hash_table_unref);
  g_clear_object (&self->remote_groups);
  g_clear_object (&self->flatpak);

  G_OBJECT_CLASS (bz_application_parent_class)->dispose (object);
}

static void
bz_application_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BzApplication *self = BZ_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;
    case PROP_BLOCKLISTS:
      g_value_set_object (value, self->blocklists);
      break;
    case PROP_CONTENT_CONFIGS:
      g_value_set_object (value, self->content_configs);
      break;
    case PROP_TRANSACTION_MANAGER:
      g_value_set_object (value, self->transactions);
      break;
    case PROP_CONTENT_PROVIDER:
      g_value_set_object (value, self->content_provider);
      break;
    case PROP_BUSY:
      g_value_set_boolean (value, self->busy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_application_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BzApplication *self = BZ_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_clear_object (&self->settings);
      self->settings = g_value_dup_object (value);
      break;
    case PROP_BLOCKLISTS:
      // TODO: this shouldn't be writable
      g_clear_object (&self->blocklists);
      self->blocklists = g_value_dup_object (value);
      break;
    case PROP_CONTENT_CONFIGS:
      // TODO: this shouldn't be writable
      g_clear_object (&self->content_configs);
      self->content_configs = g_value_dup_object (value);
      gtk_map_list_model_set_model (
          self->content_configs_to_files, self->content_configs);
      break;
    case PROP_TRANSACTION_MANAGER:
    case PROP_CONTENT_PROVIDER:
    case PROP_BUSY:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_application_activate (GApplication *app)
{
}

static int
bz_application_command_line (GApplication            *app,
                             GApplicationCommandLine *cmdline)
{
  BzApplication *self                = BZ_APPLICATION (app);
  gint           argc                = 0;
  g_auto (GStrv) argv                = NULL;
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error           = NULL;

  argv = g_application_command_line_get_arguments (cmdline, &argc);
  if (argv == NULL || argc <= 1 || g_strcmp0 (argv[1], "--help") == 0)
    {
      if (self->running)
        g_application_command_line_printerr (
            cmdline,
            "The Bazaar service is running. The available commands are:\n\n"
            "  window|status|query|transact|quit\n\n"
            "Add \"--help\" to a command to get information specific to that command.\n");
      else
        g_application_command_line_printerr (
            cmdline,
            "The Bazzar service is not running.\n"
            "Invoke \"bazaar service\" to initialize the daemon.\n");
      return EXIT_FAILURE;
    }

  context = g_option_context_new ("- an app center for GNOME");
  g_option_context_set_help_enabled (context, FALSE);

  if (g_strcmp0 (argv[1], "service") == 0)
    {
      gboolean help                             = FALSE;
      gboolean is_running                       = FALSE;
      g_auto (GStrv) blocklists_strv            = NULL;
      g_autoptr (GtkStringList) blocklists      = NULL;
      g_auto (GStrv) content_configs_strv       = NULL;
      g_autoptr (GtkStringList) content_configs = NULL;

      GOptionEntry main_entries[] = {
        { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
        { "is-running", 0, 0, G_OPTION_ARG_NONE, &is_running, "Exit successfully if the Bazaar service is running" },
        { "extra-blocklist", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &blocklists_strv, "Add an extra blocklist to read from" },
        { "extra-content-config", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &content_configs_strv, "Add an extra yaml file with which to configure the app browser" },
        { NULL }
      };

      g_option_context_add_main_entries (context, main_entries, NULL);
      if (!g_option_context_parse (context, &argc, &argv, &error))
        {
          g_application_command_line_printerr (cmdline, "%s\n", error->message);
          return EXIT_FAILURE;
        }

      if (help)
        {
          g_autofree char *help_text = NULL;

          g_option_context_set_summary (context, "Options for command \"service\"");

          help_text = g_option_context_get_help (context, TRUE, NULL);
          g_application_command_line_printerr (cmdline, "%s\n", help_text);
          return EXIT_SUCCESS;
        }

      if (is_running)
        return self->running ? EXIT_SUCCESS : EXIT_FAILURE;

      if (self->running)
        {
          g_application_command_line_printerr (
              cmdline,
              "The Bazzar service is already running.\n"
              "Invoke \"bazaar --help\" to for available commands.\n");
          return EXIT_FAILURE;
        }

      g_application_hold (G_APPLICATION (self));
      self->running = TRUE;

      if (self->settings == NULL)
        {
          const char *app_id = NULL;

          app_id = g_application_get_application_id (G_APPLICATION (self));
          g_assert (app_id != NULL);
          self->settings = g_settings_new (app_id);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SETTINGS]);
        }

      if (self->css == NULL)
        {
          self->css = gtk_css_provider_new ();
          gtk_css_provider_load_from_resource (
              self->css, "/io/github/kolunmi/bazaar/gtk/styles.css");
          gtk_style_context_add_provider_for_display (
              gdk_display_get_default (),
              GTK_STYLE_PROVIDER (self->css),
              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

      blocklists = gtk_string_list_new (NULL);
#ifdef HARDCODED_BLOCKLIST
      gtk_string_list_append (blocklists, HARDCODED_BLOCKLIST);
#endif
      if (blocklists_strv != NULL)
        gtk_string_list_splice (
            blocklists,
            g_list_model_get_n_items (G_LIST_MODEL (blocklists)),
            0,
            (const char *const *) blocklists_strv);

      content_configs = gtk_string_list_new (NULL);
#ifdef HARDCODED_CONTENT_CONFIG
      gtk_string_list_append (content_configs, HARDCODED_CONTENT_CONFIG);
#endif
      if (content_configs_strv != NULL)
        gtk_string_list_splice (
            content_configs,
            g_list_model_get_n_items (G_LIST_MODEL (content_configs)),
            0,
            (const char *const *) content_configs_strv);

      g_object_set (
          self,
          "blocklists", blocklists,
          "content-configs", content_configs,
          NULL);

      refresh (self);
    }
  else if (!self->running)
    {
      g_application_command_line_printerr (
          cmdline,
          "The Bazzar service is not running.\n"
          "Invoke \"bazaar service\" to initialize the daemon.\n");
      return EXIT_FAILURE;
    }
  else if (g_strcmp0 (argv[1], "window") == 0)
    {
      gboolean         help        = FALSE;
      gboolean         search      = FALSE;
      g_autofree char *search_text = NULL;
      GtkWindow       *window      = NULL;

      GOptionEntry main_entries[] = {
        { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
        { "search", 0, 0, G_OPTION_ARG_NONE, &search, "Immediately open the search dialog upon startup" },
        { "search-text", 0, 0, G_OPTION_ARG_STRING, &search_text, "Specify the initial text used with --search" },
        { NULL }
      };

      g_option_context_add_main_entries (context, main_entries, NULL);
      if (!g_option_context_parse (context, &argc, &argv, &error))
        {
          g_application_command_line_printerr (cmdline, "%s\n", error->message);
          return EXIT_FAILURE;
        }

      if (help)
        {
          g_autofree char *help_text = NULL;

          g_option_context_set_summary (context, "Options for command \"window\"");

          help_text = g_option_context_get_help (context, TRUE, NULL);
          g_application_command_line_printerr (cmdline, "%s\n", help_text);
          return EXIT_SUCCESS;
        }

      window = g_object_new (
          BZ_TYPE_WINDOW,
          "application", app,
          "settings", self->settings,
          "transaction-manager", self->transactions,
          "content-provider", self->content_provider,
          "remote-groups", self->remote_groups,
          "busy", self->busy,
          NULL);
      g_object_bind_property (
          self, "busy",
          window, "busy",
          G_BINDING_SYNC_CREATE);

      gtk_window_present (window);

      if (search)
        bz_window_search (BZ_WINDOW (window), search_text);
    }
  else if (g_strcmp0 (argv[1], "status") == 0)
    {
    }
  else if (g_strcmp0 (argv[1], "query") == 0)
    {
    }
  else if (g_strcmp0 (argv[1], "transact") == 0)
    {
    }
  else if (g_strcmp0 (argv[1], "quit") == 0)
    g_application_quit (G_APPLICATION (self));
  else
    {
      g_application_command_line_printerr (
          cmdline,
          "Unrecognized command \"%s\"\n"
          "Invoke \"bazaar --help\" to for available commands.\n",
          argv[1]);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

static gboolean
bz_application_local_command_line (GApplication *application,
                                   gchar      ***arguments,
                                   int          *exit_status)
{
  return FALSE;
}

static void
bz_application_class_init (BzApplicationClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class    = G_APPLICATION_CLASS (klass);

  object_class->dispose      = bz_application_dispose;
  object_class->get_property = bz_application_get_property;
  object_class->set_property = bz_application_set_property;

  props[PROP_SETTINGS] =
      g_param_spec_object (
          "settings",
          NULL, NULL,
          G_TYPE_SETTINGS,
          G_PARAM_READWRITE);

  props[PROP_BLOCKLISTS] =
      g_param_spec_object (
          "blocklists",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_CONTENT_CONFIGS] =
      g_param_spec_object (
          "content-configs",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_TRANSACTION_MANAGER] =
      g_param_spec_object (
          "transaction-manager",
          NULL, NULL,
          BZ_TYPE_TRANSACTION_MANAGER,
          G_PARAM_READABLE);

  props[PROP_CONTENT_PROVIDER] =
      g_param_spec_object (
          "content-provider",
          NULL, NULL,
          BZ_TYPE_CONTENT_PROVIDER,
          G_PARAM_READABLE);

  props[PROP_BUSY] =
      g_param_spec_boolean (
          "busy",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  app_class->activate           = bz_application_activate;
  app_class->command_line       = bz_application_command_line;
  app_class->local_command_line = bz_application_local_command_line;
}

static void
bz_application_donate_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  g_app_info_launch_default_for_uri (
      DONATE_LINK, NULL, NULL);
}

static void
bz_application_toggle_transactions_action (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data)
{
  BzApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  bz_window_toggle_transactions (BZ_WINDOW (window));
}

static void
bz_application_refresh_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  refresh (self);
}

static void
bz_application_search_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  BzApplication *self         = user_data;
  GtkWindow     *window       = NULL;
  const char    *initial_text = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  if (parameter != NULL)
    initial_text = g_variant_get_string (parameter, NULL);

  bz_window_search (BZ_WINDOW (window), initial_text);
}

static void
bz_application_about_action (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  static const char *developers[] = { "Adam Masciola", NULL };
  BzApplication     *self         = user_data;
  GtkWindow         *window       = NULL;
  AdwDialog         *dialog       = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  dialog = adw_about_dialog_new ();
  g_object_set (
      dialog,
      "application-name", "Bazaar",
      "application-icon", "io.github.kolunmi.bazaar",
      "developer-name", "Adam Masciola",
      "translator-credits", _ ("translator-credits"),
      "version", PACKAGE_VERSION,
      "developers", developers,
      "copyright", "© 2025 Adam Masciola",
      "license-type", GTK_LICENSE_GPL_3_0,
      "website", "https://github.com/kolunmi/bazaar",
      "issue-url", "https://github.com/kolunmi/bazaar/issues",
      NULL);

  adw_dialog_present (dialog, GTK_WIDGET (window));
}

static void
bz_application_preferences_action (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  BzApplication *self        = user_data;
  GtkWindow     *window      = NULL;
  AdwDialog     *preferences = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window      = gtk_application_get_active_window (GTK_APPLICATION (self));
  preferences = bz_preferences_dialog_new (self->settings);

  adw_dialog_present (preferences, GTK_WIDGET (window));
}

static void
bz_application_quit_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  {                "quit",                bz_application_quit_action, NULL },
  {         "preferences",         bz_application_preferences_action, NULL },
  {               "about",               bz_application_about_action, NULL },
  {              "search",              bz_application_search_action,  "s" },
  {             "refresh",             bz_application_refresh_action, NULL },
  { "toggle-transactions", bz_application_toggle_transactions_action, NULL },
  {              "donate",              bz_application_donate_action, NULL },
};

static gpointer
map_strings_to_files (GtkStringObject *string,
                      gpointer         data)
{
  const char *path   = NULL;
  GFile      *result = NULL;

  path   = gtk_string_object_get_string (string);
  result = g_file_new_for_path (path);

  g_object_unref (string);
  return result;
}

static void
bz_application_init (BzApplication *self)
{
  self->running = FALSE;
  self->busy    = FALSE;

  self->transactions = bz_transaction_manager_new ();
  g_signal_connect (self->transactions, "notify::last-success",
                    G_CALLBACK (last_success_changed), self);

  self->remote_groups          = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->id_to_entry_group_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->unique_id_to_entry_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->installed_unique_ids_set = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, NULL);

  self->content_provider = bz_content_provider_new ();
  bz_content_provider_set_group_hash (
      self->content_provider, self->id_to_entry_group_hash);

  self->content_configs_to_files = gtk_map_list_model_new (
      NULL, (GtkMapListModelMapFunc) map_strings_to_files, NULL, NULL);
  bz_content_provider_set_input_files (
      self->content_provider, G_LIST_MODEL (self->content_configs_to_files));

  g_action_map_add_action_entries (
      G_ACTION_MAP (self),
      app_actions,
      G_N_ELEMENTS (app_actions),
      self);
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.quit",
      (const char *[]) { "<primary>q", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.search('')",
      (const char *[]) { "<primary>f", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.refresh",
      (const char *[]) { "<primary>r", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.toggle-transactions",
      (const char *[]) { "<primary>d", NULL });
}

static void
gather_entries_progress (BzEntry       *entry,
                         BzApplication *self)
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
  bz_entry_group_add (group, entry, TRUE, FALSE, FALSE);

  unique_id = bz_entry_get_unique_id (entry);
  g_hash_table_replace (
      self->unique_id_to_entry_hash,
      g_strdup (unique_id),
      g_object_ref (entry));
}

static void
last_success_changed (BzTransactionManager *manager,
                      GParamSpec           *pspec,
                      BzApplication        *self)
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
            bz_entry_group_install (group, entry);
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
            bz_entry_group_remove (group, entry);
        }
    }
}

static DexFuture *
refresh_then (DexFuture     *future,
              BzApplication *self)
{
  g_autoptr (GError) local_error  = NULL;
  const GValue *value             = NULL;
  DexFuture    *ref_remote_future = NULL;

  value         = dex_future_get_value (future, &local_error);
  self->flatpak = g_value_dup_object (value);
  bz_transaction_manager_set_backend (self->transactions, BZ_BACKEND (self->flatpak));

  ref_remote_future = bz_backend_retrieve_remote_entries_with_blocklists (
      BZ_BACKEND (self->flatpak),
      NULL,
      self->blocklists,
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
fetch_refs_then (DexFuture     *future,
                 BzApplication *self)
{
  return bz_backend_retrieve_install_ids (BZ_BACKEND (self->flatpak));
}

static DexFuture *
fetch_installs_then (DexFuture     *future,
                     BzApplication *self)
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
            bz_entry_group_install (group, entry);
        }
    }

  return bz_backend_retrieve_update_ids (BZ_BACKEND (self->flatpak));
}

static DexFuture *
fetch_updates_then (DexFuture     *future,
                    BzApplication *self)
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
          GtkWindow *window = NULL;

          window = gtk_application_get_active_window (GTK_APPLICATION (self));
          if (window != NULL)
            bz_window_push_update_dialog (
                BZ_WINDOW (window), G_LIST_MODEL (updates));
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
refresh_finally (DexFuture     *future,
                 BzApplication *self)
{
  bz_content_provider_unblock (self->content_provider);

  self->busy = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BUSY]);

  return dex_future_new_true ();
}

static void
refresh (BzApplication *self)
{
  DexFuture *future = NULL;

  if (self->busy)
    return;

  bz_content_provider_block (self->content_provider);

  self->busy = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BUSY]);

  g_hash_table_remove_all (self->id_to_entry_group_hash);
  g_hash_table_remove_all (self->unique_id_to_entry_hash);
  g_list_store_remove_all (self->remote_groups);
  g_clear_object (&self->flatpak);

  future = bz_flatpak_instance_new ();
  future = dex_future_then (
      future, (DexFutureCallback) refresh_then,
      g_object_ref (self), g_object_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) refresh_finally,
      g_object_ref (self), g_object_unref);
  dex_future_disown (future);
}

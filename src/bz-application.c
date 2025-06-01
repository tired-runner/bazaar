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
#include "bz-preferences-dialog.h"
#include "bz-window.h"

struct _BzApplication
{
  AdwApplication parent_instance;

  GSettings  *settings;
  GListModel *blocklists;
  GListModel *content_configs;

  GtkMapListModel *content_configs_to_files;

  gboolean initial_search;
  char    *initial_search_text;
};

G_DEFINE_FINAL_TYPE (BzApplication, bz_application, ADW_TYPE_APPLICATION)

enum
{
  PROP_0,

  PROP_SETTINGS,
  PROP_BLOCKLISTS,
  PROP_CONTENT_CONFIGS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_application_dispose (GObject *object)
{
  BzApplication *self = BZ_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->blocklists);
  g_clear_object (&self->content_configs);
  g_clear_object (&self->content_configs_to_files);
  g_clear_pointer (&self->initial_search_text, g_free);

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
      g_clear_object (&self->blocklists);
      self->blocklists = g_value_dup_object (value);
      break;
    case PROP_CONTENT_CONFIGS:
      g_clear_object (&self->content_configs);
      self->content_configs = g_value_dup_object (value);
      gtk_map_list_model_set_model (
          self->content_configs_to_files, self->content_configs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_application_activate (GApplication *app)
{
  BzApplication *self = BZ_APPLICATION (app);
  GtkWindow     *window;

  if (self->settings == NULL)
    {
      const char *app_id = NULL;

      app_id = g_application_get_application_id (G_APPLICATION (self));
      g_assert (app_id != NULL);
      self->settings = g_settings_new (app_id);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SETTINGS]);
    }

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    {
      g_autoptr (GtkCssProvider) css = NULL;

      css = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (
          css, "/io/github/kolunmi/bazaar/gtk/styles.css");
      gtk_style_context_add_provider_for_display (
          gdk_display_get_default (),
          GTK_STYLE_PROVIDER (css),
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

      window = g_object_new (
          BZ_TYPE_WINDOW,
          "application", app,
          "settings", self->settings,
          "content-configs", self->content_configs_to_files,
          NULL);
    }

  gtk_window_present (window);

  if (self->initial_search)
    bz_window_search (BZ_WINDOW (window), self->initial_search_text);
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

  g_object_class_install_properties (object_class, LAST_PROP, props);

  app_class->activate = bz_application_activate;
}

BzApplication *
bz_application_new (const char       *application_id,
                    GListModel       *blocklists,
                    GListModel       *content_configs,
                    GApplicationFlags flags,
                    gboolean          search,
                    const char       *search_text)
{
  BzApplication *self = NULL;

  g_return_val_if_fail (application_id != NULL, NULL);

  self = g_object_new (
      BZ_TYPE_APPLICATION,
      "application-id", application_id,
      "flags", flags,
      "resource-base-path", "/io/github/kolunmi/bazaar",
      "blocklists", blocklists,
      "content-configs", content_configs,
      NULL);

  self->initial_search = search;
  if (search && search_text != NULL)
    self->initial_search_text = g_strdup (search_text);

  return self;
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
  BzApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  bz_window_refresh (BZ_WINDOW (window));
}

static void
bz_application_browse_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  BzApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  bz_window_browse (BZ_WINDOW (window));
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
  {              "browse",              bz_application_browse_action, NULL },
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
  self->content_configs_to_files = gtk_map_list_model_new (
      NULL, (GtkMapListModelMapFunc) map_strings_to_files, NULL, NULL);

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
      "app.browse",
      (const char *[]) { "<primary>b", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.refresh",
      (const char *[]) { "<primary>r", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.toggle-transactions",
      (const char *[]) { "<primary>d", NULL });
}

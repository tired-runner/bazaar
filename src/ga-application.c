/* ga-application.c
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

#include "ga-application.h"
#include "ga-window.h"

struct _GaApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (GaApplication, ga_application, ADW_TYPE_APPLICATION)

GaApplication *
ga_application_new (const char       *application_id,
                    GApplicationFlags flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (
      GA_TYPE_APPLICATION,
      "application-id", application_id,
      "flags", flags,
      "resource-base-path", "/org/gnome/Example",
      NULL);
}

static void
ga_application_activate (GApplication *app)
{
  GtkWindow *window;

  g_assert (GA_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));

  if (window == NULL)
    {
      g_autoptr (GtkCssProvider) css = NULL;

      css = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (
          css, "/org/gnome/Example/gtk/styles.css");
      gtk_style_context_add_provider_for_display (
          gdk_display_get_default (),
          GTK_STYLE_PROVIDER (css),
          GTK_STYLE_PROVIDER_PRIORITY_USER);

      window = g_object_new (
          GA_TYPE_WINDOW,
          "application", app,
          NULL);
    }

  gtk_window_present (window);
}

static void
ga_application_class_init (GaApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->activate = ga_application_activate;
}

static void
ga_application_refresh_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  GaApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (GA_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  ga_window_refresh (GA_WINDOW (window));
}

static void
ga_application_search_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  GaApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (GA_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  ga_window_search (GA_WINDOW (window));
}

static void
ga_application_about_action (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  static const char *developers[] = { "Adam Masciola", NULL };
  GaApplication     *self         = user_data;
  GtkWindow         *window       = NULL;

  g_assert (GA_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  adw_show_about_dialog (
      GTK_WIDGET (window),
      "application-name", "gnome-apps-next",
      "application-icon", "org.gnome.Example",
      "developer-name", "Adam Masciola",
      "translator-credits", _ ("translator-credits"),
      "version", "0.1.0",
      "developers", developers,
      "copyright", "© 2025 Adam Masciola",
      NULL);
}

static void
ga_application_quit_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GaApplication *self = user_data;

  g_assert (GA_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  {    "quit",    ga_application_quit_action },
  {   "about",   ga_application_about_action },
  {  "search",  ga_application_search_action },
  { "refresh", ga_application_refresh_action },
};

static void
ga_application_init (GaApplication *self)
{
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
      "app.search",
      (const char *[]) { "<primary>f", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.refresh",
      (const char *[]) { "<primary>r", NULL });
}

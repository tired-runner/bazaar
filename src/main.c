/* main.c
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

int
main (int   argc,
      char *argv[])
{
  g_autoptr (GOptionContext) context   = NULL;
  g_autoptr (GError) error             = NULL;
  gboolean version                     = FALSE;
  g_auto (GStrv) blocklists_strv       = NULL;
  g_autoptr (GtkStringList) blocklists = NULL;
  gboolean         search              = FALSE;
  g_autofree char *search_text         = NULL;
  g_autoptr (BzApplication) app        = NULL;
  int ret;

  GOptionEntry main_entries[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show program version" },
    { "extra-blocklists", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &blocklists_strv, "A list of extra blocklists to read from" },
    { "search", 0, 0, G_OPTION_ARG_NONE, &search, "Immediately open the search dialog upon startup" },
    { "search-text", 0, 0, G_OPTION_ARG_STRING, &search_text, "Specify the initial text used with --search" },
    { NULL }
  };

  context = g_option_context_new ("- an app center for GNOME");
  g_option_context_add_main_entries (context, main_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }
  if (version)
    {
      g_printerr ("%s\n", PACKAGE_VERSION);
      return EXIT_SUCCESS;
    }

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

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

  app = bz_application_new (
      "io.github.kolunmi.bazaar",
      G_LIST_MODEL (blocklists),
      G_APPLICATION_DEFAULT_FLAGS,
      search,
      search_text);
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  return ret;
}

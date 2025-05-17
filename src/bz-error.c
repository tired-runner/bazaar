/* bz-error.c
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

#include <adwaita.h>

#include "bz-error.h"

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GtkWidget      *widget);

void
bz_show_error_for_widget (GtkWidget  *widget,
                          GtkWidget  *parent,
                          const char *text)
{
  AdwDialog *alert = NULL;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (parent == NULL || GTK_IS_WIDGET (parent));
  g_return_if_fail (text != NULL);

  if (parent == NULL)
    parent = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
  g_assert (parent != NULL);

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_format_heading (
      ADW_ALERT_DIALOG (alert), "An Error Occured");
  adw_alert_dialog_format_body (
      ADW_ALERT_DIALOG (alert),
      "%s", text);
  adw_alert_dialog_add_responses (
      ADW_ALERT_DIALOG (alert),
      "close", "Close",
      "copy", "Copy and Close",
      NULL);
  adw_alert_dialog_set_response_appearance (
      ADW_ALERT_DIALOG (alert), "copy", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "close");

  g_signal_connect (alert, "response", G_CALLBACK (error_alert_response), widget);
  adw_dialog_present (alert, GTK_WIDGET (widget));
}

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GtkWidget      *widget)
{
  if (g_strcmp0 (response, "copy") == 0)
    {
      const char   *body = NULL;
      GdkClipboard *clipboard;

      body      = adw_alert_dialog_get_body (alert);
      clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

      gdk_clipboard_set_text (clipboard, body);
    }
}

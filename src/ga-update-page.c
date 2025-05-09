/* ga-update-page.c
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

#include "ga-entry.h"
#include "ga-update-page.h"

struct _GaUpdatePage
{
  AdwBin parent_instance;

  GListModel *updates;
  gboolean    install_accepted;

  /* Template widgets */
  GtkListView        *list_view;
  GtkSingleSelection *selection_model;
  GtkButton          *install;
};

G_DEFINE_FINAL_TYPE (GaUpdatePage, ga_update_page, ADW_TYPE_BIN)

static void
install_clicked (GtkButton    *button,
                 GaUpdatePage *self);

static void
ga_update_page_dispose (GObject *object)
{
  GaUpdatePage *self = GA_UPDATE_PAGE (object);

  g_clear_object (&self->updates);

  G_OBJECT_CLASS (ga_update_page_parent_class)->dispose (object);
}

static void
ga_update_page_class_init (GaUpdatePageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ga_update_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/ga-update-page.ui");
  gtk_widget_class_bind_template_child (widget_class, GaUpdatePage, list_view);
  gtk_widget_class_bind_template_child (widget_class, GaUpdatePage, selection_model);
  gtk_widget_class_bind_template_child (widget_class, GaUpdatePage, install);
}

static void
ga_update_page_init (GaUpdatePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->install, "clicked", G_CALLBACK (install_clicked), self);
}

GtkWidget *
ga_update_page_new (GListModel *updates)
{
  GaUpdatePage *update_page = NULL;

  g_return_val_if_fail (G_IS_LIST_MODEL (updates), NULL);
  g_return_val_if_fail (g_list_model_get_item_type (updates) == GA_TYPE_ENTRY, NULL);

  update_page          = g_object_new (GA_TYPE_UPDATE_PAGE, NULL);
  update_page->updates = g_object_ref (updates);

  gtk_single_selection_set_model (update_page->selection_model, updates);

  return GTK_WIDGET (update_page);
}

GListModel *
ga_updated_page_was_accepted (GaUpdatePage *self)
{
  g_return_val_if_fail (GA_IS_UPDATE_PAGE (self), FALSE);

  return self->install_accepted
             ? g_object_ref (self->updates)
             : NULL;
}

static void
install_clicked (GtkButton    *button,
                 GaUpdatePage *self)
{
  self->install_accepted = TRUE;
}

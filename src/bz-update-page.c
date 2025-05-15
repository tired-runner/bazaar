/* bz-update-page.c
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

#include "bz-entry.h"
#include "bz-update-page.h"

struct _BzUpdatePage
{
  AdwBin parent_instance;

  GListModel *updates;
  gboolean    install_accepted;

  /* Template widgets */
  GtkListView        *list_view;
  GtkSingleSelection *selection_model;
  GtkButton          *install;
};

G_DEFINE_FINAL_TYPE (BzUpdatePage, bz_update_page, ADW_TYPE_BIN)

static void
install_clicked (GtkButton    *button,
                 BzUpdatePage *self);

static void
bz_update_page_dispose (GObject *object)
{
  BzUpdatePage *self = BZ_UPDATE_PAGE (object);

  g_clear_object (&self->updates);

  G_OBJECT_CLASS (bz_update_page_parent_class)->dispose (object);
}

static void
bz_update_page_class_init (BzUpdatePageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_update_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/bz-update-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzUpdatePage, list_view);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatePage, selection_model);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatePage, install);
}

static void
bz_update_page_init (BzUpdatePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->install, "clicked", G_CALLBACK (install_clicked), self);
}

GtkWidget *
bz_update_page_new (GListModel *updates)
{
  BzUpdatePage *update_page = NULL;

  g_return_val_if_fail (G_IS_LIST_MODEL (updates), NULL);
  g_return_val_if_fail (g_list_model_get_item_type (updates) == BZ_TYPE_ENTRY, NULL);

  update_page          = g_object_new (BZ_TYPE_UPDATE_PAGE, NULL);
  update_page->updates = g_object_ref (updates);

  gtk_single_selection_set_model (update_page->selection_model, updates);

  return GTK_WIDGET (update_page);
}

GListModel *
bz_updated_page_was_accepted (BzUpdatePage *self)
{
  g_return_val_if_fail (BZ_IS_UPDATE_PAGE (self), FALSE);

  return self->install_accepted
             ? g_object_ref (self->updates)
             : NULL;
}

static void
install_clicked (GtkButton    *button,
                 BzUpdatePage *self)
{
  self->install_accepted = TRUE;
}

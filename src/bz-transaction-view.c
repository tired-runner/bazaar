/* bz-transaction-view.c
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

#include "bz-transaction-view.h"

struct _BzTransactionView
{
  AdwBin parent_instance;

  BzTransaction *transaction;

  /* Template widgets */
  GtkWidget    *installs;
  GtkSeparator *separator_1;
  GtkWidget    *updates;
  GtkSeparator *separator_2;
  GtkWidget    *removals;
};

G_DEFINE_FINAL_TYPE (BzTransactionView, bz_transaction_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_TRANSACTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_transaction_view_dispose (GObject *object)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  g_clear_object (&self->transaction);

  G_OBJECT_CLASS (bz_transaction_view_parent_class)->dispose (object);
}

static void
bz_transaction_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      g_value_set_object (value, bz_transaction_view_get_transaction (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      bz_transaction_view_set_transaction (self, g_value_get_object (value));
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
bz_transaction_view_class_init (BzTransactionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_transaction_view_dispose;
  object_class->get_property = bz_transaction_view_get_property;
  object_class->set_property = bz_transaction_view_set_property;

  props[PROP_TRANSACTION] =
      g_param_spec_object (
          "transaction",
          NULL, NULL,
          BZ_TYPE_TRANSACTION,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-transaction-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzTransactionView, installs);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionView, separator_1);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionView, updates);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionView, separator_2);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionView, removals);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
}

static void
bz_transaction_view_init (BzTransactionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_transaction_view_new (BzTransaction *transaction)
{
  return g_object_new (
      BZ_TYPE_TRANSACTION_VIEW,
      "transaction", transaction,
      NULL);
}

void
bz_transaction_view_set_transaction (BzTransactionView *self,
                                     BzTransaction     *transaction)
{
  g_return_if_fail (BZ_IS_TRANSACTION_VIEW (self));
  g_return_if_fail (transaction == NULL || BZ_IS_TRANSACTION (transaction));

  g_clear_object (&self->transaction);
  if (transaction != NULL)
    {
      GListModel *installs       = NULL;
      GListModel *updates        = NULL;
      GListModel *removals       = NULL;
      gboolean    installs_valid = FALSE;
      gboolean    updates_valid  = FALSE;
      gboolean    removals_valid = FALSE;

      self->transaction = g_object_ref (transaction);

      installs = bz_transaction_get_installs (transaction);
      updates  = bz_transaction_get_updates (transaction);
      removals = bz_transaction_get_removals (transaction);

      installs_valid = installs != NULL && g_list_model_get_n_items (G_LIST_MODEL (installs)) > 0;
      updates_valid  = updates != NULL && g_list_model_get_n_items (G_LIST_MODEL (updates)) > 0;
      removals_valid = removals != NULL && g_list_model_get_n_items (G_LIST_MODEL (removals)) > 0;

      gtk_widget_set_visible (self->installs, installs_valid);
      gtk_widget_set_visible (GTK_WIDGET (self->separator_1), installs_valid && (updates_valid || removals_valid));
      gtk_widget_set_visible (self->updates, updates_valid);
      gtk_widget_set_visible (GTK_WIDGET (self->separator_2), updates_valid && removals_valid);
      gtk_widget_set_visible (self->removals, removals_valid);
    }
  else
    {
      gtk_widget_set_visible (self->installs, FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->separator_1), FALSE);
      gtk_widget_set_visible (self->updates, FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->separator_2), FALSE);
      gtk_widget_set_visible (self->removals, FALSE);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSACTION]);
}

BzTransaction *
bz_transaction_view_get_transaction (BzTransactionView *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_VIEW (self), NULL);
  return self->transaction;
}

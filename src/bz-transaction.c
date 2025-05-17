/* bz-transaction.c
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

#include "bz-transaction.h"

typedef struct
{
  GListModel *installs;
  GListModel *updates;
  GListModel *removals;

  gboolean pending;
  char    *status;
  double   progress;
  gboolean finished;
  gboolean success;
  char    *error;
} BzTransactionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BzTransaction, bz_transaction, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_INSTALLS,
  PROP_UPDATES,
  PROP_REMOVALS,

  PROP_PENDING,
  PROP_STATUS,
  PROP_PROGRESS,
  PROP_FINISHED,
  PROP_SUCCESS,
  PROP_ERROR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_transaction_dispose (GObject *object)
{
  BzTransaction        *self = BZ_TRANSACTION (object);
  BzTransactionPrivate *priv = bz_transaction_get_instance_private (self);

  g_clear_object (&priv->installs);
  g_clear_object (&priv->updates);
  g_clear_object (&priv->removals);

  g_clear_pointer (&priv->status, g_free);
  g_clear_pointer (&priv->error, g_free);

  G_OBJECT_CLASS (bz_transaction_parent_class)->dispose (object);
}

static void
bz_transaction_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BzTransaction        *self = BZ_TRANSACTION (object);
  BzTransactionPrivate *priv = bz_transaction_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INSTALLS:
      g_value_set_object (value, priv->installs);
      break;
    case PROP_UPDATES:
      g_value_set_object (value, priv->updates);
      break;
    case PROP_REMOVALS:
      g_value_set_object (value, priv->removals);
      break;
    case PROP_PENDING:
      g_value_set_boolean (value, priv->pending);
      break;
    case PROP_STATUS:
      g_value_set_string (value, priv->status);
      break;
    case PROP_PROGRESS:
      g_value_set_double (value, priv->progress);
      break;
    case PROP_FINISHED:
      g_value_set_boolean (value, priv->finished);
      break;
    case PROP_SUCCESS:
      g_value_set_boolean (value, priv->success);
      break;
    case PROP_ERROR:
      g_value_set_string (value, priv->error);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  BzTransaction        *self = BZ_TRANSACTION (object);
  BzTransactionPrivate *priv = bz_transaction_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INSTALLS:
      g_clear_object (&priv->installs);
      priv->installs = g_value_dup_object (value);
      break;
    case PROP_UPDATES:
      g_clear_object (&priv->updates);
      priv->updates = g_value_dup_object (value);
      break;
    case PROP_REMOVALS:
      g_clear_object (&priv->removals);
      priv->removals = g_value_dup_object (value);
      break;
    case PROP_PENDING:
      priv->pending = g_value_get_boolean (value);
      break;
    case PROP_STATUS:
      g_clear_pointer (&priv->status, g_free);
      priv->status = g_value_dup_string (value);
      break;
    case PROP_PROGRESS:
      priv->progress = g_value_get_double (value);
      break;
    case PROP_FINISHED:
      priv->finished = g_value_get_boolean (value);
      break;
    case PROP_SUCCESS:
      priv->success = g_value_get_boolean (value);
      break;
    case PROP_ERROR:
      g_clear_pointer (&priv->error, g_free);
      priv->error = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_class_init (BzTransactionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_transaction_set_property;
  object_class->get_property = bz_transaction_get_property;
  object_class->dispose      = bz_transaction_dispose;

  props[PROP_INSTALLS] =
      g_param_spec_object (
          "installs",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_UPDATES] =
      g_param_spec_object (
          "updates",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_REMOVALS] =
      g_param_spec_object (
          "removals",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_PENDING] =
      g_param_spec_boolean (
          "pending",
          NULL, NULL, TRUE,
          G_PARAM_READWRITE);

  props[PROP_STATUS] =
      g_param_spec_string (
          "status",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_PROGRESS] =
      g_param_spec_double (
          "progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_FINISHED] =
      g_param_spec_boolean (
          "finished",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_SUCCESS] =
      g_param_spec_boolean (
          "success",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_ERROR] =
      g_param_spec_string (
          "error",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_transaction_init (BzTransaction *self)
{
  BzTransactionPrivate *priv = NULL;

  priv = bz_transaction_get_instance_private (self);

  priv->installs = G_LIST_MODEL (g_list_store_new (BZ_TYPE_ENTRY));
  priv->updates  = G_LIST_MODEL (g_list_store_new (BZ_TYPE_ENTRY));
  priv->removals = G_LIST_MODEL (g_list_store_new (BZ_TYPE_ENTRY));

  priv->pending = TRUE;
  priv->status  = g_strdup ("Pending");
}

BzTransaction *
bz_transaction_new_full (BzEntry **installs,
                         guint     n_installs,
                         BzEntry **updates,
                         guint     n_updates,
                         BzEntry **removals,
                         guint     n_removals)

{
  g_autoptr (BzTransaction) self = NULL;
  BzTransactionPrivate *priv     = NULL;

  g_return_val_if_fail ((installs != NULL && n_installs > 0) ||
                            (updates != NULL && n_updates > 0) ||
                            (removals != NULL && n_removals),
                        NULL);

  if (n_installs > 0)
    {
      for (guint i = 0; i < n_installs; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (installs[i]), NULL);
    }
  if (n_updates > 0)
    {
      for (guint i = 0; i < n_updates; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (updates[i]), NULL);
    }
  if (n_removals > 0)
    {
      for (guint i = 0; i < n_removals; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (removals[i]), NULL);
    }

  self = g_object_new (BZ_TYPE_TRANSACTION, NULL);
  priv = bz_transaction_get_instance_private (self);

  if (n_installs > 0)
    {
      for (guint i = 0; i < n_installs; i++)
        g_list_store_append (G_LIST_STORE (priv->installs), installs[i]);
    }
  if (n_updates > 0)
    {
      for (guint i = 0; i < n_updates; i++)
        g_list_store_append (G_LIST_STORE (priv->updates), updates[i]);
    }
  if (n_removals > 0)
    {
      for (guint i = 0; i < n_removals; i++)
        g_list_store_append (G_LIST_STORE (priv->removals), removals[i]);
    }

  return g_steal_pointer (&self);
}

GListModel *
bz_transaction_get_installs (BzTransaction *self)
{
  BzTransactionPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_TRANSACTION (self), NULL);

  priv = bz_transaction_get_instance_private (self);
  return priv->installs;
}

GListModel *
bz_transaction_get_updates (BzTransaction *self)
{
  BzTransactionPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_TRANSACTION (self), NULL);

  priv = bz_transaction_get_instance_private (self);
  return priv->updates;
}

GListModel *
bz_transaction_get_removals (BzTransaction *self)
{
  BzTransactionPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_TRANSACTION (self), NULL);

  priv = bz_transaction_get_instance_private (self);
  return priv->removals;
}

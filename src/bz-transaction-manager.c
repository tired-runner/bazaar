/* bz-transaction-manager.c
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

#include "bz-transaction-manager.h"
#include "bz-transaction-view.h"
#include "bz-util.h"

struct _BzTransactionManager
{
  GObject parent_instance;

  BzBackend  *backend;
  gboolean    paused;
  GListStore *transactions;
  double      current_progress;

  DexFuture    *current_task;
  GCancellable *cancellable;

  GQueue queue;
};

G_DEFINE_FINAL_TYPE (BzTransactionManager, bz_transaction_manager, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_BACKEND,
  PROP_PAUSED,
  PROP_TRANSACTIONS,
  PROP_HAS_TRANSACTIONS,
  PROP_ACTIVE,
  PROP_CURRENT_PROGRESS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SUCCESS,
  SIGNAL_FAILURE,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

BZ_DEFINE_DATA (
    queued_schedule,
    QueuedSchedule,
    {
      BzTransactionManager *self;
      BzBackend            *backend;
      BzTransaction        *transaction;
      GTimer               *timer;
    },
    BZ_RELEASE_DATA (backend, g_object_unref);
    BZ_RELEASE_DATA (transaction, bz_transaction_dismiss);
    BZ_RELEASE_DATA (timer, g_timer_destroy));

static void
transaction_progress (BzEntry            *entry,
                      const char         *status,
                      gboolean            is_estimating,
                      double              progress,
                      guint64             bytes_transferred,
                      guint64             start_time,
                      QueuedScheduleData *data);

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data);

static void
dispatch_next (BzTransactionManager *self);

static void
bz_transaction_manager_dispose (GObject *object)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  g_clear_object (&self->backend);
  g_clear_object (&self->transactions);
  g_queue_clear_full (&self->queue, queued_schedule_data_unref);
  dex_clear (&self->current_task);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (bz_transaction_manager_parent_class)->dispose (object);
}

static void
bz_transaction_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      g_value_set_object (value, bz_transaction_manager_get_backend (self));
      break;
    case PROP_PAUSED:
      g_value_set_boolean (value, bz_transaction_manager_get_paused (self));
      break;
    case PROP_TRANSACTIONS:
      g_value_set_object (value, self->transactions);
      break;
    case PROP_HAS_TRANSACTIONS:
      g_value_set_boolean (value, bz_transaction_manager_get_has_transactions (self));
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, bz_transaction_manager_get_active (self));
      break;
    case PROP_CURRENT_PROGRESS:
      g_value_set_double (value, self->current_progress);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BACKEND:
      bz_transaction_manager_set_backend (self, g_value_get_object (value));
      break;
    case PROP_PAUSED:
      bz_transaction_manager_set_paused (self, g_value_get_boolean (value));
      break;
    case PROP_TRANSACTIONS:
    case PROP_HAS_TRANSACTIONS:
    case PROP_ACTIVE:
    case PROP_CURRENT_PROGRESS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_manager_class_init (BzTransactionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_transaction_manager_dispose;
  object_class->get_property = bz_transaction_manager_get_property;
  object_class->set_property = bz_transaction_manager_set_property;

  props[PROP_BACKEND] =
      g_param_spec_object (
          "backend",
          NULL, NULL,
          BZ_TYPE_BACKEND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PAUSED] =
      g_param_spec_boolean (
          "paused",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSACTIONS] =
      g_param_spec_object (
          "transactions",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_HAS_TRANSACTIONS] =
      g_param_spec_boolean (
          "has-transactions",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_CURRENT_PROGRESS] =
      g_param_spec_double (
          "current-progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_type_ensure (BZ_TYPE_TRANSACTION_VIEW);
  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SUCCESS] =
      g_signal_new (
          "success",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SUCCESS],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_FAILURE] =
      g_signal_new (
          "failure",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_FAILURE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);
}

static void
bz_transaction_manager_init (BzTransactionManager *self)
{
  self->transactions = g_list_store_new (BZ_TYPE_TRANSACTION);
  g_queue_init (&self->queue);
}

BzTransactionManager *
bz_transaction_manager_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_MANAGER, NULL);
}

void
bz_transaction_manager_set_backend (BzTransactionManager *self,
                                    BzBackend            *backend)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  g_return_if_fail (backend == NULL || BZ_IS_BACKEND (backend));

  g_clear_object (&self->backend);
  if (backend != NULL)
    self->backend = g_object_ref (backend);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BACKEND]);
}

BzBackend *
bz_transaction_manager_get_backend (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), NULL);
  return self->backend;
}

void
bz_transaction_manager_set_paused (BzTransactionManager *self,
                                   gboolean              paused)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  if ((self->paused && paused) ||
      (!self->paused && !paused))
    return;

  self->paused = paused;
  if (!paused)
    dispatch_next (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAUSED]);
}

gboolean
bz_transaction_manager_get_paused (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return self->paused;
}

gboolean
bz_transaction_manager_get_active (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return self->current_task != NULL;
}

gboolean
bz_transaction_manager_get_has_transactions (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return g_list_model_get_n_items (G_LIST_MODEL (self->transactions)) > 0;
}

void
bz_transaction_manager_add (BzTransactionManager *self,
                            BzTransaction        *transaction)
{
  g_autoptr (QueuedScheduleData) data = NULL;

  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  g_return_if_fail (self->backend != NULL);
  g_return_if_fail (BZ_IS_TRANSACTION (transaction));

  bz_transaction_hold (transaction);

  if (self->queue.length > 0)
    {
      BzTransaction *to_merge[2] = { 0 };
      guint          position    = 0;

      data = g_queue_pop_head (&self->queue);

      to_merge[0] = data->transaction;
      to_merge[1] = g_steal_pointer (&transaction);
      transaction = bz_transaction_new_merged (to_merge, G_N_ELEMENTS (to_merge));

      g_list_store_find (self->transactions, data->transaction, &position);
      g_list_store_splice (self->transactions, position, 1, (gpointer *) &transaction, 1);

      g_clear_object (&data->transaction);
      data->transaction = transaction;
    }
  else
    {
      data              = queued_schedule_data_new ();
      data->self        = self;
      data->backend     = g_object_ref (self->backend);
      data->transaction = g_object_ref (transaction);

      g_list_store_insert (self->transactions, 0, transaction);
    }

  g_queue_push_head (&self->queue, queued_schedule_data_ref (data));
  if (self->current_task == NULL)
    dispatch_next (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_TRANSACTIONS]);
}

DexFuture *
bz_transaction_manager_cancel_current (BzTransactionManager *self)
{
  dex_return_error_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  if (self->current_task != NULL)
    {
      DexFuture *task = NULL;

      g_cancellable_cancel (self->cancellable);
      task = g_steal_pointer (&self->current_task);

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
      return task;
    }
  else
    return NULL;
}

void
bz_transaction_manager_clear_finished (BzTransactionManager *self)
{
  guint    n_items   = 0;
  gboolean had_items = FALSE;

  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  n_items   = g_list_model_get_n_items (G_LIST_MODEL (self->transactions));
  had_items = n_items > 0;

  for (guint i = 0; i < n_items;)
    {
      g_autoptr (BzTransaction) transaction = NULL;
      gboolean finished                     = FALSE;

      transaction = g_list_model_get_item (G_LIST_MODEL (self->transactions), i);
      g_object_get (transaction, "finished", &finished, NULL);

      if (finished)
        {
          g_list_store_remove (self->transactions, i);
          n_items--;
        }
      else
        i++;
    }

  if (had_items && n_items == 0)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_TRANSACTIONS]);
}

static void
transaction_progress (BzEntry            *entry,
                      const char         *status,
                      gboolean            is_estimating,
                      double              progress,
                      guint64             bytes_transferred,
                      guint64             start_time,
                      QueuedScheduleData *data)
{
  g_object_set (
      data->transaction,
      "pending", is_estimating,
      "status", status,
      "progress", progress,
      NULL);

  data->self->current_progress = progress;
  g_object_notify_by_pspec (G_OBJECT (data->self), props[PROP_CURRENT_PROGRESS]);
}

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data)
{
  g_autoptr (GError) local_error = NULL;
  const GValue    *value         = NULL;
  g_autofree char *status        = NULL;

  value = dex_future_get_value (future, &local_error);

  g_timer_stop (data->timer);
  status = g_strdup_printf (
      _ ("Finished in %.02f seconds"),
      g_timer_elapsed (data->timer, NULL));

  g_object_set (
      data->transaction,
      "status", status,
      "progress", 1.0,
      "finished", TRUE,
      "success", value != NULL,
      "error", local_error != NULL ? local_error->message : NULL,
      NULL);

  if (value != NULL)
    g_signal_emit (data->self, signals[SIGNAL_SUCCESS], 0, data->transaction);
  else
    g_signal_emit (data->self, signals[SIGNAL_FAILURE], 0, data->transaction);

  g_clear_object (&data->self->cancellable);
  data->self->current_task = NULL;
  dispatch_next (data->self);

  return NULL;
}

static void
dispatch_next (BzTransactionManager *self)
{
  g_autoptr (QueuedScheduleData) data  = NULL;
  g_autoptr (GListStore) store         = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (DexFuture) future         = NULL;

  if (self->paused ||
      self->cancellable != NULL ||
      self->queue.length == 0)
    goto done;

  data        = g_queue_pop_tail (&self->queue);
  data->timer = g_timer_new ();

  store = g_list_store_new (BZ_TYPE_TRANSACTION);
  g_list_store_append (store, data->transaction);

  cancellable = g_cancellable_new ();
  future      = bz_backend_merge_and_schedule_transactions (
      self->backend,
      G_LIST_MODEL (store),
      (BzBackendTransactionProgressFunc) transaction_progress,
      cancellable,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) transaction_finally,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  self->cancellable  = g_steal_pointer (&cancellable);
  self->current_task = g_steal_pointer (&future);

done:
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
}

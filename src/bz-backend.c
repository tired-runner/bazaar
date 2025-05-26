/* bz-backend.c
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

#include "bz-backend.h"
#include "bz-util.h"

G_DEFINE_INTERFACE (BzBackend, bz_backend, G_TYPE_OBJECT)

BZ_DEFINE_DATA (
    retrieve_with_blocklists,
    RetrieveWithBlocklists,
    {
      BzBackend                 *backend;
      DexScheduler              *home_scheduler;
      GPtrArray                 *blocklists;
      BzBackendGatherEntriesFunc progress_func;
      gpointer                   user_data;
      GDestroyNotify             destroy_user_data;
    },
    BZ_RELEASE_DATA (backend, g_object_unref);
    BZ_RELEASE_DATA (home_scheduler, dex_unref);
    BZ_RELEASE_DATA (blocklists, g_ptr_array_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
retrieve_with_blocklists_fiber (RetrieveWithBlocklistsData *data);

static DexFuture *
bz_backend_real_refresh (BzBackend *self)
{
  return NULL;
}

static DexFuture *
bz_backend_real_retrieve_remote_entries (BzBackend                 *self,
                                         DexScheduler              *home_scheduler,
                                         GPtrArray                 *blocked_names,
                                         BzBackendGatherEntriesFunc progress_func,
                                         gpointer                   user_data,
                                         GDestroyNotify             destroy_user_data)
{
  return NULL;
}

static DexFuture *
bz_backend_real_retrieve_install_ids (BzBackend *self)
{
  return NULL;
}

static DexFuture *
bz_backend_real_retrieve_update_ids (BzBackend *self)
{
  return NULL;
}

static DexFuture *
bz_backend_real_schedule_transaction (BzBackend                       *self,
                                      BzEntry                        **installs,
                                      guint                            n_installs,
                                      BzEntry                        **updates,
                                      guint                            n_updates,
                                      BzEntry                        **removals,
                                      guint                            n_removals,
                                      BzBackendTransactionProgressFunc progress_func,
                                      gpointer                         user_data,
                                      GDestroyNotify                   destroy_user_data)
{
  return NULL;
}

static void
bz_backend_default_init (BzBackendInterface *iface)
{
  iface->refresh                 = bz_backend_real_refresh;
  iface->retrieve_remote_entries = bz_backend_real_retrieve_remote_entries;
  iface->retrieve_install_ids    = bz_backend_real_retrieve_install_ids;
  iface->retrieve_update_ids     = bz_backend_real_retrieve_update_ids;
  iface->schedule_transaction    = bz_backend_real_schedule_transaction;
}

DexFuture *
bz_backend_refresh (BzBackend *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);

  return BZ_BACKEND_GET_IFACE (self)->refresh (self);
}

DexFuture *
bz_backend_retrieve_remote_entries (BzBackend                 *self,
                                    DexScheduler              *home_scheduler,
                                    GPtrArray                 *blocked_names,
                                    BzBackendGatherEntriesFunc progress_func,
                                    gpointer                   user_data,
                                    GDestroyNotify             destroy_user_data)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);
  g_return_val_if_fail (home_scheduler == NULL || DEX_IS_SCHEDULER (home_scheduler), NULL);

  return BZ_BACKEND_GET_IFACE (self)->retrieve_remote_entries (
      self,
      home_scheduler != NULL ? home_scheduler : dex_scheduler_get_thread_default (),
      blocked_names,
      progress_func,
      user_data,
      destroy_user_data);
}

DexFuture *
bz_backend_retrieve_remote_entries_with_blocklists (BzBackend                 *self,
                                                    DexScheduler              *home_scheduler,
                                                    GListModel                *blocklists,
                                                    BzBackendGatherEntriesFunc progress_func,
                                                    gpointer                   user_data,
                                                    GDestroyNotify             destroy_user_data)
{

  g_autoptr (RetrieveWithBlocklistsData) data = NULL;
  guint n_blocklists                          = 0;

  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);
  g_return_val_if_fail (home_scheduler == NULL || DEX_IS_SCHEDULER (home_scheduler), NULL);
  g_return_val_if_fail (G_LIST_MODEL (blocklists), NULL);

  data                    = retrieve_with_blocklists_data_new ();
  data->backend           = g_object_ref (self);
  data->home_scheduler    = home_scheduler != NULL ? dex_ref (home_scheduler) : dex_scheduler_ref_thread_default ();
  data->blocklists        = g_ptr_array_new_with_free_func (g_free);
  data->progress_func     = progress_func;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  n_blocklists = g_list_model_get_n_items (blocklists);
  for (guint i = 0; i < n_blocklists; i++)
    {
      g_autoptr (GtkStringObject) string = NULL;
      const char *path                   = NULL;

      string = g_list_model_get_item (blocklists, i);
      path   = gtk_string_object_get_string (string);

      g_ptr_array_add (data->blocklists, g_strdup (path));
    }

  return dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      0,
      (DexFiberFunc) retrieve_with_blocklists_fiber,
      retrieve_with_blocklists_data_ref (data),
      retrieve_with_blocklists_data_unref);
}

DexFuture *
bz_backend_retrieve_install_ids (BzBackend *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);

  return BZ_BACKEND_GET_IFACE (self)->retrieve_install_ids (self);
}

DexFuture *
bz_backend_retrieve_update_ids (BzBackend *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);

  return BZ_BACKEND_GET_IFACE (self)->retrieve_update_ids (self);
}

DexFuture *
bz_backend_schedule_transaction (BzBackend                       *self,
                                 BzEntry                        **installs,
                                 guint                            n_installs,
                                 BzEntry                        **updates,
                                 guint                            n_updates,
                                 BzEntry                        **removals,
                                 guint                            n_removals,
                                 BzBackendTransactionProgressFunc progress_func,
                                 gpointer                         user_data,
                                 GDestroyNotify                   destroy_user_data)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);
  g_return_val_if_fail ((installs != NULL && n_installs > 0) ||
                            (updates != NULL && n_updates > 0) ||
                            (removals != NULL && n_removals),
                        NULL);
  if (installs != NULL)
    {
      for (guint i = 0; i < n_installs; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (installs[i]), NULL);
    }
  if (updates != NULL)
    {
      for (guint i = 0; i < n_updates; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (updates[i]), NULL);
    }
  if (removals != NULL)
    {
      for (guint i = 0; i < n_removals; i++)
        g_return_val_if_fail (BZ_IS_ENTRY (removals[i]), NULL);
    }

  return BZ_BACKEND_GET_IFACE (self)->schedule_transaction (
      self,
      installs,
      n_installs,
      updates,
      n_updates,
      removals,
      n_removals,
      progress_func,
      user_data,
      destroy_user_data);
}

DexFuture *
bz_backend_merge_and_schedule_transactions (BzBackend                       *self,
                                            GListModel                      *transactions,
                                            BzBackendTransactionProgressFunc progress_func,
                                            gpointer                         user_data,
                                            GDestroyNotify                   destroy_user_data)
{
  guint n_items                     = 0;
  g_autoptr (GPtrArray) installs_pa = NULL;
  g_autoptr (GPtrArray) updates_pa  = NULL;
  g_autoptr (GPtrArray) removals_pa = NULL;

  g_return_val_if_fail (G_IS_LIST_MODEL (transactions), NULL);
  g_return_val_if_fail (g_list_model_get_item_type (transactions) == BZ_TYPE_TRANSACTION, NULL);

  n_items = g_list_model_get_n_items (transactions);
  g_return_val_if_fail (n_items > 0, NULL);

  installs_pa = g_ptr_array_new_with_free_func (g_object_unref);
  updates_pa  = g_ptr_array_new_with_free_func (g_object_unref);
  removals_pa = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzTransaction) transaction = NULL;
      GListModel *installs                  = NULL;
      GListModel *updates                   = NULL;
      GListModel *removals                  = NULL;
      guint       n_installs                = 0;
      guint       n_updates                 = 0;
      guint       n_removals                = 0;

      transaction = g_list_model_get_item (transactions, i);
      installs    = bz_transaction_get_installs (transaction);
      updates     = bz_transaction_get_updates (transaction);
      removals    = bz_transaction_get_removals (transaction);

      if (installs != NULL)
        n_installs = g_list_model_get_n_items (installs);
      if (updates != NULL)
        n_updates = g_list_model_get_n_items (updates);
      if (removals != NULL)
        n_removals = g_list_model_get_n_items (removals);

      for (guint j = 0; j < n_installs; j++)
        g_ptr_array_add (installs_pa, g_list_model_get_item (installs, j));
      for (guint j = 0; j < n_updates; j++)
        g_ptr_array_add (updates_pa, g_list_model_get_item (updates, j));
      for (guint j = 0; j < n_removals; j++)
        g_ptr_array_add (removals_pa, g_list_model_get_item (removals, j));
    }

  return bz_backend_schedule_transaction (
      self,
      (BzEntry **) installs_pa->pdata,
      installs_pa->len,
      (BzEntry **) updates_pa->pdata,
      updates_pa->len,
      (BzEntry **) removals_pa->pdata,
      removals_pa->len,
      progress_func,
      user_data,
      destroy_user_data);
}

static DexFuture *
retrieve_with_blocklists_fiber (RetrieveWithBlocklistsData *data)
{
  GPtrArray *blocklists               = data->blocklists;
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (GPtrArray) blocked_names = NULL;

  blocked_names = g_ptr_array_new_with_free_func (g_free);

  for (guint i = 0; i < blocklists->len; i++)
    {
      const char *path       = NULL;
      g_autoptr (GFile) file = NULL;

      path = g_ptr_array_index (blocklists, i);
      file = g_file_new_for_path (path);

      if (file != NULL)
        {
          g_autoptr (GBytes) bytes     = NULL;
          const char       *bytes_data = NULL;
          g_autofree char **lines      = NULL;

          bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &local_error);
          if (bytes == NULL)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          bytes_data = g_bytes_get_data (bytes, NULL);
          lines      = g_strsplit (bytes_data, "\n", -1);

          for (char **line = lines; *line != NULL; line++)
            {
              if (**line != '\0')
                g_ptr_array_add (blocked_names, *line);
              else
                g_free (*line);
            }
        }
    }

  return BZ_BACKEND_GET_IFACE (data->backend)
      ->retrieve_remote_entries (
          data->backend,
          data->home_scheduler,
          blocked_names,
          data->progress_func,
          g_steal_pointer (&data->user_data),
          data->destroy_user_data);
}

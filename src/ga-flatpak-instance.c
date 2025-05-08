/* ga-flatpak-instance.c
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

#include <flatpak.h>

#include "ga-flatpak-private.h"
#include "ga-util.h"

struct _GaFlatpakInstance
{
  GObject parent_instance;

  DexScheduler        *scheduler;
  FlatpakInstallation *installation;
};

G_DEFINE_FINAL_TYPE (GaFlatpakInstance, ga_flatpak_instance, G_TYPE_OBJECT)

GA_DEFINE_DATA (
    init,
    Init,
    { GaFlatpakInstance *fp; },
    GA_RELEASE_DATA (fp, g_object_unref))
static DexFuture *
init_fiber (InitData *data);

GA_DEFINE_DATA (
    ref_installed_apps,
    RefInstalledApps,
    { GaFlatpakInstance *fp; },
    GA_RELEASE_DATA (fp, g_object_unref))
static DexFuture *
ref_installed_apps_fiber (RefInstalledAppsData *data);

GA_DEFINE_DATA (
    ref_remote_apps,
    RefRemoteApps,
    {
      GaFlatpakInstance         *fp;
      DexScheduler              *home_scheduler;
      GaFlatpakGatherEntriesFunc progress_func;
      gpointer                   user_data;
      GDestroyNotify             destroy_user_data;
    },
    GA_RELEASE_DATA (fp, g_object_unref);
    GA_RELEASE_DATA (home_scheduler, dex_unref);
    GA_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
ref_remote_apps_fiber (RefRemoteAppsData *data);

GA_DEFINE_DATA (
    ref_remote_apps_job,
    RefRemoteAppsJob,
    {
      RefRemoteAppsData *parent;
      FlatpakRemoteRef  *rref;
    },
    GA_RELEASE_DATA (parent, ref_remote_apps_data_unref);
    GA_RELEASE_DATA (rref, g_object_unref));
static DexFuture *
ref_remote_apps_job_fiber (RefRemoteAppsJobData *data);

typedef struct
{
  RefRemoteAppsJobData *parent;
  GaEntry              *entry;
} RefRemoteAppsJobUpdateData;
static DexFuture *
ref_remote_apps_job_update (RefRemoteAppsJobUpdateData *data);

GA_DEFINE_DATA (
    install,
    Install,
    {
      GaFlatpakInstance           *fp;
      GaFlatpakEntry              *entry;
      GaFlatpakInstallProgressFunc progress_func;
      gpointer                     user_data;
      GDestroyNotify               destroy_user_data;
      guint                        timeout_handle;
    },
    GA_RELEASE_DATA (fp, g_object_unref);
    GA_RELEASE_DATA (entry, g_object_unref);
    GA_RELEASE_DATA (user_data, self->destroy_user_data);
    GA_RELEASE_UTAG (timeout_handle, g_source_remove))
static DexFuture *
install_fiber (InstallData *data);
static void
install_new_operation (FlatpakTransaction          *object,
                       FlatpakTransactionOperation *operation,
                       FlatpakTransactionProgress  *progress,
                       InstallData                 *data);
static void
install_progress_changed (FlatpakTransactionProgress *object,
                          InstallData                *data);
GA_DEFINE_DATA (
    idle_install,
    IdleInstall,
    {
      InstallData *install;
      char        *status;
      gboolean     is_estimating;
      int          progress_num;
      guint64      bytes_transferred;
      guint64      start_time;
    },
    GA_RELEASE_DATA (install, install_data_unref);
    GA_RELEASE_DATA (status, g_free));
static gboolean
install_progress_idle (IdleInstallData *data);
static gboolean
install_progress_timeout (IdleInstallData *data);

static void
ga_flatpak_instance_dispose (GObject *object)
{
  GaFlatpakInstance *self = GA_FLATPAK_INSTANCE (object);

  dex_clear (&self->scheduler);
  g_clear_object (&self->installation);

  G_OBJECT_CLASS (ga_flatpak_instance_parent_class)->dispose (object);
}

static void
ga_flatpak_instance_class_init (GaFlatpakInstanceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ga_flatpak_instance_dispose;
}

static void
ga_flatpak_instance_init (GaFlatpakInstance *self)
{
  self->scheduler = dex_thread_pool_scheduler_new ();
}

FlatpakInstallation *
ga_flatpak_instance_get_installation (GaFlatpakInstance *self)
{
  g_return_val_if_fail (GA_IS_FLATPAK_INSTANCE (self), NULL);

  return self->installation;
}

DexFuture *
ga_flatpak_instance_new (void)
{
  g_autoptr (InitData) data = NULL;

  data     = init_data_new ();
  data->fp = g_object_new (GA_TYPE_FLATPAK_INSTANCE, NULL);

  return dex_scheduler_spawn (
      data->fp->scheduler, 0, (DexFiberFunc) init_fiber,
      init_data_ref (data), init_data_unref);
}

static DexFuture *
init_fiber (InitData *data)
{
  GaFlatpakInstance *fp          = data->fp;
  g_autoptr (GError) local_error = NULL;

  fp->installation = flatpak_installation_new_system (NULL, &local_error);
  if (fp->installation == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_for_object (fp);
}

DexFuture *
ga_flatpak_instance_ref_installed_apps (GaFlatpakInstance *self)
{
  g_autoptr (RefInstalledAppsData) data = NULL;

  g_return_val_if_fail (GA_IS_FLATPAK_INSTANCE (self), NULL);

  data     = ref_installed_apps_data_new ();
  data->fp = g_object_ref (self);

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) ref_installed_apps_fiber,
      ref_installed_apps_data_ref (data), ref_installed_apps_data_unref);
}

static DexFuture *
ref_installed_apps_fiber (RefInstalledAppsData *data)
{
  GaFlatpakInstance *fp          = data->fp;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GPtrArray) refs     = NULL;
  g_autoptr (GListStore) store   = NULL;

  refs = flatpak_installation_list_installed_refs_by_kind (
      fp->installation,
      FLATPAK_REF_KIND_APP,
      NULL,
      &local_error);
  if (refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  store = g_list_store_new (FLATPAK_TYPE_REF);
  g_list_store_splice (store, 0, 0, refs->pdata, refs->len);

  return dex_future_new_for_object (store);
}

DexFuture *
ga_flatpak_instance_ref_remote_apps (GaFlatpakInstance         *self,
                                     GaFlatpakGatherEntriesFunc progress_func,
                                     gpointer                   user_data,
                                     GDestroyNotify             destroy_user_data)
{
  g_autoptr (RefRemoteAppsData) data = NULL;

  g_return_val_if_fail (GA_IS_FLATPAK_INSTANCE (self), NULL);
  g_return_val_if_fail (progress_func != NULL, NULL);

  data                    = ref_remote_apps_data_new ();
  data->fp                = g_object_ref (self);
  data->home_scheduler    = dex_scheduler_ref_thread_default ();
  data->progress_func     = progress_func;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) ref_remote_apps_fiber,
      ref_remote_apps_data_ref (data), ref_remote_apps_data_unref);
}

static DexFuture *
ref_remote_apps_fiber (RefRemoteAppsData *data)
{
  GaFlatpakInstance *fp          = data->fp;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GPtrArray) refs     = NULL;
  g_autofree DexFuture **jobs    = NULL;
  DexFuture             *future  = NULL;

  refs = flatpak_installation_list_remote_refs_sync (
      fp->installation, "flathub", NULL, &local_error);
  if (refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  jobs = g_malloc0_n (refs->len, sizeof (*jobs));
  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakRemoteRef *rref                    = NULL;
      g_autoptr (RefRemoteAppsJobData) job_data = NULL;

      rref = g_ptr_array_index (refs, i);

      job_data         = ref_remote_apps_job_data_new ();
      job_data->parent = ref_remote_apps_data_ref (data);
      job_data->rref   = g_object_ref (rref);

      jobs[i] = dex_scheduler_spawn (
          fp->scheduler, 0,
          (DexFiberFunc) ref_remote_apps_job_fiber,
          ref_remote_apps_job_data_ref (job_data),
          ref_remote_apps_job_data_unref);
    }

  future = dex_future_allv (jobs, refs->len);
  for (guint i = 0; i < refs->len; i++)
    dex_unref (jobs[i]);

  return future;
}

static DexFuture *
ref_remote_apps_job_fiber (RefRemoteAppsJobData *data)
{
  g_autoptr (GError) local_error         = NULL;
  g_autoptr (GaFlatpakEntry) entry       = NULL;
  RefRemoteAppsJobUpdateData update_data = { 0 };
  DexFuture                 *update      = NULL;

  entry = ga_flatpak_entry_new_for_remote_ref (
      data->parent->fp, data->rref, &local_error);
  if (entry == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  update_data.parent = data;
  update_data.entry  = GA_ENTRY (entry);

  update = dex_scheduler_spawn (
      data->parent->home_scheduler, 0,
      (DexFiberFunc) ref_remote_apps_job_update,
      &update_data, NULL);
  if (!dex_await (update, &local_error))
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_true ();
}

static DexFuture *
ref_remote_apps_job_update (RefRemoteAppsJobUpdateData *data)
{
  data->parent->parent->progress_func (
      data->entry,
      data->parent->parent->user_data);

  return dex_future_new_true ();
}

DexFuture *
ga_flatpak_instance_install (GaFlatpakInstance           *self,
                             GaFlatpakEntry              *entry,
                             GaFlatpakInstallProgressFunc progress_func,
                             gpointer                     user_data,
                             GDestroyNotify               destroy_user_data)
{
  g_autoptr (InstallData) data = NULL;

  g_return_val_if_fail (GA_IS_FLATPAK_INSTANCE (self), NULL);
  g_return_val_if_fail (GA_IS_FLATPAK_ENTRY (entry), NULL);

  data                    = install_data_new ();
  data->fp                = g_object_ref (self);
  data->entry             = g_object_ref (entry);
  data->progress_func     = progress_func;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) install_fiber,
      install_data_ref (data), install_data_unref);
}

static DexFuture *
install_fiber (InstallData *data)
{
  GaFlatpakInstance *fp                           = data->fp;
  GaFlatpakEntry    *entry                        = data->entry;
  g_autoptr (GError) local_error                  = NULL;
  g_autoptr (FlatpakTransaction) transaction      = NULL;
  FlatpakRef      *ref                            = NULL;
  g_autofree char *ref_fmt                        = NULL;
  gboolean         result                         = FALSE;
  g_autoptr (FlatpakTransactionProgress) progress = NULL;

  transaction = flatpak_transaction_new_for_installation (fp->installation, NULL, &local_error);
  if (transaction == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));
  g_signal_connect (transaction, "new-operation", G_CALLBACK (install_new_operation), data);

  ref     = ga_flatpak_entry_get_ref (entry);
  ref_fmt = flatpak_ref_format_ref (ref);
  result  = flatpak_transaction_add_install (
      transaction,
      flatpak_remote_ref_get_remote_name (FLATPAK_REMOTE_REF (ref)),
      ref_fmt,
      NULL,
      &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  result = flatpak_transaction_run (transaction, NULL, &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_true ();
}

static void
install_new_operation (FlatpakTransaction          *transaction,
                       FlatpakTransactionOperation *operation,
                       FlatpakTransactionProgress  *progress,
                       InstallData                 *data)
{
  flatpak_transaction_progress_set_update_frequency (progress, 10);

  if (data->progress_func != NULL)
    g_signal_connect (
        progress, "changed",
        G_CALLBACK (install_progress_changed),
        data);
}

static void
install_progress_changed (FlatpakTransactionProgress *progress,
                          InstallData                *data)
{
  g_autoptr (IdleInstallData) idle_data = NULL;

  idle_data                    = idle_install_data_new ();
  idle_data->install           = install_data_ref (data);
  idle_data->status            = flatpak_transaction_progress_get_status (progress);
  idle_data->is_estimating     = flatpak_transaction_progress_get_is_estimating (progress);
  idle_data->progress_num      = flatpak_transaction_progress_get_progress (progress);
  idle_data->bytes_transferred = flatpak_transaction_progress_get_bytes_transferred (progress);
  idle_data->start_time        = flatpak_transaction_progress_get_start_time (progress);

  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) install_progress_idle,
      idle_install_data_ref (idle_data),
      idle_install_data_unref);

  if (idle_data->is_estimating)
    {
      if (data->timeout_handle == 0)
        /* We'll send an update periodically so the UI can pulse */
        data->timeout_handle = g_timeout_add_full (
            G_PRIORITY_DEFAULT,
            10,
            (GSourceFunc) install_progress_timeout,
            idle_install_data_ref (idle_data),
            idle_install_data_unref);
    }
  else
    g_clear_handle_id (&data->timeout_handle,
                       g_source_remove);
}

static gboolean
install_progress_idle (IdleInstallData *data)
{
  data->install->progress_func (
      data->status,
      data->is_estimating,
      data->progress_num,
      data->bytes_transferred,
      data->start_time,
      data->install->user_data);

  return G_SOURCE_REMOVE;
}

static gboolean
install_progress_timeout (IdleInstallData *data)
{
  data->install->progress_func (
      data->status,
      data->is_estimating,
      data->progress_num,
      data->bytes_transferred,
      data->start_time,
      data->install->user_data);

  return G_SOURCE_CONTINUE;
}

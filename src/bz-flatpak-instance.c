/* bz-flatpak-instance.c
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
#include <glycin-gtk4.h>
#include <xmlb.h>

#include "bz-backend.h"
#include "bz-flatpak-private.h"
#include "bz-util.h"

struct _BzFlatpakInstance
{
  GObject parent_instance;

  DexScheduler        *scheduler;
  FlatpakInstallation *system;
  FlatpakInstallation *user;
};

static void
backend_iface_init (BzBackendInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzFlatpakInstance,
    bz_flatpak_instance,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (BZ_TYPE_BACKEND, backend_iface_init));

BZ_DEFINE_DATA (
    init,
    Init,
    { BzFlatpakInstance *instance; },
    BZ_RELEASE_DATA (instance, g_object_unref))
static DexFuture *
init_fiber (InitData *data);

BZ_DEFINE_DATA (
    gather_entries,
    GatherEntries,
    {
      BzFlatpakInstance         *instance;
      GPtrArray                 *blocked_names;
      DexScheduler              *home_scheduler;
      BzBackendGatherEntriesFunc progress_func;
      gpointer                   user_data;
      GDestroyNotify             destroy_user_data;
    },
    BZ_RELEASE_DATA (instance, g_object_unref);
    BZ_RELEASE_DATA (blocked_names, g_ptr_array_unref);
    BZ_RELEASE_DATA (home_scheduler, dex_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
ref_remote_apps_fiber (GatherEntriesData *data);
static DexFuture *
ref_installs_fiber (GatherEntriesData *data);
static DexFuture *
ref_updates_fiber (GatherEntriesData *data);

BZ_DEFINE_DATA (
    ref_remote_apps_for_remote,
    RefRemoteAppsForRemote,
    {
      GatherEntriesData   *parent;
      FlatpakInstallation *installation;
      FlatpakRemote       *remote;
      GHashTable          *blocked_names_hash;
    },
    BZ_RELEASE_DATA (parent, gather_entries_data_unref);
    BZ_RELEASE_DATA (installation, g_object_unref);
    BZ_RELEASE_DATA (remote, g_object_unref);
    BZ_RELEASE_DATA (blocked_names_hash, g_hash_table_unref));
static DexFuture *
ref_remote_apps_for_single_remote_fiber (RefRemoteAppsForRemoteData *data);

BZ_DEFINE_DATA (
    ref_remote_apps_job,
    RefRemoteAppsJob,
    {
      RefRemoteAppsForRemoteData *parent;
      FlatpakRemoteRef           *rref;
      AsComponent                *component;
      char                       *appstream_dir;
      GdkPaintable               *remote_icon;
    },
    BZ_RELEASE_DATA (parent, ref_remote_apps_for_remote_data_unref);
    BZ_RELEASE_DATA (rref, g_object_unref);
    BZ_RELEASE_DATA (component, g_object_unref);
    BZ_RELEASE_DATA (appstream_dir, g_free);
    BZ_RELEASE_DATA (remote_icon, g_object_unref));
static DexFuture *
ref_remote_apps_job_fiber (RefRemoteAppsJobData *data);

static void
gather_entries_update_progress (const char        *status,
                                guint              progress,
                                gboolean           estimating,
                                GatherEntriesData *data);

typedef struct
{
  GatherEntriesData *parent;
  BzEntry           *entry;
} GatherEntriesUpdateData;
static DexFuture *
gather_entries_job_update (GatherEntriesUpdateData *data);

BZ_DEFINE_DATA (
    transaction,
    Transaction,
    {
      BzFlatpakInstance               *instance;
      GPtrArray                       *installs;
      GPtrArray                       *updates;
      GPtrArray                       *removals;
      GHashTable                      *ref_to_entry_hash;
      BzBackendTransactionProgressFunc progress_func;
      gpointer                         user_data;
      GDestroyNotify                   destroy_user_data;
    },
    BZ_RELEASE_DATA (instance, g_object_unref);
    BZ_RELEASE_DATA (installs, g_ptr_array_unref);
    BZ_RELEASE_DATA (updates, g_ptr_array_unref);
    BZ_RELEASE_DATA (removals, g_ptr_array_unref);
    BZ_RELEASE_DATA (ref_to_entry_hash, g_hash_table_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
transaction_fiber (TransactionData *data);
static void
transaction_new_operation (FlatpakTransaction          *object,
                           FlatpakTransactionOperation *operation,
                           FlatpakTransactionProgress  *progress,
                           TransactionData             *data);
static BzFlatpakEntry *
find_entry_from_operation (TransactionData             *data,
                           FlatpakTransactionOperation *operation);

BZ_DEFINE_DATA (
    transaction_operation,
    TransactionOperation,
    {
      TransactionData *parent;
      BzFlatpakEntry  *entry;
      guint            timeout_handle;
    },
    BZ_RELEASE_DATA (parent, transaction_data_unref);
    BZ_RELEASE_DATA (entry, g_object_unref);
    BZ_RELEASE_UTAG (timeout_handle, g_source_remove))
static void
transaction_progress_changed (FlatpakTransactionProgress *object,
                              TransactionOperationData   *data);

BZ_DEFINE_DATA (
    idle_transaction,
    IdleTransaction,
    {
      TransactionOperationData *parent;
      char                     *status;
      gboolean                  is_estimating;
      int                       progress_num;
      guint64                   bytes_transferred;
      guint64                   start_time;
    },
    BZ_RELEASE_DATA (parent, transaction_operation_data_unref);
    BZ_RELEASE_DATA (status, g_free));
static gboolean
transaction_progress_idle (IdleTransactionData *data);
static gboolean
transaction_progress_timeout (IdleTransactionData *data);

static void
bz_flatpak_instance_dispose (GObject *object)
{
  BzFlatpakInstance *self = BZ_FLATPAK_INSTANCE (object);

  dex_clear (&self->scheduler);
  g_clear_object (&self->system);
  g_clear_object (&self->user);

  G_OBJECT_CLASS (bz_flatpak_instance_parent_class)->dispose (object);
}

static void
bz_flatpak_instance_class_init (BzFlatpakInstanceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_flatpak_instance_dispose;
}

static void
bz_flatpak_instance_init (BzFlatpakInstance *self)
{
  self->scheduler = dex_thread_pool_scheduler_new ();
}

static DexFuture *
bz_flatpak_instance_refresh (BzBackend *backend)
{
  /* TODO: move appstream sync here */
  return dex_future_new_true ();
}

static DexFuture *
bz_flatpak_instance_retrieve_remote_entries (BzBackend                 *backend,
                                             DexScheduler              *home_scheduler,
                                             GPtrArray                 *blocked_names,
                                             BzBackendGatherEntriesFunc progress_func,
                                             gpointer                   user_data,
                                             GDestroyNotify             destroy_user_data)
{
  BzFlatpakInstance *self            = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherEntriesData) data = NULL;

  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (self), NULL);
  g_return_val_if_fail (progress_func != NULL, NULL);

  data                    = gather_entries_data_new ();
  data->instance          = g_object_ref (self);
  data->blocked_names     = g_ptr_array_ref (blocked_names);
  data->home_scheduler    = dex_ref (home_scheduler);
  data->progress_func     = progress_func;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) ref_remote_apps_fiber,
      gather_entries_data_ref (data), gather_entries_data_unref);
}

static DexFuture *
bz_flatpak_instance_retrieve_install_ids (BzBackend *backend)
{
  BzFlatpakInstance *self            = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherEntriesData) data = NULL;

  data                    = gather_entries_data_new ();
  data->instance          = g_object_ref (self);
  data->home_scheduler    = dex_scheduler_ref_thread_default ();
  data->progress_func     = NULL;
  data->user_data         = NULL;
  data->destroy_user_data = NULL;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) ref_installs_fiber,
      gather_entries_data_ref (data), gather_entries_data_unref);
}

static DexFuture *
bz_flatpak_instance_retrieve_update_ids (BzBackend *backend)
{
  BzFlatpakInstance *self            = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherEntriesData) data = NULL;

  data                    = gather_entries_data_new ();
  data->instance          = g_object_ref (self);
  data->home_scheduler    = dex_scheduler_ref_thread_default ();
  data->progress_func     = NULL;
  data->user_data         = NULL;
  data->destroy_user_data = NULL;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) ref_updates_fiber,
      gather_entries_data_ref (data), gather_entries_data_unref);
}

static DexFuture *
bz_flatpak_instance_schedule_transaction (BzBackend                       *backend,
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
  BzFlatpakInstance *self          = BZ_FLATPAK_INSTANCE (backend);
  BzFlatpakEntry   **installs_dup  = NULL;
  BzFlatpakEntry   **updates_dup   = NULL;
  BzFlatpakEntry   **removals_dup  = NULL;
  g_autoptr (TransactionData) data = NULL;

  if (n_installs > 0)
    {
      for (guint i = 0; i < n_installs; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (installs[i]), NULL);
    }
  if (n_updates > 0)
    {
      for (guint i = 0; i < n_updates; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (updates[i]), NULL);
    }
  if (n_removals > 0)
    {
      for (guint i = 0; i < n_removals; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (removals[i]), NULL);
    }

  if (n_installs > 0)
    {
      installs_dup = g_malloc0_n (n_installs, sizeof (*installs_dup));
      for (guint i = 0; i < n_installs; i++)
        installs_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (installs[i]));
    }
  if (n_updates > 0)
    {
      updates_dup = g_malloc0_n (n_updates, sizeof (*updates_dup));
      for (guint i = 0; i < n_updates; i++)
        updates_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (updates[i]));
    }
  if (n_removals > 0)
    {
      removals_dup = g_malloc0_n (n_removals, sizeof (*removals_dup));
      for (guint i = 0; i < n_removals; i++)
        removals_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (removals[i]));
    }

  data                    = transaction_data_new ();
  data->instance          = g_object_ref (self);
  data->installs          = installs_dup != NULL ? g_ptr_array_new_take ((gpointer *) installs_dup, n_installs, g_object_unref) : NULL;
  data->updates           = updates_dup != NULL ? g_ptr_array_new_take ((gpointer *) updates_dup, n_updates, g_object_unref) : NULL;
  data->removals          = removals_dup != NULL ? g_ptr_array_new_take ((gpointer *) removals_dup, n_removals, g_object_unref) : NULL;
  data->ref_to_entry_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  data->progress_func     = progress_func;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  return dex_scheduler_spawn (
      self->scheduler, 0, (DexFiberFunc) transaction_fiber,
      transaction_data_ref (data), transaction_data_unref);
}

static void
backend_iface_init (BzBackendInterface *iface)
{
  iface->refresh                 = bz_flatpak_instance_refresh;
  iface->retrieve_remote_entries = bz_flatpak_instance_retrieve_remote_entries;
  iface->retrieve_install_ids    = bz_flatpak_instance_retrieve_install_ids;
  iface->retrieve_update_ids     = bz_flatpak_instance_retrieve_update_ids;
  iface->schedule_transaction    = bz_flatpak_instance_schedule_transaction;
}

FlatpakInstallation *
bz_flatpak_instance_get_installation (BzFlatpakInstance *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (self), NULL);

  return self->system;
}

DexFuture *
bz_flatpak_instance_new (void)
{
  g_autoptr (InitData) data = NULL;

  data           = init_data_new ();
  data->instance = g_object_new (BZ_TYPE_FLATPAK_INSTANCE, NULL);

  return dex_scheduler_spawn (
      data->instance->scheduler, 0, (DexFiberFunc) init_fiber,
      init_data_ref (data), init_data_unref);
}

static DexFuture *
init_fiber (InitData *data)
{
  BzFlatpakInstance *instance    = data->instance;
  g_autoptr (GError) local_error = NULL;

  instance->system = flatpak_installation_new_system (NULL, &local_error);
  if (instance->system == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  instance->user = flatpak_installation_new_user (NULL, &local_error);
  if (instance->user == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_for_object (instance);
}

static DexFuture *
ref_remote_apps_fiber (GatherEntriesData *data)
{
  BzFlatpakInstance *instance               = data->instance;
  GPtrArray         *blocked_names          = data->blocked_names;
  g_autoptr (GError) local_error            = NULL;
  g_autoptr (GPtrArray) system_remotes      = NULL;
  g_autoptr (GPtrArray) user_remotes        = NULL;
  g_autoptr (GHashTable) blocked_names_hash = NULL;
  g_autofree DexFuture **jobs               = NULL;
  DexFuture             *future             = NULL;

  system_remotes = flatpak_installation_list_remotes (
      instance->system, NULL, &local_error);
  if (system_remotes == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  user_remotes = flatpak_installation_list_remotes (
      instance->user, NULL, &local_error);
  if (user_remotes == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  if (system_remotes->len + user_remotes->len == 0)
    return dex_future_new_true ();

  if (blocked_names != NULL)
    {
      blocked_names_hash = g_hash_table_new (g_str_hash, g_str_equal);
      for (guint i = 0; i < blocked_names->len; i++)
        {
          const char *name = NULL;

          name = g_ptr_array_index (blocked_names, i);
          g_hash_table_add (blocked_names_hash, (gpointer) name);
        }
    }

  jobs = g_malloc0_n (system_remotes->len + user_remotes->len, sizeof (*jobs));
  for (guint i = 0; i < system_remotes->len + user_remotes->len; i++)
    {
      FlatpakInstallation *installation               = NULL;
      FlatpakRemote       *remote                     = NULL;
      g_autoptr (RefRemoteAppsForRemoteData) job_data = NULL;

      if (i < system_remotes->len)
        {
          installation = instance->system;
          remote       = g_ptr_array_index (system_remotes, i);
        }
      else
        {
          installation = instance->user;
          remote       = g_ptr_array_index (user_remotes, i - system_remotes->len);
        }

      job_data                     = ref_remote_apps_for_remote_data_new ();
      job_data->parent             = gather_entries_data_ref (data);
      job_data->installation       = g_object_ref (installation);
      job_data->remote             = g_object_ref (remote);
      job_data->blocked_names_hash = blocked_names_hash != NULL ? g_hash_table_ref (blocked_names_hash) : NULL;

      jobs[i] = dex_scheduler_spawn (
          instance->scheduler, 0,
          (DexFiberFunc) ref_remote_apps_for_single_remote_fiber,
          ref_remote_apps_for_remote_data_ref (job_data),
          ref_remote_apps_for_remote_data_unref);
    }

  future = dex_future_allv (jobs, system_remotes->len + user_remotes->len);
  for (guint i = 0; i < system_remotes->len + user_remotes->len; i++)
    dex_unref (jobs[i]);

  return future;
}

static void
gather_entries_update_progress (const char        *status,
                                guint              progress,
                                gboolean           estimating,
                                GatherEntriesData *data)
{
}

static DexFuture *
ref_remote_apps_for_single_remote_fiber (RefRemoteAppsForRemoteData *data)
{
  BzFlatpakInstance   *instance           = data->parent->instance;
  FlatpakInstallation *installation       = data->installation;
  FlatpakRemote       *remote             = data->remote;
  GHashTable          *blocked_names_hash = data->blocked_names_hash;
  const char          *remote_name        = NULL;
  g_autoptr (GError) local_error          = NULL;
  gboolean result                         = FALSE;
  g_autoptr (GFile) appstream_dir         = NULL;
  g_autofree char *appstream_dir_path     = NULL;
  g_autofree char *appstream_xml_path     = NULL;
  g_autoptr (GFile) appstream_xml         = NULL;
  g_autoptr (XbBuilderSource) source      = NULL;
  g_autoptr (XbBuilder) builder           = NULL;
  const gchar *const *locales             = NULL;
  g_autoptr (XbSilo) silo                 = NULL;
  g_autoptr (XbNode) root                 = NULL;
  g_autoptr (GPtrArray) children          = NULL;
  g_autoptr (AsMetadata) metadata         = NULL;
  AsComponentBox *components              = NULL;
  g_autoptr (GHashTable) id_hash          = NULL;
  g_autofree char *remote_icon_name       = NULL;
  g_autoptr (GdkPaintable) remote_icon    = NULL;
  g_autoptr (GPtrArray) refs              = NULL;
  guint                  n_jobs           = 0;
  g_autofree DexFuture **jobs             = NULL;
  g_autoptr (DexFuture) future            = NULL;

  remote_name = flatpak_remote_get_name (remote);

  result = flatpak_installation_update_remote_sync (
      installation,
      remote_name,
      NULL,
      &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  result = flatpak_installation_update_appstream_full_sync (
      installation,
      remote_name,
      NULL,
      (FlatpakProgressCallback) gather_entries_update_progress,
      data,
      NULL,
      NULL,
      &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  appstream_dir = flatpak_remote_get_appstream_dir (remote, NULL);
  /* TODO: make a proper error */
  g_assert (appstream_dir != NULL);

  appstream_dir_path = g_file_get_path (appstream_dir);
  appstream_xml_path = g_build_filename (appstream_dir_path, "appstream.xml.gz", NULL);
  /* TODO: make a proper error */
  g_assert (g_file_test (appstream_xml_path, G_FILE_TEST_EXISTS));
  appstream_xml = g_file_new_for_path (appstream_xml_path);

  source = xb_builder_source_new ();
  result = xb_builder_source_load_file (
      source,
      appstream_xml,
      XB_BUILDER_SOURCE_FLAG_WATCH_FILE |
          XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT,
      NULL,
      &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));
  builder = xb_builder_new ();
  locales = g_get_language_names ();
  for (guint i = 0; locales[i] != NULL; i++)
    xb_builder_add_locale (builder, locales[i]);
  xb_builder_import_source (builder, source);

  silo = xb_builder_compile (
      builder,
      XB_BUILDER_COMPILE_FLAG_SINGLE_LANG,
      NULL,
      &local_error);
  if (silo == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  root     = xb_silo_get_root (silo);
  children = xb_node_get_children (root);
  metadata = as_metadata_new ();

  for (guint i = 0; i < children->len; i++)
    {
      XbNode          *component_node = NULL;
      g_autofree char *component_xml  = NULL;

      component_node = g_ptr_array_index (children, i);

      component_xml = xb_node_export (
          component_node, XB_NODE_EXPORT_FLAG_NONE, &local_error);
      if (component_xml == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      result = as_metadata_parse_data (
          metadata, component_xml, -1,
          AS_FORMAT_KIND_XML, &local_error);
      if (!result)
        return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  components = as_metadata_get_components (metadata);
  id_hash    = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  for (guint i = 0; i < as_component_box_len (components); i++)
    {
      AsComponent *component = NULL;
      const char  *id        = NULL;

      component = as_component_box_index (components, i);
      id        = as_component_get_id (component);

      if (!g_hash_table_contains (id_hash, id) &&
          (blocked_names_hash == NULL || !g_hash_table_contains (blocked_names_hash, id)))
        g_hash_table_replace (id_hash, g_strdup (id), g_object_ref (component));
    }

  remote_icon_name = flatpak_remote_get_icon (remote);
  if (remote_icon_name != NULL)
    {
      g_autoptr (GFile) remote_icon_file = NULL;

      remote_icon_file = g_file_new_for_uri (remote_icon_name);
      if (remote_icon_file != NULL)
        {
          g_autoptr (GlyLoader) loader = NULL;
          g_autoptr (GlyImage) image   = NULL;
          g_autoptr (GlyFrame) frame   = NULL;
          GdkTexture *texture          = NULL;

          loader = gly_loader_new (remote_icon_file);
          image  = gly_loader_load (loader, &local_error);
          if (image == NULL)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          frame = gly_image_next_frame (image, &local_error);
          if (frame == NULL)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          texture = gly_gtk_frame_get_texture (frame);
          if (texture != NULL)
            remote_icon = GDK_PAINTABLE (texture);
        }
    }

  refs = flatpak_installation_list_remote_refs_sync (
      installation, remote_name, NULL, &local_error);
  if (refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  jobs = g_malloc0_n (refs->len, sizeof (*jobs));
  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakRemoteRef *rref                    = NULL;
      const char       *name                    = NULL;
      AsComponent      *component               = NULL;
      g_autoptr (RefRemoteAppsJobData) job_data = NULL;

      rref = g_ptr_array_index (refs, i);
      name = flatpak_ref_get_name (FLATPAK_REF (rref));
      if (blocked_names_hash != NULL && g_hash_table_contains (blocked_names_hash, name))
        continue;
      component = g_hash_table_lookup (id_hash, name);

      job_data            = ref_remote_apps_job_data_new ();
      job_data->parent    = ref_remote_apps_for_remote_data_ref (data);
      job_data->rref      = g_object_ref (rref);
      job_data->component = component != NULL ? g_object_ref (component) : NULL;
      /* TODO: this is bad, should just steal once */
      job_data->appstream_dir = g_strdup (appstream_dir_path);
      job_data->remote_icon   = remote_icon != NULL ? g_object_ref (remote_icon) : NULL;

      jobs[n_jobs++] = dex_scheduler_spawn (
          instance->scheduler, 0,
          (DexFiberFunc) ref_remote_apps_job_fiber,
          ref_remote_apps_job_data_ref (job_data),
          ref_remote_apps_job_data_unref);
    }

  if (n_jobs == 0)
    return dex_future_new_true ();

  future = dex_future_allv (jobs, n_jobs);
  for (guint i = 0; i < n_jobs; i++)
    dex_unref (jobs[i]);

  return g_steal_pointer (&future);
}

static DexFuture *
ref_remote_apps_job_fiber (RefRemoteAppsJobData *data)
{
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (BzFlatpakEntry) entry    = NULL;
  GatherEntriesUpdateData update_data = { 0 };
  DexFuture              *update      = NULL;

  entry = bz_flatpak_entry_new_for_remote_ref (
      data->parent->parent->instance,
      data->parent->installation == data->parent->parent->instance->user,
      data->parent->remote,
      data->rref,
      data->component,
      data->appstream_dir,
      data->remote_icon,
      &local_error);
  if (entry == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  update_data.parent = data->parent->parent;
  update_data.entry  = BZ_ENTRY (entry);

  update = dex_scheduler_spawn (
      data->parent->parent->home_scheduler, 0,
      (DexFiberFunc) gather_entries_job_update,
      &update_data, NULL);
  if (!dex_await (update, &local_error))
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_true ();
}

static DexFuture *
ref_installs_fiber (GatherEntriesData *data)
{
  BzFlatpakInstance *instance       = data->instance;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GPtrArray) system_refs = NULL;
  g_autoptr (GPtrArray) user_refs   = NULL;
  g_autoptr (GHashTable) ids        = NULL;

  system_refs = flatpak_installation_list_installed_refs (
      instance->system, NULL, &local_error);
  if (system_refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  user_refs = flatpak_installation_list_installed_refs (
      instance->user, NULL, &local_error);
  if (user_refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (guint i = 0; i < system_refs->len + user_refs->len; i++)
    {
      gboolean             user = FALSE;
      FlatpakInstalledRef *iref = NULL;

      if (i < system_refs->len)
        {
          user = FALSE;
          iref = g_ptr_array_index (system_refs, i);
        }
      else
        {
          user = TRUE;
          iref = g_ptr_array_index (user_refs, i - system_refs->len);
        }

      g_hash_table_add (ids, bz_flatpak_ref_format_unique (FLATPAK_REF (iref), user));
    }

  return dex_future_new_take_boxed (
      G_TYPE_HASH_TABLE, g_steal_pointer (&ids));
}

static DexFuture *
ref_updates_fiber (GatherEntriesData *data)
{
  BzFlatpakInstance *instance       = data->instance;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GPtrArray) system_refs = NULL;
  g_autoptr (GPtrArray) user_refs   = NULL;
  g_autoptr (GPtrArray) ids         = NULL;

  system_refs = flatpak_installation_list_installed_refs_for_update (
      instance->system, NULL, &local_error);
  if (system_refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  user_refs = flatpak_installation_list_installed_refs_for_update (
      instance->user, NULL, &local_error);
  if (user_refs == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  ids = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_set_size (ids, system_refs->len + user_refs->len);

  for (guint i = 0; i < system_refs->len + user_refs->len; i++)
    {
      gboolean             user = FALSE;
      FlatpakInstalledRef *iref = NULL;

      if (i < system_refs->len)
        {
          user = FALSE;
          iref = g_ptr_array_index (system_refs, i);
        }
      else
        {
          user = TRUE;
          iref = g_ptr_array_index (user_refs, i - system_refs->len);
        }

      g_ptr_array_index (ids, i) =
          bz_flatpak_ref_format_unique (FLATPAK_REF (iref), user);
    }

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY, g_steal_pointer (&ids));
}

static DexFuture *
gather_entries_job_update (GatherEntriesUpdateData *data)
{
  data->parent->progress_func (
      data->entry,
      data->parent->user_data);

  return dex_future_new_true ();
}

static DexFuture *
transaction_fiber (TransactionData *data)
{
  BzFlatpakInstance *instance                       = data->instance;
  GPtrArray         *installations                  = data->installs;
  GPtrArray         *updates                        = data->updates;
  GPtrArray         *removals                       = data->removals;
  g_autoptr (GError) local_error                    = NULL;
  g_autoptr (FlatpakTransaction) system_transaction = NULL;
  g_autoptr (FlatpakTransaction) user_transaction   = NULL;
  gboolean result                                   = FALSE;
  g_autoptr (FlatpakTransactionProgress) progress   = NULL;

  system_transaction = flatpak_transaction_new_for_installation (instance->system, NULL, &local_error);
  if (system_transaction == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));
  user_transaction = flatpak_transaction_new_for_installation (instance->user, NULL, &local_error);
  if (user_transaction == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  if (installations != NULL)
    {
      for (guint i = 0; i < installations->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (installations, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          ref_fmt = flatpak_ref_format_ref (ref);
          result  = flatpak_transaction_add_install (
              bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry))
                  ? user_transaction
                  : system_transaction,
              flatpak_remote_ref_get_remote_name (FLATPAK_REMOTE_REF (ref)),
              ref_fmt,
              NULL,
              &local_error);
          if (!result)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (updates != NULL)
    {
      for (guint i = 0; i < updates->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (updates, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          ref_fmt = flatpak_ref_format_ref (ref);
          result  = flatpak_transaction_add_update (
              bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry))
                  ? user_transaction
                  : system_transaction,
              ref_fmt,
              NULL,
              NULL,
              &local_error);
          if (!result)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (removals != NULL)
    {
      for (guint i = 0; i < removals->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (removals, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          ref_fmt = flatpak_ref_format_ref (ref);
          result  = flatpak_transaction_add_uninstall (
              bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry))
                  ? user_transaction
                  : system_transaction,
              ref_fmt,
              &local_error);
          if (!result)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (!flatpak_transaction_is_empty (system_transaction))
    {
      g_signal_connect (system_transaction, "new-operation", G_CALLBACK (transaction_new_operation), data);
      result = flatpak_transaction_run (system_transaction, NULL, &local_error);
      if (!result)
        return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  if (!flatpak_transaction_is_empty (user_transaction))
    {
      g_signal_connect (user_transaction, "new-operation", G_CALLBACK (transaction_new_operation), data);
      result = flatpak_transaction_run (user_transaction, NULL, &local_error);
      if (!result)
        return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  return dex_future_new_true ();
}

static void
transaction_new_operation (FlatpakTransaction          *transaction,
                           FlatpakTransactionOperation *operation,
                           FlatpakTransactionProgress  *progress,
                           TransactionData             *data)
{
  flatpak_transaction_progress_set_update_frequency (progress, 50);

  if (data->progress_func != NULL)
    {
      BzFlatpakEntry *entry                               = NULL;
      g_autoptr (TransactionOperationData) operation_data = NULL;

      entry = find_entry_from_operation (data, operation);

      operation_data                 = transaction_operation_data_new ();
      operation_data->parent         = transaction_data_ref (data);
      operation_data->entry          = entry != NULL ? g_object_ref (entry) : NULL;
      operation_data->timeout_handle = 0;

      g_signal_connect_data (
          progress, "changed",
          G_CALLBACK (transaction_progress_changed),
          transaction_operation_data_ref (operation_data),
          transaction_operation_data_unref_closure,
          G_CONNECT_DEFAULT);
    }
}

static BzFlatpakEntry *
find_entry_from_operation (TransactionData             *data,
                           FlatpakTransactionOperation *operation)
{
  const char     *ref_fmt        = NULL;
  BzFlatpakEntry *entry          = NULL;
  GPtrArray      *related_to_ops = NULL;

  ref_fmt = flatpak_transaction_operation_get_ref (operation);
  entry   = g_hash_table_lookup (data->ref_to_entry_hash, ref_fmt);
  if (entry != NULL)
    return entry;

  related_to_ops = flatpak_transaction_operation_get_related_to_ops (operation);
  if (related_to_ops == NULL)
    return NULL;

  for (guint i = 0; i < related_to_ops->len; i++)
    {
      FlatpakTransactionOperation *related_op = NULL;

      related_op = g_ptr_array_index (related_to_ops, i);
      entry      = find_entry_from_operation (data, related_op);
      if (entry != NULL)
        return entry;
    }

  return NULL;
}

static void
transaction_progress_changed (FlatpakTransactionProgress *progress,
                              TransactionOperationData   *data)
{
  g_autoptr (IdleTransactionData) idle_data = NULL;

  idle_data                    = idle_transaction_data_new ();
  idle_data->parent            = transaction_operation_data_ref (data);
  idle_data->status            = flatpak_transaction_progress_get_status (progress);
  idle_data->is_estimating     = flatpak_transaction_progress_get_is_estimating (progress);
  idle_data->progress_num      = flatpak_transaction_progress_get_progress (progress);
  idle_data->bytes_transferred = flatpak_transaction_progress_get_bytes_transferred (progress);
  idle_data->start_time        = flatpak_transaction_progress_get_start_time (progress);

  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) transaction_progress_idle,
      idle_transaction_data_ref (idle_data),
      idle_transaction_data_unref);

  if (idle_data->is_estimating)
    {
      if (data->timeout_handle == 0)
        /* We'll send an update periodically so the UI can pulse */
        data->timeout_handle = g_timeout_add_full (
            G_PRIORITY_DEFAULT,
            50,
            (GSourceFunc) transaction_progress_timeout,
            idle_transaction_data_ref (idle_data),
            idle_transaction_data_unref);
    }
  else
    g_clear_handle_id (&data->timeout_handle,
                       g_source_remove);
}

static gboolean
transaction_progress_idle (IdleTransactionData *data)
{
  data->parent->parent->progress_func (
      data->parent->entry != NULL
          ? BZ_ENTRY (data->parent->entry)
          : NULL,
      data->status,
      data->is_estimating,
      data->progress_num,
      data->bytes_transferred,
      data->start_time,
      data->parent->parent->user_data);

  return G_SOURCE_REMOVE;
}

static gboolean
transaction_progress_timeout (IdleTransactionData *data)
{
  data->parent->parent->progress_func (
      data->parent->entry != NULL
          ? BZ_ENTRY (data->parent->entry)
          : NULL,
      data->status,
      data->is_estimating,
      data->progress_num,
      data->bytes_transferred,
      data->start_time,
      data->parent->parent->user_data);

  return G_SOURCE_CONTINUE;
}

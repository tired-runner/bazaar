/* bz-content-provider.c
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

#include <libdex.h>
#include <yaml.h>

#include "bz-content-provider.h"
#include "bz-content-section.h"
#include "bz-entry-group.h"
#include "bz-paintable-model.h"
#include "bz-util.h"

/* clang-format off */
G_DEFINE_QUARK (bz-content-yaml-error-quark, bz_content_yaml_error);
/* clang-format on */

enum
{
  SECTION_PROP_0 = 0,

  SECTION_PROP_TITLE,
  SECTION_PROP_SUBTITLE,
  SECTION_PROP_DESCRIPTION,
  SECTION_PROP_IMAGES,
  SECTION_PROP_GROUPS,

  SECTION_PROP_N_PROPS,
};

enum
{
  NOT_INSIDE = -1,
  INSIDE_KEY = -2,
  INSIDE_VAL = -3,
};

BZ_DEFINE_DATA (
    block_counter,
    BlockCounter,
    { gint blk; },
    (void) self;)

struct _BzContentProvider
{
  GObject parent_instance;

  GHashTable *group_hash;
  GListModel *input_files;

  GListStore *input_mirror;
  GHashTable *input_tracking;

  GListStore          *outputs;
  GtkFlattenListModel *impl_model;

  BlockCounterData *block;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzContentProvider,
    bz_content_provider,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum
{
  PROP_0,

  PROP_GROUP_HASH,
  PROP_INPUT_FILES,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    input_init,
    InputInit,
    { GFile *file; },
    BZ_RELEASE_DATA (file, g_object_unref))
static DexFuture *
input_init_fiber (InputInitData *data);

BZ_DEFINE_DATA (
    input_load,
    InputLoad,
    {
      GFile      *file;
      GHashTable *group_hash;
    },
    BZ_RELEASE_DATA (file, g_object_unref);
    BZ_RELEASE_DATA (group_hash, g_hash_table_unref))
static DexFuture *
input_load_fiber (InputLoadData *data);

BZ_DEFINE_DATA (
    input_tracking,
    InputTracking,
    {
      BlockCounterData *block;
      GHashTable       *group_hash;
      char             *path;
      GFileMonitor     *monitor;
      DexFuture        *task;
      GListStore       *output;
    },
    BZ_RELEASE_DATA (block, block_counter_data_unref);
    BZ_RELEASE_DATA (group_hash, g_hash_table_unref);
    BZ_RELEASE_DATA (path, g_free);
    BZ_RELEASE_DATA (monitor, g_object_unref);
    BZ_RELEASE_DATA (task, dex_unref);
    BZ_RELEASE_DATA (output, g_object_unref))
static DexFuture *
input_init_finally (DexFuture         *future,
                    InputTrackingData *data);
static DexFuture *
input_load_finally (DexFuture         *future,
                    InputTrackingData *data);

static void
impl_model_changed (GListModel        *impl_model,
                    guint              position,
                    guint              removed,
                    guint              added,
                    BzContentProvider *self);

static void
input_files_changed (GListModel        *input_files,
                     guint              position,
                     guint              removed,
                     guint              added,
                     BzContentProvider *self);

static void
input_file_changed_on_disk (GFileMonitor      *self,
                            GFile             *file,
                            GFile             *other_file,
                            GFileMonitorEvent  event_type,
                            InputTrackingData *data);

static gboolean
commence_reload (InputTrackingData *data);

static void
bz_content_provider_dispose (GObject *object)
{
  BzContentProvider *self = BZ_CONTENT_PROVIDER (object);

  g_clear_pointer (&self->group_hash, g_hash_table_unref);
  g_clear_object (&self->input_files);
  g_clear_object (&self->input_mirror);
  g_clear_pointer (&self->input_tracking, g_hash_table_unref);
  g_clear_object (&self->outputs);
  g_clear_object (&self->impl_model);
  g_clear_pointer (&self->block, block_counter_data_unref);

  G_OBJECT_CLASS (bz_content_provider_parent_class)->dispose (object);
}

static void
bz_content_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzContentProvider *self = BZ_CONTENT_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_GROUP_HASH:
      g_value_set_boxed (value, bz_content_provider_get_group_hash (self));
      break;
    case PROP_INPUT_FILES:
      g_value_set_object (value, bz_content_provider_get_input_files (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzContentProvider *self = BZ_CONTENT_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_GROUP_HASH:
      bz_content_provider_set_group_hash (self, g_value_get_object (value));
      break;
    case PROP_INPUT_FILES:
      bz_content_provider_set_input_files (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_provider_class_init (BzContentProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_content_provider_set_property;
  object_class->get_property = bz_content_provider_get_property;
  object_class->dispose      = bz_content_provider_dispose;

  props[PROP_GROUP_HASH] =
      g_param_spec_boxed (
          "group-hash",
          NULL, NULL,
          G_TYPE_HASH_TABLE,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_INPUT_FILES] =
      g_param_spec_object (
          "input-files",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_content_provider_init (BzContentProvider *self)
{
  self->input_mirror   = g_list_store_new (G_TYPE_FILE);
  self->input_tracking = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      g_object_unref, input_tracking_data_unref);

  self->outputs    = g_list_store_new (G_TYPE_LIST_MODEL);
  self->impl_model = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->outputs)));

  g_signal_connect (
      self->impl_model, "items-changed",
      G_CALLBACK (impl_model_changed), self);

  self->block = block_counter_data_new ();
  g_atomic_int_set (&self->block->blk, 0);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return BZ_TYPE_CONTENT_SECTION;
}

static guint
list_model_get_n_items (GListModel *list)
{
  BzContentProvider *self = BZ_CONTENT_PROVIDER (list);
  return g_list_model_get_n_items (G_LIST_MODEL (self->impl_model));
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  BzContentProvider *self = BZ_CONTENT_PROVIDER (list);
  return g_list_model_get_item (G_LIST_MODEL (self->impl_model), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

BzContentProvider *
bz_content_provider_new (void)
{
  return g_object_new (BZ_TYPE_CONTENT_PROVIDER, NULL);
}

void
bz_content_provider_set_input_files (BzContentProvider *self,
                                     GListModel        *input_files)
{
  guint old_length = 0;

  g_return_if_fail (BZ_IS_CONTENT_PROVIDER (self));
  g_return_if_fail (input_files == NULL || G_IS_LIST_MODEL (input_files));

  if (self->input_files != NULL)
    {
      old_length = g_list_model_get_n_items (self->input_files);
      g_signal_handlers_disconnect_by_func (
          self->input_files, input_files_changed, self);
    }
  g_clear_object (&self->input_files);

  g_hash_table_remove_all (self->input_tracking);
  g_list_store_remove_all (self->input_mirror);
  g_list_store_remove_all (self->outputs);

  if (input_files != NULL)
    {
      self->input_files = g_object_ref (input_files);
      g_signal_connect (
          input_files, "items-changed",
          G_CALLBACK (input_files_changed), self);

      input_files_changed (
          input_files, 0, old_length,
          g_list_model_get_n_items (input_files),
          self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INPUT_FILES]);
}

GListModel *
bz_content_provider_get_input_files (BzContentProvider *self)
{
  g_return_val_if_fail (BZ_IS_CONTENT_PROVIDER (self), NULL);
  return self->input_files;
}

void
bz_content_provider_set_group_hash (BzContentProvider *self,
                                    GHashTable        *group_hash)
{
  g_return_if_fail (BZ_IS_CONTENT_PROVIDER (self));

  g_clear_pointer (&self->group_hash, g_hash_table_unref);
  if (group_hash != NULL)
    self->group_hash = g_hash_table_ref (group_hash);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP_HASH]);
}

GHashTable *
bz_content_provider_get_group_hash (BzContentProvider *self)
{
  g_return_val_if_fail (BZ_IS_CONTENT_PROVIDER (self), NULL);
  return self->group_hash;
}

void
bz_content_provider_block (BzContentProvider *self)
{
  g_return_if_fail (BZ_IS_CONTENT_PROVIDER (self));

  g_atomic_int_inc (&self->block->blk);

  if (self->input_files != NULL)
    {
      guint n_inputs = 0;

      n_inputs = g_list_model_get_n_items (self->input_files);
      for (guint i = 0; i < n_inputs; i++)
        {
          g_autoptr (GFile) file  = NULL;
          InputTrackingData *data = NULL;

          file = g_list_model_get_item (self->input_files, i);
          data = g_hash_table_lookup (self->input_tracking, file);
          g_assert (data != NULL);

          dex_clear (&data->task);
        }
    }
}

void
bz_content_provider_unblock (BzContentProvider *self)
{
  g_return_if_fail (BZ_IS_CONTENT_PROVIDER (self));

  if (g_atomic_int_dec_and_test (&self->block->blk) &&
      self->input_files != NULL)
    {
      guint n_inputs = 0;

      n_inputs = g_list_model_get_n_items (self->input_files);
      for (guint i = 0; i < n_inputs; i++)
        {
          g_autoptr (GFile) file  = NULL;
          InputTrackingData *data = NULL;

          file = g_list_model_get_item (self->input_files, i);
          data = g_hash_table_lookup (self->input_tracking, file);
          g_assert (data != NULL);

          commence_reload (data);
        }
    }
}

static void
impl_model_changed (GListModel        *impl_model,
                    guint              position,
                    guint              removed,
                    guint              added,
                    BzContentProvider *self)
{
  g_list_model_items_changed (
      G_LIST_MODEL (self), position, removed, added);
}

static void
input_files_changed (GListModel        *input_files,
                     guint              position,
                     guint              removed,
                     guint              added,
                     BzContentProvider *self)
{
  g_autofree GFile      **additions   = NULL;
  g_autofree GListStore **new_outputs = NULL;

  if (removed > 0)
    {
      for (guint i = 0; i < removed; i++)
        {
          g_autoptr (GFile) removal = NULL;
          InputTrackingData *data   = NULL;

          removal = g_list_model_get_item (
              G_LIST_MODEL (self->input_mirror), position + i);
          data = g_hash_table_lookup (self->input_tracking, removal);
          g_assert (data != NULL);

          dex_clear (&data->task);
          g_hash_table_remove (self->input_tracking, removal);
        }
    }

  if (added > 0)
    {
      additions = g_malloc0_n (added, sizeof (*additions));
      for (guint i = 0; i < added; i++)
        additions[i] = g_list_model_get_item (
            G_LIST_MODEL (self->input_files), position + i);

      new_outputs = g_malloc0_n (added, sizeof (*new_outputs));
      for (guint i = 0; i < added; i++)
        new_outputs[i] = g_list_store_new (BZ_TYPE_CONTENT_SECTION);
    }

  g_list_store_splice (self->input_mirror, position, removed,
                       (gpointer *) additions, added);
  g_list_store_splice (self->outputs, position, removed,
                       (gpointer *) new_outputs, added);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (InputInitData) init_data = NULL;
      g_autoptr (InputTrackingData) data  = NULL;
      DexFuture *future                   = NULL;

      init_data       = input_init_data_new ();
      init_data->file = g_object_ref (additions[i]);

      future = dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          0, (DexFiberFunc) input_init_fiber,
          input_init_data_ref (init_data),
          input_init_data_unref);

      data             = input_tracking_data_new ();
      data->block      = block_counter_data_ref (self->block);
      data->group_hash = self->group_hash != NULL ? g_hash_table_ref (self->group_hash) : NULL;
      data->path       = g_file_get_path (additions[i]);
      data->output     = g_steal_pointer (&new_outputs[i]);

      future = dex_future_finally (
          future,
          (DexFutureCallback) input_init_finally,
          input_tracking_data_ref (data),
          input_tracking_data_unref);
      data->task = future;

      g_hash_table_replace (
          self->input_tracking,
          g_steal_pointer (&additions[i]),
          input_tracking_data_ref (data));
    }
}

static void
input_file_changed_on_disk (GFileMonitor      *self,
                            GFile             *file,
                            GFile             *other_file,
                            GFileMonitorEvent  event_type,
                            InputTrackingData *data)
{
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
    g_idle_add_full (
        G_PRIORITY_DEFAULT,
        (GSourceFunc) commence_reload,
        input_tracking_data_ref (data),
        input_tracking_data_unref);
}

static DexFuture *
input_init_fiber (InputInitData *data)
{
  GFile *file                      = data->file;
  g_autoptr (GError) local_error   = NULL;
  g_autoptr (GFileMonitor) monitor = NULL;

  monitor = g_file_monitor_file (
      file, G_FILE_MONITOR_NONE, NULL, &local_error);
  if (monitor == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_for_object (monitor);
}

static DexFuture *
input_init_finally (DexFuture         *future,
                    InputTrackingData *data)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  dex_clear (&data->task);
  g_list_store_remove_all (data->output);

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      data->monitor = g_value_dup_object (value);
      g_signal_connect (
          data->monitor, "changed",
          G_CALLBACK (input_file_changed_on_disk),
          data);

      commence_reload (data);
    }
  else
    {
      g_autoptr (BzContentSection) error_section = NULL;

      error_section = g_object_new (
          BZ_TYPE_CONTENT_SECTION,
          "error", local_error->message,
          NULL);
      g_list_store_append (data->output, error_section);
    }

  return NULL;
}

static DexFuture *
input_load_fiber (InputLoadData *data)
{
  GFile      *file                              = data->file;
  GHashTable *group_hash                        = data->group_hash;
  g_autoptr (GPtrArray) sections                = NULL;
  g_autoptr (GError) local_error                = NULL;
  gsize bytes_size                              = 0;
  g_autoptr (GBytes) bytes                      = NULL;
  const guchar *bytes_data                      = NULL;
  yaml_parser_t parser                          = { 0 };
  gboolean      done_parsing                    = FALSE;
  int           inside_sections                 = NOT_INSIDE;
  g_autoptr (BzContentSection) current_section  = NULL;
  int current_section_prop                      = NOT_INSIDE;
  g_autoptr (GListStore) current_section_images = NULL;
  g_autoptr (GListStore) current_section_groups = NULL;

  sections = g_ptr_array_new_with_free_func (g_object_unref);

  bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &local_error);
  if (bytes == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  bytes_data = g_bytes_get_data (bytes, &bytes_size);

  yaml_parser_initialize (&parser);
  yaml_parser_set_input_string (&parser, bytes_data, bytes_size);

  /* TODO: completely rework this to take a schema or something */
  while (!done_parsing)
    {
      yaml_event_t event = { 0 };

      if (!yaml_parser_parse (&parser, &event))
        {
          DexFuture *ret = NULL;

          ret = dex_future_new_reject (
              BZ_CONTENT_YAML_ERROR,
              BZ_CONTENT_YAML_ERROR_INVALID_YAML,
              "Failed to parse YAML at byte offset %zu: %s",
              parser.problem_offset,
              parser.problem);
          yaml_parser_delete (&parser);
          return ret;
        }

#define ERROR_OUT(...)                                           \
  G_STMT_START                                                   \
  {                                                              \
    g_autofree char *formatted = NULL;                           \
    DexFuture       *ret       = NULL;                           \
                                                                 \
    formatted = g_strdup_printf (__VA_ARGS__);                   \
                                                                 \
    ret = dex_future_new_reject (                                \
        BZ_CONTENT_YAML_ERROR,                                   \
        BZ_CONTENT_YAML_ERROR_INVALID_STRUCTURE,                 \
        "Failed to validate YAML at byte region %zu to %zu: %s", \
        event.start_mark.index,                                  \
        event.end_mark.index,                                    \
        formatted);                                              \
                                                                 \
    yaml_event_delete (&event);                                  \
    yaml_parser_delete (&parser);                                \
    return ret;                                                  \
  }                                                              \
  G_STMT_END

      if (inside_sections == INSIDE_KEY)
        {
          if (event.type == YAML_MAPPING_END_EVENT)
            inside_sections = NOT_INSIDE;
          else if (event.type != YAML_SCALAR_EVENT)
            ERROR_OUT ("Expected a scalar");
          else if (g_strcmp0 ((const char *) event.data.scalar.value, "sections") != 0)
            ERROR_OUT ("Unexpected scalar '%s'", event.data.scalar.value);
          else
            inside_sections = INSIDE_VAL;
        }
      else if (inside_sections == INSIDE_VAL)
        {
          if (current_section != NULL)
            {
              if (current_section_prop == INSIDE_KEY)
                {
                  if (event.type == YAML_MAPPING_END_EVENT)
                    g_ptr_array_add (sections, g_steal_pointer (&current_section));
                  else if (event.type != YAML_SCALAR_EVENT)
                    ERROR_OUT ("Expected a scalar");
                  else if (g_strcmp0 ((const char *) event.data.scalar.value, "title") == 0)
                    current_section_prop = SECTION_PROP_TITLE;
                  else if (g_strcmp0 ((const char *) event.data.scalar.value, "subtitle") == 0)
                    current_section_prop = SECTION_PROP_SUBTITLE;
                  else if (g_strcmp0 ((const char *) event.data.scalar.value, "description") == 0)
                    current_section_prop = SECTION_PROP_DESCRIPTION;
                  else if (g_strcmp0 ((const char *) event.data.scalar.value, "images") == 0)
                    current_section_prop = SECTION_PROP_IMAGES;
                  else if (g_strcmp0 ((const char *) event.data.scalar.value, "appids") == 0)
                    current_section_prop = SECTION_PROP_GROUPS;
                  else
                    ERROR_OUT ("Unexpected section property '%s'", event.data.scalar.value);
                }
              else if (current_section_prop > SECTION_PROP_0)
                {
                  switch (current_section_prop)
                    {
                    case SECTION_PROP_TITLE:
                      if (event.type != YAML_SCALAR_EVENT)
                        ERROR_OUT ("Expected a scalar");
                      else
                        {
                          g_object_set (current_section, "title", event.data.scalar.value, NULL);
                          current_section_prop = INSIDE_KEY;
                        }
                      break;
                    case SECTION_PROP_SUBTITLE:
                      if (event.type != YAML_SCALAR_EVENT)
                        ERROR_OUT ("Expected a scalar");
                      else
                        {
                          g_object_set (current_section, "subtitle", event.data.scalar.value, NULL);
                          current_section_prop = INSIDE_KEY;
                        }
                      break;
                    case SECTION_PROP_DESCRIPTION:
                      if (event.type != YAML_SCALAR_EVENT)
                        ERROR_OUT ("Expected a scalar");
                      else
                        {
                          g_object_set (current_section, "description", event.data.scalar.value, NULL);
                          current_section_prop = INSIDE_KEY;
                        }
                      break;
                    case SECTION_PROP_IMAGES:
                      if (current_section_images != NULL)
                        {
                          if (event.type == YAML_SEQUENCE_END_EVENT)
                            {
                              g_autoptr (BzPaintableModel) model = NULL;

                              model = bz_paintable_model_new (
                                  dex_thread_pool_scheduler_get_default (),
                                  G_LIST_MODEL (current_section_images));
                              g_object_set (current_section, "images", model, NULL);
                              g_clear_object (&current_section_images);

                              current_section_prop = INSIDE_KEY;
                            }
                          else if (event.type != YAML_SCALAR_EVENT)
                            ERROR_OUT ("Expected a scalar");
                          else
                            {
                              g_autoptr (GFile) url = NULL;

                              url = g_file_new_for_uri ((const char *) event.data.scalar.value);
                              g_list_store_append (current_section_images, url);
                            }
                        }
                      else if (event.type != YAML_SEQUENCE_START_EVENT)
                        ERROR_OUT ("Expected a start of a sequence");
                      else
                        current_section_images = g_list_store_new (G_TYPE_FILE);
                      break;
                    case SECTION_PROP_GROUPS:
                      if (current_section_groups != NULL)
                        {
                          if (event.type == YAML_SEQUENCE_END_EVENT)
                            {
                              if (g_list_model_get_n_items (G_LIST_MODEL (current_section_groups)) == 0)
                                ERROR_OUT ("A section must have at least one appid");
                              else
                                {
                                  g_object_set (current_section, "groups", current_section_groups, NULL);
                                  g_clear_object (&current_section_groups);
                                  current_section_prop = INSIDE_KEY;
                                }
                            }
                          else if (event.type != YAML_SCALAR_EVENT)
                            ERROR_OUT ("Expected a scalar");
                          else
                            {
                              BzEntryGroup *group = NULL;

                              if (group_hash != NULL)
                                group = g_hash_table_lookup (group_hash, event.data.scalar.value);
                              if (group != NULL)
                                g_list_store_append (current_section_groups, group);
                              else
                                g_warning ("appid '%s' was not found", event.data.scalar.value);
                            }
                        }
                      else if (event.type != YAML_SEQUENCE_START_EVENT)
                        ERROR_OUT ("Expected a start of a sequence");
                      else
                        current_section_groups = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
                      break;
                    default:
                      g_assert_not_reached ();
                    }
                }
              else if (event.type != YAML_MAPPING_START_EVENT)
                ERROR_OUT ("Expected a mapping");
              else
                current_section_prop = INSIDE_KEY;
            }
          else if (event.type == YAML_SEQUENCE_END_EVENT)
            {
              current_section_prop = NOT_INSIDE;
              inside_sections      = NOT_INSIDE;
            }
          else if (current_section_prop != INSIDE_KEY &&
                   event.type != YAML_SEQUENCE_START_EVENT)
            ERROR_OUT ("Expected a start of a sequence");
          else
            current_section = g_object_new (BZ_TYPE_CONTENT_SECTION, NULL);
        }
      else if (event.type == YAML_MAPPING_START_EVENT)
        inside_sections = INSIDE_KEY;

      done_parsing = event.type == YAML_STREAM_END_EVENT;
      yaml_event_delete (&event);
    }
  yaml_parser_delete (&parser);

#undef ERROR_OUT

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY,
      g_steal_pointer (&sections));
}

static DexFuture *
input_load_finally (DexFuture         *future,
                    InputTrackingData *data)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  dex_clear (&data->task);
  g_list_store_remove_all (data->output);

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      GPtrArray *sections = NULL;

      sections = g_value_get_boxed (value);
      g_list_store_splice (data->output, 0, 0, sections->pdata, sections->len);
    }
  else
    {
      g_autoptr (BzContentSection) error_section = NULL;

      error_section = g_object_new (
          BZ_TYPE_CONTENT_SECTION,
          "error", local_error->message,
          NULL);
      g_list_store_append (data->output, error_section);
    }

  return NULL;
}

static gboolean
commence_reload (InputTrackingData *data)
{
  g_autoptr (InputLoadData) load_data = NULL;
  DexFuture *future                   = NULL;

  dex_clear (&data->task);
  if (g_atomic_int_get (&data->block->blk) != 0)
    goto done;

  load_data             = input_load_data_new ();
  load_data->group_hash = g_hash_table_ref (data->group_hash);
  load_data->file       = g_file_new_for_path (data->path);

  future = dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      0, (DexFiberFunc) input_load_fiber,
      input_load_data_ref (load_data),
      input_load_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) input_load_finally,
      input_tracking_data_ref (data),
      input_tracking_data_unref);
  data->task = future;

done:
  return G_SOURCE_REMOVE;
}

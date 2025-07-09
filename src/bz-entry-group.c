/* bz-entry-group.c
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

#include "bz-entry-group.h"

struct _BzEntryGroup
{
  GObject parent_instance;

  GListStore *store;
  BzEntry    *ui_entry;

  GPtrArray *installable;
  GPtrArray *updatable;
  GPtrArray *removable;
};

G_DEFINE_FINAL_TYPE (BzEntryGroup, bz_entry_group, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_UI_ENTRY,
  PROP_REMOTE_REPOS_STRING,
  PROP_INSTALLABLE,
  PROP_UPDATABLE,
  PROP_REMOVABLE,
  PROP_INSTALLABLE_AND_AVAILABLE,
  PROP_UPDATABLE_AND_AVAILABLE,
  PROP_REMOVABLE_AND_AVAILABLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
holding_changed (BzEntryGroup *self,
                 GParamSpec   *pspec,
                 BzEntry      *entry);

static void
bz_entry_group_dispose (GObject *object)
{
  BzEntryGroup *self    = BZ_ENTRY_GROUP (object);
  guint         n_items = 0;

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (G_LIST_MODEL (self->store), i);
      g_signal_handlers_disconnect_by_func (entry, holding_changed, self);
    }

  g_clear_object (&self->store);
  g_clear_object (&self->ui_entry);
  g_clear_pointer (&self->installable, g_ptr_array_unref);
  g_clear_pointer (&self->updatable, g_ptr_array_unref);
  g_clear_pointer (&self->removable, g_ptr_array_unref);

  G_OBJECT_CLASS (bz_entry_group_parent_class)->dispose (object);
}

static void
bz_entry_group_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BzEntryGroup *self = BZ_ENTRY_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->store);
      break;
    case PROP_UI_ENTRY:
      g_value_set_object (value, self->ui_entry);
      break;
    case PROP_REMOTE_REPOS_STRING:
      {
        guint n_items = 0;

        /* TODO: bring this into a function and cache result */

        n_items = g_list_model_get_n_items (G_LIST_MODEL (self->store));
        if (n_items > 0)
          {
            g_autoptr (GHashTable) set = NULL;
            g_autoptr (GPtrArray) vals = NULL;
            g_autoptr (GString) joined = NULL;

            set = g_hash_table_new (g_str_hash, g_str_equal);
            for (guint i = 0; i < n_items; i++)
              {
                g_autoptr (BzEntry) entry = NULL;
                const char *remote_repo   = NULL;

                entry       = g_list_model_get_item (G_LIST_MODEL (self->store), i);
                remote_repo = bz_entry_get_remote_repo_name (entry);

                g_hash_table_add (set, (gpointer) remote_repo);
              }

            vals   = g_hash_table_get_values_as_ptr_array (set);
            joined = g_string_new (g_ptr_array_index (vals, 0));

            for (guint i = 1; i < vals->len; i++)
              g_string_append_printf (
                  joined,
                  ", %s",
                  (const char *) g_ptr_array_index (vals, i));

            g_value_set_string (value, joined->str);
          }
        else
          g_value_set_string (value, NULL);
      }
      break;
    case PROP_INSTALLABLE:
      g_value_set_int (value, bz_entry_group_get_installable (self));
      break;
    case PROP_UPDATABLE:
      g_value_set_int (value, bz_entry_group_get_updatable (self));
      break;
    case PROP_REMOVABLE:
      g_value_set_int (value, bz_entry_group_get_removable (self));
      break;
    case PROP_INSTALLABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_installable_and_available (self));
      break;
    case PROP_UPDATABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_updatable_and_available (self));
      break;
    case PROP_REMOVABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_removable_and_available (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_group_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  // BzEntryGroup *self = BZ_ENTRY_GROUP (object);

  switch (prop_id)
    {
    case PROP_INSTALLABLE:
    case PROP_UPDATABLE:
    case PROP_REMOVABLE:
    case PROP_MODEL:
    case PROP_UI_ENTRY:
    case PROP_REMOTE_REPOS_STRING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_group_class_init (BzEntryGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_group_set_property;
  object_class->get_property = bz_entry_group_get_property;
  object_class->dispose      = bz_entry_group_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE);

  props[PROP_UI_ENTRY] =
      g_param_spec_object (
          "ui-entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READABLE);

  props[PROP_REMOTE_REPOS_STRING] =
      g_param_spec_string (
          "remote-repos-string",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_INSTALLABLE] =
      g_param_spec_int (
          "installable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_UPDATABLE] =
      g_param_spec_int (
          "updatable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_REMOVABLE] =
      g_param_spec_int (
          "removable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_INSTALLABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "installable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_UPDATABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "updatable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_REMOVABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "removable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_group_init (BzEntryGroup *self)
{
  self->store       = g_list_store_new (BZ_TYPE_ENTRY);
  self->installable = g_ptr_array_new_with_free_func (g_object_unref);
  self->updatable   = g_ptr_array_new_with_free_func (g_object_unref);
  self->removable   = g_ptr_array_new_with_free_func (g_object_unref);
}

BzEntryGroup *
bz_entry_group_new (void)
{
  return g_object_new (BZ_TYPE_ENTRY_GROUP, NULL);
}

GListModel *
bz_entry_group_get_model (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return G_LIST_MODEL (self->store);
}

BzEntry *
bz_entry_group_get_ui_entry (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->ui_entry;
}

int
bz_entry_group_get_installable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->installable->len;
}

int
bz_entry_group_get_updatable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->updatable->len;
}

int
bz_entry_group_get_removable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->removable->len;
}

int
bz_entry_group_get_installable_and_available (BzEntryGroup *self)
{
  int available = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);

  for (guint i = 0; i < self->installable->len; i++)
    {
      BzEntry *entry = NULL;

      entry = g_ptr_array_index (self->installable, i);
      if (!bz_entry_is_holding (entry))
        available++;
    }

  return available;
}

int
bz_entry_group_get_updatable_and_available (BzEntryGroup *self)
{
  int available = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);

  for (guint i = 0; i < self->updatable->len; i++)
    {
      BzEntry *entry = NULL;

      entry = g_ptr_array_index (self->updatable, i);
      if (!bz_entry_is_holding (entry))
        available++;
    }

  return available;
}

int
bz_entry_group_get_removable_and_available (BzEntryGroup *self)
{
  int available = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);

  for (guint i = 0; i < self->removable->len; i++)
    {
      BzEntry *entry = NULL;

      entry = g_ptr_array_index (self->removable, i);
      if (!bz_entry_is_holding (entry))
        available++;
    }

  return available;
}

void
bz_entry_group_add (BzEntryGroup *self,
                    BzEntry      *entry,
                    gboolean      installable,
                    gboolean      updatable,
                    gboolean      removable)
{
  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (BZ_IS_ENTRY (entry));

  g_list_store_insert_sorted (self->store, entry, bz_entry_cmp_usefulness, NULL);

  g_clear_object (&self->ui_entry);
  self->ui_entry = g_list_model_get_item (G_LIST_MODEL (self->store), 0);

  if (installable)
    {
      g_ptr_array_add (self->installable, g_object_ref (entry));
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
      if (!bz_entry_is_holding (entry))
        g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
    }
  if (updatable)
    {
      g_ptr_array_add (self->updatable, g_object_ref (entry));
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UPDATABLE]);
      if (!bz_entry_is_holding (entry))
        g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UPDATABLE_AND_AVAILABLE]);
    }
  if (removable)
    {
      g_ptr_array_add (self->removable, g_object_ref (entry));
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
      if (!bz_entry_is_holding (entry))
        g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
    }

  g_signal_connect_swapped (entry, "notify::holding", G_CALLBACK (holding_changed), self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UI_ENTRY]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOTE_REPOS_STRING]);
}

void
bz_entry_group_install (BzEntryGroup *self,
                        BzEntry      *entry)
{
  guint idx = 0;

  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (BZ_IS_ENTRY (entry));

  if (!g_ptr_array_find (self->installable, entry, &idx))
    {
      g_critical ("Entry %s not found to be installable!", bz_entry_get_unique_id (entry));
      return;
    }

  g_ptr_array_remove_fast (self->installable, entry);
  g_ptr_array_add (self->removable, g_object_ref (entry));

  g_object_set (entry, "installed", TRUE, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
}

void
bz_entry_group_remove (BzEntryGroup *self,
                       BzEntry      *entry)
{
  guint idx = 0;

  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (BZ_IS_ENTRY (entry));

  if (!g_ptr_array_find (self->removable, entry, &idx))
    {
      g_critical ("Entry %s not found to be removable!", bz_entry_get_unique_id (entry));
      return;
    }

  g_ptr_array_remove_fast (self->removable, entry);
  g_ptr_array_add (self->installable, g_object_ref (entry));

  g_object_set (entry, "installed", FALSE, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
}

gboolean
bz_entry_group_query_installable (BzEntryGroup *self,
                                  BzEntry      *entry)
{
  guint idx = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  g_return_val_if_fail (BZ_IS_ENTRY (entry), FALSE);

  return g_ptr_array_find (self->installable, entry, &idx);
}

gboolean
bz_entry_group_query_updatable (BzEntryGroup *self,
                                BzEntry      *entry)
{
  guint idx = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  g_return_val_if_fail (BZ_IS_ENTRY (entry), FALSE);

  return g_ptr_array_find (self->updatable, entry, &idx);
}

gboolean
bz_entry_group_query_removable (BzEntryGroup *self,
                                BzEntry      *entry)
{
  guint idx = 0;

  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  g_return_val_if_fail (BZ_IS_ENTRY (entry), FALSE);

  return g_ptr_array_find (self->removable, entry, &idx);
}

static void
holding_changed (BzEntryGroup *self,
                 GParamSpec   *pspec,
                 BzEntry      *entry)
{
  if (bz_entry_group_query_installable (self, entry))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
  if (bz_entry_group_query_updatable (self, entry))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UPDATABLE_AND_AVAILABLE]);
  if (bz_entry_group_query_removable (self, entry))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
}

/* bz-entry.c
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

/* TODO: clean this mess up */

#define G_LOG_DOMAIN  "BAZAAR::ENTRY"
#define BAZAAR_MODULE "entry"

#include <json-glib/json-glib.h>

#include "bz-async-texture.h"
#include "bz-country-data-point.h"
#include "bz-data-point.h"
#include "bz-entry.h"
#include "bz-env.h"
#include "bz-global-state.h"
#include "bz-io.h"
#include "bz-issue.h"
#include "bz-release.h"
#include "bz-serializable.h"
#include "bz-url.h"
#include "bz-util.h"

G_DEFINE_FLAGS_TYPE (
    BzEntryKind,
    bz_entry_kind,
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_APPLICATION, "application"),
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_RUNTIME, "runtime"),
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_ADDON, "addon"))

typedef struct
{
  gint     hold;
  gboolean installed;

  guint         kinds;
  GListModel   *addons;
  char         *id;
  char         *unique_id;
  char         *unique_id_checksum;
  char         *title;
  char         *eol;
  char         *description;
  char         *long_description;
  char         *remote_repo_name;
  char         *url;
  guint64       size;
  GdkPaintable *icon_paintable;
  GIcon        *mini_icon;
  GdkPaintable *remote_repo_icon;
  GPtrArray    *search_tokens;
  char         *metadata_license;
  char         *project_license;
  gboolean      is_floss;
  char         *project_group;
  char         *developer;
  char         *developer_id;
  GListModel   *screenshot_paintables;
  GListModel   *share_urls;
  char         *donation_url;
  char         *forge_url;
  GListModel   *reviews;
  double        average_rating;
  char         *ratings_summary;
  GListModel   *version_history;
  char         *light_accent_color;
  char         *dark_accent_color;

  gboolean    is_flathub;
  gboolean    verified;
  GListModel *download_stats;
  GListModel *download_stats_per_country;
  int         recent_downloads;

  GHashTable *flathub_prop_queries;
  DexFuture  *mini_icon_future;
} BzEntryPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzEntry, bz_entry, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_HOLDING,
  PROP_INSTALLED,
  PROP_KINDS,
  PROP_ADDONS,
  PROP_ID,
  PROP_UNIQUE_ID,
  PROP_UNIQUE_ID_CHECKSUM,
  PROP_TITLE,
  PROP_EOL,
  PROP_DESCRIPTION,
  PROP_DOWNLOAD_STATS_PER_COUNTRY,
  PROP_LONG_DESCRIPTION,
  PROP_REMOTE_REPO_NAME,
  PROP_URL,
  PROP_SIZE,
  PROP_ICON_PAINTABLE,
  PROP_MINI_ICON,
  PROP_SEARCH_TOKENS,
  PROP_REMOTE_REPO_ICON,
  PROP_METADATA_LICENSE,
  PROP_PROJECT_LICENSE,
  PROP_IS_FLOSS,
  PROP_PROJECT_GROUP,
  PROP_DEVELOPER,
  PROP_DEVELOPER_ID,
  PROP_SCREENSHOT_PAINTABLES,
  PROP_SHARE_URLS,
  PROP_DONATION_URL,
  PROP_FORGE_URL,
  PROP_REVIEWS,
  PROP_AVERAGE_RATING,
  PROP_RATINGS_SUMMARY,
  PROP_VERSION_HISTORY,
  PROP_IS_FLATHUB,
  PROP_VERIFIED,
  PROP_DOWNLOAD_STATS,
  PROP_RECENT_DOWNLOADS,
  PROP_LIGHT_ACCENT_COLOR,
  PROP_DARK_ACCENT_COLOR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    query_flathub,
    QueryFlathub,
    {
      GWeakRef self;
      int      prop;
      char    *id;
    },
    g_weak_ref_clear (&self->self);
    BZ_RELEASE_DATA (id, g_free));
static DexFuture *
query_flathub_fiber (QueryFlathubData *data);
static DexFuture *
query_flathub_then (DexFuture        *future,
                    QueryFlathubData *data);

static void
query_flathub (BzEntry *self,
               int      prop);

static void
download_stats_per_day_foreach (JsonObject  *object,
                                const gchar *member_name,
                                JsonNode    *member_node,
                                GListStore  *store);
static void
download_stats_per_country_foreach (JsonObject  *object,
                                    const gchar *member_name,
                                    JsonNode    *member_node,
                                    GListStore  *store);

static gboolean
maybe_save_paintable (BzEntryPrivate  *priv,
                      const char      *key,
                      GdkPaintable    *paintable,
                      GVariantBuilder *builder);

static GdkPaintable *
make_async_texture (GVariant *parse);

static DexFuture *
icon_paintable_future_then (DexFuture *future,
                            BzEntry   *entry);

BZ_DEFINE_DATA (
    load_mini_icon,
    LoadMiniIcon,
    {
      BzEntry *self;
      char    *path;
      GIcon   *result;
    },
    BZ_RELEASE_DATA (self, g_object_unref);
    BZ_RELEASE_DATA (path, g_free);
    BZ_RELEASE_DATA (result, g_object_unref))
static DexFuture *
load_mini_icon_fiber (LoadMiniIconData *data);
static DexFuture *
load_mini_icon_notify (LoadMiniIconData *data);

static GIcon *
load_mini_icon_sync (const char *unique_id_checksum,
                     const char *path);

static void
clear_entry (BzEntry *self);

static void
bz_entry_dispose (GObject *object)
{
  BzEntry *self = BZ_ENTRY (object);

  clear_entry (self);

  G_OBJECT_CLASS (bz_entry_parent_class)->dispose (object);
}

static void
bz_entry_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HOLDING:
      g_value_set_boolean (value, bz_entry_is_holding (self));
      break;
    case PROP_INSTALLED:
      g_value_set_boolean (value, priv->installed);
      break;
    case PROP_ADDONS:
      g_value_set_object (value, priv->addons);
      break;
    case PROP_KINDS:
      g_value_set_flags (value, priv->kinds);
      break;
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_UNIQUE_ID:
      g_value_set_string (value, priv->unique_id);
      break;
    case PROP_UNIQUE_ID_CHECKSUM:
      g_value_set_string (value, priv->unique_id_checksum);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_EOL:
      g_value_set_string (value, priv->eol);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_LONG_DESCRIPTION:
      g_value_set_string (value, priv->long_description);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_value_set_string (value, priv->remote_repo_name);
      break;
    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;
    case PROP_SIZE:
      g_value_set_uint64 (value, priv->size);
      break;
    case PROP_ICON_PAINTABLE:
      g_value_set_object (value, priv->icon_paintable);
      dex_unref (bz_entry_load_mini_icon (self));
      break;
    case PROP_MINI_ICON:
      g_value_set_object (value, priv->mini_icon);
      break;
    case PROP_SEARCH_TOKENS:
      g_value_set_boxed (value, priv->search_tokens);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_value_set_object (value, priv->remote_repo_icon);
      break;
    case PROP_METADATA_LICENSE:
      g_value_set_string (value, priv->metadata_license);
      break;
    case PROP_PROJECT_LICENSE:
      g_value_set_string (value, priv->project_license);
      break;
    case PROP_IS_FLOSS:
      g_value_set_boolean (value, priv->is_floss);
      break;
    case PROP_PROJECT_GROUP:
      g_value_set_string (value, priv->project_group);
      break;
    case PROP_DEVELOPER:
      g_value_set_string (value, priv->developer);
      break;
    case PROP_DEVELOPER_ID:
      g_value_set_string (value, priv->developer_id);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_value_set_object (value, priv->screenshot_paintables);
      break;
    case PROP_SHARE_URLS:
      g_value_set_object (value, priv->share_urls);
      break;
    case PROP_DONATION_URL:
      g_value_set_string (value, priv->donation_url);
      break;
    case PROP_FORGE_URL:
      g_value_set_string (value, priv->forge_url);
      break;
    case PROP_REVIEWS:
      g_value_set_object (value, priv->reviews);
      break;
    case PROP_AVERAGE_RATING:
      g_value_set_double (value, priv->average_rating);
      break;
    case PROP_RATINGS_SUMMARY:
      g_value_set_string (value, priv->ratings_summary);
      break;
    case PROP_VERSION_HISTORY:
      g_value_set_object (value, priv->version_history);
      break;
    case PROP_LIGHT_ACCENT_COLOR:
      g_value_set_string (value, priv->light_accent_color);
      break;
    case PROP_DARK_ACCENT_COLOR:
      g_value_set_string (value, priv->dark_accent_color);
      break;
    case PROP_IS_FLATHUB:
      g_value_set_boolean (value, priv->is_flathub);
      break;
    case PROP_VERIFIED:
      query_flathub (self, PROP_VERIFIED);
      g_value_set_boolean (value, priv->verified);
      break;
    case PROP_DOWNLOAD_STATS:
      query_flathub (self, PROP_DOWNLOAD_STATS);
      g_value_set_object (value, priv->download_stats);
      break;
    case PROP_DOWNLOAD_STATS_PER_COUNTRY:
      query_flathub (self, PROP_DOWNLOAD_STATS_PER_COUNTRY);
      g_value_set_object (value, priv->download_stats_per_country);
      break;
    case PROP_RECENT_DOWNLOADS:
      query_flathub (self, PROP_DOWNLOAD_STATS);
      g_value_set_int (value, priv->recent_downloads);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INSTALLED:
      priv->installed = g_value_get_boolean (value);
      break;
    case PROP_ADDONS:
      g_clear_object (&priv->addons);
      priv->addons = g_value_dup_object (value);
      break;
    case PROP_KINDS:
      priv->kinds = g_value_get_flags (value);
      break;
    case PROP_ID:
      g_clear_pointer (&priv->id, g_free);
      priv->id = g_value_dup_string (value);
      break;
    case PROP_UNIQUE_ID:
      g_clear_pointer (&priv->unique_id, g_free);
      priv->unique_id = g_value_dup_string (value);
      break;
    case PROP_UNIQUE_ID_CHECKSUM:
      g_clear_pointer (&priv->unique_id_checksum, g_free);
      priv->unique_id_checksum = g_value_dup_string (value);
      break;
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_EOL:
      g_clear_pointer (&priv->eol, g_free);
      priv->eol = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_LONG_DESCRIPTION:
      g_clear_pointer (&priv->long_description, g_free);
      priv->long_description = g_value_dup_string (value);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_clear_pointer (&priv->remote_repo_name, g_free);
      priv->remote_repo_name = g_value_dup_string (value);
      priv->is_flathub       = g_strcmp0 (priv->remote_repo_name, "flathub") == 0;
      g_object_notify_by_pspec (object, props[PROP_IS_FLATHUB]);
      break;
    case PROP_URL:
      g_clear_pointer (&priv->url, g_free);
      priv->url = g_value_dup_string (value);
      break;
    case PROP_SIZE:
      priv->size = g_value_get_uint64 (value);
      break;
    case PROP_ICON_PAINTABLE:
      g_clear_object (&priv->icon_paintable);
      priv->icon_paintable = g_value_dup_object (value);
      break;
    case PROP_MINI_ICON:
      g_clear_object (&priv->mini_icon);
      priv->mini_icon = g_value_dup_object (value);
      break;
    case PROP_SEARCH_TOKENS:
      g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
      priv->search_tokens = g_value_dup_boxed (value);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_clear_object (&priv->remote_repo_icon);
      priv->remote_repo_icon = g_value_dup_object (value);
      break;
    case PROP_METADATA_LICENSE:
      g_clear_pointer (&priv->metadata_license, g_free);
      priv->metadata_license = g_value_dup_string (value);
      break;
    case PROP_PROJECT_LICENSE:
      g_clear_pointer (&priv->project_license, g_free);
      priv->project_license = g_value_dup_string (value);
      break;
    case PROP_IS_FLOSS:
      priv->is_floss = g_value_get_boolean (value);
      break;
    case PROP_PROJECT_GROUP:
      g_clear_pointer (&priv->project_group, g_free);
      priv->project_group = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER:
      g_clear_pointer (&priv->developer, g_free);
      priv->developer = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER_ID:
      g_clear_pointer (&priv->developer_id, g_free);
      priv->developer_id = g_value_dup_string (value);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_clear_object (&priv->screenshot_paintables);
      priv->screenshot_paintables = g_value_dup_object (value);
      break;
    case PROP_SHARE_URLS:
      g_clear_object (&priv->share_urls);
      priv->share_urls = g_value_dup_object (value);
      break;
    case PROP_DONATION_URL:
      g_clear_pointer (&priv->donation_url, g_free);
      priv->donation_url = g_value_dup_string (value);
      break;
    case PROP_FORGE_URL:
      g_clear_pointer (&priv->forge_url, g_free);
      priv->forge_url = g_value_dup_string (value);
      break;
    case PROP_REVIEWS:
      g_clear_object (&priv->reviews);
      priv->reviews = g_value_dup_object (value);
      break;
    case PROP_AVERAGE_RATING:
      priv->average_rating = g_value_get_double (value);
      break;
    case PROP_RATINGS_SUMMARY:
      g_clear_pointer (&priv->ratings_summary, g_free);
      priv->ratings_summary = g_value_dup_string (value);
      break;
    case PROP_VERSION_HISTORY:
      g_clear_object (&priv->version_history);
      priv->version_history = g_value_dup_object (value);
      break;
    case PROP_LIGHT_ACCENT_COLOR:
      g_clear_pointer (&priv->light_accent_color, g_free);
      priv->light_accent_color = g_value_dup_string (value);
      break;
    case PROP_DARK_ACCENT_COLOR:
      g_clear_pointer (&priv->dark_accent_color, g_free);
      priv->dark_accent_color = g_value_dup_string (value);
      break;
    case PROP_IS_FLATHUB:
      priv->is_flathub = g_value_get_boolean (value);
      break;
    case PROP_VERIFIED:
      priv->verified = g_value_get_boolean (value);
      break;
    case PROP_DOWNLOAD_STATS:
    case PROP_DOWNLOAD_STATS_PER_COUNTRY:
      {
        if (prop_id == PROP_DOWNLOAD_STATS)
          {
            g_clear_object (&priv->download_stats);
            priv->download_stats = g_value_dup_object (value);

            if (priv->download_stats != NULL)
              {
                guint n_items          = 0;
                guint start            = 0;
                guint recent_downloads = 0;

                n_items = g_list_model_get_n_items (priv->download_stats);
                start   = n_items - MIN (n_items, 30);

                for (guint i = start; i < n_items; i++)
                  {
                    g_autoptr (BzDataPoint) point = NULL;

                    point = g_list_model_get_item (priv->download_stats, i);
                    recent_downloads += bz_data_point_get_dependent (point);
                  }
                priv->recent_downloads = recent_downloads;
              }
            else
              priv->recent_downloads = 0;
            g_object_notify_by_pspec (object, props[PROP_RECENT_DOWNLOADS]);
          }
        else
          {
            g_clear_object (&priv->download_stats_per_country);
            priv->download_stats_per_country = g_value_dup_object (value);
          }
      }
      break;
    case PROP_RECENT_DOWNLOADS:
      priv->recent_downloads = g_value_get_int (value);
      break;
    case PROP_HOLDING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_class_init (BzEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_set_property;
  object_class->get_property = bz_entry_get_property;
  object_class->dispose      = bz_entry_dispose;

  props[PROP_HOLDING] =
      g_param_spec_boolean (
          "holding",
          NULL, NULL, FALSE,
          G_PARAM_READABLE);

  props[PROP_INSTALLED] =
      g_param_spec_boolean (
          "installed",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_ADDONS] =
      g_param_spec_object (
          "addons",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_KINDS] =
      g_param_spec_flags (
          "kinds",
          NULL, NULL,
          BZ_TYPE_ENTRY_KIND, 0,
          G_PARAM_READWRITE);

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_UNIQUE_ID] =
      g_param_spec_string (
          "unique-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_UNIQUE_ID_CHECKSUM] =
      g_param_spec_string (
          "unique-id-checksum",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_EOL] =
      g_param_spec_string (
          "eol",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_LONG_DESCRIPTION] =
      g_param_spec_string (
          "long-description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_URL] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_NAME] =
      g_param_spec_string (
          "remote-repo-name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SIZE] =
      g_param_spec_uint64 (
          "size",
          NULL, NULL,
          0, G_MAXUINT64, 0,
          G_PARAM_READWRITE);

  props[PROP_ICON_PAINTABLE] =
      g_param_spec_object (
          "icon-paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_MINI_ICON] =
      g_param_spec_object (
          "mini-icon",
          NULL, NULL,
          G_TYPE_ICON,
          G_PARAM_READWRITE);

  props[PROP_SEARCH_TOKENS] =
      g_param_spec_boxed (
          "search-tokens",
          NULL, NULL,
          G_TYPE_PTR_ARRAY,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_ICON] =
      g_param_spec_object (
          "remote-repo-icon",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_METADATA_LICENSE] =
      g_param_spec_string (
          "metadata-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_LICENSE] =
      g_param_spec_string (
          "project-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_IS_FLOSS] =
      g_param_spec_boolean (
          "is-floss",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_GROUP] =
      g_param_spec_string (
          "project-group",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER] =
      g_param_spec_string (
          "developer",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER_ID] =
      g_param_spec_string (
          "developer-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SCREENSHOT_PAINTABLES] =
      g_param_spec_object (
          "screenshot-paintables",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_SHARE_URLS] =
      g_param_spec_object (
          "share-urls",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_DONATION_URL] =
      g_param_spec_string (
          "donation-url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_FORGE_URL] =
      g_param_spec_string (
          "forge-url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REVIEWS] =
      g_param_spec_object (
          "reviews",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_AVERAGE_RATING] =
      g_param_spec_double (
          "average-rating",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_RATINGS_SUMMARY] =
      g_param_spec_string (
          "ratings-summary",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_VERSION_HISTORY] =
      g_param_spec_object (
          "version-history",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_LIGHT_ACCENT_COLOR] =
      g_param_spec_string (
          "light-accent-color",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DARK_ACCENT_COLOR] =
      g_param_spec_string (
          "dark-accent-color",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_IS_FLATHUB] =
      g_param_spec_boolean (
          "is-flathub",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_VERIFIED] =
      g_param_spec_boolean (
          "verified",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_DOWNLOAD_STATS] =
      g_param_spec_object (
          "download-stats",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_DOWNLOAD_STATS_PER_COUNTRY] =
      g_param_spec_object (
          "download-stats-per-country",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_RECENT_DOWNLOADS] =
      g_param_spec_int (
          "recent-downloads",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_init (BzEntry *self)
{
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  priv->hold = 0;
}

static void
bz_entry_real_serialize (BzSerializable  *serializable,
                         GVariantBuilder *builder)
{
  BzEntry        *self = BZ_ENTRY (serializable);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  g_variant_builder_add (builder, "{sv}", "installed", g_variant_new_boolean (priv->installed));
  g_variant_builder_add (builder, "{sv}", "kinds", g_variant_new_uint32 (priv->kinds));
  if (priv->addons != NULL)
    {
      guint n_items = 0;

      n_items = g_list_model_get_n_items (priv->addons);
      if (n_items > 0)
        {
          g_autoptr (GVariantBuilder) sub_builder = NULL;

          sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr (GtkStringObject) string = NULL;

              string = g_list_model_get_item (priv->addons, i);
              g_variant_builder_add (sub_builder, "s", gtk_string_object_get_string (string));
            }

          g_variant_builder_add (builder, "{sv}", "addons", g_variant_builder_end (sub_builder));
        }
    }
  if (priv->id != NULL)
    g_variant_builder_add (builder, "{sv}", "id", g_variant_new_string (priv->id));
  if (priv->unique_id != NULL)
    g_variant_builder_add (builder, "{sv}", "unique-id", g_variant_new_string (priv->unique_id));
  if (priv->unique_id_checksum != NULL)
    g_variant_builder_add (builder, "{sv}", "unique-id-checksum", g_variant_new_string (priv->unique_id_checksum));
  if (priv->title != NULL)
    g_variant_builder_add (builder, "{sv}", "title", g_variant_new_string (priv->title));
  if (priv->eol != NULL)
    g_variant_builder_add (builder, "{sv}", "eol", g_variant_new_string (priv->eol));
  if (priv->description != NULL)
    g_variant_builder_add (builder, "{sv}", "description", g_variant_new_string (priv->description));
  if (priv->long_description != NULL)
    g_variant_builder_add (builder, "{sv}", "long-description", g_variant_new_string (priv->long_description));
  if (priv->remote_repo_name != NULL)
    g_variant_builder_add (builder, "{sv}", "remote-repo-name", g_variant_new_string (priv->remote_repo_name));
  if (priv->url != NULL)
    g_variant_builder_add (builder, "{sv}", "url", g_variant_new_string (priv->url));
  if (priv->size > 0)
    g_variant_builder_add (builder, "{sv}", "size", g_variant_new_uint64 (priv->size));
  if (priv->icon_paintable != NULL)
    maybe_save_paintable (priv, "icon-paintable", priv->icon_paintable, builder);
  if (priv->mini_icon != NULL)
    {
      g_autoptr (GVariant) serialized = NULL;

      serialized = g_icon_serialize (priv->mini_icon);
      g_variant_builder_add (builder, "{sv}", "mini-icon", serialized);
    }
  if (priv->remote_repo_icon != NULL)
    maybe_save_paintable (priv, "remote-repo-icon", priv->remote_repo_icon, builder);
  if (priv->search_tokens != NULL && priv->search_tokens->len > 0)
    {
      g_autoptr (GVariantBuilder) sub_builder = NULL;

      sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
      for (guint i = 0; i < priv->search_tokens->len; i++)
        {
          const char *token = NULL;

          token = g_ptr_array_index (priv->search_tokens, i);
          g_variant_builder_add (sub_builder, "s", token);
        }
      g_variant_builder_add (builder, "{sv}", "search-tokens", g_variant_builder_end (sub_builder));
    }
  if (priv->metadata_license != NULL)
    g_variant_builder_add (builder, "{sv}", "metadata-license", g_variant_new_string (priv->metadata_license));
  if (priv->project_license != NULL)
    g_variant_builder_add (builder, "{sv}", "project-license", g_variant_new_string (priv->project_license));
  g_variant_builder_add (builder, "{sv}", "is-floss", g_variant_new_boolean (priv->is_floss));
  if (priv->project_group != NULL)
    g_variant_builder_add (builder, "{sv}", "project-group", g_variant_new_string (priv->project_group));
  if (priv->developer != NULL)
    g_variant_builder_add (builder, "{sv}", "developer", g_variant_new_string (priv->developer));
  if (priv->developer_id != NULL)
    g_variant_builder_add (builder, "{sv}", "developer-id", g_variant_new_string (priv->developer_id));
  if (priv->screenshot_paintables != NULL)
    {
      guint n_items = 0;

      n_items = g_list_model_get_n_items (priv->screenshot_paintables);
      if (n_items > 0)
        {
          g_autoptr (GVariantBuilder) sub_builder = NULL;

          sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr (GdkPaintable) paintable = NULL;
              g_autofree char *key               = NULL;

              paintable = g_list_model_get_item (priv->screenshot_paintables, i);
              key       = g_strdup_printf ("screenshot_%d.png", i);

              maybe_save_paintable (priv, key, paintable, sub_builder);
            }

          g_variant_builder_add (builder, "{sv}", "screenshot-paintables", g_variant_builder_end (sub_builder));
        }
    }
  if (priv->share_urls != NULL)
    {
      guint n_items = 0;

      n_items = g_list_model_get_n_items (priv->share_urls);
      if (n_items > 0)
        {
          g_autoptr (GVariantBuilder) sub_builder = NULL;

          sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("a(ss)"));
          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr (BzUrl) url = NULL;
              const char *name      = NULL;
              const char *url_str   = NULL;

              url     = g_list_model_get_item (priv->share_urls, i);
              name    = bz_url_get_name (url);
              url_str = bz_url_get_url (url);
              g_variant_builder_add (sub_builder, "(ss)", name, url_str);
            }
          g_variant_builder_add (builder, "{sv}", "share-urls", g_variant_builder_end (sub_builder));
        }
    }
  if (priv->donation_url != NULL)
    g_variant_builder_add (builder, "{sv}", "donation-url", g_variant_new_string (priv->donation_url));
  if (priv->forge_url != NULL)
    g_variant_builder_add (builder, "{sv}", "forge-url", g_variant_new_string (priv->forge_url));
  if (priv->version_history != NULL)
    {
      guint n_items = 0;

      n_items = g_list_model_get_n_items (priv->version_history);
      if (n_items > 0)
        {
          g_autoptr (GVariantBuilder) sub_builder = NULL;

          sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("a(msmvtmsms)"));
          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr (BzRelease) release              = NULL;
              GListModel *issues                         = NULL;
              g_autoptr (GVariantBuilder) issues_builder = NULL;
              guint       n_issues                       = 0;
              guint64     timestamp                      = 0;
              const char *url                            = NULL;
              const char *version                        = NULL;
              const char *description                    = NULL;

              release     = g_list_model_get_item (priv->version_history, i);
              issues      = bz_release_get_issues (release);
              timestamp   = bz_release_get_timestamp (release);
              url         = bz_release_get_url (release);
              version     = bz_release_get_version (release);
              description = bz_release_get_description (release);

              if (issues != NULL)
                {
                  n_issues = g_list_model_get_n_items (issues);
                  if (n_issues > 0)
                    {
                      issues_builder = g_variant_builder_new (G_VARIANT_TYPE ("a(msms)"));
                      for (guint j = 0; j < n_issues; j++)
                        {
                          g_autoptr (BzIssue) issue = NULL;
                          const char *issue_id      = NULL;
                          const char *issue_url     = NULL;

                          issue     = g_list_model_get_item (issues, j);
                          issue_id  = bz_issue_get_id (issue);
                          issue_url = bz_issue_get_url (issue);

                          g_variant_builder_add (issues_builder, "(msms)", issue_id, issue_url);
                        }
                    }
                }

              g_variant_builder_add (
                  sub_builder,
                  "(msmvtmsms)",
                  description,
                  issues_builder != NULL
                      ? g_variant_builder_end (issues_builder)
                      : NULL,
                  timestamp,
                  url,
                  version);
            }

          g_variant_builder_add (builder, "{sv}", "version-history", g_variant_builder_end (sub_builder));
        }
    }
  if (priv->light_accent_color != NULL)
    g_variant_builder_add (builder, "{sv}", "light-accent-color", g_variant_new_string (priv->light_accent_color));
  if (priv->dark_accent_color != NULL)
    g_variant_builder_add (builder, "{sv}", "dark-accent-color", g_variant_new_string (priv->dark_accent_color));
  g_variant_builder_add (builder, "{sv}", "is-flathub", g_variant_new_boolean (priv->is_flathub));
  if (priv->is_flathub)
    {
      if (priv->flathub_prop_queries != NULL)
        {
          if (g_hash_table_contains (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_VERIFIED)))
            g_variant_builder_add (builder, "{sv}", "verified", g_variant_new_boolean (priv->verified));
          if (g_hash_table_contains (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_DOWNLOAD_STATS)) &&
              priv->download_stats != NULL)
            {
              guint n_items = 0;

              n_items = g_list_model_get_n_items (priv->download_stats);
              if (n_items > 0)
                {
                  g_autoptr (GVariantBuilder) sub_builder = NULL;

                  sub_builder = g_variant_builder_new (G_VARIANT_TYPE ("a(ddms)"));
                  for (guint i = 0; i < n_items; i++)
                    {
                      g_autoptr (BzDataPoint) point = NULL;
                      double      independent       = 0.0;
                      double      dependent         = 0.0;
                      const char *label             = NULL;

                      point       = g_list_model_get_item (priv->download_stats, i);
                      independent = bz_data_point_get_independent (point);
                      dependent   = bz_data_point_get_dependent (point);
                      label       = bz_data_point_get_label (point);

                      g_variant_builder_add (sub_builder, "(ddms)", independent, dependent, label);
                    }
                  g_variant_builder_add (builder, "{sv}", "download-stats", g_variant_builder_end (sub_builder));
                }
            }
          if (g_hash_table_contains (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_RECENT_DOWNLOADS)))
            g_variant_builder_add (builder, "{sv}", "recent-downloads", g_variant_new_int32 (priv->recent_downloads));
        }
    }
}

static gboolean
bz_entry_real_deserialize (BzSerializable *serializable,
                           GVariant       *import,
                           GError        **error)
{
  BzEntry        *self          = BZ_ENTRY (serializable);
  BzEntryPrivate *priv          = bz_entry_get_instance_private (self);
  g_autoptr (GVariantIter) iter = NULL;

  clear_entry (self);

  iter = g_variant_iter_new (import);
  for (;;)
    {
      g_autofree char *key       = NULL;
      g_autoptr (GVariant) value = NULL;

      if (!g_variant_iter_next (iter, "{sv}", &key, &value))
        break;

      if (g_strcmp0 (key, "installed") == 0)
        priv->installed = g_variant_get_boolean (value);
      else if (g_strcmp0 (key, "kinds") == 0)
        priv->kinds = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "addons") == 0)
        {
          g_autoptr (GListStore) store        = NULL;
          g_autoptr (GVariantIter) addon_iter = NULL;

          store = g_list_store_new (GTK_TYPE_STRING_OBJECT);

          addon_iter = g_variant_iter_new (value);
          for (;;)
            {
              g_autofree char *unique_id         = NULL;
              g_autoptr (GtkStringObject) string = NULL;

              if (!g_variant_iter_next (addon_iter, "s", &unique_id))
                break;
              string = gtk_string_object_new (unique_id);
              g_list_store_append (store, string);
            }

          priv->addons = G_LIST_MODEL (g_steal_pointer (&store));
        }
      else if (g_strcmp0 (key, "id") == 0)
        priv->id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "unique-id") == 0)
        priv->unique_id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "unique-id-checksum") == 0)
        priv->unique_id_checksum = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "title") == 0)
        priv->title = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "eol") == 0)
        priv->eol = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "description") == 0)
        priv->description = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "long-description") == 0)
        priv->long_description = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "remote-repo-name") == 0)
        priv->remote_repo_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "url") == 0)
        priv->url = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "size") == 0)
        priv->size = g_variant_get_uint64 (value);
      else if (g_strcmp0 (key, "icon-paintable") == 0)
        priv->icon_paintable = make_async_texture (value);
      else if (g_strcmp0 (key, "mini-icon") == 0)
        priv->mini_icon = g_icon_deserialize (value);
      else if (g_strcmp0 (key, "remote-repo-icon") == 0)
        priv->remote_repo_icon = make_async_texture (value);
      else if (g_strcmp0 (key, "search-tokens") == 0)
        {
          g_autoptr (GPtrArray) search_tokens = NULL;
          g_autoptr (GVariantIter) token_iter = NULL;

          search_tokens = g_ptr_array_new_with_free_func (g_free);

          token_iter = g_variant_iter_new (value);
          for (;;)
            {
              g_autofree char *token = NULL;

              if (!g_variant_iter_next (token_iter, "s", &token))
                break;
              g_ptr_array_add (search_tokens, g_steal_pointer (&token));
            }
          priv->search_tokens = g_steal_pointer (&search_tokens);
        }
      else if (g_strcmp0 (key, "metadata-license") == 0)
        priv->metadata_license = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "project-license") == 0)
        priv->project_license = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "is-floss") == 0)
        priv->is_floss = g_variant_get_boolean (value);
      else if (g_strcmp0 (key, "developer") == 0)
        priv->developer = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "developer-id") == 0)
        priv->developer_id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "screenshot-paintables") == 0)
        {
          g_autoptr (GListStore) store             = NULL;
          g_autoptr (GVariantIter) screenshot_iter = NULL;

          store = g_list_store_new (BZ_TYPE_ASYNC_TEXTURE);

          screenshot_iter = g_variant_iter_new (value);
          for (;;)
            {
              g_autofree char *basename        = NULL;
              g_autoptr (GVariant) screenshot  = NULL;
              g_autoptr (GdkPaintable) texture = NULL;

              if (!g_variant_iter_next (screenshot_iter, "{sv}", &basename, &screenshot))
                break;
              texture = make_async_texture (screenshot);
              g_list_store_append (store, texture);
            }

          priv->screenshot_paintables = G_LIST_MODEL (g_steal_pointer (&store));
        }
      else if (g_strcmp0 (key, "share-urls") == 0)
        {
          g_autoptr (GListStore) store      = NULL;
          g_autoptr (GVariantIter) url_iter = NULL;

          store = g_list_store_new (BZ_TYPE_URL);

          url_iter = g_variant_iter_new (value);
          for (;;)
            {
              g_autofree char *name    = NULL;
              g_autofree char *url_str = NULL;
              g_autoptr (BzUrl) url    = NULL;

              if (!g_variant_iter_next (url_iter, "(ss)", &name, &url_str))
                break;
              url = bz_url_new ();
              bz_url_set_name (url, name);
              bz_url_set_url (url, url_str);
              g_list_store_append (store, url);
            }

          priv->share_urls = G_LIST_MODEL (g_steal_pointer (&store));
        }
      else if (g_strcmp0 (key, "donation-url") == 0)
        priv->donation_url = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "forge-url") == 0)
        priv->forge_url = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "version-history") == 0)
        {
          g_autoptr (GListStore) store          = NULL;
          g_autoptr (GVariantIter) version_iter = NULL;

          store = g_list_store_new (BZ_TYPE_RELEASE);

          version_iter = g_variant_iter_new (value);
          for (;;)
            {
              g_autoptr (GVariant) issues         = NULL;
              g_autoptr (GListStore) issues_store = NULL;
              guint64          timestamp          = 0;
              g_autofree char *url                = NULL;
              g_autofree char *description        = NULL;
              g_autofree char *version            = NULL;
              g_autoptr (BzRelease) release       = NULL;

              if (!g_variant_iter_next (version_iter, "(msmvtmsms)", &description, &issues, &timestamp, &url, &version))
                break;

              if (issues != NULL)
                {
                  g_autoptr (GVariantIter) issues_iter = NULL;

                  issues_store = g_list_store_new (BZ_TYPE_ISSUE);

                  issues_iter = g_variant_iter_new (issues);
                  for (;;)
                    {
                      g_autofree char *issue_id  = NULL;
                      g_autofree char *issue_url = NULL;
                      g_autoptr (BzIssue) issue  = NULL;

                      if (!g_variant_iter_next (issues_iter, "(msms)", &issue_id, &issue_url))
                        break;

                      issue = bz_issue_new ();
                      bz_issue_set_id (issue, issue_id);
                      bz_issue_set_url (issue, issue_url);
                      g_list_store_append (issues_store, issue);
                    }
                }

              release = bz_release_new ();
              if (issues_store != NULL)
                bz_release_set_issues (release, G_LIST_MODEL (issues_store));
              bz_release_set_timestamp (release, timestamp);
              bz_release_set_url (release, url);
              bz_release_set_version (release, version);
              bz_release_set_description (release, description);
              g_list_store_append (store, release);
            }

          priv->version_history = G_LIST_MODEL (g_steal_pointer (&store));
        }
      else if (g_strcmp0 (key, "light-accent-color") == 0)
        priv->light_accent_color = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "dark-accent-color") == 0)
        priv->dark_accent_color = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "is-flathub") == 0)
        priv->is_flathub = g_variant_get_boolean (value);

      /* Disabling these since it updates so often and downloading is cheap */
      // else if (g_strcmp0 (key, "verified") == 0)
      //   {
      //     priv->verified = g_variant_get_boolean (value);
      //     if (priv->flathub_prop_queries == NULL)
      //       priv->flathub_prop_queries = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, dex_unref);
      //     g_hash_table_replace (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_VERIFIED), dex_future_new_true ());
      //   }
      // else if (g_strcmp0 (key, "download-stats") == 0)
      //   {
      //     g_autoptr (GListStore) store        = NULL;
      //     g_autoptr (GVariantIter) point_iter = NULL;

      //     store = g_list_store_new (BZ_TYPE_DATA_POINT);

      //     point_iter = g_variant_iter_new (value);
      //     for (;;)
      //       {
      //         double           independent  = 0.0;
      //         double           dependent    = 0.0;
      //         g_autofree char *label        = NULL;
      //         g_autoptr (BzDataPoint) point = NULL;

      //         if (!g_variant_iter_next (point_iter, "(ddms)", &independent, &dependent, &label))
      //           break;
      //         point = bz_data_point_new ();
      //         bz_data_point_set_independent (point, independent);
      //         bz_data_point_set_dependent (point, dependent);
      //         bz_data_point_set_label (point, label);
      //         g_list_store_append (store, point);
      //       }

      //     priv->download_stats = G_LIST_MODEL (g_steal_pointer (&store));
      //     if (priv->flathub_prop_queries == NULL)
      //       priv->flathub_prop_queries = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, dex_unref);
      //     g_hash_table_replace (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_DOWNLOAD_STATS), dex_future_new_true ());
      //   }
      // else if (g_strcmp0 (key, "recent-downloads") == 0)
      //   {
      //     priv->recent_downloads = g_variant_get_int32 (value);
      //     if (priv->flathub_prop_queries == NULL)
      //       priv->flathub_prop_queries = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, dex_unref);
      //     g_hash_table_replace (priv->flathub_prop_queries, GINT_TO_POINTER (PROP_RECENT_DOWNLOADS), dex_future_new_true ());
      //   }
    }

  return TRUE;
}

void
bz_entry_hold (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));
  priv = bz_entry_get_instance_private (self);

  if (++priv->hold == 1)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HOLDING]);
}

void
bz_entry_release (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));
  priv = bz_entry_get_instance_private (self);

  if (--priv->hold == 0)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HOLDING]);
}

gboolean
bz_entry_is_holding (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  return priv->hold > 0;
}

gboolean
bz_entry_is_installed (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  return priv->installed;
}

void
bz_entry_set_installed (BzEntry *self,
                        gboolean installed)
{
  BzEntryPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));
  priv = bz_entry_get_instance_private (self);

  priv->installed = installed;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLED]);
}

gboolean
bz_entry_is_of_kinds (BzEntry *self,
                      guint    kinds)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  return (priv->kinds & kinds) == kinds;
}

void
bz_entry_append_addon (BzEntry    *self,
                       const char *id)
{
  BzEntryPrivate *priv               = NULL;
  g_autoptr (GtkStringObject) string = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));
  g_return_if_fail (id != NULL);
  priv = bz_entry_get_instance_private (self);

  string = gtk_string_object_new (id);
  if (priv->addons == NULL)
    {
      priv->addons = (GListModel *) g_list_store_new (GTK_TYPE_STRING_OBJECT);
      g_list_store_append (G_LIST_STORE (priv->addons), string);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ADDONS]);
    }
  else
    g_list_store_append (G_LIST_STORE (priv->addons), string);
}

GListModel *
bz_entry_get_addons (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->addons;
}

const char *
bz_entry_get_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->id;
}

const char *
bz_entry_get_unique_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->unique_id;
}

const char *
bz_entry_get_unique_id_checksum (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->unique_id_checksum;
}

const char *
bz_entry_get_title (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->title;
}

const char *
bz_entry_get_developer (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->developer;
}

const char *
bz_entry_get_eol (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);
  priv = bz_entry_get_instance_private (self);

  return priv->eol;
}

const char *
bz_entry_get_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->description;
}

const char *
bz_entry_get_long_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->long_description;
}

const char *
bz_entry_get_remote_repo_name (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->remote_repo_name;
}

guint64
bz_entry_get_size (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);
  priv = bz_entry_get_instance_private (self);

  return priv->size;
}

GdkPaintable *
bz_entry_get_icon_paintable (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->icon_paintable;
}

GListModel *
bz_entry_get_screenshot_paintables (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->screenshot_paintables;
}

GIcon *
bz_entry_get_mini_icon (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->mini_icon;
}

GPtrArray *
bz_entry_get_search_tokens (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->search_tokens;
}

GListModel *
bz_entry_get_share_urls (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->share_urls;
}

const char *
bz_entry_get_url (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->url;
}

const char *
bz_entry_get_donation_url (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->donation_url;
}

const char *
bz_entry_get_forge_url (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->forge_url;
}

gboolean
bz_entry_get_is_foss (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  return priv->is_floss;
}

const char *
bz_entry_get_light_accent_color (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->light_accent_color;
}

const char *
bz_entry_get_dark_accent_color (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);
  priv = bz_entry_get_instance_private (self);

  return priv->dark_accent_color;
}

gboolean
bz_entry_get_is_flathub (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  return priv->is_flathub;
}

DexFuture *
bz_entry_load_mini_icon (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  dex_return_error_if_fail (BZ_IS_ENTRY (self));
  priv = bz_entry_get_instance_private (self);

  if (priv->mini_icon == NULL &&
      priv->mini_icon_future == NULL &&
      BZ_IS_ASYNC_TEXTURE (priv->icon_paintable))
    {
      dex_clear (&priv->mini_icon_future);
      priv->mini_icon_future = dex_future_then (
          bz_async_texture_dup_future (BZ_ASYNC_TEXTURE (priv->icon_paintable)),
          (DexFutureCallback) icon_paintable_future_then,
          self, NULL);
      return dex_ref (priv->mini_icon_future);
    }
  else
    return dex_future_new_true ();
}

GIcon *
bz_load_mini_icon_sync (const char *unique_id_checksum,
                        const char *path)
{
  return load_mini_icon_sync (unique_id_checksum, path);
}

gint
bz_entry_calc_usefulness (BzEntry *self)
{
  BzEntryPrivate *priv  = NULL;
  gint            score = 0;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  priv = bz_entry_get_instance_private (self);

  score += priv->is_flathub ? 1000 : 0;

  score += priv->title != NULL ? 5 : 0;
  score += priv->description != NULL ? 1 : 0;
  score += priv->long_description != NULL ? 5 : 0;
  score += priv->url != NULL ? 1 : 0;
  score += priv->size > 0 ? 1 : 0;
  score += priv->icon_paintable != NULL ? 15 : 0;
  score += priv->remote_repo_icon != NULL ? 1 : 0;
  score += priv->metadata_license != NULL ? 1 : 0;
  score += priv->project_license != NULL ? 1 : 0;
  score += priv->project_group != NULL ? 1 : 0;
  score += priv->developer != NULL ? 1 : 0;
  score += priv->developer_id != NULL ? 1 : 0;
  score += priv->screenshot_paintables != NULL ? 5 : 0;
  score += priv->share_urls != NULL ? 5 : 0;

  return score;
}

void
bz_entry_serialize (BzEntry         *self,
                    GVariantBuilder *builder)
{
  g_return_if_fail (BZ_IS_ENTRY (self));
  g_return_if_fail (builder != NULL);

  return bz_entry_real_serialize (BZ_SERIALIZABLE (self), builder);
}

gboolean
bz_entry_deserialize (BzEntry  *self,
                      GVariant *import,
                      GError  **error)
{
  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);
  g_return_val_if_fail (import != NULL, FALSE);

  return bz_entry_real_deserialize (BZ_SERIALIZABLE (self), import, error);
}

static void
query_flathub (BzEntry *self,
               int      prop)
{
  BzEntryPrivate *priv              = NULL;
  g_autoptr (QueryFlathubData) data = NULL;
  g_autoptr (DexFuture) future      = NULL;

  priv = bz_entry_get_instance_private (self);

  if (!priv->is_flathub)
    return;
  if (priv->id == NULL)
    return;

  if (priv->flathub_prop_queries == NULL)
    priv->flathub_prop_queries = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, dex_unref);
  else if (g_hash_table_contains (priv->flathub_prop_queries, GINT_TO_POINTER (prop)))
    return;

  data = query_flathub_data_new ();
  g_weak_ref_init (&data->self, self);
  data->prop = prop;
  data->id   = g_strdup (priv->id);

  future = dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) query_flathub_fiber,
      query_flathub_data_ref (data), query_flathub_data_unref);
  future = dex_future_then (
      future, (DexFutureCallback) query_flathub_then,
      query_flathub_data_ref (data), query_flathub_data_unref);
  g_hash_table_replace (
      priv->flathub_prop_queries,
      GINT_TO_POINTER (prop),
      g_steal_pointer (&future));
}

static DexFuture *
query_flathub_fiber (QueryFlathubData *data)
{
  int   prop                     = data->prop;
  char *id                       = data->id;
  g_autoptr (GError) local_error = NULL;
  g_autofree char *request       = NULL;
  g_autoptr (JsonNode) node      = NULL;

  switch (prop)
    {
    case PROP_VERIFIED:
      request = g_strdup_printf ("/verification/%s/status", id);
      break;
    case PROP_DOWNLOAD_STATS:
    case PROP_DOWNLOAD_STATS_PER_COUNTRY:
      request = g_strdup_printf ("/stats/%s?all=false&days=175", id);
      break;
    default:
      g_assert_not_reached ();
      return NULL;
    }

  node = dex_await_boxed (bz_query_flathub_v2_json (request), &local_error);
  if (node == NULL)
    {
      if (!g_error_matches (local_error, DEX_ERROR, DEX_ERROR_FIBER_CANCELLED))
        g_critical ("Could not retrieve property %s for %s from flathub: %s",
                    props[prop]->name, id, local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  switch (prop)
    {
    case PROP_VERIFIED:
      return dex_future_new_for_boolean (
          json_object_get_boolean_member (
              json_node_get_object (node),
              "verified"));
      break;
    case PROP_DOWNLOAD_STATS:
      {
        JsonObject *per_day          = NULL;
        g_autoptr (GListStore) store = NULL;

        per_day = json_object_get_object_member (
            json_node_get_object (node),
            "installs_per_day");
        store = g_list_store_new (BZ_TYPE_DATA_POINT);

        json_object_foreach_member (
            per_day,
            (JsonObjectForeach) download_stats_per_day_foreach,
            store);

        return dex_future_new_for_object (store);
      }
      break;

    case PROP_DOWNLOAD_STATS_PER_COUNTRY:
      {
        JsonObject *per_country      = NULL;
        g_autoptr (GListStore) store = NULL;

        per_country = json_object_get_object_member (
            json_node_get_object (node),
            "installs_per_country");

        store = g_list_store_new (BZ_TYPE_COUNTRY_DATA_POINT);

        json_object_foreach_member (
            per_country,
            (JsonObjectForeach) download_stats_per_country_foreach,
            store);

        return dex_future_new_for_object (store);
      }
      break;

    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static DexFuture *
query_flathub_then (DexFuture        *future,
                    QueryFlathubData *data)
{
  g_autoptr (BzEntry) self = NULL;
  int           prop       = data->prop;
  const GValue *value      = NULL;

  self = g_weak_ref_get (&data->self);
  if (self == NULL)
    return NULL;

  value = dex_future_get_value (future, NULL);
  g_object_set_property (G_OBJECT (self), props[prop]->name, value);
  return NULL;
}

static void
download_stats_per_day_foreach (JsonObject  *object,
                                const gchar *member_name,
                                JsonNode    *member_node,
                                GListStore  *store)
{
  double independent               = 0;
  double dependent                 = 0;
  g_autoptr (BzDataPoint) point    = NULL;
  g_autoptr (GDateTime) date       = NULL;
  g_autofree char *formatted_label = NULL;
  g_autofree char *iso_with_tz     = NULL;

  independent = g_list_model_get_n_items (G_LIST_MODEL (store));
  dependent   = json_node_get_int (member_node);

  iso_with_tz = g_strdup_printf ("%sT00:00:00Z", member_name);
  date        = g_date_time_new_from_iso8601 (iso_with_tz, NULL);

  if (date != NULL)
    formatted_label = g_date_time_format (date, "%-d %b");
  else
    formatted_label = g_strdup (member_name);

  point = g_object_new (
      BZ_TYPE_DATA_POINT,
      "independent", independent,
      "dependent", dependent,
      "label", formatted_label,
      NULL);
  g_list_store_append (store, point);
}

static void
download_stats_per_country_foreach (JsonObject  *object,
                                    const gchar *member_name,
                                    JsonNode    *member_node,
                                    GListStore  *store)
{
  guint downloads                      = 0;
  g_autoptr (BzCountryDataPoint) point = NULL;

  downloads = json_node_get_int (member_node);

  point = g_object_new (
      BZ_TYPE_COUNTRY_DATA_POINT,
      "country-code", member_name,
      "downloads", downloads,
      NULL);

  g_list_store_append (store, point);
}

static gboolean
maybe_save_paintable (BzEntryPrivate  *priv,
                      const char      *key,
                      GdkPaintable    *paintable,
                      GVariantBuilder *builder)
{
  g_autoptr (GError) local_error = NULL;
  const char *source_uri         = NULL;
  const char *cache_into_path    = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GFile) save_file    = NULL;
  gboolean result                = FALSE;

  if (!BZ_IS_ASYNC_TEXTURE (paintable))
    {
      g_warning ("Paintable must be of type BzAsyncTexture to be serialized!");
      return FALSE;
    }

  source_uri      = bz_async_texture_get_source_uri (BZ_ASYNC_TEXTURE (paintable));
  cache_into_path = bz_async_texture_get_cache_into_path (BZ_ASYNC_TEXTURE (paintable));
  if (cache_into_path == NULL)
    goto done;

  if (bz_async_texture_get_loaded (BZ_ASYNC_TEXTURE (paintable)))
    texture = bz_async_texture_dup_texture (BZ_ASYNC_TEXTURE (paintable));
  else
    goto done;

  save_file = g_file_new_for_path (cache_into_path);
  if (!g_file_query_exists (save_file, NULL))
    {
      g_autoptr (GFile) parent_file        = NULL;
      g_autoptr (GBytes) png_bytes         = NULL;
      g_autoptr (GFileOutputStream) output = NULL;
      gssize bytes_written                 = 0;

      parent_file = g_file_get_parent (save_file);
      result      = g_file_make_directory_with_parents (
          parent_file, NULL, &local_error);
      if (!result)
        {
          if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            g_clear_pointer (&local_error, g_error_free);
          else
            {
              g_warning ("Couldn't serialize texture to %s: %s\n",
                         cache_into_path, local_error->message);
              goto done;
            }
        }

      png_bytes = gdk_texture_save_to_png_bytes (texture);
      if (png_bytes == NULL)
        goto done;

      output = g_file_replace (
          save_file,
          NULL,
          FALSE,
          G_FILE_CREATE_REPLACE_DESTINATION,
          NULL,
          &local_error);
      if (output == NULL)
        {
          g_warning ("Couldn't serialize texture to %s: %s\n",
                     cache_into_path, local_error->message);
          goto done;
        }

      bytes_written = g_output_stream_write_bytes (
          G_OUTPUT_STREAM (output), png_bytes, NULL, &local_error);
      if (bytes_written < 0)
        {
          g_warning ("Couldn't serialize texture to %s: %s\n",
                     cache_into_path, local_error->message);
          goto done;
        }

      result = g_output_stream_close (G_OUTPUT_STREAM (output), NULL, &local_error);
      if (!result)
        {
          g_warning ("Couldn't serialize texture to %s: %s\n",
                     cache_into_path, local_error->message);
          goto done;
        }
    }

done:
  g_variant_builder_add (builder, "{sv}", key, g_variant_new ("(sms)", source_uri, cache_into_path));
  return TRUE;
}

static GdkPaintable *
make_async_texture (GVariant *parse)
{
  g_autofree char *source            = NULL;
  g_autofree char *cache_into        = NULL;
  g_autoptr (GFile) source_file      = NULL;
  g_autoptr (GFile) cache_into_file  = NULL;
  g_autoptr (BzAsyncTexture) texture = NULL;

  g_variant_get (parse, "(sms)", &source, &cache_into);
  source_file = g_file_new_for_uri (source);
  if (cache_into != NULL)
    cache_into_file = g_file_new_for_path (cache_into);

  texture = bz_async_texture_new_lazy (source_file, cache_into_file);
  return GDK_PAINTABLE (g_steal_pointer (&texture));
}

static DexFuture *
icon_paintable_future_then (DexFuture *future,
                            BzEntry   *self)
{
  BzEntryPrivate *priv              = NULL;
  const char     *icon_path         = NULL;
  g_autoptr (LoadMiniIconData) data = NULL;

  priv = bz_entry_get_instance_private (self);

  /* ? */
  if (!BZ_IS_ASYNC_TEXTURE (priv->icon_paintable))
    return NULL;

  icon_path = bz_async_texture_get_cache_into_path (BZ_ASYNC_TEXTURE (priv->icon_paintable));
  if (icon_path == NULL)
    return NULL;

  data       = load_mini_icon_data_new ();
  data->self = g_object_ref (self);
  data->path = g_strdup (icon_path);

  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) load_mini_icon_fiber,
      load_mini_icon_data_ref (data),
      load_mini_icon_data_unref);
}

static DexFuture *
load_mini_icon_fiber (LoadMiniIconData *data)
{
  BzEntry *self = data->self;
  char    *path = data->path;

  data->result = load_mini_icon_sync (
      bz_entry_get_unique_id_checksum (BZ_ENTRY (self)),
      path);
  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) load_mini_icon_notify,
      load_mini_icon_data_ref (data),
      load_mini_icon_data_unref);
}

static GIcon *
load_mini_icon_sync (const char *unique_id_checksum,
                     const char *path)
{
  g_autofree char *main_cache            = NULL;
  g_autoptr (GString) mini_icon_basename = NULL;
  g_autofree char *mini_icon_path        = NULL;
  g_autoptr (GBytes) bytes               = NULL;
  cairo_surface_t *surface_in            = NULL;
  int              width                 = 0;
  int              height                = 0;
  cairo_surface_t *surface_out           = NULL;
  cairo_t         *cairo                 = NULL;
  g_autoptr (GFile) parent_file          = NULL;
  g_autoptr (GFile) mini_icon_file       = NULL;
  g_autoptr (GIcon) mini_icon            = NULL;

  main_cache         = bz_dup_module_dir ();
  mini_icon_basename = g_string_new (unique_id_checksum);
  g_string_append (mini_icon_basename, "-24x24.png");
  mini_icon_path = g_build_filename (main_cache, mini_icon_basename->str, NULL);

  if (g_file_test (mini_icon_path, G_FILE_TEST_EXISTS))
    /* Assume the icon left behind by last writer */
    goto done;

  surface_in = cairo_image_surface_create_from_png (path);
  width      = cairo_image_surface_get_width (surface_in);
  height     = cairo_image_surface_get_height (surface_in);

  /* 24x24 for the gnome-shell search provider */
  surface_out = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 24, 24);
  cairo       = cairo_create (surface_out);

  cairo_scale (cairo, 24.0 / (double) width, 24.0 / (double) height);
  cairo_set_source_surface (cairo, surface_in, 0, 0);
  cairo_paint (cairo);
  cairo_restore (cairo);

  parent_file = g_file_new_for_path (main_cache);
  g_file_make_directory_with_parents (parent_file, NULL, NULL);

  cairo_surface_flush (surface_out);
  cairo_surface_write_to_png (surface_out, mini_icon_path);
  cairo_destroy (cairo);
  cairo_surface_destroy (surface_in);
  cairo_surface_destroy (surface_out);

done:
  mini_icon_file = g_file_new_for_path (mini_icon_path);
  mini_icon      = g_file_icon_new (mini_icon_file);
  return g_steal_pointer (&mini_icon);
}

static DexFuture *
load_mini_icon_notify (LoadMiniIconData *data)
{
  BzEntry *self   = data->self;
  GIcon   *result = data->result;

  g_object_set (
      self,
      "mini-icon", result,
      NULL);
  return dex_future_new_true ();
}

static void
clear_entry (BzEntry *self)
{
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  dex_clear (&priv->mini_icon_future);
  g_clear_pointer (&priv->flathub_prop_queries, g_hash_table_unref);
  g_clear_object (&priv->addons);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->unique_id, g_free);
  g_clear_pointer (&priv->unique_id_checksum, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->eol, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->long_description, g_free);
  g_clear_pointer (&priv->remote_repo_name, g_free);
  g_clear_pointer (&priv->url, g_free);
  g_clear_object (&priv->icon_paintable);
  g_clear_object (&priv->mini_icon);
  g_clear_object (&priv->remote_repo_icon);
  g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
  g_clear_pointer (&priv->metadata_license, g_free);
  g_clear_pointer (&priv->project_license, g_free);
  g_clear_pointer (&priv->project_group, g_free);
  g_clear_pointer (&priv->developer, g_free);
  g_clear_pointer (&priv->developer_id, g_free);
  g_clear_object (&priv->screenshot_paintables);
  g_clear_object (&priv->share_urls);
  g_clear_pointer (&priv->donation_url, g_free);
  g_clear_pointer (&priv->forge_url, g_free);
  g_clear_object (&priv->reviews);
  g_clear_pointer (&priv->ratings_summary, g_free);
  g_clear_object (&priv->version_history);
  g_clear_pointer (&priv->light_accent_color, g_free);
  g_clear_pointer (&priv->dark_accent_color, g_free);
  g_clear_object (&priv->download_stats);
}

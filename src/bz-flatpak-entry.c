/* bz-flatpak-entry.c
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

#include <xmlb.h>

#include "bz-flatpak-private.h"
#include "bz-issue.h"
#include "bz-paintable-model.h"
#include "bz-release.h"
#include "bz-url.h"
// #include "bz-review.h"

enum
{
  NO_ELEMENT,
  PARAGRAPH,
  ORDERED_LIST,
  UNORDERED_LIST,
  LIST_ITEM,
  CODE,
  EMPHASIS,
};

struct _BzFlatpakEntry
{
  BzEntry parent_instance;

  gboolean user;
  char    *flatpak_id;
  char    *application_name;
  char    *application_runtime;
  char    *application_command;
  char    *runtime_name;
  char    *addon_extension_of_ref;

  BzFlatpakInstance *flatpak;
  FlatpakRemoteRef  *rref;
};

G_DEFINE_FINAL_TYPE (BzFlatpakEntry, bz_flatpak_entry, BZ_TYPE_ENTRY)

enum
{
  PROP_0,

  PROP_INSTANCE,
  PROP_USER,
  PROP_FLATPAK_ID,
  PROP_APPLICATION_NAME,
  PROP_APPLICATION_RUNTIME,
  PROP_APPLICATION_COMMAND,
  PROP_RUNTIME_NAME,
  PROP_ADDON_OF_REF,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
compile_appstream_description (XbNode  *node,
                               GString *string,
                               int      parent_kind,
                               int      idx);

static inline void
append_markup_escaped (GString    *string,
                       const char *append);

static void
bz_flatpak_entry_dispose (GObject *object)
{
  BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  g_clear_pointer (&self->flatpak_id, g_free);
  g_clear_pointer (&self->application_name, g_free);
  g_clear_pointer (&self->application_runtime, g_free);
  g_clear_pointer (&self->application_command, g_free);
  g_clear_pointer (&self->runtime_name, g_free);
  g_clear_pointer (&self->addon_extension_of_ref, g_free);

  g_clear_object (&self->flatpak);
  g_clear_object (&self->rref);

  G_OBJECT_CLASS (bz_flatpak_entry_parent_class)->dispose (object);
}

static void
bz_flatpak_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
      g_value_set_object (value, self->flatpak);
      break;
    case PROP_USER:
      g_value_set_boolean (value, self->user);
      break;
    case PROP_FLATPAK_ID:
      g_value_set_string (value, self->flatpak_id);
      break;
    case PROP_APPLICATION_NAME:
      g_value_set_string (value, self->application_name);
      break;
    case PROP_APPLICATION_RUNTIME:
      g_value_set_string (value, self->application_runtime);
      break;
    case PROP_APPLICATION_COMMAND:
      g_value_set_string (value, self->application_command);
      break;
    case PROP_RUNTIME_NAME:
      g_value_set_string (value, self->runtime_name);
      break;
    case PROP_ADDON_OF_REF:
      g_value_set_string (value, self->addon_extension_of_ref);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flatpak_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  // BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
    case PROP_USER:
    case PROP_FLATPAK_ID:
    case PROP_APPLICATION_NAME:
    case PROP_APPLICATION_RUNTIME:
    case PROP_APPLICATION_COMMAND:
    case PROP_RUNTIME_NAME:
    case PROP_ADDON_OF_REF:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flatpak_entry_class_init (BzFlatpakEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_flatpak_entry_set_property;
  object_class->get_property = bz_flatpak_entry_get_property;
  object_class->dispose      = bz_flatpak_entry_dispose;

  props[PROP_INSTANCE] =
      g_param_spec_object (
          "instance",
          NULL, NULL,
          BZ_TYPE_FLATPAK_INSTANCE,
          G_PARAM_READABLE);

  props[PROP_USER] =
      g_param_spec_boolean (
          "user",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  props[PROP_FLATPAK_ID] =
      g_param_spec_string (
          "flatpak-id",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_NAME] =
      g_param_spec_string (
          "application-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_RUNTIME] =
      g_param_spec_string (
          "application-runtime",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_COMMAND] =
      g_param_spec_string (
          "application-command",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_RUNTIME_NAME] =
      g_param_spec_string (
          "runtime-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_ADDON_OF_REF] =
      g_param_spec_string (
          "addon-extension-of-ref",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_flatpak_entry_init (BzFlatpakEntry *self)
{
}

BzFlatpakEntry *
bz_flatpak_entry_new_for_remote_ref (BzFlatpakInstance *instance,
                                     gboolean           user,
                                     FlatpakRemote     *remote,
                                     FlatpakRemoteRef  *rref,
                                     AsComponent       *component,
                                     const char        *appstream_dir,
                                     GdkPaintable      *remote_icon,
                                     GError           **error)
{
  g_autoptr (BzFlatpakEntry) self         = NULL;
  GBytes *bytes                           = NULL;
  g_autoptr (GKeyFile) key_file           = NULL;
  gboolean         result                 = FALSE;
  guint            kinds                  = 0;
  const char      *id                     = NULL;
  g_autofree char *unique_id              = NULL;
  guint64          download_size          = 0;
  const char      *title                  = NULL;
  const char      *eol                    = NULL;
  const char      *description            = NULL;
  const char      *metadata_license       = NULL;
  const char      *project_license        = NULL;
  gboolean         is_floss               = FALSE;
  const char      *project_group          = NULL;
  const char      *developer              = NULL;
  const char      *developer_id           = NULL;
  g_autofree char *long_description       = NULL;
  const char      *remote_name            = NULL;
  const char      *project_url            = NULL;
  g_autoptr (GPtrArray) as_search_tokens  = NULL;
  g_autoptr (GPtrArray) search_tokens     = NULL;
  g_autoptr (GdkTexture) icon_paintable   = NULL;
  g_autoptr (BzPaintableModel) paintables = NULL;
  g_autoptr (GListStore) share_urls       = NULL;
  g_autofree char *donation_url           = NULL;
  g_autoptr (GListStore) native_reviews   = NULL;
  double           average_rating         = 0.0;
  g_autofree char *ratings_summary        = NULL;
  g_autoptr (GListStore) version_history  = NULL;

  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (instance), NULL);
  g_return_val_if_fail (FLATPAK_IS_REMOTE_REF (rref), NULL);
  g_return_val_if_fail (appstream_dir != NULL, NULL);

  self          = g_object_new (BZ_TYPE_FLATPAK_ENTRY, NULL);
  self->flatpak = g_object_ref (instance);
  self->user    = user;
  self->rref    = g_object_ref (rref);

  key_file = g_key_file_new ();
  bytes    = flatpak_remote_ref_get_metadata (rref);

  result = g_key_file_load_from_bytes (
      key_file, bytes, G_KEY_FILE_NONE, error);
  if (!result)
    return NULL;

#define GET_STRING(member, group_name, key) \
  G_STMT_START                              \
  {                                         \
    self->member = g_key_file_get_string (  \
        key_file, group_name, key, error);  \
    if (self->member == NULL)               \
      return NULL;                          \
  }                                         \
  G_STMT_END

  if (g_key_file_has_group (key_file, "Application"))
    {
      kinds |= BZ_ENTRY_KIND_APPLICATION;

      GET_STRING (application_name, "Application", "name");
      GET_STRING (application_runtime, "Application", "runtime");
      GET_STRING (application_command, "Application", "command");
    }

  if (g_key_file_has_group (key_file, "Runtime"))
    {
      kinds |= BZ_ENTRY_KIND_RUNTIME;

      GET_STRING (runtime_name, "Runtime", "name");
    }

  if (g_key_file_has_group (key_file, "ExtensionOf"))
    {
      kinds |= BZ_ENTRY_KIND_ADDON;

      GET_STRING (addon_extension_of_ref, "ExtensionOf", "ref");
    }

#undef GET_STRING

  if (kinds == 0)
    return NULL;

  self->flatpak_id = flatpak_ref_format_ref (FLATPAK_REF (rref));

  id            = flatpak_ref_get_name (FLATPAK_REF (rref));
  unique_id     = bz_flatpak_ref_format_unique (FLATPAK_REF (rref), user);
  download_size = flatpak_remote_ref_get_download_size (rref);

  if (component != NULL)
    {
      const char    *long_description_raw = NULL;
      AsIcon        *icon                 = NULL;
      AsDeveloper   *developer_obj        = NULL;
      GPtrArray     *screenshots          = NULL;
      AsReleaseList *releases             = NULL;
      GPtrArray     *releases_arr         = NULL;
      // GPtrArray   *reviews              = NULL;

      title = as_component_get_name (component);
      if (title == NULL)
        title = as_component_get_id (component);

      description      = as_component_get_summary (component);
      metadata_license = as_component_get_metadata_license (component);
      project_license  = as_component_get_project_license (component);
      is_floss         = as_component_is_floss (component);
      project_group    = as_component_get_project_group (component);
      project_url      = as_component_get_url (component, AS_URL_KIND_HOMEPAGE);
      as_search_tokens = as_component_get_search_tokens (component);

      developer_obj = as_component_get_developer (component);
      if (developer_obj != NULL)
        {
          developer    = as_developer_get_name (developer_obj);
          developer_id = as_developer_get_id (developer_obj);
        }

      long_description_raw = as_component_get_description (component);
      if (long_description_raw != NULL)
        {
          g_autoptr (XbSilo) silo          = NULL;
          g_autoptr (XbNode) root          = NULL;
          g_autoptr (GString) string       = NULL;
          g_autoptr (GRegex) cleanup_regex = NULL;

          silo = xb_silo_new_from_xml (long_description_raw, error);
          if (silo == NULL)
            return NULL;

          root   = xb_silo_get_root (silo);
          string = g_string_new (NULL);

          cleanup_regex = g_regex_new (
              "^ +| +$|\\t+|\\A\\s+|\\s+\\z",
              G_REGEX_MULTILINE,
              G_REGEX_MATCH_DEFAULT,
              NULL);
          g_assert (cleanup_regex != NULL);

          for (int i = 0; root != NULL; i++)
            {
              const char *tail = NULL;
              XbNode     *next = NULL;

              compile_appstream_description (root, string, NO_ELEMENT, i);

              tail = xb_node_get_tail (root);
              if (tail != NULL)
                append_markup_escaped (string, tail);

              next = xb_node_get_next (root);
              g_object_unref (root);
              root = next;
            }

          /* Could be better but bleh */
          g_string_replace (string, "  ", "", 0);
          long_description = g_regex_replace (
              cleanup_regex, string->str, -1, 0,
              "", G_REGEX_MATCH_DEFAULT, NULL);
        }

      icon = as_component_get_icon_stock (component);
      if (icon != NULL)
        {
          for (int i = 128; i > 0; i -= 64)
            {
              g_autofree char *resolution = NULL;
              g_autofree char *basename   = NULL;
              g_autoptr (GFile) icon_file = NULL;

              resolution = g_strdup_printf ("%dx%d", i, i);
              basename   = g_strdup_printf ("%s.png", as_icon_get_name (icon));
              icon_file  = g_file_new_build_filename (
                  appstream_dir,
                  "icons",
                  "flatpak",
                  resolution,
                  basename,
                  NULL);
              if (g_file_query_exists (icon_file, NULL))
                {
                  /* Using glycin here is extremely slow for,
                   * some reason so we'll opt for the old method.
                   */
                  icon_paintable = gdk_texture_new_from_file (icon_file, NULL);
                  break;
                }
            }
        }

      screenshots = as_component_get_screenshots_all (component);
      if (screenshots != NULL)
        {
          g_autoptr (GListStore) files = NULL;

          files = g_list_store_new (G_TYPE_FILE);

          for (guint i = 0; i < screenshots->len; i++)
            {
              AsScreenshot *screenshot = NULL;
              GPtrArray    *images     = NULL;

              screenshot = g_ptr_array_index (screenshots, i);
              images     = as_screenshot_get_images_all (screenshot);

              for (guint j = 0; j < images->len; j++)
                {
                  AsImage    *image_obj             = NULL;
                  const char *url                   = NULL;
                  g_autoptr (GFile) screenshot_file = NULL;

                  image_obj       = g_ptr_array_index (images, j);
                  url             = as_image_get_url (image_obj);
                  screenshot_file = g_file_new_for_uri (url);

                  if (screenshot_file != NULL)
                    {
                      g_list_store_append (files, screenshot_file);
                      break;
                    }
                }
            }

          if (g_list_model_get_n_items (G_LIST_MODEL (files)) > 0)
            paintables = bz_paintable_model_new (G_LIST_MODEL (files));
        }

      share_urls = g_list_store_new (BZ_TYPE_URL);
      for (int e = AS_URL_KIND_UNKNOWN + 1; e < AS_URL_KIND_LAST; e++)
        {
          const char *url = NULL;

          url = as_component_get_url (component, e);
          if (url != NULL)
            {
              const char *enum_string     = NULL;
              g_autoptr (BzUrl) share_url = NULL;

              switch (e)
                {
                case AS_URL_KIND_HOMEPAGE:
                  enum_string = "Homepage";
                  break;
                case AS_URL_KIND_BUGTRACKER:
                  enum_string = "Bugtracker";
                  break;
                case AS_URL_KIND_FAQ:
                  enum_string = "FAQ";
                  break;
                case AS_URL_KIND_HELP:
                  enum_string = "Help";
                  break;
                case AS_URL_KIND_DONATION:
                  enum_string = "Donation";
                  break;
                case AS_URL_KIND_TRANSLATE:
                  enum_string = "Translate";
                  break;
                case AS_URL_KIND_CONTACT:
                  enum_string = "Contact";
                  break;
                case AS_URL_KIND_VCS_BROWSER:
                  enum_string = "VCS Browser";
                  break;
                case AS_URL_KIND_CONTRIBUTE:
                  enum_string = "Contribute";
                  break;
                default:
                  break;
                }

              share_url = g_object_new (
                  BZ_TYPE_URL,
                  "name", enum_string,
                  "url", url,
                  NULL);
              g_list_store_append (share_urls, share_url);

              if (e == AS_URL_KIND_DONATION)
                {
                  g_clear_pointer (&donation_url, g_free);
                  donation_url = g_strdup (url);
                }
            }
        }
      if (g_list_model_get_n_items (G_LIST_MODEL (share_urls)) == 0)
        g_clear_object (&share_urls);

      releases = as_component_load_releases (component, TRUE, error);
      if (releases == NULL)
        return NULL;
      releases_arr = as_release_list_get_entries (releases);
      if (releases_arr != NULL)
        {
          version_history = g_list_store_new (BZ_TYPE_RELEASE);

          for (guint i = 0; i < releases_arr->len; i++)
            {
              AsRelease *as_release         = NULL;
              GPtrArray *as_issues          = NULL;
              g_autoptr (GListStore) issues = NULL;
              g_autoptr (BzRelease) release = NULL;

              as_release = g_ptr_array_index (releases_arr, i);
              as_issues  = as_release_get_issues (as_release);

              if (as_issues != NULL && as_issues->len > 0)
                {
                  issues = g_list_store_new (BZ_TYPE_ISSUE);

                  for (guint j = 0; j < as_issues->len; j++)
                    {
                      AsIssue *as_issue         = NULL;
                      g_autoptr (BzIssue) issue = NULL;

                      as_issue = g_ptr_array_index (as_issues, j);

                      issue = g_object_new (
                          BZ_TYPE_ISSUE,
                          "id", as_issue_get_id (as_issue),
                          "url", as_issue_get_url (as_issue),
                          NULL);
                      g_list_store_append (issues, issue);
                    }
                }

              release = g_object_new (
                  BZ_TYPE_RELEASE,
                  "issues", issues,
                  "timestamp", as_release_get_timestamp (as_release),
                  "url", as_release_get_url (as_release, AS_RELEASE_URL_KIND_DETAILS),
                  "version", as_release_get_version (as_release),
                  NULL);
              g_list_store_append (version_history, release);
            }
        }

      // reviews = as_component_get_reviews (component);
      // if (reviews != NULL && reviews->len > 0)
      //   {
      //     double ratings_sum = 0.0;

      //     native_reviews = g_list_store_new (BZ_TYPE_REVIEW);

      //     for (guint i = 0; i < reviews->len; i++)
      //       {
      //         AsReview *review                   = NULL;
      //         double    rating                   = 0.0;
      //         g_autoptr (BzReview) native_review = NULL;

      //         review = g_ptr_array_index (reviews, i);
      //         rating = (double) as_review_get_rating (review) / 100.0;
      //         ratings_sum += rating;

      //         native_review = g_object_new (
      //             BZ_TYPE_REVIEW,
      //             "priority", as_review_get_priority (review),
      //             "id", as_review_get_id (review),
      //             "summary", as_review_get_summary (review),
      //             "description", as_review_get_description (review),
      //             "locale", as_review_get_locale (review),
      //             "rating", rating,
      //             "version", as_review_get_version (review),
      //             "reviewer-id", as_review_get_reviewer_id (review),
      //             "reviewer-name", as_review_get_reviewer_name (review),
      //             "date", as_review_get_date (review),
      //             "was-self", (gboolean) (as_review_get_flags (review) & AS_REVIEW_FLAG_SELF),
      //             "self-voted", (gboolean) (as_review_get_flags (review) & AS_REVIEW_FLAG_VOTED),
      //             NULL);
      //         g_list_store_append (native_reviews, native_review);
      //       }

      //     average_rating  = ratings_sum / (double) reviews->len;
      //     ratings_summary = g_strdup_printf (
      //         "%.2f stars (%d reviews)",
      //         average_rating * 5.0,
      //         reviews->len);
      //   }
    }

  search_tokens = g_ptr_array_new_with_free_func (g_free);
  if (as_search_tokens != NULL)
    {
      for (guint i = 0; i < as_search_tokens->len; i++)
        g_ptr_array_add (
            search_tokens,
            g_strdup (g_ptr_array_index (as_search_tokens, i)));
    }

  g_ptr_array_add (search_tokens, g_strdup (self->application_name));
  g_ptr_array_add (search_tokens, g_strdup (self->application_runtime));
  g_ptr_array_add (search_tokens, g_strdup (self->application_command));

  if (title != NULL)
    g_ptr_array_add (search_tokens, g_strdup (title));
  else if (self->application_name != NULL)
    title = self->application_name;
  else if (self->runtime_name != NULL)
    title = self->runtime_name;
  else
    title = self->flatpak_id;

  eol = flatpak_remote_ref_get_eol (rref);
  if (eol != NULL)
    g_ptr_array_add (search_tokens, g_strdup (eol));

  if (description != NULL)
    g_ptr_array_add (search_tokens, g_strdup (description));
  if (long_description != NULL)
    g_ptr_array_add (search_tokens, g_strdup (long_description));

  remote_name = flatpak_remote_get_name (remote);
  if (remote_name != NULL)
    g_ptr_array_add (search_tokens, g_strdup (remote_name));

  if (metadata_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (metadata_license));
  if (project_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (project_license));
  if (project_group != NULL)
    g_ptr_array_add (search_tokens, g_strdup (project_group));
  if (developer != NULL)
    g_ptr_array_add (search_tokens, g_strdup (developer));
  if (developer_id != NULL)
    g_ptr_array_add (search_tokens, g_strdup (developer_id));
  if (metadata_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (metadata_license));
  if (project_url != NULL)
    g_ptr_array_add (search_tokens, g_strdup (project_url));

  g_object_set (
      self,
      "kinds", kinds,
      "id", id,
      "unique-id", unique_id,
      "title", title,
      "eol", eol,
      "description", description,
      "long-description", long_description,
      "remote-repo-name", remote_name,
      "url", project_url,
      "size", download_size,
      "icon-paintable", icon_paintable,
      "search-tokens", search_tokens,
      "remote-repo-icon", remote_icon,
      "metadata-license", metadata_license,
      "project-license", project_license,
      "is-floss", is_floss,
      "project-group", project_group,
      "developer", developer,
      "developer-id", developer_id,
      "screenshot-paintables", paintables,
      "share-urls", share_urls,
      "donation-url", donation_url,
      "reviews", native_reviews,
      "average-rating", average_rating,
      "ratings-summary", ratings_summary,
      "version-history", version_history,
      NULL);

  return g_steal_pointer (&self);
}

char *
bz_flatpak_ref_format_unique (FlatpakRef *ref,
                              gboolean    user)
{
  g_autofree char *fmt = NULL;

  fmt = flatpak_ref_format_ref (FLATPAK_REF (ref));
  return g_strdup_printf (
      "FLATPAK %s: %s",
      user ? "USER" : "SYSTEM",
      fmt);
}

FlatpakRef *
bz_flatpak_entry_get_ref (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return FLATPAK_REF (self->rref);
}

gboolean
bz_flatpak_entry_is_user (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), FALSE);
  return self->user;
}

const char *
bz_flatpak_entry_get_flatpak_id (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->flatpak_id;
}

const char *
bz_flatpak_entry_get_application_name (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->application_name;
}

const char *
bz_flatpak_entry_get_runtime_name (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->runtime_name;
}

const char *
bz_flatpak_entry_get_addon_extension_of_ref (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->addon_extension_of_ref;
}

gboolean
bz_flatpak_entry_launch (BzFlatpakEntry *self,
                         GError        **error)
{
  FlatpakInstallation *installation = NULL;

  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), FALSE);

  installation = bz_flatpak_instance_get_installation (self->flatpak);

  /* async? */
  return flatpak_installation_launch (
      installation,
      flatpak_ref_get_name (FLATPAK_REF (self->rref)),
      flatpak_ref_get_arch (FLATPAK_REF (self->rref)),
      flatpak_ref_get_branch (FLATPAK_REF (self->rref)),
      flatpak_ref_get_commit (FLATPAK_REF (self->rref)),
      NULL, error);
}

static void
compile_appstream_description (XbNode  *node,
                               GString *string,
                               int      parent_kind,
                               int      idx)
{
  XbNode     *child   = NULL;
  const char *element = NULL;
  const char *text    = NULL;
  int         kind    = NO_ELEMENT;

  child   = xb_node_get_child (node);
  element = xb_node_get_element (node);
  text    = xb_node_get_text (node);

  if (element != NULL)
    {
      if (g_strcmp0 (element, "p") == 0)
        kind = PARAGRAPH;
      else if (g_strcmp0 (element, "ol") == 0)
        kind = ORDERED_LIST;
      else if (g_strcmp0 (element, "ul") == 0)
        kind = UNORDERED_LIST;
      else if (g_strcmp0 (element, "li") == 0)
        kind = LIST_ITEM;
      else if (g_strcmp0 (element, "code") == 0)
        kind = CODE;
      else if (g_strcmp0 (element, "em") == 0)
        kind = EMPHASIS;
    }

  if (string->len > 0 &&
      (kind == PARAGRAPH ||
       kind == ORDERED_LIST ||
       kind == UNORDERED_LIST))
    g_string_append (string, "\n");

  if (kind == EMPHASIS)
    g_string_append (string, "<b>");
  else if (kind == CODE)
    g_string_append (string, "<tt>");

  if (kind == LIST_ITEM)
    {
      switch (parent_kind)
        {
        case ORDERED_LIST:
          g_string_append_printf (string, "%d. ", idx);
          break;
        case UNORDERED_LIST:
          g_string_append (string, "- ");
          break;
        default:
          break;
        }
    }

  if (text != NULL)
    append_markup_escaped (string, text);

  for (int i = 0; child != NULL; i++)
    {
      const char *tail = NULL;
      XbNode     *next = NULL;

      compile_appstream_description (child, string, kind, i);

      tail = xb_node_get_tail (child);
      if (tail != NULL)
        append_markup_escaped (string, tail);

      next = xb_node_get_next (child);
      g_object_unref (child);
      child = next;
    }

  if (kind == EMPHASIS)
    g_string_append (string, "</b>");
  else if (kind == CODE)
    g_string_append (string, "</tt>");
  else
    g_string_append (string, "\n");
}

static inline void
append_markup_escaped (GString    *string,
                       const char *append)
{
  g_autofree char *escaped = NULL;

  escaped = g_markup_escape_text (append, -1);
  g_string_append (string, escaped);
}

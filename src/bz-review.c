/* bz-review.c
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

#include "bz-review.h"

typedef struct
{
  int        priority;
  char      *id;
  char      *summary;
  char      *description;
  char      *locale;
  double     rating;
  char      *version;
  char      *reviewer_id;
  char      *reviewer_name;
  GDateTime *date;
  gboolean   was_self;
  gboolean   self_voted;
} BzReviewPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzReview, bz_review, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_PRIORITY,
  PROP_ID,
  PROP_SUMMARY,
  PROP_DESCRIPTION,
  PROP_LOCALE,
  PROP_RATING,
  PROP_VERSION,
  PROP_REVIEWER_ID,
  PROP_REVIEWER_NAME,
  PROP_DATE,
  PROP_WAS_SELF,
  PROP_SELF_VOTED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_review_dispose (GObject *object)
{
  BzReview        *self = BZ_REVIEW (object);
  BzReviewPrivate *priv = bz_review_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->summary, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->locale, g_free);
  g_clear_pointer (&priv->version, g_free);
  g_clear_pointer (&priv->reviewer_id, g_free);
  g_clear_pointer (&priv->reviewer_name, g_free);
  g_clear_pointer (&priv->date, g_date_time_unref);

  G_OBJECT_CLASS (bz_review_parent_class)->dispose (object);
}

static void
bz_review_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BzReview        *self = BZ_REVIEW (object);
  BzReviewPrivate *priv = bz_review_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, priv->priority);
      break;
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_SUMMARY:
      g_value_set_string (value, priv->summary);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_LOCALE:
      g_value_set_string (value, priv->locale);
      break;
    case PROP_RATING:
      g_value_set_double (value, priv->rating);
      break;
    case PROP_VERSION:
      g_value_set_string (value, priv->version);
      break;
    case PROP_REVIEWER_ID:
      g_value_set_string (value, priv->reviewer_id);
      break;
    case PROP_REVIEWER_NAME:
      g_value_set_string (value, priv->reviewer_name);
      break;
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    case PROP_WAS_SELF:
      g_value_set_boolean (value, priv->was_self);
      break;
    case PROP_SELF_VOTED:
      g_value_set_boolean (value, priv->self_voted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_review_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BzReview        *self = BZ_REVIEW (object);
  BzReviewPrivate *priv = bz_review_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      priv->priority = g_value_get_int (value);
      break;
    case PROP_ID:
      g_clear_pointer (&priv->id, g_free);
      priv->id = g_value_dup_string (value);
      break;
    case PROP_SUMMARY:
      g_clear_pointer (&priv->summary, g_free);
      priv->summary = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_LOCALE:
      g_clear_pointer (&priv->locale, g_free);
      priv->locale = g_value_dup_string (value);
      break;
    case PROP_RATING:
      priv->rating = g_value_get_double (value);
      break;
    case PROP_VERSION:
      g_clear_pointer (&priv->version, g_free);
      priv->version = g_value_dup_string (value);
      break;
    case PROP_REVIEWER_ID:
      g_clear_pointer (&priv->reviewer_id, g_free);
      priv->reviewer_id = g_value_dup_string (value);
      break;
    case PROP_REVIEWER_NAME:
      g_clear_pointer (&priv->reviewer_name, g_free);
      priv->reviewer_name = g_value_dup_string (value);
      break;
    case PROP_DATE:
      g_clear_pointer (&priv->date, g_date_time_unref);
      priv->date = g_value_dup_boxed (value);
      break;
    case PROP_WAS_SELF:
      priv->was_self = g_value_get_boolean (value);
      break;
    case PROP_SELF_VOTED:
      priv->self_voted = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_review_class_init (BzReviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_review_set_property;
  object_class->get_property = bz_review_get_property;
  object_class->dispose      = bz_review_dispose;

  props[PROP_PRIORITY] =
      g_param_spec_int (
          "priority",
          NULL, NULL,
          G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE);

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SUMMARY] =
      g_param_spec_string (
          "summary",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_LOCALE] =
      g_param_spec_string (
          "locale",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_RATING] =
      g_param_spec_double (
          "rating",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_VERSION] =
      g_param_spec_string (
          "version",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REVIEWER_ID] =
      g_param_spec_string (
          "reviewer-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REVIEWER_NAME] =
      g_param_spec_string (
          "reviewer-name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DATE] =
      g_param_spec_boxed (
          "date",
          NULL, NULL,
          G_TYPE_DATE_TIME,
          G_PARAM_READWRITE);

  props[PROP_WAS_SELF] =
      g_param_spec_boolean (
          "was-self",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_SELF_VOTED] =
      g_param_spec_boolean (
          "self-voted",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_review_init (BzReview *self)
{
}

/* bz-global-state.c
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

#include "bz-global-state.h"

static DexFuture *
http_send_fiber (SoupMessage *message);

static void
http_send_finish (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data);

SoupSession *
bz_get_global_http_session (void)
{
  static SoupSession *session = NULL;

  if (g_once_init_enter_pointer (&session))
    g_once_init_leave_pointer (&session, soup_session_new ());

  return session;
}

DexFuture *
bz_send_with_global_http_session (SoupMessage *message)
{
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));

  return dex_scheduler_spawn (
      dex_scheduler_get_default (), 0,
      (DexFiberFunc) http_send_fiber,
      g_object_ref (message), g_object_unref);
}

static DexFuture *
http_send_fiber (SoupMessage *message)
{
  g_autoptr (DexPromise) promise = NULL;

  promise = dex_promise_new_cancellable ();
  soup_session_send_async (
      bz_get_global_http_session (),
      message,
      G_PRIORITY_DEFAULT,
      dex_promise_get_cancellable (promise),
      http_send_finish,
      dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static void
http_send_finish (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  DexPromise *promise             = user_data;
  g_autoptr (GError) local_error  = NULL;
  g_autoptr (GInputStream) stream = NULL;

  g_assert (SOUP_IS_SESSION (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  stream = soup_session_send_finish (SOUP_SESSION (object), result, &local_error);
  if (stream != NULL)
    dex_promise_resolve_object (promise, g_steal_pointer (&stream));
  else
    dex_promise_reject (promise, g_steal_pointer (&local_error));

  dex_unref (promise);
}

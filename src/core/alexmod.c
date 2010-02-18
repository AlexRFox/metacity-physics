/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "alexmod.h"
#include <config.h>
#include "main.h"
#include "util.h"
#include "display-private.h"
#include "errors.h"
#include "ui.h"
#include "session.h"
#include "prefs.h"

#include <stdlib.h>
#include <sys/types.h>
#include <wait.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <time.h>

#include "frame-private.h"
#include <time.h>
#include <sys/time.h>
#include <math.h>

static double
get_secs (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return (tv.tv_sec + tv.tv_usec/1e6);
}

static void
alex_window_proc (gpointer spec, gpointer user_data)
{
  MetaWindow *window = spec;
  MetaRectangle rect;
  MetaFrameGeometry geomp;
  double now, dt, dx, dy, inst_vel, time_const, factor;
  int left, top, right, bottom, newx, newy, screenx, screeny, bounce_flag;
  static double velx, vely;

  now = get_secs ();
  dt = now - window->lasttime;

  window->lasttime = now;

  meta_window_get_client_root_coords (window, &rect);

  alex_window_get_position (window, &left, &top);

  if (window->frame)
    meta_frame_calc_geometry (window->frame, &geomp);
  else
    return;

  right = left + geomp.left_width + rect.width + geomp.right_width;
  bottom = top + geomp.top_height + rect.height + geomp.bottom_height;

  switch (window->phys_state) {
  case 1:
    time_const = dt;
    
    dx = rect.x - window->lastx;
    inst_vel = (dx / dt);
    factor = dt / time_const;
    if (factor < 0)
      factor = 0;
    if (factor > 1)
      factor = 1;
    velx = velx * (1 - factor) + inst_vel * factor;

    dy = rect.y - window->lasty;
    inst_vel = (dy / dt);
    factor = dt / time_const;
    if (factor < 0)
      factor = 0;
    if (factor > 1)
      factor = 1;
    vely = vely * (1 - factor) + inst_vel * factor;

    window->theta = atan2 (vely, velx);
    /* window->speed = hypot (vely, velx); */
    window->speed = sqrt (vely * vely + velx * velx);
    break;
  case 2:
    if (now - window->lasttimemouse > .1) {
      window->speed = 0;
      window->velx = 0;
      window->vely = 0;
      window->phys_state = 0;
      break;
    }

    window->phys_state = 3;
  case 3:
    bounce_flag = 0;

    velx = window->speed * cos (window->theta);
    vely = window->speed * sin (window->theta);

    newx = rect.x + velx * dt;
    newy = rect.y + vely * dt;

    if (left + velx * dt <= 0) {
      meta_window_move (window, TRUE, 0, newy);

      window->theta = atan2 (vely, -velx);
      window->speed -= 200;

      bounce_flag = 1;
    }

    if (top + vely * dt <= 25) {
      meta_window_move (window, TRUE, newx, 25 + geomp.top_height);

      window->theta = atan2 (-vely, velx);
      window->speed -= 200;

      bounce_flag = 1;
    }

   screenx = window->screen->xscreen->width;
   screeny = window->screen->xscreen->height;

    if (right + velx * dt >= screenx) {
      meta_window_move (window, TRUE, screenx - geomp.right_width - rect.width, newy);

      window->theta = atan2 (vely, -velx);
      window->speed -= 200;

      bounce_flag = 1;
    }

    if (bottom + vely * dt >= screeny - 25) {
      meta_window_move (window, TRUE, newx, screeny - 25 - geomp.bottom_height - rect.height);

      window->theta = atan2 (-vely, velx);
      window->speed -= 200;

      bounce_flag = 1;
    }

    if (bounce_flag == 0) {
      meta_window_move (window, TRUE, newx, newy);
    }

    if (window->speed < 20) {
      window->speed = 0;
      window->phys_state = 0;
    }
    
    window->speed -= 1000 * dt;
    break;
  }
}

gboolean
alex_windows_loop (gpointer data)
{
  MetaDisplay *alexdisplay;
  GSList *display_windows;

  alexdisplay = meta_get_display ();

  display_windows = meta_display_list_windows (alexdisplay);

  g_slist_foreach (display_windows, alex_window_proc, NULL);

  return (1);
}

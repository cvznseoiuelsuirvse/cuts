#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "wayland/proto/wayland.h"
#include "wayland/impl/wayland.h"
#include "wayland/proto/xdg-shell.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/proto/viewporter.h"
#include "wayland/impl/viewporter.h"
#include "wayland/proto/xdg-decoration-unstable-v1.h"
#include "wayland/proto/cursor-shape-v1.h"

#include "compositor/window.h"
#include "compositor/scene.h"

#include "output/output.h"
#include "cursor/cursor.h"

#include "seat/session/session.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"
#include "util/malloc.h"

#include "config.h"

#define MIN_WINDOW_SIZE 2
#define MAX_STDIN_CHARS 128

#define LAYOUT(mon)                                                            \
  {                                                                            \
    cuts.layout.func();                                                        \
    output_damage(mon->output);                                                \
  }

#define clients_for_each_in_tag(client) \
  c_list_for_each(cuts.clients, (client)) \
    if ((client->tag & cuts.focused_tag))

#define check_init(t, ret, out_label)                                          \
  if ((t) == NULL) {                                                           \
    (ret) = 1;                                                                 \
    c_log(C_LOG_ERROR, "failed to initialize " #t);                            \
    goto out_label;                                                            \
  }

#define set_color(dst, src)                                                    \
  {                                                                            \
    dst[0] = src[0];                                                           \
    dst[1] = src[1];                                                           \
    dst[2] = src[2];                                                           \
    dst[3] = src[3];                                                           \
  }

#define pointer_x cuts.pointer.x[cuts.pointer.coords]
#define pointer_y cuts.pointer.y[cuts.pointer.coords]
#define pointer_x_prev cuts.pointer.x[cuts.pointer.coords ^ 1]
#define pointer_y_prev cuts.pointer.y[cuts.pointer.coords ^ 1]

#define bit_index(n, i)                                                        \
  {                                                                            \
    i = 0;                                                                     \
    uint32_t _n = n;                                                           \
    while ((_n >>= 1))                                                         \
      i++;                                                                     \
  }

#define is_bar_horizontal(bar) ((bar)->cfg->pos == BAR_TOP) || ((bar)->cfg->pos == BAR_BOTTOM)
#define is_bar_vertical(bar) ((bar)->cfg->pos == BAR_RIGHT) || ((bar)->cfg->pos == BAR_LEFT)
#define is_window_fullscreen(window) (window->state & C_WINDOW_FULLSCREEN)
#define is_window_floating(window) (window->state & C_WINDOW_FLOAT)

#define text_rect_width(text_len, bar)                                         \
  ((text_len * (bar->glyph.width + bar->glyph.spacing)) - bar->glyph.spacing +  \
      bar->h_padding * 2)

#define text_rect_height(text_len, bar)                                        \
  ((text_len * (bar->glyph.height + bar->glyph.spacing)) - bar->glyph.spacing +  \
      bar->v_padding * 2)

struct bar_block {
  struct c_scene_rect   rect;
  struct c_scene_node   *rect_node;

  struct c_scene_buffer text;
  struct c_scene_node   *text_node;
};

struct bar {
  const struct bar_config *cfg;

  struct {
    FT_Library library;
    FT_Face face;
  } ft;

  struct {
    uint32_t spacing;
    uint32_t width, height;
  } glyph;

  uint32_t h_padding, v_padding;
  uint32_t width, height;

  struct bar_block blocks[12];
  size_t block_n;
};

struct monitor {
  struct c_output *output;
  struct monitor_config *cfg;
};

struct client {
  uint32_t tag;
  uint64_t z;

  struct monitor *mon;
  struct c_window *window;

  // top, right, bottom, left
  struct c_scene_node *border[4];
  struct c_scene_rect  border_rect[4];
};

struct {
  struct c_event_loop *loop;
  struct c_wl_display *display;
  struct c_session *session;
  struct c_output_manager *mgr;
  struct c_scene *scene;

  struct {
    struct c_cursor *cur;
    double x[2];
    double y[2];
    int coords;
    int is_dragging;
  } pointer;

  struct {
    struct c_wl_connection  *src;
    struct c_wl_connection  *dst;
    struct c_wl_data_source *data_source;
    struct c_wl_data_device *data_device;
  } cb;

  struct {
    struct c_wl_connection  *src;
    struct c_wl_connection  *dst;
    struct c_wl_data_source *data_source;
    struct c_wl_data_device *data_device;
    enum wl_data_device_manager_dnd_action_enum negotiated_actions;
  } dnd;


  c_list *clients;
  struct client *focused_client;

  c_list *monitors;
  struct monitor *focused_mon;

  uint32_t focused_tag;
  int focused_client_idx;
	struct layout layout;
  struct bar bar;
  struct c_scene_rect background;

  uint64_t next_z;
  uint32_t fullscreen;
  int is_quitting;

} cuts = {0};

struct tile_layout {
  uint32_t x, y, height, width;

  struct {
    uint32_t x, width;
  } master;

  struct {
    uint32_t x, width;
  } stack;

};

int get_fontpath(const char *font, char *fontpath, size_t size);
uint32_t utf8_char(const char *s, size_t *i);
size_t utf8_len(const char *s);
void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height);
void collect_window_tree(struct c_window *window, struct c_wl_surface *surface,
                        double x, double y, c_list *quads, int depth);
static void scene_collect_window(struct c_scene_node *node, c_list *quads);
void output_damage(struct c_output *output);
struct c_wl_surface *find_root_surface(struct c_wl_surface *surface);
void set_background(uint32_t width, uint32_t height, double x, double y);
void cursor_move(struct c_cursor *cur, double x, double y);
int count_tiled();
void calc_tile_layout(struct monitor *mon, struct tile_layout *layout);

struct client *tag_select_client(int direction);
struct client *client_new(struct c_wl_connection *connection);
void client_free(struct client *client);
void client_unfocus(struct client *client);
void client_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_change_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_close(struct client *client);
struct client *client_from_connection(struct c_wl_connection *connection);
struct client *client_from_surface(struct c_wl_surface *surface);
void client_border_create(struct client *client);
void client_border_delete(struct client *client);
void client_border_set_color(struct client *client, const uint32_t color[4]);
void client_border_sync(struct client *client);
void client_border_set_visibility(struct client *client, int is_visible);
void client_border_raise(struct client *client);
void client_raise(struct client *client);
void client_set_fullscreen(struct client *client, struct monitor *mon);
void client_unset_fullscreen(struct client *client);
void client_set_visibility(struct client *client, int is_visible);
void client_toggle_floating(struct client *client);

void on_mouse_movement(struct c_input_mouse_event *event, void *userdata);
void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata);
void on_mouse_button(struct c_input_mouse_event *event, void *userdata);
void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata);

static void *on_wl_output_bind(struct c_wl_connection *conn,
                               struct c_wl_object *wl_output, void *userdata);
int on_get_keyboard(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_get_pointer(struct c_wl_connection *conn, c_wl_args args, void *userdata);
void assign_output_to_surface(struct c_wl_object *wl_surface);
int on_surface_commit(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_surface_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_window_new(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_set_title(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_window_unfullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_window_unset_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_window_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_window_close(struct c_wl_connection *conn, c_wl_args args, void *userdata);
void on_connection_gone(struct c_wl_connection *conn, void *userdata);
int on_data_source_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_data_offer_receive(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_data_offer_set_actions(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_data_offer_accept(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_decorations_set_mode(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_start_drag(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_set_selection(struct c_wl_connection *conn, c_wl_args args, void *userdata);
int on_set_shape(struct c_wl_connection *conn, c_wl_args args, void *userdata);

void quit(bind_args *args);
void spawn(bind_args *args);
void window_kill(bind_args *args);
void set_layout(bind_args *args);
void toggle_fullscreen(bind_args *args);
void move_focus(bind_args *args);
void switch_tag(bind_args *args);
void toggle_floating(bind_args *args);
void change_mfact(bind_args *args);
void change_nmaster(bind_args *args);
void window_move(int done, bind_args *args);
void window_move_to_workspace(bind_args *args);
void window_resize(int done, bind_args *args);
void change_border(bind_args *args);
void change_gap(bind_args *args);

void zoom();
void tile();

void bar_destroy();
void bar_set_title(const char *title);
void bar_clear_title();
void bar_set_layout(struct layout *layout);
void bar_switch_tag(uint32_t current, uint32_t next);
void bar_block_init(struct bar *bar, struct bar_block *block);
void bar_block_update(struct bar_block *block);
void bar_block_clear_text(struct bar *bar, struct bar_block *block);
void bar_block_write_text(struct bar *bar, struct bar_block *block, const char *text, const uint32_t color[4]);
void bar_create(struct c_output_mode *mode);

C_EVENT_CALLBACK stdin_text(struct c_event_loop *loop, int fd, void *userdata);
void signal_handler(int signal, void *userdata);
void cleanup(int err);
int main();

uint32_t utf8_char(const char *s, size_t *i) {
  const unsigned char *c = (const unsigned char *)s;
  uint32_t u = c[*i];
  size_t bytes = 0;

  (*i)++;

  if (u < 0x80) return u;
  else if ((u & 0xE0) == 0xC0) { u &= 0x3F; bytes = 1; }
  else if ((u & 0xF0) == 0xE0) { u &= 0x1F; bytes = 2; }
  else if ((u & 0xF8) == 0xF0) { u &= 0xF;  bytes = 3; }
  else return 0xFFFD;

  for (size_t j = 1; j <= bytes; j++, ++(*i)) {
    if (!((c[*i]) & 0x80)) return 0xFFFD;
    u = (u << 6) | (c[*i] & 0x3f);
  }

  return u;
}

size_t utf8_len(const char *s) {
 size_t i = 0, n = 0;
 while (s[i]) { utf8_char(s, &i); n++; }
 return n;
}

void get_surface_buf_size(struct c_wl_surface *surface, int32_t *width, int32_t *height) {
  if (surface->active->dma) {
    *width = surface->active->dma->width / surface->active->scale;
    *height = surface->active->dma->height / surface->active->scale;
  } else {
    *width = surface->active->shm->width / surface->active->scale;
    *height = surface->active->shm->height / surface->active->scale;
  }
}

void collect_window_tree(struct c_window *window, struct c_wl_surface *surface,
                        double x, double y,
                        c_list *quads, int depth) {
  if (!surface->active) return;

  int32_t buf_width, buf_height;
  get_surface_buf_size(surface, &buf_width, &buf_height);

  struct c_renderer_quad q = {
    .type = C_RENDERER_BUFFER,
    .buffer = surface->active->dma ? (void *)surface->active->dma : (void *)surface->active->shm,
    .buffer_type = surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,
    .uv_offset = {0.0f, 0.0f},
    .uv_scale = {1.0f, 1.0f},
  };

  double base_x, base_y;

  if (window) {
    q.width = window->width;
    q.height = window->height;
    q.x = window->x;
    q.y = window->y;
    base_x = window->x;
    base_y = window->y;
  } else {
    q.width = buf_width;
    q.height = buf_height;
    q.x = x;
    q.y = y;
    base_x = x;
    base_y = y;
  }

  int surf_w = buf_width, surf_h = buf_height;
  double surf_x = 0, surf_y = 0;
  if (surface->viewport) {
    struct c_wp_viewport *vp = surface->viewport;
    if ((vp->src.x + vp->src.width > buf_width) ||
        (vp->src.y + vp->src.height > buf_height)) {
      wl_display_error(window->conn, 1, surface->viewport->obj->id,
                       WP_VIEWPORT_ERROR_OUT_OF_BUFFER,
                       "specified source viewport exceeds associated wl_surface's buffer");
      return;
    }

    if (vp->dst.width > 0)
      surf_w = vp->dst.width;
    if (vp->src.width > 0)
      surf_w = vp->src.width;

    if (vp->dst.height > 0)
      surf_h = vp->dst.height;
    if (vp->src.height > 0)
      surf_h = vp->src.height;

    surf_x = vp->src.x > 0 ? vp->src.x : 0;
    surf_y = vp->src.y > 0 ? vp->src.y : 0;
  }
    
  if (surface->xdg_surface && surface->xdg_surface->width > 0) {
    struct c_xdg_surface *xs = surface->xdg_surface;
    surf_x = MAX(surf_x, xs->x);
    surf_y = MAX(surf_y, xs->y);
    surf_w = MIN(surf_w, xs->width);
    surf_h = MIN(surf_h, xs->height);

  }

  q.width = surf_w;
  q.height = surf_h;

  q.uv_offset[0] = surf_x / buf_width;
  q.uv_offset[1] = surf_y / buf_height;
  q.uv_scale[0] = (double)surf_w / buf_width;
  q.uv_scale[1] = (double)surf_h / buf_height;

  // c_log(C_LOG_DEBUG, "%*s surface#%d (%d) %p %dx%d x=%f y=%f", depth, " ",
  //       surface->id, q.buffer_type, surface, q.width, q.height, q.x, q.y);

  c_list_push(quads, &q, sizeof(q));

  if (surface->sub.children) {
    struct c_wl_subsurface *sub_s;
    c_list_for_each(surface->sub.children, sub_s) {
      if (!sub_s->surface->active) continue;

      get_surface_buf_size(sub_s->surface, &buf_width, &buf_height);

      struct c_renderer_quad q_sub = {
          .type = C_RENDERER_BUFFER,
          .buffer = sub_s->surface->active->dma
                        ? (void *)sub_s->surface->active->dma
                        : (void *)sub_s->surface->active->shm,
          .buffer_type =
              sub_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

          .width = buf_width,
          .height = buf_height,

          .x = base_x + sub_s->x - surf_x,
          .y = base_y + sub_s->y - surf_y,

          .uv_scale = {1.0f, 1.0f},
      };

      // c_log(C_LOG_DEBUG, "%*s SUB-surface#%d (%d) %p %dx%d x=%f y=%f",
      //       depth + 2, " ", sub_s->id, q_sub.buffer_type, sub_s, q_sub.width,
      //       q_sub.height, q_sub.x, q_sub.y);

      c_list_push(quads, &q_sub, sizeof(q_sub));
      collect_window_tree(NULL, sub_s->surface, q_sub.x, q_sub.y, quads, depth + 1);
    }
  }

  if (surface->xdg_surface && surface->xdg_surface->children) {
    struct c_xdg_surface *xdg_s;
    c_list_for_each(surface->xdg_surface->children, xdg_s) {
      if (!xdg_s->surface->active) continue;

      get_surface_buf_size(xdg_s->surface, &buf_width, &buf_height);

      struct c_renderer_quad q_sub = {
        .type = C_RENDERER_BUFFER,
        .buffer = xdg_s->surface->active->dma
                      ? (void *)xdg_s->surface->active->dma
                      : (void *)xdg_s->surface->active->shm,
        .buffer_type = xdg_s->surface->active->dma ? C_BUFFER_DMA : C_BUFFER_RAW,

        .width = xdg_s->popup.positioner.width ? xdg_s->popup.positioner.width : buf_width,
        .height = xdg_s->popup.positioner.height ? xdg_s->popup.positioner.height : buf_height,

        .x = base_x + xdg_s->x + xdg_s->popup.x,
        .y = base_y + xdg_s->y + xdg_s->popup.y,

        .uv_scale = {1.0f, 1.0f},
      };

      // c_log(C_LOG_DEBUG, "%*s XDG-surface#%d (%d) %p %dx%d x=%f y=%f", depth,
      //       " ", xdg_s->id, q_sub.buffer_type, xdg_s, q_sub.width, q_sub.height,
      //       q_sub.x, q_sub.y);

      c_list_push(quads, &q_sub, sizeof(q_sub));
      collect_window_tree(NULL, xdg_s->surface, q_sub.x, q_sub.y, quads, depth + 1);
    }
  }
}

static void scene_collect_window(struct c_scene_node *node, c_list *quads) {
  struct c_window *window = c_scene_node_data(node);
  collect_window_tree(window, window->surface->surface, window->x, window->y, quads, 0);
}

void output_damage(struct c_output *output) {
  if (c_output_damage(cuts.mgr, output))
    c_event_loop_stop(cuts.loop);
}

struct c_wl_surface *find_root_surface(struct c_wl_surface *surface) {
 while (surface->sub.surface && surface->sub.surface->parent) {
   surface = surface->sub.surface->parent;
 }

 while (surface->xdg_surface && surface->xdg_surface->parent) {
   surface = surface->xdg_surface->parent->surface;
 }
 return surface;
}

void set_background(uint32_t width, uint32_t height, double x, double y) {
  struct c_scene_rect *background = &cuts.background;

  set_color(background->color, background_color);

  background->x = x;
  background->y = y;

  background->width = width;
  background->height = height;

  c_scene_add_rect(cuts.scene, background);
}

void cursor_move(struct c_cursor *cur, double x, double y) {
  c_cursor_move(cur, x, y);
  cuts.pointer.coords ^= 1;
  pointer_x = x;
  pointer_y = y;
}

int count_tiled() {
  struct client *client;
  int i = 0;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    if (!(window->state & (C_WINDOW_FLOAT | C_WINDOW_FULLSCREEN))) i++;
  }
  return i;
}

void calc_tile_layout(struct monitor *mon, struct tile_layout *layout) {
  struct c_output_mode *mode = cuts.focused_mon->output->current_mode;
  struct bar *bar = &cuts.bar;

  layout->width = mode->width - gap * 2;
  layout->height = mode->height - gap * 2;

  layout->y = layout->x = gap;

  if (bar->cfg->pos == BAR_TOP) {
    layout->height -= cuts.bar.height;
    layout->y += cuts.bar.height;
  }

  if (bar->cfg->pos == BAR_BOTTOM) {
    layout->height -= cuts.bar.height;
  }

  if (bar->cfg->pos == BAR_RIGHT) {
    layout->width -= cuts.bar.width;
  }

  if (bar->cfg->pos == BAR_LEFT) {
    layout->width -= cuts.bar.width;
    layout->x += cuts.bar.width;
  }

  layout->master.x = layout->x;
  layout->master.width = layout->width * mfact - (uint32_t)(gap / 2);

  layout->stack.x = layout->master.x + layout->master.width + gap;
  layout->stack.width = layout->width - layout->master.width - gap;
}

struct client *tag_select_client(int direction) {
  uint32_t clients = 0;

  struct client *client;
  clients_for_each_in_tag(client)
    clients++;

  if (!clients) return NULL;

  cuts.focused_client_idx =
      (cuts.focused_client_idx + direction + clients) % clients;

  int c = 0;
  clients_for_each_in_tag(client) {
    if (cuts.focused_client_idx == c++) {
      return client;
    }
  }

  return NULL;
}

struct client *client_new(struct c_wl_connection *connection) {
  struct client *client = calloc(1, sizeof(*client));
  if (!client) {
    c_log_errno(C_LOG_ERROR, "failed to allocate a new client");
    return NULL;
  }

  client->tag = cuts.focused_tag;
  client->mon = cuts.focused_mon;
  client_border_create(client);
  return client;
}

void client_free(struct client *client) {
  struct c_window *window = client->window;
  c_window_free(cuts.scene, window);
  client_border_delete(client);
  free(client);
}

void client_unfocus(struct client *client) {
  struct c_window *window = client->window;
  struct c_wl_object *o;
  struct c_wl_connection *conn = window->conn;

  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_data_device")) {
      wl_data_device_leave(conn, o->id);
    }
  }

  c_window_unfocus(window);
  client_border_set_color(cuts.focused_client, border_inactive);
  cuts.focused_client = NULL;
}

void client_focus(struct client *client, double hotspot_x, double hotspot_y) {
  struct c_window *window = client->window;

  if (*window->title)
    bar_set_title(*window->title);

  client_border_set_color(client, border_active);

  struct c_wl_object *o;
  struct c_wl_connection *conn = window->conn;

  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_data_device")) {
      if (!cuts.cb.src) {
        wl_data_device_selection(conn, o->id, 0);
        goto out;

      }
      struct c_wl_data_offer *data_offer = c_malloc(sizeof(*data_offer));
      struct c_wl_data_source *data_source = cuts.cb.data_source;

      data_offer->obj =
          c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, o->version,
                          c_wl_interface_get("wl_data_offer"), data_offer);

      wl_data_device_data_offer(conn, o->id, data_offer->obj->id);
      for (size_t i = 0; i < cuts.cb.data_source->mimes; i++)
        wl_data_offer_offer(conn, data_offer->obj->id, data_source->mimetypes[i]);

      if (cuts.cb.src) {
        wl_data_device_selection(conn, o->id, data_offer->obj->id);
        cuts.cb.dst = conn;

      } else if (cuts.dnd.src) {
        cuts.dnd.dst = conn;
        double lx, ly;
        struct c_wl_surface *target = c_window_surface_at(window, pointer_x, pointer_y, &lx, &ly);
        wl_data_device_enter(conn, o->id, c_wl_serial(), target->obj->id,
                             C_WL_FIXED_FROM_DOUBLE(lx),
                             C_WL_FIXED_FROM_DOUBLE(ly), data_offer->obj->id);
        wl_data_offer_source_actions(conn, data_offer->obj->id, data_source->actions);

      }
    }
  }

out:
  c_window_focus(window, hotspot_x, hotspot_y);
  cuts.focused_client = client;

}

void client_change_focus(struct client *client, double hotspot_x, double hotspot_y) {
  if (client == cuts.focused_client) return;

  if (cuts.focused_client)
    client_unfocus(cuts.focused_client);

  client_focus(client, hotspot_x, hotspot_y);

}

void client_close(struct client *client) {
  int is_focused = cuts.focused_client == client;

  if (is_focused)
    client_unfocus(client);

  client_free(client);
  c_list_remove(&cuts.clients, client);

  if (is_focused) {
    struct client *prev = tag_select_client(-1);

    if (prev) {
      client_change_focus(prev, pointer_x, pointer_y);
    } else {
      cuts.focused_client = NULL;
      bar_clear_title();
    }
  }
}

struct client *client_from_connection(struct c_wl_connection *connection) {
  struct client *client;
  struct c_window *window;
  c_list_for_each(cuts.clients, client) {
    if ((window = client->window) && window->conn == connection)
      return client;
  }

  return NULL;
}

struct client *client_from_xdg_surface(struct c_xdg_surface *surface) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    struct c_window *window = client->window;
    if (window->surface == surface) {
      return client;
    }
  }

  return NULL;
}

void client_border_create(struct client *client) {
  client->border[0] = c_scene_add_rect(cuts.scene, &client->border_rect[0]);
  client->border[1] = c_scene_add_rect(cuts.scene, &client->border_rect[1]);
  client->border[2] = c_scene_add_rect(cuts.scene, &client->border_rect[2]);
  client->border[3] = c_scene_add_rect(cuts.scene, &client->border_rect[3]);
}

void client_border_delete(struct client *client) {
  c_scene_node_remove(cuts.scene, client->border[0]);
  c_scene_node_remove(cuts.scene, client->border[1]);
  c_scene_node_remove(cuts.scene, client->border[2]);
  c_scene_node_remove(cuts.scene, client->border[3]);
}

void client_border_set_color(struct client *client, const uint32_t color[4]) {
  set_color(client->border_rect[0].color, color);
  c_scene_node_update(client->border[0]);

  set_color(client->border_rect[1].color, color);
  c_scene_node_update(client->border[1]);

  set_color(client->border_rect[2].color, color);
  c_scene_node_update(client->border[2]);
   
  set_color(client->border_rect[3].color, color);
  c_scene_node_update(client->border[3]);
}

void client_border_sync(struct client *client) {
#define set_geom(dst, _x, _y, _width, _height)                                 \
  {                                                                            \
    (dst).x = (_x);                                                            \
    (dst).y = (_y);                                                            \
    (dst).width = (_width);                                                    \
    (dst).height = (_height);                                                  \
  }

  struct c_window *window = client->window;

  double x, y;
  uint32_t width, height;

  x = window->x;
  y = window->y;
  width = window->width;
  height = window->height;

  // top
  set_geom(client->border_rect[0], x - border_width, y - border_width,
           width + border_width * 2, border_width);
  c_scene_node_update(client->border[0]);

  // right
  set_geom(client->border_rect[1], x + width, y - border_width,
           border_width, height + border_width * 2);
  c_scene_node_update(client->border[1]);

  // bottom
  set_geom(client->border_rect[2], x - border_width, y + height,
           width + border_width * 2, border_width);
  c_scene_node_update(client->border[2]);
   
  // left
  set_geom(client->border_rect[3], x - border_width, y - border_width,
           border_width, height + border_width * 2);
  c_scene_node_update(client->border[3]);
}

void client_border_set_visibility(struct client *client, int is_visible) {
  c_scene_node_set_visibility(client->border[0], is_visible);
  c_scene_node_set_visibility(client->border[1], is_visible);
  c_scene_node_set_visibility(client->border[2], is_visible);
  c_scene_node_set_visibility(client->border[3], is_visible);

}

void client_border_raise(struct client *client) {
  c_scene_node_raise(cuts.scene, client->border[0]);
  c_scene_node_raise(cuts.scene, client->border[1]);
  c_scene_node_raise(cuts.scene, client->border[2]);
  c_scene_node_raise(cuts.scene, client->border[3]);
}

void client_raise(struct client *client) {
  client_border_raise(client);
  c_scene_node_raise(cuts.scene, client->window->node);
  client->z = ++cuts.next_z;
}

void client_set_fullscreen(struct client *client, struct monitor *mon) {
  struct c_window *window = client->window;
  struct c_output *output = mon->output;

  window->width = output->current_mode->width;
  window->height = output->current_mode->height;

  window->x = output->x;
  window->y = output->y;
  window->state = (window->state & ~C_WINDOW_FLOAT) | C_WINDOW_FULLSCREEN;

  cuts.fullscreen |= client->tag;
  client_raise(client);
  client_border_set_visibility(client, 0);
  LAYOUT(mon);
}

void client_unset_fullscreen(struct client *client) {
  struct c_window *window = client->window;
  window->state &= ~C_WINDOW_FULLSCREEN;
  cuts.fullscreen &= ~client->tag;
  client_border_set_visibility(client, 1);

  LAYOUT(client->mon);
}

void client_set_visibility(struct client *client, int is_visible) {
  struct c_window *window = client->window;
  c_scene_node_set_visibility(client->window->node, is_visible);
  client_border_set_visibility(client, is_visible && (window->state ^ C_WINDOW_FULLSCREEN));
}

void client_toggle_floating(struct client *client) {
  struct c_window *window = client->window;
  window->state ^= C_WINDOW_FLOAT;
}

void on_mouse_movement(struct c_input_mouse_event *event, void *userdata) {
  struct c_cursor *cur = cuts.pointer.cur;
  struct c_output *output = cur->output;
  struct c_output_mode *mode = output->current_mode;

  uint32_t width = mode->width;
  uint32_t height = mode->height;

  double new_x, new_y;
  if (!event->abs) {
    new_x = event->x + pointer_x;
    new_y = event->y + pointer_y;
  } else {
    new_x = libinput_event_pointer_get_absolute_x_transformed(event->libinput_event, width);
    new_y = libinput_event_pointer_get_absolute_y_transformed(event->libinput_event, height);
  }

  new_x = CLAMP(new_x, 0, mode->width);
  new_y = CLAMP(new_y, 0, mode->height);

  cursor_move(cur, new_x, new_y);

  if (cuts.pointer.is_dragging || !cuts.focused_client) return;

  double hotx = new_x + cur->frame->hot_x;
  double hoty = new_y + cur->frame->hot_y;

  struct client *focused = NULL;
  uint64_t top = 0;

  struct client *client;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    if (CURSOR_INSIDE(hotx, hoty, window) && client->z >= top) {
        focused = client;
        top = client->z;
    }
  }

  if (focused == cuts.focused_client) {
    c_window_pointer_move(focused->window, hotx, hoty);

    if (cuts.dnd.src) {
      struct c_wl_object *o;
      c_wl_objects_for_each(focused->window->conn, o) {
        if (STREQ(o->iface->name, "wl_data_device")) {
          double lx, ly;
          c_window_surface_at(focused->window, pointer_x, pointer_y, &lx, &ly);
          wl_data_device_motion(focused->window->conn, o->id, now_ms(),
                                C_WL_FIXED_FROM_DOUBLE(lx),
                                C_WL_FIXED_FROM_DOUBLE(ly));
        }
      }
    }

  } else if (focused) {
    client_change_focus(focused, hotx, hoty);
  }

}

void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!cuts.focused_client) return;
  c_window_pointer_scroll(cuts.focused_client->window, event->axis,
                          event->axis120,
                          (enum wl_pointer_axis_source_enum)event->axis_source,
                          WL_POINTER_AXIS_VERTICAL_SCROLL,
                          event->axis_discrete);
}

void on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (cuts.dnd.src && !event->is_pressed) {
    struct client *client = cuts.focused_client;
    struct c_window *window = client->window;

    struct c_wl_object *o;
    c_wl_objects_for_each(window->conn, o) {
      if (STREQ(o->iface->name, "wl_data_device")) {
        wl_data_device_drop(cuts.dnd.dst, o->id);
      }
    }

    wl_data_source_dnd_drop_performed(cuts.dnd.src, cuts.dnd.data_source->obj->id);
  }

  if (cuts.focused_client)
    c_window_pointer_button(cuts.focused_client->window, event->button, event->is_pressed);
}

void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  if (!cuts.focused_client) return;

  c_window_keyboard_key(cuts.focused_client->window, event->key, event->pressed,
                        event->mods_depressed, event->mods_latched,
                        event->mods_locked, event->group, event->changed);
}

int on_wl_output_release(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_output *output = c_wl_self(conn, args)->data;
  c_unref(output);
  return 0;
}

void *on_wl_output_bind(struct c_wl_connection *conn, struct c_wl_object *wl_output, void *userdata) {
  struct monitor *monitor = userdata;
  struct c_output *output = monitor->output;

  struct c_wl_output *_output = c_malloc(sizeof(*_output));
  _output->obj = wl_output;
  _output->output = output;

  wl_output->data = _output;

  if (wl_output->version >= C_WL_OUTPUT_NAME_SINCE)
    wl_output_name(conn, wl_output->id, output->name);

  char model[5];
  snprintf(model, sizeof(model), "%04X", output->model);

  if (wl_output->version >= C_WL_OUTPUT_SCALE_SINCE)
    wl_output_scale(conn, wl_output->id, monitor->cfg ? (c_wl_int)monitor->cfg->scale : 1);

  wl_output_geometry(conn, wl_output->id, 0, 0, output->mm_width,
                     output->mm_height, output->subpixel - 1, output->make,
                     model, WL_OUTPUT_TRANSFORM_NORMAL);

  if (wl_output->version >= C_WL_OUTPUT_DESCRIPTION_SINCE) {
    char desc[64];
    snprintf(desc, sizeof(desc), "%s %s %04X %d", output->manufacturer_name,
             output->make, output->model, output->serial);
    wl_output_description(conn, wl_output->id, desc);
  }

  struct c_output_mode *mode;
  c_list_for_each(output->modes, mode) {
    int flags = 0;
    if (mode->preferred)
      flags |= WL_OUTPUT_MODE_PREFERRED;

    if (mode == output->current_mode)
      flags |= WL_OUTPUT_MODE_CURRENT;

    wl_output_mode(conn, wl_output->id, flags, mode->width, mode->height, mode->refresh_rate * 1000);

  }

  if (wl_output->version >= C_WL_OUTPUT_DONE_SINCE)
    wl_output_done(conn, wl_output->id);

  return _output;
}

int on_get_keyboard(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  c_wl_new_id wl_keyboard_id = args[1].n;

  struct c_input *input = userdata;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_KEYBOARD))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  int keymap_fd;
  int keymap_len = c_input_get_xkb_keymap_fd(input, &keymap_fd);
  if (keymap_len < 0)
    c_wl_error_set_and_return(args[0].o, WL_DISPLAY_ERROR_IMPLEMENTATION, "failed ot get xkb_keymap");

  wl_keyboard_keymap(conn, wl_keyboard_id, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_len);
  if (self->version >= C_WL_KEYBOARD_REPEAT_INFO_SINCE)
    wl_keyboard_repeat_info(conn, wl_keyboard_id, keyboard_repeat_rate, keyboard_repeat_delay);

  return 0;
}

int on_get_pointer(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_input *input = userdata;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  return 0;
}

void assign_output_to_surface(struct c_wl_object *wl_surface) {
  struct c_wl_surface *surface = wl_surface->data;

  if (surface->output) return;

  struct c_wl_object *o;
  struct c_wl_output *output;

  c_wl_objects_for_each(wl_surface->conn, o) {
    if (STREQ(o->iface->name, "wl_output") && (output = o->data)->output == cuts.focused_mon->output) {
      surface->output = output;
      c_ref(output);
      return;
    }
  }
}

int on_surface_commit(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_surface = c_wl_self(conn, args);
  struct c_wl_surface *surface = wl_surface->data;

  struct client *client;
  if ((client = client_from_connection(conn)))
    c_scene_node_update(client->window->node);

  assign_output_to_surface(wl_surface);

  if (surface->output) {
    struct c_output *output = surface->output->output;
    if (surface->frames_n > 0 && c_list_idx(output->active_surfaces, wl_surface) < 0)
      c_list_push(output->active_surfaces, wl_surface, 0);
    output_damage(surface->output->output);
  }

  return 0;
}

int on_surface_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_surface = c_wl_self(conn, args);
  struct c_wl_surface *surface = wl_surface->data;

  struct client *client = NULL;
  if (!cuts.is_quitting && (client = client_from_connection(conn)))
      c_scene_node_update(client->window->node);

  if (client && surface == client->window->focused)
      client->window->focused = NULL;

  if (!surface->output) return 0;

  struct c_output *output = surface->output->output;
  c_list_remove(&output->active_surfaces, wl_surface);
  output_damage(output);
  c_unref(surface->output);
  return 0;
}

int on_window_new(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;

  struct client *client = client_new(conn);
  if (!client) {
    quit(NULL);
    return -1;
  }
  
  client->window = c_window_new(cuts.scene, conn, surface);
  c_scene_node_set_collect(client->window->node, scene_collect_window);

  client->window->xdg_states = (1 << XDG_TOPLEVEL_STATE_TILED_TOP)       |
                               (1 << XDG_TOPLEVEL_STATE_TILED_RIGHT)     |
                               (1 << XDG_TOPLEVEL_STATE_TILED_BOTTOM)    |
                               (1 << XDG_TOPLEVEL_STATE_TILED_LEFT)      ;

  c_list_insert(&cuts.clients, 0, client, 0);

  c_wl_enum wm_caps[1] = {XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN};
  c_wl_array arr = {
    .size = sizeof(wm_caps),
    .data = wm_caps,
  };

  xdg_toplevel_wm_capabilities(conn, surface->toplevel.obj->id, &arr);
  LAYOUT(client->mon);

  client_change_focus(client, pointer_x, pointer_y);
  return 0;
}

int on_set_title(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_from_connection(conn);
  assert(client);

  if (client == cuts.focused_client) bar_set_title(surface->toplevel.title);

  return 0;
}

int on_window_unfullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct client *client = client_from_connection(conn);
  client_unset_fullscreen(client);
  LAYOUT(client->mon);
  return 0;
}

int on_window_unset_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct client *client = client_from_connection(conn);
  client_unset_fullscreen(client);
  return 0;
}

int on_window_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct client *client = client_from_connection(conn);

  struct c_wl_object *wl_output = c_wl_object_get(conn, args[1].o);

  struct monitor *mon;
  if (wl_output) {
    struct c_output *output = ((struct c_wl_output *)wl_output->data)->output;
    c_list_for_each(cuts.monitors, mon)
      if (mon->output == output) break;
  } else {
    mon = client->mon;
  }

  client_set_fullscreen(client, mon);
  return 0;
}

int on_window_close(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  if (cuts.is_quitting) return 0;

  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;

  struct client *client;
  c_list_for_each(cuts.clients, client) {
    struct c_window *window = client->window;
    if (window->surface == surface) {
      int is_visible = client->tag & cuts.focused_tag;
      struct monitor *mon = client->mon;

      client_close(client);
      if (is_visible) {
        LAYOUT(mon)
      }
      break;
    }
  }

  return 0;
}

void on_connection_gone(struct c_wl_connection *conn, void *userdata) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    struct c_window *window = client->window;
    if (window->conn == conn) {
      int is_visible = client->tag & cuts.focused_tag;
      struct monitor *mon = client->mon;

      client_close(client);
      if (is_visible) {
        LAYOUT(mon)
      }
      break;
    }
  }
}

int on_data_source_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_data_source *data_source = c_wl_self(conn, args)->data;

  if (cuts.cb.src == conn && cuts.cb.data_source == data_source)
    cuts.cb.data_source = NULL;
  else if (cuts.dnd.src == conn && cuts.dnd.data_source == data_source)
    cuts.dnd.data_source = NULL;

  return 0;
}

int on_data_offer_receive(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_data_source *data_source;
  struct c_wl_connection  *src;

  if (cuts.cb.dst == conn) {
    data_source = cuts.cb.data_source;
    src = cuts.cb.src;
  } else {
    data_source = cuts.dnd.data_source;
    src = cuts.dnd.src;
  }

  if (!data_source) return 0;

  c_wl_string mimetype = args[1].s;
  c_wl_fd wfd = args[2].F;

  for (size_t i = 0; i < data_source->mimes; i++) {
    if (STREQ(data_source->mimetypes[i], mimetype)) {
      wl_data_source_send(src, data_source->obj->id, mimetype, wfd);
      c_wl_connection_flush(src);
      return 0;
    }
  }

  return 0;
}

int on_data_offer_set_actions(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *data_offer = self->data;
  struct c_wl_data_source *source = cuts.dnd.data_source;

  enum wl_data_device_manager_dnd_action_enum available = data_offer->actions & source->actions;
  enum wl_data_device_manager_dnd_action_enum selected = available & data_offer->preferred;

  if (selected != cuts.dnd.negotiated_actions) {
    wl_data_source_action(cuts.dnd.src, source->obj->id, selected);
    wl_data_offer_action(conn, self->id, selected);
    cuts.dnd.negotiated_actions = selected;
  }

  return 0;
}

int on_data_offer_accept(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *offer = self->data;

  wl_data_source_target(cuts.dnd.src, cuts.dnd.data_source->obj->id, offer->mimetype);
  return 0;
}

int on_decorations_set_mode(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_xdg_surface *surface = self->data;
  zxdg_toplevel_decoration_v1_configure(conn, self->id, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  xdg_surface_configure(conn, surface->obj->id, c_wl_serial());
  return 0;
}

int on_start_drag(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_data_device = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = wl_data_device->data;
  struct c_wl_data_source *data_source = data_device->dnd.source;

  cuts.dnd.src = conn;
  cuts.dnd.data_source = data_source;
  cuts.dnd.data_device = data_device;
  
  c_log_value(data_source, "%p");
  c_log_value(data_device, "%p");

  struct c_wl_data_offer *data_offer = c_malloc(sizeof(*data_offer));
  data_offer->obj =
      c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, wl_data_device->version,
                      c_wl_interface_get("wl_data_offer"), data_offer);

  data_device->offer = data_offer;
  c_ref(data_offer);
  data_offer->device = data_device;
  c_ref(data_device);

  wl_data_device_data_offer(conn, wl_data_device->id, data_offer->obj->id);
  for (size_t i = 0; i < data_source->mimes; i++)
    wl_data_offer_offer(conn, data_offer->obj->id, data_source->mimetypes[i]);

  wl_data_offer_source_actions(conn, data_offer->obj->id, data_source->actions);

  double lx, ly;
  struct c_wl_surface *target = c_window_surface_at(cuts.focused_client->window, pointer_x, pointer_y, &lx, &ly);
  wl_data_device_enter(conn, wl_data_device->id, c_wl_serial(), target->obj->id,
                       C_WL_FIXED_FROM_DOUBLE(lx), C_WL_FIXED_FROM_DOUBLE(ly),
                       data_offer->obj->id);
  cuts.dnd.dst = conn;
  return 0;
}

int on_set_selection(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_data_device = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = wl_data_device->data;
  struct c_wl_data_source *data_source = data_device->source;

  if (cuts.cb.data_source == data_source) return 0;

  if (cuts.cb.src)
    wl_data_source_cancelled(cuts.cb.src, cuts.cb.data_source->obj->id);

  cuts.cb.src = conn;
  cuts.cb.data_source = data_source;
  cuts.cb.data_device = data_device;
  
  struct c_wl_data_offer *data_offer = c_malloc(sizeof(*data_offer));
  data_offer->obj =
      c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, wl_data_device->version,
                      c_wl_interface_get("wl_data_offer"), data_offer);

  data_device->offer = data_offer;
  c_ref(data_offer);
  data_offer->device = data_device;
  c_ref(data_device);

  wl_data_device_data_offer(conn, wl_data_device->id, data_offer->obj->id);
  for (size_t i = 0; i < data_source->mimes; i++)
    wl_data_offer_offer(conn, data_offer->obj->id, data_source->mimetypes[i]);
  wl_data_device_selection(conn, wl_data_device->id, data_offer->obj->id);

  cuts.cb.dst = conn;
  return 0;
}

int on_set_shape(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct client *client = cuts.focused_client;
  if (!client) return 0;

  enum c_cursor_shape shape = args[2].u;
  if (conn == client->window->conn) {
    if (shape > C_CURSOR_ALL_RESIZE) {
      c_wl_error_set_and_return(args[0].o, WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE, "invalid shape");
    }

    c_cursor_set_shape(cuts.pointer.cur, shape);
  }
  return 0;
}

void quit(bind_args *args) {
  c_event_loop_stop(cuts.loop);
}

void spawn(bind_args *args) {
  if (fork() == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", (char *const)args->s, NULL});
  }
}

void window_kill(bind_args *args) {
  if (!cuts.focused_client) return;
  c_window_close(cuts.focused_client->window);
}

void set_layout(bind_args *args) {
  cuts.layout = *(struct layout *)args->p;

  bar_set_layout(&cuts.layout);

  struct monitor *mon;
  c_list_for_each(cuts.monitors, mon)
    LAYOUT(mon);
}

void toggle_fullscreen(bind_args *args) {
  struct client *client = cuts.focused_client;
  if (!client) return;

  if (is_window_fullscreen(client->window))
    client_unset_fullscreen(client);
  else
    client_set_fullscreen(client, client->mon);
}

void move_focus(bind_args *args) {
  if (cuts.fullscreen & cuts.focused_tag) return;

  struct client *next = tag_select_client(args->i);
  if (next) {
    client_change_focus(next, pointer_x, pointer_y);
    if (cuts.layout.func == zoom) {
      client_raise(next);
    }
  }
}

void switch_tag(bind_args *args) {
  if (cuts.focused_tag == args->u) return;

  bar_switch_tag(cuts.focused_tag, args->u);

  cuts.focused_tag = args->u;

  struct client *c;
  c_list_for_each(cuts.clients, c)
    client_set_visibility(c, c->tag & cuts.focused_tag);

  LAYOUT(cuts.focused_mon);

  struct client *prev = tag_select_client(0);

  if (prev) {
    client_change_focus(prev, pointer_x, pointer_y);
    return;

  } 
  
  if (cuts.focused_client)
    client_unfocus(cuts.focused_client);
   
  bar_clear_title();
}

void toggle_floating(bind_args *args) {
  if (!cuts.focused_client || (cuts.fullscreen & cuts.focused_tag)) return;

  client_raise(cuts.focused_client);
  client_toggle_floating(cuts.focused_client);

  LAYOUT(cuts.focused_client->mon);
}

void change_mfact(bind_args *args) {
  mfact += args->d;
  mfact = CLAMP(mfact, 0.05f, 0.95f);
  
  struct monitor *mon;
  c_list_for_each(cuts.monitors, mon)
    LAYOUT(mon);
}

void change_nmaster(bind_args *args) {
  nmaster += args->i;
  nmaster = CLAMP(nmaster, 1, 10);
  
  struct monitor *mon;
  c_list_for_each(cuts.monitors, mon)
    LAYOUT(mon);
}

void window_move(int done, bind_args *args) {
  if (!cuts.focused_client || (cuts.fullscreen & cuts.focused_tag)) return;

  struct client *focused = cuts.focused_client;
  struct c_window *window = focused->window;

  client_raise(focused);
  if (!(window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);

  if (done) {
    pointer_x_prev = pointer_x;
    pointer_y_prev = pointer_y;
  }
    
  double dist_x = pointer_x - pointer_x_prev;
  double dist_y = pointer_y - pointer_y_prev;

  window->x+=dist_x;
  window->y+=dist_y;

  if (!cuts.pointer.is_dragging && !done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_GRAB);
  } else if (done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_DEFAULT);
  }

  cuts.pointer.is_dragging = !done;

  LAYOUT(focused->mon);
}

void window_move_to_workspace(bind_args *args) {
  if (!cuts.focused_client || cuts.focused_tag == args->u) return;

  cuts.focused_client->tag = args->u;
  client_set_visibility(cuts.focused_client, 0);
  client_raise(cuts.focused_client);

  struct client *next;
  if ((next = tag_select_client(1)))
    client_change_focus(next, pointer_x, pointer_y);


  LAYOUT(cuts.focused_client->mon);
}

void window_resize(int done, bind_args *args) {
  if (!cuts.focused_client || (cuts.fullscreen & cuts.focused_tag)) return;

  struct client *focused = cuts.focused_client;
  struct c_window *window = focused->window;

  client_raise(focused);
  if (!(window->state & C_WINDOW_FLOAT))
    client_toggle_floating(focused);

  if (done) {
    pointer_x_prev = pointer_x;
    pointer_y_prev = pointer_y;
  }
    
  float dist_x = pointer_x - pointer_x_prev;
  float dist_y = pointer_y - pointer_y_prev;

  window->width = CLAMP(window->width + dist_x, MIN_WINDOW_SIZE, UINT32_MAX);
  window->height = CLAMP(window->height + dist_y, MIN_WINDOW_SIZE, UINT32_MAX);

  c_window_activate(window);

  if (!cuts.pointer.is_dragging && !done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_NWSE_RESIZE);
  } else if (done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_DEFAULT);
  }

  cuts.pointer.is_dragging = !done;
  LAYOUT(focused->mon);
}

void change_border(bind_args *args) {
  border_width = CLAMP(border_width + args->i, 0, 50);

  struct monitor *mon;
  c_list_for_each(cuts.monitors, mon)
    LAYOUT(mon);
}

void change_gap(bind_args *args) {
  gap = CLAMP(gap + args->i, 0, 150);

  struct monitor *mon;
  c_list_for_each(cuts.monitors, mon)
    LAYOUT(mon);
}

void zoom() {
  struct tile_layout layout;
  calc_tile_layout(cuts.focused_mon, &layout);

  if (cuts.clients->size == 0) return;

  struct client *client;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    if (is_window_floating(window)) goto floating;
    if (is_window_fullscreen(window)) goto fullscreen;

    window->width = layout.width - border_width * 2;
    window->height = layout.height - border_width * 2;
    window->x = layout.x + border_width;
    window->y = layout.y + border_width;

floating:
    client_border_sync(client);

fullscreen:
    if (cuts.focused_client == client) {
      c_window_activate(window);
    } else {
      c_window_deactivate(window);
    }
     
    c_scene_node_update(client->window->node);

  }

}

void tile() {
  // FIXME: map tags to outputs and get output from current tag or something
  struct tile_layout layout;
  calc_tile_layout(cuts.focused_mon, &layout);

  if (cuts.clients->size == 0) return;

  uint32_t tiled_clients = count_tiled();
  uint32_t stack_clients, master_clients, master_client_height, stack_client_height;

  if (tiled_clients) {
    stack_clients = (tiled_clients - nmaster) & -(tiled_clients >= nmaster);
    master_clients = tiled_clients - stack_clients;

    master_client_height = (layout.height - gap * (master_clients -1)) / master_clients;
    stack_client_height = stack_clients ? (layout.height - gap * (stack_clients - 1)) / stack_clients : 0;

  } else {
    stack_clients = master_clients = master_client_height = stack_client_height = 0;
  }

  struct client *client;
  size_t i = 0;
  clients_for_each_in_tag(client) {
    struct c_window *window = client->window;
    if (is_window_fullscreen(window)) goto fullscreen;
    if (is_window_floating(window)) goto floating;

    if (i < nmaster) {
      window->x = layout.master.x;
      window->width = stack_clients > 0 ? layout.master.width : layout.width;

      window->height = master_client_height;
      window->y = layout.y + master_client_height * i + gap * i;
        
    } else {
      window->x = layout.stack.x;
      window->width = layout.stack.width;

      window->height = stack_client_height;
      window->y = layout.y + stack_client_height * (i - master_clients) + gap * (i - master_clients);
    }

    window->x += border_width;
    window->y += border_width;
    window->width -= border_width * 2;
    window->height -= border_width * 2;
    i++;

floating:
    client_border_sync(client);

fullscreen:
    if (cuts.focused_client == client)
      c_window_activate(window);
    else
      c_window_deactivate(window);

    c_scene_node_update(client->window->node);

  }
}

void bar_destroy() {
  struct bar *bar = &cuts.bar;
  for (size_t i = 0; i < bar->block_n; i++) {
    struct bar_block block = bar->blocks[i];
    free(block.text.buffer);
  }

  if (bar->ft.face) FT_Done_Face(bar->ft.face);
  if (bar->ft.library) FT_Done_FreeType(bar->ft.library);
}

void bar_set_title(const char *title) {
  struct bar_block *block;
  struct bar *bar = &cuts.bar;

  block = &cuts.bar.blocks[tags + 1];
   
  bar_block_write_text(&cuts.bar, block, title, bar->cfg->title.font_color);
  bar_block_update(block);
}

void bar_clear_title() {
  struct bar_block *block;
  block = &cuts.bar.blocks[tags + 1];
  bar_block_clear_text(&cuts.bar, block);
  bar_block_update(block);
}

void bar_set_layout(struct layout *layout) {
  struct bar_block *block = &cuts.bar.blocks[tags];
  struct bar *bar = &cuts.bar;

  bar_block_write_text(&cuts.bar, block, layout->repr, bar->cfg->layout.font_color);
  bar_block_update(block);
}

void bar_switch_tag(uint32_t current, uint32_t next) {
  struct bar *bar = &cuts.bar;

  char label[16];
  struct bar_block *block;
  int tag_i;

  bit_index(current, tag_i);
  block = &cuts.bar.blocks[tag_i];

  set_color(block->rect.color, bar->cfg->tag.background_inactive);
  snprintf(label, sizeof(label), "%s", tag_lables[tag_i]);

  bar_block_write_text(&cuts.bar, block, label, bar->cfg->tag.font_inactive);
  bar_block_update(block);

  bit_index(next, tag_i);
  block = &cuts.bar.blocks[tag_i];

  set_color(block->rect.color, bar->cfg->tag.background_active);
  snprintf(label, sizeof(label), "%s", tag_lables[tag_i]);

  bar_block_write_text(&cuts.bar, block, label, bar->cfg->tag.font_active);
  bar_block_update(block);
}

void bar_block_init(struct bar *bar, struct bar_block *block) {
  struct c_scene_rect *rect = &block->rect;
  block->rect_node = c_scene_add_rect(cuts.scene, &block->rect);

  block->text.width = rect->width;
  block->text.height = rect->height;

  uint32_t buffer_size = block->text.width * 4 * block->text.height;
  uint8_t *buffer = calloc(buffer_size, 1);

  block->text.buffer = buffer;

  block->text.x = block->rect.x + bar->h_padding;
  block->text.y = block->rect.y + bar->v_padding;

  block->text_node = c_scene_add_buffer(cuts.scene, &block->text);
  
}

void bar_block_update(struct bar_block *block) {
  c_scene_node_update(block->rect_node);
  c_scene_node_update(block->text_node);
}

void bar_block_clear_text(struct bar *bar, struct bar_block *block) {
  uint8_t *buffer = block->text.buffer;
  uint32_t buffer_size = block->text.width * 4 * block->text.height;
  memset(buffer, 0, buffer_size);
}

void bar_block_write_text(struct bar *bar, struct bar_block *block, const char *text, const uint32_t color[4]) {
  FT_Face face = bar->ft.face;
  FT_GlyphSlot slot = bar->ft.face->glyph;

  bar_block_clear_text(bar, block);

  uint8_t *buffer = block->text.buffer;
  uint32_t width = block->text.width;

  int ascent = bar->ft.face->size->metrics.ascender >> 6;
  uint32_t baseline = ascent;

  uint32_t pos_x = 0;
  uint32_t pos_y = 0;

  FT_Int32 load_flags;
  if (is_bar_horizontal(bar))
    load_flags = FT_LOAD_DEFAULT;
  else
    load_flags = FT_LOAD_VERTICAL_LAYOUT;

  size_t n = 0, len = strlen(text);

  while (n < len) {
    uint32_t charcode = utf8_char(text, &n);
    FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);

    FT_Load_Glyph(face, glyph_index, load_flags);
    FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

    for (size_t g_y = 0; g_y < slot->bitmap.rows; g_y++) {
      for (size_t g_x = 0; g_x < slot->bitmap.width; g_x++) {
        int buffer_i;

        if (is_bar_horizontal(bar))
          buffer_i = (baseline - slot->bitmap_top + g_y) * width + pos_x + g_x + slot->bitmap_left;

        else
          buffer_i = (pos_y + baseline - slot->bitmap_top + g_y) * width + g_x + slot->bitmap_left;

        uint8_t coverage = slot->bitmap.buffer[g_y * slot->bitmap.width + g_x];
        uint8_t r = (color[0] * coverage) / 255;
        uint8_t g = (color[1] * coverage) / 255;
        uint8_t b = (color[2] * coverage) / 255;
        uint32_t color_u32 = coverage << 24 | b << 16 | g << 8 | r;
        *((uint32_t *)buffer + buffer_i) = color_u32;
      }
    }

    pos_x += (slot->advance.x >> 6) + bar->glyph.spacing;
    pos_y += (slot->advance.y >> 6) + bar->glyph.spacing;
  }

}

void bar_create(struct c_output_mode *mode) {
  struct bar *bar = &cuts.bar;
  FT_Error error = 0;

  bar->cfg = &bar_cfg;
  bar->block_n = tags + 3; // tags + layout indicator + title + stdin

  if ((error = FT_Init_FreeType(&bar->ft.library))) {
    c_log(C_LOG_ERROR, "failed to initialize freetype: %s", FT_Error_String(error));
    quit(NULL);
  }

  char font[512];
  const char *default_font = "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf";

  int font_status = 0;
  if ((font_status = get_fontpath(bar->cfg->font.name, font, 512)) != 0)
    c_log(C_LOG_WARNING, "couldn't find '%s' font. falling back to '%s'", bar->cfg->font.name, default_font);

  if ((error = FT_New_Face(bar->ft.library, font_status == 0 ? font : default_font, 0, &bar->ft.face))) {
    c_log(C_LOG_ERROR, "failed to create new face: %s", FT_Error_String(error));
    goto ft_error;
  }

  if ((error = FT_Set_Pixel_Sizes(bar->ft.face, 0, bar->cfg->font.size))) {
    c_log(C_LOG_ERROR, "failed to set pixel sizes: %s" , FT_Error_String(error));
    goto ft_error;
  }

  FT_UInt glyph_index = FT_Get_Char_Index(bar->ft.face, 'a');
  FT_Load_Glyph(bar->ft.face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(bar->ft.face->glyph, FT_RENDER_MODE_NORMAL);


  bar->glyph.spacing = 1;

  int ascent  = bar->ft.face->size->metrics.ascender  >> 6;
  int descent = -(bar->ft.face->size->metrics.descender >> 6);
  bar->glyph.height = ascent + descent;
  bar->glyph.width = (bar->ft.face->glyph->advance.x) >> 6;

  uint32_t pad       = bar->glyph.height / 4;
  uint32_t thickness = bar->glyph.height + 2 * pad;

  if (is_bar_horizontal(bar)) {
   bar->width  = mode->width;
   bar->height = thickness;
  } else {
   bar->height = mode->height;
   bar->width  = thickness;
  }

  bar->h_padding = (MIN(bar->width, bar->height) - bar->glyph.width) / 2;
  bar->v_padding = (MIN(bar->width, bar->height) - bar->glyph.height) / 2;

  uint32_t pen = 0;

  for (size_t i = 0; i < bar->block_n; i++) {
    struct bar_block *block = &bar->blocks[i];
    struct c_scene_rect *rect = &block->rect;

    if (is_bar_vertical(bar)) {
      rect->x = bar->cfg->pos == BAR_RIGHT ? mode->width - bar->width : 0;
      rect->y = pen;
    } else if (is_bar_horizontal(bar)) {
      rect->x = pen;
      rect->y = bar->cfg->pos == BAR_TOP ? 0 : mode->height - bar->height;
    }

    rect->height = bar->height;
    rect->width = bar->width;

    if (0 <= i && i < tags) { // tag blocks
      char label[16];
      snprintf(label, sizeof(label), "%s", tag_lables[i]);

      if (is_bar_horizontal(bar))
        rect->width = text_rect_width(utf8_len(label), bar);
      else
        rect->height = text_rect_width(utf8_len(label), bar);;

      set_color(rect->color, (!i ? bar->cfg->tag.background_active : bar->cfg->tag.background_inactive)); 
      bar_block_init(bar, block);

      bar_block_write_text(bar, block, label, (!i ? bar->cfg->tag.font_active : bar->cfg->tag.font_inactive));

    } else if (i == tags) { // layout indicator
      if (is_bar_horizontal(bar))
        rect->width = text_rect_width(strlen(cuts.layout.repr), bar);
      else
        rect->height = text_rect_height(strlen(cuts.layout.repr), bar);

      set_color(rect->color, bar->cfg->layout.background_color);
      bar_block_init(bar, block);
      bar_block_write_text(bar, block, cuts.layout.repr, bar->cfg->layout.font_color);

    } else if (i == tags + 1) { // title
      set_color(rect->color, bar->cfg->title.background_color);
      if (is_bar_horizontal(bar))
        rect->width = mode->width - pen;
      else
        rect->height = mode->height - pen;

      bar_block_init(bar, block);
                                  
    } else if (i == tags + 2) { // stdin
      const char *label = "cuts o_0";

      if (is_bar_horizontal(bar)) {
        rect->width = text_rect_width(MAX_STDIN_CHARS, bar);
        rect->x = pen - text_rect_width(strlen(label), bar);
      } else {
        rect->height = text_rect_height(MAX_STDIN_CHARS, bar);
        rect->y = pen - text_rect_height(strlen(label), bar);
      }

      set_color(rect->color, bar->cfg->text.background_color);
      bar_block_init(bar, block);
      bar_block_write_text(bar, block, label, bar->cfg->text.font_color);
    }

    pen += is_bar_horizontal(bar) ? rect->width : rect->height;
  } 

  return;

ft_error:
  bar_destroy();
  quit(NULL);
}

C_EVENT_CALLBACK stdin_text(struct c_event_loop *loop, int fd, void *userdata) {
  char buffer[MAX_STDIN_CHARS] = {0};

  int read_b;
  if (!(read_b = read(STDIN_FILENO, buffer, sizeof(buffer) - 1))) {
    c_log(C_LOG_WARNING, "failed to read stdin");
    goto out;
  } 

  if (buffer[read_b-1] == '\n')
    buffer[read_b-1] = 0;

  buffer[read_b] = 0;

  struct bar *bar = &cuts.bar;
  struct bar_block *block = &cuts.bar.blocks[tags + 2];

  struct c_output *output = cuts.focused_mon->output;
  struct c_output_mode *mode = output->current_mode;
  
  if (is_bar_horizontal(bar)) {
    uint32_t text_width = text_rect_width(utf8_len(buffer), bar);
    block->rect.x = mode->width - text_width;
    block->text.x = block->rect.x + bar->h_padding;

  } else {
    uint32_t text_height = text_rect_height(utf8_len(buffer), bar);
    
    block->rect.y = mode->height - text_height;
    block->text.y = block->rect.y + bar->v_padding;

  }

  bar_block_write_text(bar, block, buffer, bar->cfg->text.font_color);
  bar_block_update(block);
  c_output_damage(cuts.mgr, output);

out:
  return C_EVENT_OK;
}

void signal_handler(int signal, void *userdata) {
  c_event_loop_stop(cuts.loop);
}

void cleanup(int err) {
  cuts.is_quitting = 1;

  if (cuts.pointer.cur)
    c_cursor_free(cuts.pointer.cur);

  if (cuts.clients) {
    struct client *client;
    c_list_for_each(cuts.clients, client)
      client_free(client);
    c_list_destroy(cuts.clients);
    cuts.clients = NULL;
  }

  if (cuts.monitors)
    c_list_destroy(cuts.monitors);

  if (cuts.display) {
    c_wl_display_free(cuts.display);
    cuts.display = NULL;
  }

  bar_destroy();

  if (cuts.scene) {
    c_scene_free(cuts.scene);
    cuts.scene = NULL;
  }

  c_output_manager_free(cuts.mgr);
  cuts.mgr = NULL;

  if (cuts.session) {
    c_session_free(cuts.session);
    cuts.session = NULL;
  }

  if (cuts.loop) {
    c_event_loop_free(cuts.loop);
    cuts.loop = NULL;
  }

  exit(err);
}

int main() {
  int ret = 0;
  cuts.focused_tag = 1 << 0;
  cuts.dnd.negotiated_actions = 0xff;

  c_signal_handler_add(SIGTERM, signal_handler, NULL);
  c_signal_handler_add(SIGINT, signal_handler, NULL);
  c_signal_handler_add(SIGABRT, signal_handler, NULL);

  struct c_log_config cfg;
  cfg.level_mask = C_LOG_INFO | C_LOG_DEBUG | C_LOG_ERROR | C_LOG_WARNING;
  cfg.level_mask |= C_LOG_WAYLAND;
  cfg.color = 1;
  c_log_setup(&cfg);

  struct c_event_loop *loop = c_event_loop_init();
  check_init(loop, ret, out);
  cuts.loop = loop;

  struct c_wl_display *display = c_wl_display_init(loop);
  check_init(display, ret, out);
  cuts.display = display;

  struct c_input_config input_config = {
    .accel_profile = accel_profile,
  };
  struct c_session *session = c_session_init(loop, display, &input_config);
  check_init(session, ret, out);
  cuts.session = session;

  struct c_output_manager *mgr = c_output_manager_init(session, loop, display);
  check_init(mgr, ret, out);
  cuts.mgr = mgr;

  struct c_scene *scene = c_scene_init(mgr);
  check_init(scene, ret, out);
  cuts.scene = scene;

  cuts.clients = c_list_new();
  cuts.monitors = c_list_new();
  cuts.layout = layouts[0];

  struct c_cursor *cur = c_cursor_init(mgr, loop);
  check_init(cur, ret, out);
  cuts.pointer.cur = cur;

  if (c_cursor_load(cur, cursor_theme, cursor_size)) {
    c_log(C_LOG_ERROR, "failed to load cursor");
    ret = 1;
    goto out;
  }

  if (c_cursor_set_shape(cur, C_CURSOR_DEFAULT)) {
    c_log(C_LOG_ERROR, "failed to set shape");
  }

  struct c_output *output;
  c_list_for_each(mgr->outputs, output) {
    c_log(C_LOG_INFO, "Monitor %s:", output->name);

    struct monitor mon = {output, NULL};
    struct monitor *mon_cpy;

    struct c_output_mode *preferred = NULL;
    struct c_output_mode *mode;

    c_list_for_each(output->modes, mode) {
      if (mode->preferred)
        preferred = mode;

      c_log(C_LOG_INFO, "   %s%dx%d@%.3fHz", mode->preferred ? "*" : " ",
            mode->width, mode->height, mode->refresh_rate);

      for (size_t i = 0; i < LENGTH(monitors); i++) {
        struct monitor_config *cfg = &monitors[i];

        if (STREQ(cfg->name, output->name) && cfg->width == mode->width &&
            cfg->height == mode->height &&
            cfg->refresh_rate == (uint32_t)mode->refresh_rate) {

          set_background(mode->width, mode->height, output->x, output->y);
          bar_create(mode);

          c_output_set_mode(mgr, output, mode);
          mon.cfg = cfg;
          goto mode_iter_end;
        }
      }
    }

    assert(preferred);
    set_background(preferred->width, preferred->height, output->x, output->y);
    bar_create(preferred);

    c_output_set_mode(mgr, output, preferred);

mode_iter_end:
    mon_cpy = c_list_push(cuts.monitors, &mon, sizeof(mon));

    c_wl_interface_support("wl_output", on_wl_output_bind, mon_cpy);

    if (!cuts.focused_mon) cuts.focused_mon = mon_cpy;
  }

  if (c_input_init_xkb_state(session->input, &xkb_rules) < 0) goto out;

  struct c_input_event_listener_mouse mouse_listener = {
    .on_mouse_movement = on_mouse_movement,
    .on_mouse_button = on_mouse_button,
    .on_mouse_scroll = on_mouse_scroll,
  };
  c_input_add_event_listener_mouse(session->input, &mouse_listener, NULL);

  struct c_input_event_listener_keyboard listener_keyboard = {
    .on_keyboard_key = on_keyboard_key,
  };
  c_input_add_event_listener_keyboard(session->input, &listener_keyboard, NULL);

  for (size_t i = 0; i < LENGTH(keys); i++) {
    struct key_bind *b = &keys[i];
    c_input_add_combo_handler(session->input, b->modmask, b->keysym, 0, (void (*)(void *))b->handler, &b->args);
  }

  for (size_t i = 0; i < LENGTH(mouse); i++) {
    struct mouse_bind *b = &mouse[i];
    if (b->drag)
      c_input_add_drag_combo_handler(
          session->input, b->modmask, b->keysym, b->btn,
          (void (*)(int, void *))b->drag_handler, &b->args);

    else
      c_input_add_combo_handler(session->input, b->modmask, b->keysym, b->btn,
                                (void (*)(void *))b->handler, &b->args);
  }

  struct c_wl_display_connection_listener conn_listener = {
    .gone = on_connection_gone,
  };
  c_wl_display_add_connection_listener(display, &conn_listener, NULL);

  struct c_wl_seat_listeners seat_listeners = {
    .get_keyboard = on_get_keyboard,
    .get_pointer = on_get_pointer,
  };
  wl_seat_add_listener(display, &seat_listeners, session->input);

  struct c_wl_surface_listeners surface_listeners = {
    .commit = on_surface_commit,
    .destroy = on_surface_destroy,
  };
  wl_surface_add_listener(display, &surface_listeners, NULL);

  struct c_xdg_surface_listeners xdg_surface_listeners = {
    .get_toplevel = on_window_new,
  };
  xdg_surface_add_listener(display, &xdg_surface_listeners, NULL);

  struct c_xdg_toplevel_listeners xdg_toplevel_listeners = {
    .destroy = on_window_close,
    .set_title = on_set_title,
    .set_fullscreen = on_window_fullscreen,
    .unset_fullscreen = on_window_unset_fullscreen,
  };
  xdg_toplevel_add_listener(display, &xdg_toplevel_listeners, NULL);

  struct c_wl_data_device_listeners data_device_listeners = {
    .set_selection = on_set_selection,
    .start_drag = on_start_drag,
  };
  wl_data_device_add_listener(display, &data_device_listeners, NULL);

  struct c_wl_data_source_listeners data_source_listeners = {
    .destroy = on_data_source_destroy,
  };
  wl_data_source_add_listener(display, &data_source_listeners, NULL);

  struct c_wl_data_offer_listeners data_offer_listeners = {
    .receive = on_data_offer_receive,
    .set_actions = on_data_offer_set_actions,
  };
  wl_data_offer_add_listener(display, &data_offer_listeners, NULL);

  struct c_zxdg_toplevel_decoration_v1_listeners decor_listeners = {
    .set_mode = on_decorations_set_mode,
  };
  zxdg_toplevel_decoration_v1_add_listener(display, &decor_listeners, NULL);

  struct c_wp_cursor_shape_device_v1_listeners cursor_shape_listeners = {
    .set_shape = on_set_shape,
  };
  wp_cursor_shape_device_v1_add_listener(display, &cursor_shape_listeners, NULL);

  c_wl_interface_support("zxdg_decoration_manager_v1", NULL, NULL);
  c_wl_interface_support("wl_data_device_manager", NULL, NULL);
  c_wl_interface_support("wp_cursor_shape_manager_v1", NULL, NULL);

  c_event_loop_add(loop, STDIN_FILENO, stdin_text, NULL);

  ret = c_event_loop_run(loop);

out:
  cleanup(ret);
}

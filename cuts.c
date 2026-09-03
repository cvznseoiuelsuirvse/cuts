#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "wayland/proto/wayland.h"
#include "wayland/proto/xdg-shell.h"
#include "wayland/proto/xdg-decoration-unstable-v1.h"
#include "wayland/proto/cursor-shape-v1.h"
#include "wayland/proto/fractional-scale-v1.h"
#include "wayland/proto/ext-data-control-v1.h"
#include "wayland/proto/wlr-layer-shell-unstable-v1.h"

#include "wayland/display.h"
#include "wayland/impl/wayland.h"
#include "wayland/impl/xdg-shell.h"
#include "wayland/impl/ext-data-control-v1.h"
#include "wayland/impl/wlr-layer-shell-unstable-v1.h"

#include "compositor/window.h"
#include "compositor/surface.h"
#include "compositor/scene.h"

#include "output/output.h"
#include "cursor/cursor.h"

#include "seat/session/session.h"

#include "util/event_loop.h"
#include "util/helpers.h"
#include "util/signal.h"
#include "util/log.h"
#include "util/mem.h"

#include "config.h"

#define MIN_WINDOW_SIZE 2
#define MAX_STDIN_CHARS 128

#define LAYOUT(mon)                                                            \
  {                                                                            \
    cuts.layout.func();                                                        \
    output_commit(mon->output);                                                \
  }

#define clients_for_each_in_tag(client) \
  c_list_for_each(cuts.clients, (client)) \
    if ((client->tag & cuts.tag))

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

#define window_area(window) window->x, window->y, window->width, window->height

#define bit_index(n, i)                                                        \
  {                                                                            \
    i = 0;                                                                     \
    uint32_t _n = n;                                                           \
    while ((_n >>= 1))                                                         \
      i++;                                                                     \
  }

#define is_bar_horizontal(bar) ((bar)->cfg->pos == BAR_TOP) || ((bar)->cfg->pos == BAR_BOTTOM)
#define is_bar_vertical(bar) ((bar)->cfg->pos == BAR_RIGHT) || ((bar)->cfg->pos == BAR_LEFT)
#define is_client_fullscreen(client) ((client)->state & CLIENT_FULLSCREEN)
#define is_client_floating(client) ((client)->state & CLIENT_FLOAT)
#define is_client(client, _type) ((client)->node && (client)->node->type == _type)

#define WL_LISTENER(func)     int func(struct c_wl_connection *, c_wl_args, void *)

#define text_rect_width(text_len, bar)                                         \
  ((text_len * (bar->glyph.width + bar->glyph.spacing)) - bar->glyph.spacing + \
   bar->h_padding * 2)

#define text_rect_height(text_len, bar)                                        \
  ((text_len * (bar->glyph.height + bar->glyph.spacing)) -                     \
   bar->glyph.spacing + bar->v_padding * 2)

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
  double scale;
  uint32_t x, y;
};

struct clipboard_selection {
  struct c_wl_object *obj;
  c_wl_string mimetypes[64];
  size_t mimes;
};

enum client_state {
	CLIENT_NORMAL,
	CLIENT_FLOAT,
	CLIENT_FULLSCREEN,
};


struct client {
  uint32_t tag;
  enum client_state state;

  struct c_wl_connection *conn;
  struct monitor *mon;
  struct c_scene_node *node;

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
    struct c_wl_connection *src;

    struct clipboard_selection *selection;
    struct clipboard_selection *primary_selection;

    int (*send)(struct c_wl_connection *conn, c_wl_object_id data_source, c_wl_string mime_type, c_wl_fd fd);
    int (*cancelled)(struct c_wl_connection *conn, c_wl_object_id data_source);
  } cb;

  struct {
    struct c_wl_connection  *src;
    struct c_wl_connection  *dst;
    struct c_wl_data_source *data_source;
    struct c_wl_data_device *data_device;
    struct c_wl_data_offer  *data_offer;
    enum wl_data_device_manager_dnd_action_enum negotiated_actions;

    struct c_scene_surface icon;
    struct c_scene_node   *icon_node;
  } dnd;

  c_list *clients;
  struct client *focused_client;
  int focused_client_idx;

  struct client *kb_focus;

  c_list *monitors;
  struct monitor *focused_mon;

  uint32_t tag;

	struct layout layout;
  struct bar bar;
  struct c_scene_rect background;

  uint32_t fullscreen;
  int    pids[64];

  struct wl_seat                        seat;
  struct wl_surface                     surface;
  struct xdg_surface                    xdg_surface;
  struct xdg_toplevel                   xdg_toplevel;
  struct wl_data_device                 data_device;
  struct wl_data_source                 data_source;
  struct wl_data_offer                  data_offer;
  struct ext_data_control_manager_v1    data_control_manager;
  struct ext_data_control_device_v1     data_control_device;
  struct ext_data_control_offer_v1      data_control_offer;
  struct zxdg_toplevel_decoration_v1    decor;
  struct wp_cursor_shape_device_v1      cursor_shape;
  struct wp_fractional_scale_manager_v1 fraction_scale;
  struct zwlr_layer_shell_v1            layer_shell;
  struct zwlr_layer_surface_v1          layer_surface;
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

const enum c_scene_layers layer_map[4] = {
  C_SCENE_LAYER_BACKGROUND,
  C_SCENE_LAYER_BOTTOM,
  C_SCENE_LAYER_TOP,
  C_SCENE_LAYER_OVERLAY,
};


int get_fontpath(const char *font, char *fontpath, size_t size);
uint32_t utf8_char(const char *s, size_t *i);
size_t utf8_len(const char *s);
void output_commit(struct c_output *output);
void assign_output_to_surface(struct c_wl_object *wl_surface);
struct c_wl_surface *wl_surface_from_node(struct c_scene_node *node);
void set_background(struct monitor *mon, struct c_output_mode *mode);
void cursor_move(struct c_cursor *cur, double x, double y);
int count_tiled();
void calc_tile_layout(struct monitor *mon, struct tile_layout *layout);
void clear_cb(struct c_wl_connection *conn);
void clear_dnd(struct c_wl_connection *conn);
int spawn_proc(const char *s);

struct client *tag_select_client(int direction);
struct client *client_from_connection(struct c_wl_connection *connection);
struct client *client_from_surface(struct c_wl_surface *surface);
struct client *client_new(struct c_wl_connection *connection);
void client_free(struct client *client);
void client_unfocus(struct client *client);
void client_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_change_focus(struct client *client, double hotspot_x, double hotspot_y);
void client_pointer_move(struct client *client, double hotspot_x, double hotspot_y);
void client_button(struct client *client);
void client_key(struct client *client);
void client_close(struct client *client);
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

void *on_wl_output_bind(struct c_wl_connection *conn,
                               struct c_wl_object *wl_output, void *userdata);

WL_LISTENER(on_wl_seat_get_keyboard);
WL_LISTENER(on_wl_seat_get_pointer);
WL_LISTENER(on_wl_surface_commit);
WL_LISTENER(on_wl_surface_destroy);
WL_LISTENER(on_xdg_surface_get_toplevel);
WL_LISTENER(on_xdg_toplevel_set_title);
WL_LISTENER(on_xdg_toplevel_unset_fullscreen);
WL_LISTENER(on_xdg_toplevel_set_fullscreen);
WL_LISTENER(on_xdg_toplevel_destroy);
WL_LISTENER(on_wl_data_source_destroy);
WL_LISTENER(on_clipboard_offer_receive);
WL_LISTENER(on_wl_data_offer_set_actions);
WL_LISTENER(on_wl_data_offer_accept);
WL_LISTENER(on_wl_data_offer_finish);
WL_LISTENER(on_wl_data_offer_destroy);
WL_LISTENER(on_zxdg_toplevel_decoration_v1_set_mode);
WL_LISTENER(on_wl_data_device_start_drag);
WL_LISTENER(on_wl_data_device_set_selection);
WL_LISTENER(on_ext_data_control_device_v1_set_selection);
WL_LISTENER(on_ext_data_control_manager_v1_get_data_device);
WL_LISTENER(on_wp_cursor_shape_device_v1_set_shape);
WL_LISTENER(on_wp_fractional_scale_manager_v1_get_fractional_scale);
WL_LISTENER(on_zwlr_layer_shell_v1_get_layer_surface);
WL_LISTENER(on_zwlr_layer_surface_v1_set_size);
WL_LISTENER(on_zwlr_layer_surface_v1_set_keyboard_interactivity);
WL_LISTENER(on_zwlr_layer_surface_v1_destroy);

void bar_destroy();
void bar_set_title(const char *title);
void bar_clear_title();
void bar_set_layout(struct layout *layout);
void bar_switch_tag(uint32_t current, uint32_t next);
void bar_block_init(struct bar *bar, struct bar_block *block);
void bar_block_update(struct bar_block *block);
void bar_block_clear_text(struct bar *bar, struct bar_block *block);
void bar_block_write_text(struct bar *bar, struct bar_block *block, const char *text, const uint32_t color[4]);
void bar_create(struct c_output_mode *mode, double scale);

C_EVENT_CALLBACK stdin_text(struct c_event_loop *loop, int fd, void *userdata);
void signal_handler(int signal, void *userdata);
void cleanup(int err);

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

void output_commit(struct c_output *output) {
  if (c_output_commit(cuts.mgr, output))
    c_event_loop_stop(cuts.loop);
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

struct c_wl_surface *wl_surface_from_node(struct c_scene_node *node) {
  assert(node->type == C_SCENE_NODE_SURFACE);

  struct c_scene_surface *surf = node->data;
  return surf->surface;
}

void set_background(struct monitor *mon, struct c_output_mode *mode) {
  struct c_scene_rect *background = &cuts.background;

  double x = mon->x;
  double y = mon->y;

  uint32_t width = mode->width;
  uint32_t height = mode->height;

  set_color(background->color, background_color);

  background->x = x;
  background->y = y;

  background->width = width;
  background->height = height;

  background->layer = C_SCENE_LAYER_BACKGROUND;
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
    if (is_client(client, C_SCENE_NODE_WINDOW) && !client->state) i++;
  }
  return i;
}

void calc_tile_layout(struct monitor *mon, struct tile_layout *layout) {
  struct c_output_mode *mode = cuts.focused_mon->output->current_mode;
  struct bar *bar = &cuts.bar;

  uint32_t width = mode->width;
  uint32_t height = mode->height;

  layout->width = width - gap * 2;
  layout->height = height - gap * 2;

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

struct c_wl_data_offer *advertise_drag(struct c_window *window, struct c_wl_object *data_device) {
  struct c_wl_connection *conn = data_device->conn;
  struct c_wl_data_source *data_source = cuts.dnd.data_source;
  struct c_wl_data_offer *data_offer = c_malloc(sizeof(*data_offer));

  data_offer->obj =
      c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, data_device->version,
                      c_wl_interface_get("wl_data_offer"), data_offer);

  wl_data_device_data_offer(conn, data_device->id, data_offer->obj->id);
  for (size_t i = 0; i < cuts.dnd.data_source->mimes; i++)
    wl_data_offer_offer(conn, data_offer->obj->id, data_source->mimetypes[i]);

  double lx, ly;
  struct c_wl_surface *target = c_window_surface_at(window, pointer_x, pointer_y, &lx, &ly);
  wl_data_device_enter(conn, data_device->id, c_wl_serial(), target->obj->id,
                       c_wl_fixed_from_double(lx),
                       c_wl_fixed_from_double(ly), data_offer->obj->id);
  wl_data_offer_source_actions(conn, data_offer->obj->id, data_source->actions);
  return data_offer;
}

void advertise_selection(struct c_wl_object *data_device) {
  struct c_wl_connection *conn = data_device->conn;

  if (!cuts.cb.selection) {
    wl_data_device_selection(conn, data_device->id, 0);
    return;
  }
  struct c_wl_data_offer *data_offer = c_malloc(sizeof(*data_offer));
  data_offer->obj =
      c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, data_device->version,
                      c_wl_interface_get("wl_data_offer"), data_offer);
  wl_data_device_data_offer(conn, data_device->id, data_offer->obj->id);

  for (size_t i = 0; i < cuts.cb.selection->mimes; i++)
    wl_data_offer_offer(conn, data_offer->obj->id, cuts.cb.selection->mimetypes[i]);

  wl_data_device_selection(conn, data_device->id, data_offer->obj->id);
}

void advertive_selection_ext(struct c_wl_object *data_control_device) {
  struct c_wl_connection *conn = data_control_device->conn;

  if (!cuts.cb.selection) {
    ext_data_control_device_v1_selection(conn, data_control_device->id, 0);
    return;
  }

  struct c_wl_object *data_offer =
      c_wl_object_add(conn, C_WL_OBJECT_NEW_SERVER_ID, data_control_device->version,
                      c_wl_interface_get("ext_data_control_offer_v1"), NULL);
  ext_data_control_device_v1_data_offer(conn, data_control_device->id, data_offer->id);

  for (size_t i = 0; i < cuts.cb.selection->mimes; i++)
    ext_data_control_offer_v1_offer(conn, data_offer->id, cuts.cb.selection->mimetypes[i]);

  ext_data_control_device_v1_selection(conn, data_control_device->id, data_offer->id);
}

void broadcast_selection() {
  if (cuts.focused_client) {
    struct client *client = cuts.focused_client;
    struct c_wl_connection *conn = client->conn;
    struct c_wl_object *o;
    c_wl_objects_for_each(conn, o) {
      if (STREQ(o->iface->name, "wl_data_device"))
        advertise_selection(o);
    }
  }

  struct client *client;
  c_list_for_each(cuts.clients, client) {
    struct c_wl_connection *conn = client->conn;
    struct c_wl_object *o;
    c_wl_objects_for_each(conn, o) {
      if (STREQ(o->iface->name, "ext_data_control_device_v1"))
        advertive_selection_ext(o);
    }
  }

}

void clear_cb(struct c_wl_connection *conn) {
  if (conn == cuts.cb.src) {
    cuts.cb.src = NULL;
    cuts.cb.selection = NULL;
    cuts.cb.cancelled = NULL;
    cuts.cb.send  = NULL;
  }
}

void clear_dnd(struct c_wl_connection *conn) {
  if (conn == cuts.dnd.dst) {
    cuts.dnd.dst = NULL;
  } else if (conn == cuts.dnd.src) {
    cuts.dnd.src = NULL;

    cuts.dnd.data_device = NULL;

    cuts.dnd.data_source = NULL;

    cuts.dnd.data_offer = NULL;

    if (cuts.dnd.icon_node) {
      c_scene_node_remove(cuts.scene, cuts.dnd.icon_node);

      cuts.dnd.icon_node = NULL;
    }
  }

}

int spawn_proc(const char *s) {
  int pid;
  if ((pid = fork()) == 0) {
    close(STDIN_FILENO);
    open("/dev/null", O_RDWR);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    execvp("/bin/sh", (char *const []){"/bin/sh", "-c", (char *const)s, NULL});
  }
  return pid;
}

struct client *tag_select_client(int direction) {
  uint32_t clients = 0;

  struct client *client;
  clients_for_each_in_tag(client) {
    if (is_client(client, C_SCENE_NODE_WINDOW)) clients++;
  }

  if (!clients) return NULL;

  cuts.focused_client_idx =
      (cuts.focused_client_idx + direction + clients) % clients;

  int c = 0;
  clients_for_each_in_tag(client) {
    if (is_client(client, C_SCENE_NODE_WINDOW) && cuts.focused_client_idx == c++) {
      return client;
    }
  }

  return NULL;
}

struct client *client_from_connection(struct c_wl_connection *connection) {
  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (client->conn == connection)
      return client;
  }

  return NULL;
}

struct client *client_from_surface(struct c_wl_surface *surface) {
  if (!surface) return NULL;

  for (;;) {
    if (surface->sub.surface && surface->sub.surface->parent) {
      surface = surface->sub.surface->parent;
      continue;
    }
    if (surface->xdg_surface && surface->xdg_surface->parent) {
      surface = surface->xdg_surface->parent->surface;
      continue;
    }
    break;
  }

  struct client *client;
  c_list_for_each(cuts.clients, client) {
    if (!client->node) continue;

    struct c_wl_surface *root;
    if (is_client(client, C_SCENE_NODE_WINDOW)) {
      struct c_window *window = client->node->data;
      root = window->surface->surface;
    } else {
      root = wl_surface_from_node(client->node);
    }
    if (root == surface) return client;
  }

  return NULL;
}



struct client *client_new(struct c_wl_connection *connection) {
  struct client *client = calloc(1, sizeof(*client));
  if (!client) {
    c_log_errno(C_LOG_ERROR, "failed to allocate a new client");
    return NULL;
  }

  client->conn = connection;
  client->tag = cuts.tag;
  client->mon = cuts.focused_mon;

  c_list_insert(&cuts.clients, 0, client, 0);
  return client;
}

void client_free(struct client *client) {
  if (is_client(client, C_SCENE_NODE_WINDOW)) {
    struct c_window *window = client->node->data;
    c_window_free(window);
    client_border_delete(client);
  }

  if (client->node) c_scene_node_remove(cuts.scene, client->node);

  free(client);
}

void client_unfocus(struct client *client) {
  struct c_wl_object *o;
  struct c_wl_connection *conn = client->conn;

  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_data_device") && cuts.dnd.dst) {
      wl_data_device_leave(cuts.dnd.dst, o->id);
    }
  }

  if (is_client(client, C_SCENE_NODE_WINDOW))
    c_window_unfocus(client->node->data);
  else
    c_surface_leave(wl_surface_from_node(client->node));

  client_border_set_color(cuts.focused_client, border_inactive);
  cuts.focused_client = NULL;
}

void client_focus(struct client *client, double hotspot_x, double hotspot_y) {
  struct c_wl_connection *conn = client->conn;
  struct c_window *window;
  int is_window = is_client(client, C_SCENE_NODE_WINDOW);

  if (is_window) {
    window = client->node->data;
    if (*window->title)
      bar_set_title(*window->title);

    client_border_set_color(client, border_active);
  }

  struct c_wl_object *o;
  c_wl_objects_for_each(conn, o) {
    if (STREQ(o->iface->name, "wl_data_device")) {
      if (cuts.cb.src) {
        advertise_selection(o);
      } else if (is_window && cuts.dnd.src && cuts.dnd.data_device->dnd.data_source) {
        advertise_drag(window, o);
        cuts.dnd.dst = conn;
      }

      break;
    }
  }

  if (is_window)
    c_window_focus(window, hotspot_x, hotspot_y);

  else
    c_surface_enter(wl_surface_from_node(client->node), hotspot_x, hotspot_y);

  cuts.focused_client = client;
  c_log(C_LOG_DEBUG, "new focused client %p", client);
}

void client_change_focus(struct client *client, double hotspot_x, double hotspot_y) {
  if (cuts.focused_client)
    client_unfocus(cuts.focused_client);

  client_focus(client, hotspot_x, hotspot_y);
}

void client_pointer_move(struct client *client, double hotspot_x, double hotspot_y) {
  c_window_pointer_move(client->node->data, hotspot_x, hotspot_y);
}

void client_button(struct client *client) {}
void client_key(struct client *client) {}

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

void client_border_create(struct client *client) {
  client->border_rect[0].layer = C_SCENE_LAYER_NORMAL;
  client->border[0] = c_scene_add_rect(cuts.scene, &client->border_rect[0]);

  client->border_rect[1].layer = C_SCENE_LAYER_NORMAL;
  client->border[1] = c_scene_add_rect(cuts.scene, &client->border_rect[1]);

  client->border_rect[2].layer = C_SCENE_LAYER_NORMAL;
  client->border[2] = c_scene_add_rect(cuts.scene, &client->border_rect[2]);

  client->border_rect[3].layer = C_SCENE_LAYER_NORMAL;
  client->border[3] = c_scene_add_rect(cuts.scene, &client->border_rect[3]);

  client_border_set_color(client, border_inactive);
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

  struct c_window *window = client->node->data;

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
  client->border[0]->is_visible = is_visible;
  client->border[1]->is_visible = is_visible;
  client->border[2]->is_visible = is_visible;
  client->border[3]->is_visible = is_visible;

}

void client_border_raise(struct client *client) {
  c_scene_node_raise(cuts.scene, client->border[0]);
  c_scene_node_raise(cuts.scene, client->border[1]);
  c_scene_node_raise(cuts.scene, client->border[2]);
  c_scene_node_raise(cuts.scene, client->border[3]);
}

void client_raise(struct client *client) {
  client_border_raise(client);
  c_scene_node_raise(cuts.scene, client->node);
}

void client_set_fullscreen(struct client *client, struct monitor *mon) {
  struct c_window *window = client->node->data;
  struct c_output *output = mon->output;

  window->width = output->current_mode->width;
  window->height = output->current_mode->height;

  window->x = mon->x;
  window->y = mon->y;

  client->state = (client->state & ~CLIENT_FLOAT) | CLIENT_FULLSCREEN;
  window->states |= XDG_TOPLEVEL_STATE_FULLSCREEN;

  cuts.fullscreen |= client->tag;
  client->node->layer = C_SCENE_LAYER_OVERLAY;
  client_raise(client);
  client_border_set_visibility(client, 0);
  LAYOUT(mon);
}

void client_unset_fullscreen(struct client *client) {
  struct c_window *window = client->node->data;
  client->state &= ~CLIENT_FULLSCREEN;
  window->states &= ~XDG_TOPLEVEL_STATE_FULLSCREEN;
  cuts.fullscreen &= ~client->tag;
  client->node->layer = C_SCENE_LAYER_NORMAL;
  client_border_set_visibility(client, 1);

  LAYOUT(client->mon);
}

void client_set_visibility(struct client *client, int is_visible) {
  client->node->is_visible = is_visible;

  if (is_client(client, C_SCENE_NODE_WINDOW)) {
    client_border_set_visibility(client, is_visible && (client->state ^ CLIENT_FULLSCREEN));
  }
}

void client_toggle_floating(struct client *client) {
  client->state ^= CLIENT_FLOAT;
  LAYOUT(client->mon);
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

  double hot_x = new_x + cur->frame->hot_x;
  double hot_y = new_y + cur->frame->hot_y;

  cursor_move(cur, new_x, new_y);

  if (cuts.pointer.is_dragging || !cuts.focused_client) return;

  if (cuts.dnd.icon_node) {
    cuts.dnd.icon.x = hot_x;
    cuts.dnd.icon.y = hot_y;
    c_scene_node_update(cuts.dnd.icon_node);
  }

  struct client *focused = NULL;
  uint32_t top = 0;

  struct client *client;
  clients_for_each_in_tag(client) {
    if (!is_client(client, C_SCENE_NODE_WINDOW)) continue;

    struct c_window *window = client->node->data;
    if (CURSOR_INSIDE(hot_x, hot_y, window->x, window->y, window->width, window->height) && client->node->z >= top) {
        focused = client;
        top = client->node->z;
    }
  }

  if (focused == cuts.focused_client) {
    client_pointer_move(focused, hot_x, hot_y);

    if (is_client(client, C_SCENE_NODE_WINDOW) && cuts.dnd.dst) {
      struct c_window *window = client->node->data;
      struct c_wl_object *o;
      c_wl_objects_for_each(client->conn, o) {
        if (STREQ(o->iface->name, "wl_data_device")) {
          double lx, ly;
          c_window_surface_at(window, hot_x, hot_y, &lx, &ly);
          wl_data_device_motion(cuts.dnd.dst, o->id, now_ms(),
                                c_wl_fixed_from_double(lx),
                                c_wl_fixed_from_double(ly));
        }
      }
    }

  } else if (focused) {
    client_change_focus(focused, hot_x, hot_y);
  }

}

void on_mouse_scroll(struct c_input_mouse_event *event, void *userdata) {
  if (!cuts.focused_client) return;
  c_surface_pointer_scroll(cuts.focused_client->conn, event->axis,
                          event->axis120,
                          (enum wl_pointer_axis_source_enum)event->axis_source,
                          WL_POINTER_AXIS_VERTICAL_SCROLL,
                          event->axis_discrete);
}

void on_mouse_button(struct c_input_mouse_event *event, void *userdata) {
  if (cuts.dnd.src && !event->is_pressed) {
    struct client *client = cuts.focused_client;

    struct c_wl_object *o;
    c_wl_objects_for_each(client->conn, o) {
      if (STREQ(o->iface->name, "wl_data_device") && cuts.dnd.dst) {
        wl_data_device_drop(cuts.dnd.dst, o->id);
      }
    }

    wl_data_source_dnd_drop_performed(cuts.dnd.src, cuts.dnd.data_source->obj->id);
    if (cuts.dnd.icon_node) {
      c_scene_node_remove(cuts.scene, cuts.dnd.icon_node);
      cuts.dnd.icon_node = NULL;

    }
  }

  if (cuts.focused_client)
    c_surface_pointer_button(cuts.focused_client->conn, event->button, event->is_pressed);
}

void on_keyboard_key(struct c_input_keyboard_event *event, void *userdata) {
  struct c_wl_connection *conn = NULL;
  if (cuts.focused_client) conn = cuts.focused_client->conn;
  if (cuts.kb_focus)       conn = cuts.kb_focus->conn;

  if (!conn) return;

  c_surface_keyboard_key(conn, event->key, event->pressed,
                        event->mods_depressed, event->mods_latched,
                        event->mods_locked, event->group, event->changed);
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
    wl_output_scale(conn, wl_output->id, (c_wl_int)monitor->scale);

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

int on_wl_seat_get_keyboard(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
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

int on_wl_seat_get_pointer(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_input *input = userdata;

  if (!(input->capabilities & WL_SEAT_CAPABILITY_POINTER))
    c_wl_error_set_and_return(args[0].o, WL_SEAT_ERROR_MISSING_CAPABILITY, "pointer device not supported");

  return 0;
}

int on_wl_surface_commit(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_surface = c_wl_self(conn, args);
  struct c_wl_surface *surface = wl_surface->data;

  struct client *client = client_from_surface(surface);
  if (client && client->node) c_scene_node_update(client->node);

  assign_output_to_surface(wl_surface);

  if (surface->output) output_commit(surface->output->output);
  return 0;
}

int on_wl_surface_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_surface = c_wl_self(conn, args);
  struct c_wl_surface *surface = wl_surface->data;
  struct client *client = client_from_surface(surface);
  if (!client) goto no_client;

  if (client->node) c_scene_node_update(client->node);

  if (is_client(client, C_SCENE_NODE_WINDOW)) {
    struct c_window *window = client->node->data; 
    if (surface == window->focused)
      window->focused = NULL;
  }

no_client:
  if (!surface->output) return 0;

  struct c_output *output = surface->output->output;
  output_commit(output);
  c_unref(surface->output);
  return 0;
}

int on_xdg_surface_get_toplevel(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_new(conn);

  struct c_window *window = c_window_new(conn, surface);
  window->states = (1 << XDG_TOPLEVEL_STATE_TILED_TOP)       |
                               (1 << XDG_TOPLEVEL_STATE_TILED_RIGHT)     |
                               (1 << XDG_TOPLEVEL_STATE_TILED_BOTTOM)    |
                               (1 << XDG_TOPLEVEL_STATE_TILED_LEFT)      ;
  window->scale = cuts.focused_mon->scale;

  client->node = c_scene_add_window(cuts.scene, window);
  client_border_create(client);

  c_wl_enum wm_caps[1] = {XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN};
  c_wl_array arr = {
    .size = sizeof(wm_caps),
    .data = wm_caps,
  };

  xdg_toplevel_wm_capabilities(conn, surface->toplevel.obj->id, &arr);
  LAYOUT(client->mon);

  if (!(cuts.fullscreen & cuts.tag)) client_change_focus(client, pointer_x, pointer_y);
  return 0;
}

int on_xdg_toplevel_set_title(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_from_surface(surface->surface);
  assert(client);

  if (client == cuts.focused_client) bar_set_title(surface->toplevel.title);

  return 0;
}

int on_xdg_toplevel_unset_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_from_surface(surface->surface);
  client_unset_fullscreen(client);
  return 0;
}

int on_xdg_toplevel_set_fullscreen(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_from_surface(surface->surface);

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

int on_xdg_toplevel_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_xdg_surface *surface = c_wl_self(conn, args)->data;
  struct client *client = client_from_surface(surface->surface);

  int is_visible = client->tag & cuts.tag;
  struct monitor *mon = client->mon;

  client_close(client);
  if (is_visible) LAYOUT(mon)

  return 0;
}

int on_wl_data_source_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  clear_cb(conn);
  clear_dnd(conn);
  return 0;
}

int on_clipboard_offer_receive(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  c_wl_string mimetype = args[1].s;
  c_wl_fd fd = args[2].F;

  if (self->data && cuts.dnd.data_offer == self->data) {
    struct c_wl_data_source *data_source = cuts.dnd.data_source;
    struct c_wl_connection  *src = cuts.dnd.src;

    for (size_t i = 0; i < data_source->mimes; i++) {
      if (STREQ(data_source->mimetypes[i], mimetype)) {
        wl_data_source_send(src, data_source->obj->id, mimetype, fd);
        c_wl_connection_flush(src);
        goto out;
      }
    }
  }
  struct clipboard_selection *selection = cuts.cb.selection;
  if (!selection) goto out;

  for (size_t i = 0; i < selection->mimes; i++) {
    if (STREQ(selection->mimetypes[i], mimetype)) {
      cuts.cb.send(cuts.cb.src, selection->obj->id, mimetype, fd);
      c_wl_connection_flush(cuts.cb.src);
      goto out;
    }
  }

out:
  return 0;
}

int on_wl_data_offer_set_actions(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
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

int on_wl_data_offer_accept(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *offer = self->data;

  if (offer == cuts.dnd.data_offer)
    wl_data_source_target(cuts.dnd.src, cuts.dnd.data_source->obj->id, offer->mimetype);
  return 0;
}

int on_wl_data_offer_finish(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *offer = self->data;

  if (offer != cuts.dnd.data_offer)
    c_wl_error_set_and_return(self->id, WL_DATA_OFFER_ERROR_INVALID_FINISH,
                              "this wl_data_offer isn't associated with dnd");

  wl_data_source_dnd_finished(cuts.dnd.src, cuts.dnd.data_source->obj->id);
  clear_dnd(conn);
  clear_dnd(cuts.dnd.src);
  return 0;
}

int on_wl_data_offer_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_wl_data_offer *data_offer = self->data;
  struct c_wl_data_device *data_device = data_offer->data_device;

  if (data_device) {
    data_device->data_offer = NULL;
    data_offer->data_device = NULL;
  }

  return 0;
}

int on_zxdg_toplevel_decoration_v1_set_mode(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_xdg_surface *surface = self->data;
  zxdg_toplevel_decoration_v1_configure(conn, self->id, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  xdg_surface_configure(conn, surface->obj->id, c_wl_serial());
  return 0;
}

int on_wl_data_device_start_drag(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *wl_data_device = c_wl_self(conn, args);
  struct c_wl_data_device *data_device = wl_data_device->data;
  struct c_wl_data_source *data_source = data_device->dnd.data_source;

  struct client *client = client_from_surface(data_device->dnd.origin);

  assert(is_client(client, C_SCENE_NODE_WINDOW));
  struct c_window *window = client->node->data;

  cuts.dnd.src = conn;
  cuts.dnd.data_source = data_source;
  cuts.dnd.data_device = data_device;

  struct c_wl_data_offer *data_offer = advertise_drag(window, wl_data_device);
  cuts.dnd.data_offer = data_offer;
  data_offer->data_device = data_device;

  data_device->data_offer = data_offer;
  if (data_device->dnd.icon) {
    cuts.dnd.icon.surface = data_device->dnd.icon;

    cuts.dnd.icon.x = pointer_x;
    cuts.dnd.icon.y = pointer_y;
    cuts.dnd.icon.layer = C_SCENE_LAYER_NORMAL;
    cuts.dnd.icon.width = 0;
    cuts.dnd.icon.height = 0;
    cuts.dnd.icon_node = c_scene_add_surface(cuts.scene, &cuts.dnd.icon);
  }

  cuts.dnd.dst = conn;
  return 0;
}

int on_wl_data_device_set_selection(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_data_source *data_source = c_wl_object_get(conn, args[1].o)->data;

  if (cuts.cb.src == conn) {
    if (!args[1].o) {
      cuts.cb.src = NULL;
      cuts.cb.selection = NULL;
    } else {
      return 1;
    }
  } else if (cuts.cb.src)
    cuts.cb.cancelled(cuts.cb.src, cuts.cb.selection->obj->id);

  cuts.cb.src = conn;
  cuts.cb.selection = (struct clipboard_selection *)data_source;
  cuts.cb.send = wl_data_source_send;
  cuts.cb.cancelled = wl_data_source_cancelled;

  broadcast_selection();
  return 0;
}

int on_ext_data_control_device_v1_set_selection(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_ext_data_control_source *data_source = c_wl_object_get(conn, args[1].o)->data;

  if (cuts.cb.src)
    cuts.cb.cancelled(cuts.cb.src, cuts.cb.selection->obj->id);

  cuts.cb.src = conn;
  cuts.cb.selection = (struct clipboard_selection *)data_source;
  cuts.cb.send = ext_data_control_source_v1_send;
  cuts.cb.cancelled = ext_data_control_source_v1_cancelled;

  broadcast_selection();
  return 0;
}


int on_ext_data_control_manager_v1_get_data_device(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *data_device = c_wl_object_get(conn, args[1].o);
  advertive_selection_ext(data_device);
  return 0;
}

int on_wp_cursor_shape_device_v1_set_shape(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct client *client = cuts.focused_client;
  if (!client) return 0;

  enum c_cursor_shape shape = args[2].u;
  if (conn == client->conn) {
    if (shape > C_CURSOR_ALL_RESIZE) {
      c_wl_error_set_and_return(args[0].o, WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE, "invalid shape");
    }

    c_cursor_set_shape(cuts.pointer.cur, shape);
  }
  return 0;
}

int on_wp_fractional_scale_manager_v1_get_fractional_scale(
    struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  c_wl_new_id wp_fractional_scale_id = args[1].n;
  struct c_wl_surface *surface = c_wl_object_get(conn, args[2].o)->data;
  struct monitor *mon = cuts.focused_mon;

  surface->fscale = mon->scale * 120;
  wp_fractional_scale_v1_preferred_scale(conn, wp_fractional_scale_id, surface->fscale);

  return 0;
}

int on_zwlr_layer_shell_v1_get_layer_surface(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_zwlr_layer_surface *layer_surface = c_wl_object_get(conn, args[1].o)->data;

  struct client *client = client_new(conn);
  client->tag = 0xffffffff;

  struct c_scene_surface *surface = calloc(1, sizeof(*surface));

  surface->surface = layer_surface->surface;
  surface->layer = layer_map[layer_surface->pending.layer];
  surface->obj = layer_surface->obj;

  client->node = c_scene_add_surface(cuts.scene, surface);
  return 0;
}

int on_zwlr_layer_surface_v1_set_size(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;
  struct c_output *output = cuts.focused_mon->output;
  struct c_output_mode *mode = output->current_mode;

  uint32_t layer_w = layer_surface->pending.width ? layer_surface->pending.width : mode->width;
  uint32_t layer_h = layer_surface->pending.height ? layer_surface->pending.height : mode->height;

  double layer_x = (double)mode->width / 2 - (double)layer_w / 2;
  double layer_y = (double)mode->height / 2 - (double)layer_h / 2;

  c_wl_uint serial = c_wl_serial();
  zwlr_layer_surface_v1_configure(conn, self->id, serial, layer_w, layer_h);
  layer_surface->configure = serial;

  double shift_x = layer_x;
  double shift_y = layer_y;

  if (layer_surface->pending.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)
    layer_x -= shift_x;

  if (layer_surface->pending.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)
    layer_x += shift_x;

  if (layer_surface->pending.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)
    layer_y -= shift_y;

  if (layer_surface->pending.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)
    layer_y += shift_y;

  layer_surface->pending.width = layer_w;
  layer_surface->pending.height = layer_h;


  struct client *client = client_from_surface(layer_surface->surface);
  struct c_scene_surface *surface = client->node->data;

  surface->x = layer_x;
  surface->y = layer_y;
  surface->width = layer_w;
  surface->height = layer_h;

  c_scene_node_update(client->node);

  return 0;
}

int on_zwlr_layer_surface_v1_set_keyboard_interactivity(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_zwlr_layer_surface *layer_surface = c_wl_self(conn, args)->data;
  c_wl_enum ki = args[1].e;
  if (ki == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
    if (cuts.kb_focus) {
      struct c_wl_surface *surface = wl_surface_from_node(cuts.kb_focus->node);
      c_surface_leave_keyboard(surface);
    }
    cuts.kb_focus = client_from_surface(layer_surface->surface);
    assert(cuts.kb_focus);
    c_surface_enter_keyboard(layer_surface->surface);
  }
  return 0;
}

int on_zwlr_layer_surface_v1_destroy(struct c_wl_connection *conn, c_wl_args args, void *userdata) {
  struct c_wl_object *self = c_wl_self(conn, args);
  struct c_zwlr_layer_surface *layer_surface = self->data;

  struct client *client = client_from_surface(layer_surface->surface);

  if (client == cuts.kb_focus) { 
    cuts.kb_focus = NULL; 
    if (cuts.focused_client) c_window_focus(cuts.focused_client->node->data, pointer_x, pointer_y);
  }

  struct c_scene_surface *surf = client->node->data;
  client_close(client);
  free(surf);
  return 0;
}

void quit(bind_args *args) {
  c_event_loop_stop(cuts.loop);
}

void spawn(bind_args *args) {
  spawn_proc(args->s);
}

void window_kill(bind_args *args) {
  if (!cuts.focused_client) return;
  c_window_close(cuts.focused_client->node->data);
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

  if (is_client_fullscreen(client))
    client_unset_fullscreen(client);
  else
    client_set_fullscreen(client, client->mon);
}

void move_focus(bind_args *args) {
  if (cuts.fullscreen & cuts.tag) return;

  struct client *next = tag_select_client(args->i);
  if (next) {
    client_change_focus(next, pointer_x, pointer_y);
    if (cuts.layout.func == zoom) {
      client_raise(next);
    }
  }
}

void switch_tag(bind_args *args) {
  if (cuts.tag == args->u) return;

  cuts.focused_client_idx = 0;
  bar_switch_tag(cuts.tag, args->u);

  cuts.tag = args->u;

  struct client *c;
  c_list_for_each(cuts.clients, c) {
    client_set_visibility(c, c->tag & cuts.tag);
  }
  
  struct client *prev = tag_select_client(0);

  if (cuts.focused_client) client_unfocus(cuts.focused_client);
  if (prev) client_focus(prev, pointer_x, pointer_y);

  LAYOUT(cuts.focused_mon);

  if (!prev)
    bar_clear_title();
}

void toggle_floating(bind_args *args) {
  if (!cuts.focused_client || (cuts.fullscreen & cuts.tag)) return;

  client_raise(cuts.focused_client);
  client_toggle_floating(cuts.focused_client);
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
  if (!cuts.focused_client || (cuts.fullscreen & cuts.tag) ||
      !is_client(cuts.focused_client, C_SCENE_NODE_WINDOW))
    return;

  struct client *client = cuts.focused_client;
  struct c_window *window = client->node->data;

  if (!cuts.pointer.is_dragging)
    client_raise(client);

  if (!(client->state & CLIENT_FLOAT))
    client_toggle_floating(client);

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

  c_scene_node_move(client->node, dist_x, dist_y);
  client_border_sync(client);
  output_commit(client->mon->output);
}

void window_move_to_workspace(bind_args *args) {
  if (!cuts.focused_client || cuts.tag == args->u) return;

  cuts.focused_client->tag = args->u;
  client_set_visibility(cuts.focused_client, 0);
  client_raise(cuts.focused_client);

  struct client *next;
  if ((next = tag_select_client(1)))
    client_change_focus(next, pointer_x, pointer_y);
}

void window_resize(int done, bind_args *args) {
  if (!cuts.focused_client || (cuts.fullscreen & cuts.tag) ||
      !is_client(cuts.focused_client, C_SCENE_NODE_WINDOW))
    return;

  struct client *client = cuts.focused_client;
  struct c_window *window = client->node->data;

  if (!cuts.pointer.is_dragging)
    client_raise(client);

  if (!(client->state & CLIENT_FLOAT))
    client_toggle_floating(client);

  if (done) {
    pointer_x_prev = pointer_x;
    pointer_y_prev = pointer_y;
  }
    
  float dist_x = pointer_x - pointer_x_prev;
  float dist_y = pointer_y - pointer_y_prev;

  window->width = CLAMP(window->width + dist_x, MIN_WINDOW_SIZE, UINT32_MAX);
  window->height = CLAMP(window->height + dist_y, MIN_WINDOW_SIZE, UINT32_MAX);

  if (!cuts.pointer.is_dragging && !done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_NWSE_RESIZE);
  } else if (done) {
    c_cursor_set_shape(cuts.pointer.cur, C_CURSOR_DEFAULT);
  }
  
  c_scene_node_update(client->node);
  cuts.pointer.is_dragging = !done;

  if (done)
    window->states &= ~(ENUM_FLAG(XDG_TOPLEVEL_STATE_RESIZING) |
                        ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT) |
                        ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_TOP));
  else
    window->states |=  (ENUM_FLAG(XDG_TOPLEVEL_STATE_RESIZING) |
                        ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT) |
                        ENUM_FLAG(XDG_TOPLEVEL_STATE_CONSTRAINED_TOP));
  c_log_value(window->states, "%08b");

  LAYOUT(client->mon);
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
    if (!is_client(client, C_SCENE_NODE_WINDOW)) continue;

    struct c_window *window = client->node->data;
    if (is_client_floating(client)) goto floating;
    if (is_client_fullscreen(client)) goto fullscreen;

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
    if (!is_client(client, C_SCENE_NODE_WINDOW)) continue;

    struct c_window *window = client->node->data;
    if (is_client_fullscreen(client)) goto fullscreen;
    if (is_client_floating(client)) goto floating;

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
  struct c_scene_buffer *text = &block->text;

  rect->layer = C_SCENE_LAYER_TOP;
  block->rect_node = c_scene_add_rect(cuts.scene, &block->rect);

  text->width = rect->width;
  text->height = rect->height;

  uint32_t buffer_size = text->width * 4 * text->height;
  uint8_t *buffer = calloc(buffer_size, 1);
  if (!buffer)
    c_log(C_LOG_ERROR, "failed to allocate a text buffer");

  text->buffer = buffer;

  text->x = block->rect.x + bar->h_padding;
  text->y = block->rect.y + bar->v_padding;

  text->layer = C_SCENE_LAYER_TOP;
  block->text_node = c_scene_add_buffer(cuts.scene, text);
  
}

void bar_block_update(struct bar_block *block) {
  c_scene_node_update(block->rect_node);
  c_scene_node_update(block->text_node);

  output_commit(cuts.focused_mon->output);
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

void bar_create(struct c_output_mode *mode, double scale) {
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

  if ((error = FT_Set_Char_Size(bar->ft.face, 0, bar->cfg->font.size * 64, 96 * scale, 96 * scale))) {
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
  uint32_t thickness = bar->glyph.height + pad;

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
    rect->layer = C_SCENE_LAYER_NORMAL;

    if (is_bar_vertical(bar)) {
      rect->x = bar->cfg->pos == BAR_RIGHT ? mode->width - bar->width : 0;
      rect->y = pen;
    } else if (is_bar_horizontal(bar)) {
      rect->x = pen;
      rect->y = bar->cfg->pos == BAR_TOP ? 0 : mode->height - bar->height;
    }

    rect->height = bar->height;
    rect->width = bar->width;

    if (i < tags) { // tag blocks
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

out:
  return C_EVENT_OK;
}

void signal_handler(int signal, void *userdata) {
  c_event_loop_stop(cuts.loop);
}

void cleanup(int err) {
  for (size_t i = 0; i < LENGTH(autostart); i++) kill(cuts.pids[i], SIGTERM);

  if (cuts.pointer.cur) c_cursor_free(cuts.pointer.cur);
  if (cuts.display)     c_wl_display_free(cuts.display);
  bar_destroy();

  if (cuts.clients) {
    struct client *client;
    c_list_for_each(cuts.clients, client) {
      client_free(client);
    }
    c_list_destroy(cuts.clients);
  }

  if (cuts.scene)    c_scene_free(cuts.scene);
  if (cuts.mgr)      c_output_manager_free(cuts.mgr);
  if (cuts.session)  c_session_free(cuts.session);
  if (cuts.loop)     c_event_loop_free(cuts.loop);
  if (cuts.monitors) c_list_destroy(cuts.monitors);

  exit(err);
}

int main() {
  int ret = 0;
  cuts.tag = 1 << 0;
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
  struct c_session *session = c_session_init(loop, &input_config);
  check_init(session, ret, out);
  cuts.session = session;

  struct c_output_manager *mgr = c_output_manager_init(session, loop);
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

    struct monitor mon = {output, 1.0f, 0, 0};
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

          double frac120f = cfg->scale * 120;
          int32_t frac120 = frac120f;

          if (frac120 != frac120f) {
            c_log(C_LOG_WARNING, "can't use fraction scale of %f. using %f", cfg->scale, frac120 / 120.f);
            mon.scale = frac120 / 120.0f;

          } else {
            mon.scale = cfg->scale;
          }

          mon.x = cfg->x;
          mon.y = cfg->y;

          c_log(C_LOG_DEBUG, "choosen config: %s x%.02f %dx%d@%dHz",
                cfg->name, mon.scale, cfg->width, cfg->height,
                cfg->refresh_rate);

          set_background(&mon, mode);
          bar_create(mode, mon.scale);

          c_output_set_mode(mgr, output, mode);
          goto mode_iter_end;
        }
      }
    }

    assert(preferred);
    set_background(&mon, preferred);
    bar_create(preferred, mon.scale);

    c_output_set_mode(mgr, output, preferred);

mode_iter_end:
    mon_cpy = c_list_push(cuts.monitors, &mon, sizeof(mon));

    c_wl_interface_support("wl_output", on_wl_output_bind, mon_cpy);

    if (!cuts.focused_mon) cuts.focused_mon = mon_cpy;
  }

  if (c_input_init_xkb_state(session->input, &xkb_rules) < 0) goto out;

  struct c_input_events input_listener = {
    .mouse_movement = on_mouse_movement,
    .mouse_button = on_mouse_button,
    .mouse_scroll = on_mouse_scroll,
    .keyboard_key = on_keyboard_key,
  };
  c_input_add_event_listener(session->input, &input_listener, NULL);

  for (size_t i = 0; i < LENGTH(keys); i++) {
    struct key_bind *b = &keys[i];
    c_input_add_combo(session->input, b->modmask, b->keysym, 0, (void (*)(void *))b->handler, &b->args);
  }

  for (size_t i = 0; i < LENGTH(mouse); i++) {
    struct mouse_bind *b = &mouse[i];
    if (b->drag)
      c_input_add_drag_combo(
          session->input, b->modmask, b->keysym, b->btn,
          (void (*)(int, void *))b->drag_handler, &b->args);

    else
      c_input_add_combo(session->input, b->modmask, b->keysym, b->btn,
                                (void (*)(void *))b->handler, &b->args);
  }

  cuts.seat.get_keyboard = on_wl_seat_get_keyboard;
  cuts.seat.get_pointer = on_wl_seat_get_pointer;
  wl_seat_listen(&cuts.seat, session->input);

  cuts.surface.commit = on_wl_surface_commit;
  cuts.surface.destroy = on_wl_surface_destroy;
  wl_surface_listen(&cuts.surface, NULL);

  cuts.xdg_surface.get_toplevel = on_xdg_surface_get_toplevel;
  xdg_surface_listen(&cuts.xdg_surface, NULL);

  cuts.xdg_toplevel.destroy = on_xdg_toplevel_destroy;
  cuts.xdg_toplevel.set_title = on_xdg_toplevel_set_title;
  cuts.xdg_toplevel.set_fullscreen = on_xdg_toplevel_set_fullscreen;
  cuts.xdg_toplevel.unset_fullscreen = on_xdg_toplevel_unset_fullscreen;
  xdg_toplevel_listen(&cuts.xdg_toplevel, NULL);

  cuts.data_device.start_drag = on_wl_data_device_start_drag;
  cuts.data_device.set_selection = on_wl_data_device_set_selection;
  wl_data_device_listen(&cuts.data_device, NULL);

  cuts.data_source.destroy = on_wl_data_source_destroy;
  wl_data_source_listen(&cuts.data_source, NULL);

  cuts.data_offer.receive = on_clipboard_offer_receive;
  cuts.data_offer.set_actions = on_wl_data_offer_set_actions;
  cuts.data_offer.accept = on_wl_data_offer_accept;
  cuts.data_offer.finish = on_wl_data_offer_finish;
  cuts.data_offer.destroy = on_wl_data_offer_destroy;
  wl_data_offer_listen(&cuts.data_offer, NULL);

  cuts.data_control_manager.get_data_device = on_ext_data_control_manager_v1_get_data_device;
  ext_data_control_manager_v1_listen(&cuts.data_control_manager, NULL);

  cuts.data_control_device.set_selection = on_ext_data_control_device_v1_set_selection;
  ext_data_control_device_v1_listen(&cuts.data_control_device, NULL);

  cuts.data_control_offer.receive = on_clipboard_offer_receive;
  ext_data_control_offer_v1_listen(&cuts.data_control_offer, NULL);

  cuts.decor.set_mode = on_zxdg_toplevel_decoration_v1_set_mode;
  zxdg_toplevel_decoration_v1_listen(&cuts.decor, NULL);

  cuts.cursor_shape.set_shape = on_wp_cursor_shape_device_v1_set_shape;
  wp_cursor_shape_device_v1_listen(&cuts.cursor_shape, NULL);

  cuts.fraction_scale.get_fractional_scale = on_wp_fractional_scale_manager_v1_get_fractional_scale;
  wp_fractional_scale_manager_v1_listen(&cuts.fraction_scale, NULL);

  cuts.layer_shell.get_layer_surface = on_zwlr_layer_shell_v1_get_layer_surface;
  zwlr_layer_shell_v1_listen(&cuts.layer_shell, NULL);

  cuts.layer_surface.set_size = on_zwlr_layer_surface_v1_set_size;
  cuts.layer_surface.set_keyboard_interactivity = on_zwlr_layer_surface_v1_set_keyboard_interactivity;
  cuts.layer_surface.destroy = on_zwlr_layer_surface_v1_destroy;
  zwlr_layer_surface_v1_listen(&cuts.layer_surface, NULL);

  c_wl_interface_support("wl_data_device_manager", NULL, NULL);
  c_wl_interface_support("ext_data_control_manager_v1", NULL, NULL);
  c_wl_interface_support("zxdg_decoration_manager_v1", NULL, NULL);
  c_wl_interface_support("wp_cursor_shape_manager_v1", NULL, NULL);
  c_wl_interface_support("wp_fractional_scale_manager_v1", NULL, NULL);
  c_wl_interface_support("zwlr_layer_shell_v1", NULL, NULL);
  // c_wl_interface_support("wp_viewporter", NULL, NULL);

  for (size_t i = 0; i < LENGTH(autostart); i++) {
    cuts.pids[i] = spawn_proc(autostart[i]);;
  }

  c_event_loop_add(loop, STDIN_FILENO, stdin_text, NULL);

  ret = c_event_loop_run(loop);

out:
  cleanup(ret);
}

#include "server/server.h"
#include "server/wayland.h"
#include "server/error.h"

#define CHECK_OBJECT(id, iface) \
  (iface) = c_object_get(conn, (id)); \
  if ((iface)) return c_error_set((id), WL_DISPLAY_ERROR_INVALID_OBJECT, "object already registered")


int wl_display_get_registry(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id object_id = args[0].n;
  struct wl_interface *wl_registry;
  CHECK_OBJECT(object_id, wl_registry);

  c_object_add(conn, object_id, (struct wl_interface *)c_interface_get_global("wl_display"));

  const struct wl_interface *ifaces[] = {
    c_interface_get_global("wl_compositor"),
    c_interface_get_global("wl_shm"),
  };

  for (size_t i = 0; i < sizeof(ifaces) / sizeof(void *); i++) {
    wl_registry_global(conn, object_id, i+1, ifaces[i]->name, ifaces[i]->version);
  }

  return 0;
};

int wl_display_sync(struct wl_connection *conn, union wl_arg *args) {
  static int data = 1;

  wl_object object_id = args[0].o;
  struct wl_interface *wl_callback;
  CHECK_OBJECT(object_id, wl_callback);

  wl_callback_done(conn, object_id, data++);
  wl_display_delete_id(conn, 1, object_id);
  c_object_del(conn, object_id);

  return 0;
}

int wl_registry_bind(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id new_id = args[3].n;
  struct wl_interface *wl_object;
  CHECK_OBJECT(new_id, wl_object);

  const struct wl_interface *interface = c_interface_get_global(args[1].s);
  if (!interface) return c_error_set(new_id, WL_DISPLAY_ERROR_IMPLEMENTATION, "interface not supported");

  c_object_add(conn, new_id, interface);
  return 0;
}

int wl_shm_create_pool(struct wl_connection *conn, union wl_arg *args) {
  return 0;
}

int wl_compositor_create_surface(struct wl_connection *conn, union wl_arg *args) {
  wl_new_id new_id = args[3].n;
  struct wl_interface *wl_surface;
  CHECK_OBJECT(new_id, wl_surface);

  c_object_add(conn, new_id, c_interface_get_global("wl_surface"));
  return 0;
}

#include <libinput.h>
#include <libudev.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "backend/backend.h"
#include "backend/input.h"
#include "util/log.h"

enum __input_event_listener_type {
  INPUT_EVENT_LISTENER_MOUSE = 1 << 5,
  INPUT_EVENT_LISTENER_KBD,
};

struct __input_event_listener {
  void *userdata;
  void *listener;
  enum __input_event_listener_type type;
};

enum c_input_notifier {
	C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT,
	C_INPUT_NOTIFY_ON_MOUSE_SCROLL, 
	C_INPUT_NOTIFY_ON_MOUSE_BUTTON, 

	C_INPUT_NOTIFY_ON_KBD_KEY,
};

static void c_input_notify_kbd(struct c_input *input, 
                                 struct c_input_keyboard_event *event, enum c_input_notifier notifier) {
  struct __input_event_listener *l;

  #define notify_kbd(callback) \
    c_list_for_each(input->listeners, l) { \
      if (l->type == INPUT_EVENT_LISTENER_KBD && ((struct c_input_listener_kbd *)l->listener)->callback) \
        ((struct c_input_listener_kbd *)l->listener)->callback(event, l->userdata); \
    }

  switch (notifier) {
    case C_INPUT_NOTIFY_ON_KBD_KEY: notify_kbd(on_kbd_key); break;
    default: break;
  }
}

static void c_input_notify_mouse(struct c_input *input, 
                                 struct c_input_mouse_event *event, enum c_input_notifier notifier) {
  struct __input_event_listener *l;

  #define notify_mouse(callback) \
    c_list_for_each(input->listeners, l) { \
      if (l->type == INPUT_EVENT_LISTENER_MOUSE && ((struct c_input_listener_mouse *)l->listener)->callback) {\
        ((struct c_input_listener_mouse *)l->listener)->callback(event, l->userdata); \
      } \
    }

  switch (notifier) {
    case C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT:  notify_mouse(on_mouse_movement); break;
    case C_INPUT_NOTIFY_ON_MOUSE_SCROLL:    notify_mouse(on_mouse_scroll); break;
    case C_INPUT_NOTIFY_ON_MOUSE_BUTTON:    notify_mouse(on_mouse_button); break;
    default: break;
  }
}


static int open_restricted(const char *path, int flags, void *userdata) {
  struct c_backend *backend = userdata;
  struct c_backend_device *dev = c_backend_device_open(backend, path, C_BACKEND_DEV_INPUT);
  if (!dev) return -1;
  return dev->fd;
}

static void close_restricted(int fd, void *userdata) {
  struct c_backend *backend = userdata;
  struct c_backend_device *dev;
  c_list_for_each(backend->devices, dev) {
    if (dev->fd == fd)
      c_backend_device_close(backend, dev);
  }
}

static void handle_event_mouse(struct c_input *input, struct libinput_event_pointer *event, enum libinput_event_type type) {
  assert(event);
  struct c_input_mouse_event mouse_event = {0};
  mouse_event.libinput_event = event;

  switch (type) {
    case LIBINPUT_EVENT_POINTER_MOTION:
      mouse_event.x = libinput_event_pointer_get_dx(event);
      mouse_event.y = libinput_event_pointer_get_dy(event);

      c_input_notify_mouse(input, &mouse_event, C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT);
      break;

    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
      mouse_event.x = libinput_event_pointer_get_absolute_x(event);
      mouse_event.y = libinput_event_pointer_get_absolute_y(event);
      mouse_event.abs = 1;

      c_input_notify_mouse(input, &mouse_event, C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT);
      break;

    case LIBINPUT_EVENT_POINTER_BUTTON:
    case LIBINPUT_EVENT_POINTER_AXIS:
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
    default:
      return;
  }

}
static void handle_event_keyboard(struct c_input *input, struct libinput_event_keyboard *event) {
  assert(event);

  struct c_input_keyboard_event keyboard_event = {0};
  keyboard_event.key = libinput_event_keyboard_get_key(event);
  keyboard_event.pressed = libinput_event_keyboard_get_key_state(event);
  keyboard_event.pressed_time = libinput_event_keyboard_get_time_usec(event);

  c_input_notify_kbd(input, &keyboard_event, C_INPUT_NOTIFY_ON_KBD_KEY);
}

static void handle_event_dev_added(struct c_input *input, struct libinput_device *li_dev) {
  assert(li_dev);

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_POINTER)) {
    input->capabilities |= 1;
  }

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
    input->capabilities |= 2;
  }

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_TOUCH)) {
    input->capabilities |= 4;
  }

  // c_log(C_LOG_DEBUG, "caps: %d", input->capabilities);

}

static void handle_event_dev_removed(struct c_input *input, struct libinput_device *li_dev) {
  assert(li_dev);

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_POINTER)) {
    input->capabilities &= ~1;
  }

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
    input->capabilities &= ~2;
  }

  if (libinput_device_has_capability(li_dev, LIBINPUT_DEVICE_CAP_TOUCH)) {
    input->capabilities &= ~4;
  }

}

static void handle_event(struct c_input *input, struct libinput_event *event) {
  enum libinput_event_type event_type = libinput_event_get_type(event);

  switch (event_type) {
    case LIBINPUT_EVENT_DEVICE_ADDED:
      handle_event_dev_added(input, libinput_event_get_device(event));
      return;

    case LIBINPUT_EVENT_DEVICE_REMOVED:
      // FIXME: unset cap bit only if no other devices have the same cap
      handle_event_dev_removed(input, libinput_event_get_device(event));
      return;

    case LIBINPUT_EVENT_KEYBOARD_KEY:
      handle_event_keyboard(input, libinput_event_get_keyboard_event(event));
      return;

    case LIBINPUT_EVENT_POINTER_MOTION:
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
    case LIBINPUT_EVENT_POINTER_BUTTON:
    case LIBINPUT_EVENT_POINTER_AXIS:
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
      handle_event_mouse(input, libinput_event_get_pointer_event(event), event_type);
      return;

    default:
      c_log(C_LOG_WARNING, "unsupported event %d. ignoring", event_type);
      return;
  }
}

static int input_dispatch(struct c_input *input) {
  if (libinput_dispatch(input->libinput) < 0) {
    c_log_errno(C_LOG_ERROR, "libinput_dispatch failed");
    return -1;
  }

  struct libinput_event *event;
  while ((event = libinput_get_event(input->libinput))) {
    handle_event(input, event);
    libinput_event_destroy(event);
  }
  return 0;

}

C_EVENT_CALLBACK libinput_dispatch_handler(struct c_event_loop *loop, int fd, void *userdata) {
  struct c_input *input = userdata;
  int ret = input_dispatch(input);
  return ret == 0 ? C_EVENT_OK : C_EVENT_ERROR_FATAL;
}


static void add_listener(c_list *listeners, enum __input_event_listener_type type, void *listener, size_t listener_size, void *userdata) {
  struct __input_event_listener l = {
    .userdata = userdata,
    .listener = calloc(1, listener_size),
    .type = type,
  };
  memcpy(l.listener, listener, listener_size);
  c_list_push(listeners, &l, sizeof(l));
}

void c_input_add_listener_kbd(struct c_input *input, struct c_input_listener_kbd *listener, void *userdata) {
  add_listener(input->listeners, INPUT_EVENT_LISTENER_KBD, listener, sizeof(*listener), userdata);
}

void c_input_add_listener_mouse(struct c_input *input, struct c_input_listener_mouse *listener, void *userdata) {
  add_listener(input->listeners, INPUT_EVENT_LISTENER_MOUSE, listener, sizeof(*listener), userdata);
}


void c_input_free(struct c_input *input) {
  if (input->listeners) {
    struct __input_event_listener *l;
    c_list_for_each(input->listeners, l) {
      free(l->listener);
    }
    c_list_destroy(input->listeners);
  }
  free(input);
}

struct c_input *c_input_init(struct c_event_loop *loop, struct c_backend *backend) {
  struct c_input *input = calloc(1, sizeof(*input));
  if (!input) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error;
  }

  struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
  };

  struct udev *udev = udev_new();
  struct libinput *libinput = libinput_udev_create_context(&interface, backend, udev);
  if (!libinput) {
    c_log(C_LOG_ERROR, "libinput_udev_create_context failed");
    goto error;
  }

  if (libinput_udev_assign_seat(libinput, "seat0") < 0) {
    c_log_errno(C_LOG_ERROR, "libinput_udev_assign_seat failed");
    goto error;
  }

  input->libinput = libinput;
  
  if (input_dispatch(input) < 0) {
    c_log_errno(C_LOG_ERROR, "libinput_dispatch failed");
    goto error;
  }

  int fd = libinput_get_fd(libinput);
  c_event_loop_add(loop, fd, libinput_dispatch_handler, input);

  input->listeners = c_list_new();

  return input;

error:
  c_input_free(input);
  return NULL;
}

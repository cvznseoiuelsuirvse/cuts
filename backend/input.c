#include <libinput.h>
#include <libudev.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

#include "backend/input.h"
#include "util/log.h"

enum __input_event_listener_type {
  INPUT_EVENT_LISTENER_MOUSE = 1 << 5,
  INPUT_EVENT_LISTENER_keyboard,
};

struct __input_event_listener {
  void *userdata;
  void *listener;
  enum __input_event_listener_type type;
};

struct __input_shortcut_listener {
  uint32_t mod_mask;
  xkb_keysym_t keysym;
  void *userdata;
  void (*handler)(void *userdata);
  
};

enum c_input_notifier {
	C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT,
	C_INPUT_NOTIFY_ON_MOUSE_SCROLL, 
	C_INPUT_NOTIFY_ON_MOUSE_BUTTON, 

	C_INPUT_NOTIFY_ON_keyboard_KEY,
};

static struct xkb_state *get_xkb_state(struct c_input *input) {
  if (!input->xkb.state) {
    c_log(C_LOG_ERROR, "can't get xkb_state from c_input. c_input_init_xkb_state() was never called");
    return NULL;
  }
  return input->xkb.state;
}

static struct xkb_keymap *get_xkb_keymap(struct c_input *input) {
  if (!input->xkb.keymap) {
    c_log(C_LOG_ERROR, "can't get xkb_keymap from c_input. c_input_init_xkb_state() was never called");
    return NULL;
  }
  return input->xkb.keymap;
}

static void c_input_notify_keyboard(struct c_input *input, 
                                 struct c_input_keyboard_event *event, enum c_input_notifier notifier) {
  struct __input_event_listener *l;

  #define notify_keyboard(callback) \
    c_list_for_each(input->event_listeners, l) { \
      if (l->type == INPUT_EVENT_LISTENER_keyboard && ((struct c_input_event_listener_keyboard *)l->listener)->callback) \
        ((struct c_input_event_listener_keyboard *)l->listener)->callback(event, l->userdata); \
    }

  switch (notifier) {
    case C_INPUT_NOTIFY_ON_keyboard_KEY: notify_keyboard(on_keyboard_key); break;
    default: break;
  }
}

static void c_input_notify_mouse(struct c_input *input, 
                                 struct c_input_mouse_event *event, enum c_input_notifier notifier) {
  struct __input_event_listener *l;

  #define notify_mouse(callback) \
    c_list_for_each(input->event_listeners, l) { \
      if (l->type == INPUT_EVENT_LISTENER_MOUSE && ((struct c_input_event_listener_mouse *)l->listener)->callback) {\
        ((struct c_input_event_listener_mouse *)l->listener)->callback(event, l->userdata); \
      } \
    }

  switch (notifier) {
    case C_INPUT_NOTIFY_ON_MOUSE_MOVEMENT:  notify_mouse(on_mouse_movement); break;
    case C_INPUT_NOTIFY_ON_MOUSE_SCROLL:    notify_mouse(on_mouse_scroll); break;
    case C_INPUT_NOTIFY_ON_MOUSE_BUTTON:    notify_mouse(on_mouse_button); break;
    default: break;
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
      mouse_event.button = libinput_event_pointer_get_button(event);
      mouse_event.button_pressed = libinput_event_pointer_get_button_state(event);

      c_input_notify_mouse(input, &mouse_event, C_INPUT_NOTIFY_ON_MOUSE_BUTTON);
      break;

    // case LIBINPUT_EVENT_POINTER_AXIS:
    //   c_log(C_LOG_DEBUG, "%f", libinput_event_pointer_get_axis_value_discrete(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL));
    //   break;

    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
      mouse_event.axis = libinput_event_pointer_get_scroll_value(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
      mouse_event.axis_discrete = mouse_event.axis < 0 ? -1 : 1;
      mouse_event.axis_source = C_MOUSE_AXIS_SOURCE_WHEEL;

      c_input_notify_mouse(input, &mouse_event, C_INPUT_NOTIFY_ON_MOUSE_SCROLL);
      break;

    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
    default:
      return;
  }

}
static void handle_event_keyboard(struct c_input *input, struct libinput_event_keyboard *event, struct libinput_device *li_dev) {
  assert(event);

  struct xkb_state *state = get_xkb_state(input);
  if (!state) return;

  struct c_input_keyboard_event keyboard_event = {0};
  keyboard_event.key = libinput_event_keyboard_get_key(event);
  keyboard_event.pressed = libinput_event_keyboard_get_key_state(event);

  enum xkb_state_component changed = xkb_state_update_key(
    state, 
    keyboard_event.key + 8,
    keyboard_event.pressed ? XKB_KEY_DOWN : XKB_KEY_UP
  );

  if (changed & 
    (XKB_STATE_MODS_DEPRESSED | 
     XKB_STATE_MODS_LATCHED | 
     XKB_STATE_MODS_LOCKED | 
     XKB_STATE_LAYOUT_EFFECTIVE)) {
    keyboard_event.changed = 1;
  }

	keyboard_event.mods_depressed = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
	keyboard_event.mods_latched   = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
	keyboard_event.mods_locked    = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
	keyboard_event.group          = xkb_state_serialize_mods(state, XKB_STATE_LAYOUT_EFFECTIVE);;

  xkb_keysym_t keysym = xkb_state_key_get_one_sym(state, keyboard_event.key + 8);
  xkb_mod_mask_t mod_mask = xkb_state_serialize_mods(state, XKB_STATE_MODS_EFFECTIVE);
  char buffer[64] = {0};
  xkb_keysym_get_name(keysym, buffer, 64);

  if (keyboard_event.pressed == 1) {
    struct __input_shortcut_listener *sl;
    c_list_for_each(input->shortcut_listeners, sl) {
      if (sl->keysym == keysym && ((sl->mod_mask & mod_mask) == sl->mod_mask)) {
        sl->handler(sl->userdata);
        return;
      }
    }
  }

  c_input_notify_keyboard(input, &keyboard_event, C_INPUT_NOTIFY_ON_keyboard_KEY);
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
      handle_event_keyboard(input, libinput_event_get_keyboard_event(event), libinput_event_get_device(event));
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

void c_input_add_event_listener_keyboard(struct c_input *input, struct c_input_event_listener_keyboard *listener, void *userdata) {
  add_listener(input->event_listeners, INPUT_EVENT_LISTENER_keyboard, listener, sizeof(*listener), userdata);
}

void c_input_add_event_listener_mouse(struct c_input *input, struct c_input_event_listener_mouse *listener, void *userdata) {
  add_listener(input->event_listeners, INPUT_EVENT_LISTENER_MOUSE, listener, sizeof(*listener), userdata);
}

void c_input_add_shortcut_handler(struct c_input *input, uint32_t mod_mask, xkb_keysym_t keysym, 
                                  void(*handler)(void *userdata), void *userdata) {
  struct __input_shortcut_listener l = {
    .keysym = keysym,
    .mod_mask = mod_mask,
    .handler = handler,
    .userdata = userdata,
  };
  c_list_push(input->shortcut_listeners, &l, sizeof(l));
}

int c_input_init_xkb_state(struct c_input *input, struct xkb_rule_names *rule_names) {
  input->xkb.keymap = xkb_keymap_new_from_names2(input->xkb.ctx, rule_names, 
                                                        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!input->xkb.keymap) {
    c_log(C_LOG_ERROR, "xkb_keymap_new_from_names2 failed");
    return -1;
  }

  input->xkb.state = xkb_state_new(input->xkb.keymap);
  if (!input->xkb.state) {
    c_log(C_LOG_ERROR, "xkb_state_new failed");
    xkb_keymap_unref(input->xkb.keymap);
    input->xkb.keymap = NULL;
    return -1;
  }
  input->xkb.rule_names = rule_names;

  return 0;
}

int c_input_get_xkb_keymap_fd(struct c_input *input, int *fd) {
  struct xkb_keymap *keymap = get_xkb_keymap(input);
  if (!keymap) return -1;

  char *keymap_string = xkb_keymap_get_as_string2(keymap, XKB_KEYMAP_USE_ORIGINAL_FORMAT, XKB_KEYMAP_SERIALIZE_NO_FLAGS);

  size_t keymap_string_len = strlen(keymap_string) + 1;
  *fd = memfd_create("keymap", MFD_CLOEXEC);
  if (*fd < 0) {
    c_log_errno(C_LOG_ERROR, "memfd_create failed");
    keymap_string_len = -1;
    goto out;
  }

  write(*fd, keymap_string, keymap_string_len);

out:
  free(keymap_string);
  return keymap_string_len; 
}

void c_input_free(struct c_input *input) {
  if (input->event_listeners) {
    struct __input_event_listener *l;
    c_list_for_each(input->event_listeners, l) {
      free(l->listener);
    }
    c_list_destroy(input->event_listeners);
  }
  if (input->shortcut_listeners)
    c_list_destroy(input->shortcut_listeners);

  if (input->xkb.state)
    xkb_state_unref(input->xkb.state);

  if (input->xkb.keymap)
    xkb_keymap_unref(input->xkb.keymap);

  if (input->xkb.ctx)
    xkb_context_unref(input->xkb.ctx);

  
  free(input);
}

struct c_input *c_input_init(struct c_event_loop *loop, struct c_input_libinput_interface *libinput_interface) {
  struct c_input *input = calloc(1, sizeof(*input));
  if (!input) {
    c_log(C_LOG_ERROR, "calloc failed");
    goto error;
  }

  struct libinput_interface interface = {
    .open_restricted = libinput_interface->open_restricted,
    .close_restricted = libinput_interface->close_restricted,
  };

  struct udev *udev = udev_new();
  struct libinput *libinput = libinput_udev_create_context(&interface, libinput_interface->userdata, udev);
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

  input->event_listeners = c_list_new();
  input->shortcut_listeners = c_list_new();

  input->xkb.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!input->xkb.ctx) {
    c_log(C_LOG_ERROR, "xkb_context_new failed");
    goto error;
  }

  return input;

error:
  c_input_free(input);
  return NULL;
}

#ifndef CUTS_WAYLAND_H
#define CUTS_WAYLAND_H

#include "wayland/server.h"
#include <stdint.h>


 /* These errors are global and can be emitted in response to any
	server request. */
enum wl_display_error_enum {
  WL_DISPLAY_ERROR_INVALID_OBJECT = 0,
  WL_DISPLAY_ERROR_INVALID_METHOD = 1,
  WL_DISPLAY_ERROR_NO_MEMORY = 2,
  WL_DISPLAY_ERROR_IMPLEMENTATION = 3,
};

 /* These errors can be emitted in response to wl_shm requests. */
enum wl_shm_error_enum {
  WL_SHM_ERROR_INVALID_FORMAT = 0,
  WL_SHM_ERROR_INVALID_STRIDE = 1,
  WL_SHM_ERROR_INVALID_FD = 2,
};

 /* This describes the memory layout of an individual pixel.

	All renderers should support argb8888 and xrgb8888 but any other
	formats are optional and may not be supported by the particular
	renderer in use.

	The drm format codes match the macros defined in drm_fourcc.h, except
	argb8888 and xrgb8888. The formats actually supported by the compositor
	will be reported by the format event.

	For all wl_shm formats and unless specified in another protocol
	extension, pre-multiplied alpha is used for pixel values. */
enum wl_shm_format_enum {
  WL_SHM_FORMAT_ARGB8888 = 0,
  WL_SHM_FORMAT_XRGB8888 = 1,
  WL_SHM_FORMAT_C8 = 0x20203843,
  WL_SHM_FORMAT_RGB332 = 0x38424752,
  WL_SHM_FORMAT_BGR233 = 0x38524742,
  WL_SHM_FORMAT_XRGB4444 = 0x32315258,
  WL_SHM_FORMAT_XBGR4444 = 0x32314258,
  WL_SHM_FORMAT_RGBX4444 = 0x32315852,
  WL_SHM_FORMAT_BGRX4444 = 0x32315842,
  WL_SHM_FORMAT_ARGB4444 = 0x32315241,
  WL_SHM_FORMAT_ABGR4444 = 0x32314241,
  WL_SHM_FORMAT_RGBA4444 = 0x32314152,
  WL_SHM_FORMAT_BGRA4444 = 0x32314142,
  WL_SHM_FORMAT_XRGB1555 = 0x35315258,
  WL_SHM_FORMAT_XBGR1555 = 0x35314258,
  WL_SHM_FORMAT_RGBX5551 = 0x35315852,
  WL_SHM_FORMAT_BGRX5551 = 0x35315842,
  WL_SHM_FORMAT_ARGB1555 = 0x35315241,
  WL_SHM_FORMAT_ABGR1555 = 0x35314241,
  WL_SHM_FORMAT_RGBA5551 = 0x35314152,
  WL_SHM_FORMAT_BGRA5551 = 0x35314142,
  WL_SHM_FORMAT_RGB565 = 0x36314752,
  WL_SHM_FORMAT_BGR565 = 0x36314742,
  WL_SHM_FORMAT_RGB888 = 0x34324752,
  WL_SHM_FORMAT_BGR888 = 0x34324742,
  WL_SHM_FORMAT_XBGR8888 = 0x34324258,
  WL_SHM_FORMAT_RGBX8888 = 0x34325852,
  WL_SHM_FORMAT_BGRX8888 = 0x34325842,
  WL_SHM_FORMAT_ABGR8888 = 0x34324241,
  WL_SHM_FORMAT_RGBA8888 = 0x34324152,
  WL_SHM_FORMAT_BGRA8888 = 0x34324142,
  WL_SHM_FORMAT_XRGB2101010 = 0x30335258,
  WL_SHM_FORMAT_XBGR2101010 = 0x30334258,
  WL_SHM_FORMAT_RGBX1010102 = 0x30335852,
  WL_SHM_FORMAT_BGRX1010102 = 0x30335842,
  WL_SHM_FORMAT_ARGB2101010 = 0x30335241,
  WL_SHM_FORMAT_ABGR2101010 = 0x30334241,
  WL_SHM_FORMAT_RGBA1010102 = 0x30334152,
  WL_SHM_FORMAT_BGRA1010102 = 0x30334142,
  WL_SHM_FORMAT_YUYV = 0x56595559,
  WL_SHM_FORMAT_YVYU = 0x55595659,
  WL_SHM_FORMAT_UYVY = 0x59565955,
  WL_SHM_FORMAT_VYUY = 0x59555956,
  WL_SHM_FORMAT_AYUV = 0x56555941,
  WL_SHM_FORMAT_NV12 = 0x3231564e,
  WL_SHM_FORMAT_NV21 = 0x3132564e,
  WL_SHM_FORMAT_NV16 = 0x3631564e,
  WL_SHM_FORMAT_NV61 = 0x3136564e,
  WL_SHM_FORMAT_YUV410 = 0x39565559,
  WL_SHM_FORMAT_YVU410 = 0x39555659,
  WL_SHM_FORMAT_YUV411 = 0x31315559,
  WL_SHM_FORMAT_YVU411 = 0x31315659,
  WL_SHM_FORMAT_YUV420 = 0x32315559,
  WL_SHM_FORMAT_YVU420 = 0x32315659,
  WL_SHM_FORMAT_YUV422 = 0x36315559,
  WL_SHM_FORMAT_YVU422 = 0x36315659,
  WL_SHM_FORMAT_YUV444 = 0x34325559,
  WL_SHM_FORMAT_YVU444 = 0x34325659,
  WL_SHM_FORMAT_R8 = 0x20203852,
  WL_SHM_FORMAT_R16 = 0x20363152,
  WL_SHM_FORMAT_RG88 = 0x38384752,
  WL_SHM_FORMAT_GR88 = 0x38385247,
  WL_SHM_FORMAT_RG1616 = 0x32334752,
  WL_SHM_FORMAT_GR1616 = 0x32335247,
  WL_SHM_FORMAT_XRGB16161616F = 0x48345258,
  WL_SHM_FORMAT_XBGR16161616F = 0x48344258,
  WL_SHM_FORMAT_ARGB16161616F = 0x48345241,
  WL_SHM_FORMAT_ABGR16161616F = 0x48344241,
  WL_SHM_FORMAT_XYUV8888 = 0x56555958,
  WL_SHM_FORMAT_VUY888 = 0x34325556,
  WL_SHM_FORMAT_VUY101010 = 0x30335556,
  WL_SHM_FORMAT_Y210 = 0x30313259,
  WL_SHM_FORMAT_Y212 = 0x32313259,
  WL_SHM_FORMAT_Y216 = 0x36313259,
  WL_SHM_FORMAT_Y410 = 0x30313459,
  WL_SHM_FORMAT_Y412 = 0x32313459,
  WL_SHM_FORMAT_Y416 = 0x36313459,
  WL_SHM_FORMAT_XVYU2101010 = 0x30335658,
  WL_SHM_FORMAT_XVYU12_16161616 = 0x36335658,
  WL_SHM_FORMAT_XVYU16161616 = 0x38345658,
  WL_SHM_FORMAT_Y0L0 = 0x304c3059,
  WL_SHM_FORMAT_X0L0 = 0x304c3058,
  WL_SHM_FORMAT_Y0L2 = 0x324c3059,
  WL_SHM_FORMAT_X0L2 = 0x324c3058,
  WL_SHM_FORMAT_YUV420_8BIT = 0x38305559,
  WL_SHM_FORMAT_YUV420_10BIT = 0x30315559,
  WL_SHM_FORMAT_XRGB8888_A8 = 0x38415258,
  WL_SHM_FORMAT_XBGR8888_A8 = 0x38414258,
  WL_SHM_FORMAT_RGBX8888_A8 = 0x38415852,
  WL_SHM_FORMAT_BGRX8888_A8 = 0x38415842,
  WL_SHM_FORMAT_RGB888_A8 = 0x38413852,
  WL_SHM_FORMAT_BGR888_A8 = 0x38413842,
  WL_SHM_FORMAT_RGB565_A8 = 0x38413552,
  WL_SHM_FORMAT_BGR565_A8 = 0x38413542,
  WL_SHM_FORMAT_NV24 = 0x3432564e,
  WL_SHM_FORMAT_NV42 = 0x3234564e,
  WL_SHM_FORMAT_P210 = 0x30313250,
  WL_SHM_FORMAT_P010 = 0x30313050,
  WL_SHM_FORMAT_P012 = 0x32313050,
  WL_SHM_FORMAT_P016 = 0x36313050,
  WL_SHM_FORMAT_AXBXGXRX106106106106 = 0x30314241,
  WL_SHM_FORMAT_NV15 = 0x3531564e,
  WL_SHM_FORMAT_Q410 = 0x30313451,
  WL_SHM_FORMAT_Q401 = 0x31303451,
  WL_SHM_FORMAT_XRGB16161616 = 0x38345258,
  WL_SHM_FORMAT_XBGR16161616 = 0x38344258,
  WL_SHM_FORMAT_ARGB16161616 = 0x38345241,
  WL_SHM_FORMAT_ABGR16161616 = 0x38344241,
  WL_SHM_FORMAT_C1 = 0x20203143,
  WL_SHM_FORMAT_C2 = 0x20203243,
  WL_SHM_FORMAT_C4 = 0x20203443,
  WL_SHM_FORMAT_D1 = 0x20203144,
  WL_SHM_FORMAT_D2 = 0x20203244,
  WL_SHM_FORMAT_D4 = 0x20203444,
  WL_SHM_FORMAT_D8 = 0x20203844,
  WL_SHM_FORMAT_R1 = 0x20203152,
  WL_SHM_FORMAT_R2 = 0x20203252,
  WL_SHM_FORMAT_R4 = 0x20203452,
  WL_SHM_FORMAT_R10 = 0x20303152,
  WL_SHM_FORMAT_R12 = 0x20323152,
  WL_SHM_FORMAT_AVUY8888 = 0x59555641,
  WL_SHM_FORMAT_XVUY8888 = 0x59555658,
  WL_SHM_FORMAT_P030 = 0x30333050,
};

enum wl_data_offer_error_enum {
  WL_DATA_OFFER_ERROR_INVALID_FINISH = 0,
  WL_DATA_OFFER_ERROR_INVALID_ACTION_MASK = 1,
  WL_DATA_OFFER_ERROR_INVALID_ACTION = 2,
  WL_DATA_OFFER_ERROR_INVALID_OFFER = 3,
};

enum wl_data_source_error_enum {
  WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK = 0,
  WL_DATA_SOURCE_ERROR_INVALID_SOURCE = 1,
};

enum wl_data_device_error_enum {
  WL_DATA_DEVICE_ERROR_ROLE = 0,
  WL_DATA_DEVICE_ERROR_USED_SOURCE = 1,
};

 /* This is a bitmask of the available/preferred actions in a
	drag-and-drop operation.

	In the compositor, the selected action is a result of matching the
	actions offered by the source and destination sides.  "action" events
	with a "none" action will be sent to both source and destination if
	there is no match. All further checks will effectively happen on
	(source actions ∩ destination actions).

	In addition, compositors may also pick different actions in
	reaction to key modifiers being pressed. One common design that
	is used in major toolkits (and the behavior recommended for
	compositors) is:

	- If no modifiers are pressed, the first match (in bit order)
	  will be used.
	- Pressing Shift selects "move", if enabled in the mask.
	- Pressing Control selects "copy", if enabled in the mask.

	Behavior beyond that is considered implementation-dependent.
	Compositors may for example bind other modifiers (like Alt/Meta)
	or drags initiated with other buttons than BTN_LEFT to specific
	actions (e.g. "ask"). */
enum wl_data_device_manager_dnd_action_enum {
  WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE = 0,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY = 1,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE = 2,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK = 4,
};

enum wl_shell_error_enum {
  WL_SHELL_ERROR_ROLE = 0,
};

 /* These values are used to indicate which edge of a surface
	is being dragged in a resize operation. The server may
	use this information to adapt its behavior, e.g. choose
	an appropriate cursor image. */
enum wl_shell_surface_resize_enum {
  WL_SHELL_SURFACE_RESIZE_NONE = 0,
  WL_SHELL_SURFACE_RESIZE_TOP = 1,
  WL_SHELL_SURFACE_RESIZE_BOTTOM = 2,
  WL_SHELL_SURFACE_RESIZE_LEFT = 4,
  WL_SHELL_SURFACE_RESIZE_TOP_LEFT = 5,
  WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT = 6,
  WL_SHELL_SURFACE_RESIZE_RIGHT = 8,
  WL_SHELL_SURFACE_RESIZE_TOP_RIGHT = 9,
  WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT = 10,
};

 /* These flags specify details of the expected behaviour
	of transient surfaces. Used in the set_transient request. */
enum wl_shell_surface_transient_enum {
  WL_SHELL_SURFACE_TRANSIENT_INACTIVE = 0x1,
};

 /* Hints to indicate to the compositor how to deal with a conflict
	between the dimensions of the surface and the dimensions of the
	output. The compositor is free to ignore this parameter. */
enum wl_shell_surface_fullscreen_method_enum {
  WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT = 0,
  WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE = 1,
  WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER = 2,
  WL_SHELL_SURFACE_FULLSCREEN_METHOD_FILL = 3,
};

 /* These errors can be emitted in response to wl_surface requests. */
enum wl_surface_error_enum {
  WL_SURFACE_ERROR_INVALID_SCALE = 0,
  WL_SURFACE_ERROR_INVALID_TRANSFORM = 1,
  WL_SURFACE_ERROR_INVALID_SIZE = 2,
  WL_SURFACE_ERROR_INVALID_OFFSET = 3,
  WL_SURFACE_ERROR_DEFUNCT_ROLE_OBJECT = 4,
};

 /* This is a bitmask of capabilities this seat has; if a member is
	set, then it is present on the seat. */
enum wl_seat_capability_enum {
  WL_SEAT_CAPABILITY_POINTER = 1,
  WL_SEAT_CAPABILITY_KEYBOARD = 2,
  WL_SEAT_CAPABILITY_TOUCH = 4,
};

 /* These errors can be emitted in response to wl_seat requests. */
enum wl_seat_error_enum {
  WL_SEAT_ERROR_MISSING_CAPABILITY = 0,
};

enum wl_pointer_error_enum {
  WL_POINTER_ERROR_ROLE = 0,
};

 /* Describes the physical state of a button that produced the button
	event. */
enum wl_pointer_button_state_enum {
  WL_POINTER_BUTTON_STATE_RELEASED = 0,
  WL_POINTER_BUTTON_STATE_PRESSED = 1,
};

 /* Describes the axis types of scroll events. */
enum wl_pointer_axis_enum {
  WL_POINTER_AXIS_VERTICAL_SCROLL = 0,
  WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};

 /* Describes the source types for axis events. This indicates to the
	client how an axis event was physically generated; a client may
	adjust the user interface accordingly. For example, scroll events
	from a "finger" source may be in a smooth coordinate space with
	kinetic scrolling whereas a "wheel" source may be in discrete steps
	of a number of lines.

	The "continuous" axis source is a device generating events in a
	continuous coordinate space, but using something other than a
	finger. One example for this source is button-based scrolling where
	the vertical motion of a device is converted to scroll events while
	a button is held down.

	The "wheel tilt" axis source indicates that the actual device is a
	wheel but the scroll event is not caused by a rotation but a
	(usually sideways) tilt of the wheel. */
enum wl_pointer_axis_source_enum {
  WL_POINTER_AXIS_SOURCE_WHEEL = 0,
  WL_POINTER_AXIS_SOURCE_FINGER = 1,
  WL_POINTER_AXIS_SOURCE_CONTINUOUS = 2,
  WL_POINTER_AXIS_SOURCE_WHEEL_TILT = 3,
};

 /* This specifies the direction of the physical motion that caused a
	wl_pointer.axis event, relative to the wl_pointer.axis direction. */
enum wl_pointer_axis_relative_direction_enum {
  WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL = 0,
  WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED = 1,
};

 /* This specifies the format of the keymap provided to the
	client with the wl_keyboard.keymap event. */
enum wl_keyboard_keymap_format_enum {
  WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP = 0,
  WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 = 1,
};

 /* Describes the physical state of a key that produced the key event.

	Since version 10, the key can be in a "repeated" pseudo-state which
	means the same as "pressed", but is used to signal repetition in the
	key event.

	The key may only enter the repeated state after entering the pressed
	state and before entering the released state. This event may be
	generated multiple times while the key is down. */
enum wl_keyboard_key_state_enum {
  WL_KEYBOARD_KEY_STATE_RELEASED = 0,
  WL_KEYBOARD_KEY_STATE_PRESSED = 1,
  WL_KEYBOARD_KEY_STATE_REPEATED = 2,
};

 /* This enumeration describes how the physical
	pixels on an output are laid out. */
enum wl_output_subpixel_enum {
  WL_OUTPUT_SUBPIXEL_UNKNOWN = 0,
  WL_OUTPUT_SUBPIXEL_NONE = 1,
  WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB = 2,
  WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR = 3,
  WL_OUTPUT_SUBPIXEL_VERTICAL_RGB = 4,
  WL_OUTPUT_SUBPIXEL_VERTICAL_BGR = 5,
};

 /* This describes transformations that clients and compositors apply to
	buffer contents.

	The flipped values correspond to an initial flip around a
	vertical axis followed by rotation.

	The purpose is mainly to allow clients to render accordingly and
	tell the compositor, so that for fullscreen surfaces, the
	compositor will still be able to scan out directly from client
	surfaces. */
enum wl_output_transform_enum {
  WL_OUTPUT_TRANSFORM_NORMAL = 0,
  WL_OUTPUT_TRANSFORM_90 = 1,
  WL_OUTPUT_TRANSFORM_180 = 2,
  WL_OUTPUT_TRANSFORM_270 = 3,
  WL_OUTPUT_TRANSFORM_FLIPPED = 4,
  WL_OUTPUT_TRANSFORM_FLIPPED_90 = 5,
  WL_OUTPUT_TRANSFORM_FLIPPED_180 = 6,
  WL_OUTPUT_TRANSFORM_FLIPPED_270 = 7,
};

 /* These flags describe properties of an output mode.
	They are used in the flags bitfield of the mode event. */
enum wl_output_mode_enum {
  WL_OUTPUT_MODE_CURRENT = 0x1,
  WL_OUTPUT_MODE_PREFERRED = 0x2,
};

enum wl_subcompositor_error_enum {
  WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE = 0,
  WL_SUBCOMPOSITOR_ERROR_BAD_PARENT = 1,
};

enum wl_subsurface_error_enum {
  WL_SUBSURFACE_ERROR_BAD_SURFACE = 0,
};

 /* The error event is sent out when a fatal (non-recoverable)
	error has occurred.  The object_id argument is the object
	where the error occurred, most often in response to a request
	to that object.  The code identifies the error and is defined
	by the object interface.  As such, each interface defines its
	own set of error codes.  The message is a brief description
	of the error, for (debugging) convenience. */
C_WL_EVENT wl_display_error(struct c_wl_connection *conn, c_wl_object_id wl_display, c_wl_object_id object_id, c_wl_uint code, c_wl_string message);

 /* This event is used internally by the object ID management
	logic. When a client deletes an object that it had created,
	the server will send this event to acknowledge that it has
	seen the delete request. When the client receives this event,
	it will know that it can safely reuse the object ID. */
C_WL_EVENT wl_display_delete_id(struct c_wl_connection *conn, c_wl_object_id wl_display, c_wl_uint id);

   /* The sync request asks the server to emit the 'done' event
	on the returned wl_callback object.  Since requests are
	handled in-order and events are delivered in-order, this can
	be used as a barrier to ensure all previous requests and the
	resulting events have been handled.

	The object returned by this request will be destroyed by the
	compositor after the callback is fired and as such the client must not
	attempt to use it after that point.

	The callback_data passed in the callback is undefined and should be ignored.

    @arg1: c_wl_new_id wl_callback
   */
C_WL_REQUEST wl_display_sync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request creates a registry object that allows the client
	to list and bind the global objects available from the
	compositor.

	It should be noted that the server side resources consumed in
	response to a get_registry request can only be released when the
	client disconnects, not when the client side proxy is destroyed.
	Therefore, clients should invoke get_registry as infrequently as
	possible to avoid wasting memory.

    @arg1: c_wl_new_id wl_registry
   */
C_WL_REQUEST wl_display_get_registry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Notify the client of global objects.

	The event notifies the client that a global object with
	the given name is now available, and it implements the
	given version of the given interface. */
C_WL_EVENT wl_registry_global(struct c_wl_connection *conn, c_wl_object_id wl_registry, c_wl_uint name, c_wl_string interface, c_wl_uint version);

 /* Notify the client of removed global objects.

	This event notifies the client that the global identified
	by name is no longer available.  If the client bound to
	the global using the bind request, the client should now
	destroy that object.

	The object remains valid and requests to the object will be
	ignored until the client destroys it, to avoid races between
	the global going away and a client sending a request to it. */
C_WL_EVENT wl_registry_global_remove(struct c_wl_connection *conn, c_wl_object_id wl_registry, c_wl_uint name);

   /* Binds a new, client-created object to the server using the
	specified name as the identifier.

    @arg1: c_wl_uint name
    @arg2: c_wl_string interface
    @arg3: c_wl_uint version
    @arg4: c_wl_new_id id
   */
C_WL_REQUEST wl_registry_bind(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Notify the client when the related request is done. */
C_WL_EVENT wl_callback_done(struct c_wl_connection *conn, c_wl_object_id wl_callback, c_wl_uint callback_data);


   /* Ask the compositor to create a new surface.

    @arg1: c_wl_new_id wl_surface
   */
C_WL_REQUEST wl_compositor_create_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Ask the compositor to create a new region.

    @arg1: c_wl_new_id wl_region
   */
C_WL_REQUEST wl_compositor_create_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* Create a wl_buffer object from the pool.

	The buffer is created offset bytes into the pool and has
	width and height as specified.  The stride argument specifies
	the number of bytes from the beginning of one row to the beginning
	of the next.  The format is the pixel format of the buffer and
	must be one of those advertised through the wl_shm.format event.

	A buffer will keep a reference to the pool it was created from
	so it is valid to destroy the pool immediately after creating
	a buffer from it.

    @arg1: c_wl_new_id wl_buffer
    @arg2: c_wl_int offset
    @arg3: c_wl_int width
    @arg4: c_wl_int height
    @arg5: c_wl_int stride
    @arg6: enum wl_shm_format_enum format
   */
C_WL_REQUEST wl_shm_pool_create_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Destroy the shared memory pool.

	The mmapped memory will be released when all
	buffers that have been created from this pool
	are gone. */
C_WL_REQUEST wl_shm_pool_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request will cause the server to remap the backing memory
	for the pool from the file descriptor passed when the pool was
	created, but using the new size.  This request can only be
	used to make the pool bigger.

	This request only changes the amount of bytes that are mmapped
	by the server and does not touch the file corresponding to the
	file descriptor passed at creation time. It is the client's
	responsibility to ensure that the file is at least as big as
	the new pool size.

    @arg1: c_wl_int size
   */
C_WL_REQUEST wl_shm_pool_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Informs the client about a valid pixel format that
	can be used for buffers. Known formats include
	argb8888 and xrgb8888. */
C_WL_EVENT wl_shm_format(struct c_wl_connection *conn, c_wl_object_id wl_shm, enum wl_shm_format_enum format);

   /* Create a new wl_shm_pool object.

	The pool can be used to create shared memory based buffer
	objects.  The server will mmap size bytes of the passed file
	descriptor, to use as backing memory for the pool.

    @arg1: c_wl_new_id wl_shm_pool
    @arg2: c_wl_fd fd
    @arg3: c_wl_int size
   */
C_WL_REQUEST wl_shm_create_pool(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Using this request a client can tell the server that it is not going to
	use the shm object anymore.

	Objects created via this interface remain unaffected. */
C_WL_REQUEST wl_shm_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Sent when this wl_buffer is no longer used by the compositor.

	For more information on when release events may or may not be sent,
	and what consequences it has, please see the description of
	wl_surface.attach.

	If a client receives a release event before the frame callback
	requested in the same wl_surface.commit that attaches this
	wl_buffer to a surface, then the client is immediately free to
	reuse the buffer and its backing storage, and does not need a
	second buffer for the next surface content update. Typically
	this is possible, when the compositor maintains a copy of the
	wl_surface contents, e.g. as a GL texture. This is an important
	optimization for GL(ES) compositors with wl_shm clients. */
C_WL_EVENT wl_buffer_release(struct c_wl_connection *conn, c_wl_object_id wl_buffer);

   /* Destroy a buffer. If and how you need to release the backing
	storage is defined by the buffer factory interface.

	For possible side-effects to a surface, see wl_surface.attach. */
C_WL_REQUEST wl_buffer_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Sent immediately after creating the wl_data_offer object.  One
	event per offered mime type. */
C_WL_EVENT wl_data_offer_offer(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, c_wl_string mime_type);

 /* This event indicates the actions offered by the data source. It
	will be sent immediately after creating the wl_data_offer object,
	or anytime the source side changes its offered actions through
	wl_data_source.set_actions. */
C_WL_EVENT wl_data_offer_source_actions(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum source_actions);

 /* This event indicates the action selected by the compositor after
	matching the source/destination side actions. Only one action (or
	none) will be offered here.

	This event can be emitted multiple times during the drag-and-drop
	operation in response to destination side action changes through
	wl_data_offer.set_actions.

	This event will no longer be emitted after wl_data_device.drop
	happened on the drag-and-drop destination, the client must
	honor the last action received, or the last preferred one set
	through wl_data_offer.set_actions when handling an "ask" action.

	Compositors may also change the selected action on the fly, mainly
	in response to keyboard modifier changes during the drag-and-drop
	operation.

	The most recent action received is always the valid one. Prior to
	receiving wl_data_device.drop, the chosen action may change (e.g.
	due to keyboard modifiers being pressed). At the time of receiving
	wl_data_device.drop the drag-and-drop destination must honor the
	last action received.

	Action changes may still happen after wl_data_device.drop,
	especially on "ask" actions, where the drag-and-drop destination
	may choose another action afterwards. Action changes happening
	at this stage are always the result of inter-client negotiation, the
	compositor shall no longer be able to induce a different action.

	Upon "ask" actions, it is expected that the drag-and-drop destination
	may potentially choose a different action and/or mime type,
	based on wl_data_offer.source_actions and finally chosen by the
	user (e.g. popping up a menu with the available options). The
	final wl_data_offer.set_actions and wl_data_offer.accept requests
	must happen before the call to wl_data_offer.finish. */
C_WL_EVENT wl_data_offer_action(struct c_wl_connection *conn, c_wl_object_id wl_data_offer, enum wl_data_device_manager_dnd_action_enum dnd_action);

   /* Indicate that the client can accept the given mime type, or
	NULL for not accepted.

	For objects of version 2 or older, this request is used by the
	client to give feedback whether the client can receive the given
	mime type, or NULL if none is accepted; the feedback does not
	determine whether the drag-and-drop operation succeeds or not.

	For objects of version 3 or newer, this request determines the
	final result of the drag-and-drop operation. If the end result
	is that no mime types were accepted, the drag-and-drop operation
	will be cancelled and the corresponding drag source will receive
	wl_data_source.cancelled. Clients may still use this event in
	conjunction with wl_data_source.action for feedback.

    @arg1: c_wl_uint serial
    @arg2: c_wl_string mime_type
   */
C_WL_REQUEST wl_data_offer_accept(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* To transfer the offered data, the client issues this request
	and indicates the mime type it wants to receive.  The transfer
	happens through the passed file descriptor (typically created
	with the pipe system call).  The source client writes the data
	in the mime type representation requested and then closes the
	file descriptor.

	The receiving client reads from the read end of the pipe until
	EOF and then closes its end, at which point the transfer is
	complete.

	This request may happen multiple times for different mime types,
	both before and after wl_data_device.drop. Drag-and-drop destination
	clients may preemptively fetch data or examine it more closely to
	determine acceptance.

    @arg1: c_wl_string mime_type
    @arg2: c_wl_fd fd
   */
C_WL_REQUEST wl_data_offer_receive(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Destroy the data offer. */
C_WL_REQUEST wl_data_offer_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Notifies the compositor that the drag destination successfully
	finished the drag-and-drop operation.

	Upon receiving this request, the compositor will emit
	wl_data_source.dnd_finished on the drag source client.

	It is a client error to perform other requests than
	wl_data_offer.destroy after this one. It is also an error to perform
	this request after a NULL mime type has been set in
	wl_data_offer.accept or no action was received through
	wl_data_offer.action.

	If wl_data_offer.finish request is received for a non drag and drop
	operation, the invalid_finish protocol error is raised. */
C_WL_REQUEST wl_data_offer_finish(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Sets the actions that the destination side client supports for
	this operation. This request may trigger the emission of
	wl_data_source.action and wl_data_offer.action events if the compositor
	needs to change the selected action.

	This request can be called multiple times throughout the
	drag-and-drop operation, typically in response to wl_data_device.enter
	or wl_data_device.motion events.

	This request determines the final result of the drag-and-drop
	operation. If the end result is that no action is accepted,
	the drag source will receive wl_data_source.cancelled.

	The dnd_actions argument must contain only values expressed in the
	wl_data_device_manager.dnd_actions enum, and the preferred_action
	argument must only contain one of those values set, otherwise it
	will result in a protocol error.

	While managing an "ask" action, the destination drag-and-drop client
	may perform further wl_data_offer.receive requests, and is expected
	to perform one last wl_data_offer.set_actions request with a preferred
	action other than "ask" (and optionally wl_data_offer.accept) before
	requesting wl_data_offer.finish, in order to convey the action selected
	by the user. If the preferred action is not in the
	wl_data_offer.source_actions mask, an error will be raised.

	If the "ask" action is dismissed (e.g. user cancellation), the client
	is expected to perform wl_data_offer.destroy right away.

	This request can only be made on drag-and-drop offers, a protocol error
	will be raised otherwise.

    @arg1: enum wl_data_device_manager_dnd_action_enum dnd_actions
    @arg2: enum wl_data_device_manager_dnd_action_enum preferred_action
   */
C_WL_REQUEST wl_data_offer_set_actions(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Sent when a target accepts pointer_focus or motion events.  If
	a target does not accept any of the offered types, type is NULL.

	Used for feedback during drag-and-drop. */
C_WL_EVENT wl_data_source_target(struct c_wl_connection *conn, c_wl_object_id wl_data_source, c_wl_string mime_type);

 /* Request for data from the client.  Send the data as the
	specified mime type over the passed file descriptor, then
	close it. */
C_WL_EVENT wl_data_source_send(struct c_wl_connection *conn, c_wl_object_id wl_data_source, c_wl_string mime_type, c_wl_fd fd);

 /* This data source is no longer valid. There are several reasons why
	this could happen:

	- The data source has been replaced by another data source.
	- The drag-and-drop operation was performed, but the drop destination
	  did not accept any of the mime types offered through
	  wl_data_source.target.
	- The drag-and-drop operation was performed, but the drop destination
	  did not select any of the actions present in the mask offered through
	  wl_data_source.action.
	- The drag-and-drop operation was performed but didn't happen over a
	  surface.
	- The compositor cancelled the drag-and-drop operation (e.g. compositor
	  dependent timeouts to avoid stale drag-and-drop transfers).

	The client should clean up and destroy this data source.

	For objects of version 2 or older, wl_data_source.cancelled will
	only be emitted if the data source was replaced by another data
	source. */
C_WL_EVENT wl_data_source_cancelled(struct c_wl_connection *conn, c_wl_object_id wl_data_source);

 /* The user performed the drop action. This event does not indicate
	acceptance, wl_data_source.cancelled may still be emitted afterwards
	if the drop destination does not accept any mime type.

	However, this event might however not be received if the compositor
	cancelled the drag-and-drop operation before this event could happen.

	Note that the data_source may still be used in the future and should
	not be destroyed here. */
C_WL_EVENT wl_data_source_dnd_drop_performed(struct c_wl_connection *conn, c_wl_object_id wl_data_source);

 /* The drop destination finished interoperating with this data
	source, so the client is now free to destroy this data source and
	free all associated data.

	If the action used to perform the operation was "move", the
	source can now delete the transferred data. */
C_WL_EVENT wl_data_source_dnd_finished(struct c_wl_connection *conn, c_wl_object_id wl_data_source);

 /* This event indicates the action selected by the compositor after
	matching the source/destination side actions. Only one action (or
	none) will be offered here.

	This event can be emitted multiple times during the drag-and-drop
	operation, mainly in response to destination side changes through
	wl_data_offer.set_actions, and as the data device enters/leaves
	surfaces.

	It is only possible to receive this event after
	wl_data_source.dnd_drop_performed if the drag-and-drop operation
	ended in an "ask" action, in which case the final wl_data_source.action
	event will happen immediately before wl_data_source.dnd_finished.

	Compositors may also change the selected action on the fly, mainly
	in response to keyboard modifier changes during the drag-and-drop
	operation.

	The most recent action received is always the valid one. The chosen
	action may change alongside negotiation (e.g. an "ask" action can turn
	into a "move" operation), so the effects of the final action must
	always be applied in wl_data_offer.dnd_finished.

	Clients can trigger cursor surface changes from this point, so
	they reflect the current action. */
C_WL_EVENT wl_data_source_action(struct c_wl_connection *conn, c_wl_object_id wl_data_source, enum wl_data_device_manager_dnd_action_enum dnd_action);

   /* This request adds a mime type to the set of mime types
	advertised to targets.  Can be called several times to offer
	multiple types.

    @arg1: c_wl_string mime_type
   */
C_WL_REQUEST wl_data_source_offer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Destroy the data source. */
C_WL_REQUEST wl_data_source_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Sets the actions that the source side client supports for this
	operation. This request may trigger wl_data_source.action and
	wl_data_offer.action events if the compositor needs to change the
	selected action.

	The dnd_actions argument must contain only values expressed in the
	wl_data_device_manager.dnd_actions enum, otherwise it will result
	in a protocol error.

	This request must be made once only, and can only be made on sources
	used in drag-and-drop, so it must be performed before
	wl_data_device.start_drag. Attempting to use the source other than
	for drag-and-drop will raise a protocol error.

    @arg1: enum wl_data_device_manager_dnd_action_enum dnd_actions
   */
C_WL_REQUEST wl_data_source_set_actions(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* The data_offer event introduces a new wl_data_offer object,
	which will subsequently be used in either the
	data_device.enter event (for drag-and-drop) or the
	data_device.selection event (for selections).  Immediately
	following the data_device.data_offer event, the new data_offer
	object will send out data_offer.offer events to describe the
	mime types it offers. */
C_WL_EVENT wl_data_device_data_offer(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_new_id wl_data_offer);

 /* This event is sent when an active drag-and-drop pointer enters
	a surface owned by the client.  The position of the pointer at
	enter time is provided by the x and y arguments, in surface-local
	coordinates. */
C_WL_EVENT wl_data_device_enter(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_fixed x, c_wl_fixed y, c_wl_object_id wl_data_offer);

 /* This event is sent when the drag-and-drop pointer leaves the
	surface and the session ends.  The client must destroy the
	wl_data_offer introduced at enter time at this point. */
C_WL_EVENT wl_data_device_leave(struct c_wl_connection *conn, c_wl_object_id wl_data_device);

 /* This event is sent when the drag-and-drop pointer moves within
	the currently focused surface. The new position of the pointer
	is provided by the x and y arguments, in surface-local
	coordinates. */
C_WL_EVENT wl_data_device_motion(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_uint time, c_wl_fixed x, c_wl_fixed y);

 /* The event is sent when a drag-and-drop operation is ended
	because the implicit grab is removed.

	The drag-and-drop destination is expected to honor the last action
	received through wl_data_offer.action, if the resulting action is
	"copy" or "move", the destination can still perform
	wl_data_offer.receive requests, and is expected to end all
	transfers with a wl_data_offer.finish request.

	If the resulting action is "ask", the action will not be considered
	final. The drag-and-drop destination is expected to perform one last
	wl_data_offer.set_actions request, or wl_data_offer.destroy in order
	to cancel the operation. */
C_WL_EVENT wl_data_device_drop(struct c_wl_connection *conn, c_wl_object_id wl_data_device);

 /* The selection event is sent out to notify the client of a new
	wl_data_offer for the selection for this device.  The
	data_device.data_offer and the data_offer.offer events are
	sent out immediately before this event to introduce the data
	offer object.  The selection event is sent to a client
	immediately before receiving keyboard focus and when a new
	selection is set while the client has keyboard focus.  The
	data_offer is valid until a new data_offer or NULL is received
	or until the client loses keyboard focus.  Switching surface with
	keyboard focus within the same client doesn't mean a new selection
	will be sent.  The client must destroy the previous selection
	data_offer, if any, upon receiving this event. */
C_WL_EVENT wl_data_device_selection(struct c_wl_connection *conn, c_wl_object_id wl_data_device, c_wl_object_id wl_data_offer);

   /* This request asks the compositor to start a drag-and-drop
	operation on behalf of the client.

	The source argument is the data source that provides the data
	for the eventual data transfer. If source is NULL, enter, leave
	and motion events are sent only to the client that initiated the
	drag and the client is expected to handle the data passing
	internally. If source is destroyed, the drag-and-drop session will be
	cancelled.

	The origin surface is the surface where the drag originates and
	the client must have an active implicit grab that matches the
	serial.

	The icon surface is an optional (can be NULL) surface that
	provides an icon to be moved around with the cursor.  Initially,
	the top-left corner of the icon surface is placed at the cursor
	hotspot, but subsequent wl_surface.offset requests can move the
	relative position. Attach requests must be confirmed with
	wl_surface.commit as usual. The icon surface is given the role of
	a drag-and-drop icon. If the icon surface already has another role,
	it raises a protocol error.

	The input region is ignored for wl_surfaces with the role of a
	drag-and-drop icon.

	The given source may not be used in any further set_selection or
	start_drag requests. Attempting to reuse a previously-used source
	may send a used_source error.

    @arg1: c_wl_object_id wl_data_source
    @arg2: c_wl_object_id wl_surface
    @arg3: c_wl_object_id wl_surface
    @arg4: c_wl_uint serial
   */
C_WL_REQUEST wl_data_device_start_drag(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request asks the compositor to set the selection
	to the data from the source on behalf of the client.

	To unset the selection, set the source to NULL.

	The given source may not be used in any further set_selection or
	start_drag requests. Attempting to reuse a previously-used source
	may send a used_source error.

    @arg1: c_wl_object_id wl_data_source
    @arg2: c_wl_uint serial
   */
C_WL_REQUEST wl_data_device_set_selection(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request destroys the data device. */
C_WL_REQUEST wl_data_device_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* Create a new data source.

    @arg1: c_wl_new_id wl_data_source
   */
C_WL_REQUEST wl_data_device_manager_create_data_source(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Create a new data device for a given seat.

    @arg1: c_wl_new_id wl_data_device
    @arg2: c_wl_object_id wl_seat
   */
C_WL_REQUEST wl_data_device_manager_get_data_device(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* Create a shell surface for an existing surface. This gives
	the wl_surface the role of a shell surface. If the wl_surface
	already has another role, it raises a protocol error.

	Only one shell surface can be associated with a given surface.

    @arg1: c_wl_new_id wl_shell_surface
    @arg2: c_wl_object_id wl_surface
   */
C_WL_REQUEST wl_shell_get_shell_surface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Ping a client to check if it is receiving events and sending
	requests. A client is expected to reply with a pong request. */
C_WL_EVENT wl_shell_surface_ping(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface, c_wl_uint serial);

 /* The configure event asks the client to resize its surface.

	The size is a hint, in the sense that the client is free to
	ignore it if it doesn't resize, pick a smaller size (to
	satisfy aspect ratio or resize in steps of NxM pixels).

	The edges parameter provides a hint about how the surface
	was resized. The client may use this information to decide
	how to adjust its content to the new size (e.g. a scrolling
	area might adjust its content position to leave the viewable
	content unmoved).

	The client is free to dismiss all but the last configure
	event it received.

	The width and height arguments specify the size of the window
	in surface-local coordinates. */
C_WL_EVENT wl_shell_surface_configure(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface, enum wl_shell_surface_resize_enum edges, c_wl_int width, c_wl_int height);

 /* The popup_done event is sent out when a popup grab is broken,
	that is, when the user clicks a surface that doesn't belong
	to the client owning the popup surface. */
C_WL_EVENT wl_shell_surface_popup_done(struct c_wl_connection *conn, c_wl_object_id wl_shell_surface);

   /* A client must respond to a ping event with a pong request or
	the client may be deemed unresponsive.

    @arg1: c_wl_uint serial
   */
C_WL_REQUEST wl_shell_surface_pong(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Start a pointer-driven move of the surface.

	This request must be used in response to a button press event.
	The server may ignore move requests depending on the state of
	the surface (e.g. fullscreen or maximized).

    @arg1: c_wl_object_id wl_seat
    @arg2: c_wl_uint serial
   */
C_WL_REQUEST wl_shell_surface_move(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Start a pointer-driven resizing of the surface.

	This request must be used in response to a button press event.
	The server may ignore resize requests depending on the state of
	the surface (e.g. fullscreen or maximized).

    @arg1: c_wl_object_id wl_seat
    @arg2: c_wl_uint serial
    @arg3: enum wl_shell_surface_resize_enum edges
   */
C_WL_REQUEST wl_shell_surface_resize(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Map the surface as a toplevel surface.

	A toplevel surface is not fullscreen, maximized or transient. */
C_WL_REQUEST wl_shell_surface_set_toplevel(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Map the surface relative to an existing surface.

	The x and y arguments specify the location of the upper left
	corner of the surface relative to the upper left corner of the
	parent surface, in surface-local coordinates.

	The flags argument controls details of the transient behaviour.

    @arg1: c_wl_object_id wl_surface
    @arg2: c_wl_int x
    @arg3: c_wl_int y
    @arg4: enum wl_shell_surface_transient_enum flags
   */
C_WL_REQUEST wl_shell_surface_set_transient(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Map the surface as a fullscreen surface.

	If an output parameter is given then the surface will be made
	fullscreen on that output. If the client does not specify the
	output then the compositor will apply its policy - usually
	choosing the output on which the surface has the biggest surface
	area.

	The client may specify a method to resolve a size conflict
	between the output size and the surface size - this is provided
	through the method parameter.

	The framerate parameter is used only when the method is set
	to "driver", to indicate the preferred framerate. A value of 0
	indicates that the client does not care about framerate.  The
	framerate is specified in mHz, that is framerate of 60000 is 60Hz.

	A method of "scale" or "driver" implies a scaling operation of
	the surface, either via a direct scaling operation or a change of
	the output mode. This will override any kind of output scaling, so
	that mapping a surface with a buffer size equal to the mode can
	fill the screen independent of buffer_scale.

	A method of "fill" means we don't scale up the buffer, however
	any output scale is applied. This means that you may run into
	an edge case where the application maps a buffer with the same
	size of the output mode but buffer_scale 1 (thus making a
	surface larger than the output). In this case it is allowed to
	downscale the results to fit the screen.

	The compositor must reply to this request with a configure event
	with the dimensions for the output on which the surface will
	be made fullscreen.

    @arg1: enum wl_shell_surface_fullscreen_method_enum method
    @arg2: c_wl_uint framerate
    @arg3: c_wl_object_id wl_output
   */
C_WL_REQUEST wl_shell_surface_set_fullscreen(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Map the surface as a popup.

	A popup surface is a transient surface with an added pointer
	grab.

	An existing implicit grab will be changed to owner-events mode,
	and the popup grab will continue after the implicit grab ends
	(i.e. releasing the mouse button does not cause the popup to
	be unmapped).

	The popup grab continues until the window is destroyed or a
	mouse button is pressed in any other client's window. A click
	in any of the client's surfaces is reported as normal, however,
	clicks in other clients' surfaces will be discarded and trigger
	the callback.

	The x and y arguments specify the location of the upper left
	corner of the surface relative to the upper left corner of the
	parent surface, in surface-local coordinates.

    @arg1: c_wl_object_id wl_seat
    @arg2: c_wl_uint serial
    @arg3: c_wl_object_id wl_surface
    @arg4: c_wl_int x
    @arg5: c_wl_int y
    @arg6: enum wl_shell_surface_transient_enum flags
   */
C_WL_REQUEST wl_shell_surface_set_popup(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Map the surface as a maximized surface.

	If an output parameter is given then the surface will be
	maximized on that output. If the client does not specify the
	output then the compositor will apply its policy - usually
	choosing the output on which the surface has the biggest surface
	area.

	The compositor will reply with a configure event telling
	the expected new surface size. The operation is completed
	on the next buffer attach to this surface.

	A maximized surface typically fills the entire output it is
	bound to, except for desktop elements such as panels. This is
	the main difference between a maximized shell surface and a
	fullscreen shell surface.

	The details depend on the compositor implementation.

    @arg1: c_wl_object_id wl_output
   */
C_WL_REQUEST wl_shell_surface_set_maximized(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Set a short title for the surface.

	This string may be used to identify the surface in a task bar,
	window list, or other user interface elements provided by the
	compositor.

	The string must be encoded in UTF-8.

    @arg1: c_wl_string title
   */
C_WL_REQUEST wl_shell_surface_set_title(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Set a class for the surface.

	The surface class identifies the general class of applications
	to which the surface belongs. A common convention is to use the
	file name (or the full path if it is a non-standard location) of
	the application's .desktop file as the class.

    @arg1: c_wl_string class_
   */
C_WL_REQUEST wl_shell_surface_set_class(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* This is emitted whenever a surface's creation, movement, or resizing
	results in some part of it being within the scanout region of an
	output.

	Note that a surface may be overlapping with zero or more outputs. */
C_WL_EVENT wl_surface_enter(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_object_id wl_output);

 /* This is emitted whenever a surface's creation, movement, or resizing
	results in it no longer having any part of it within the scanout region
	of an output.

	Clients should not use the number of outputs the surface is on for frame
	throttling purposes. The surface might be hidden even if no leave event
	has been sent, and the compositor might expect new surface content
	updates even if no enter event has been sent. The frame event should be
	used instead. */
C_WL_EVENT wl_surface_leave(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_object_id wl_output);

 /* This event indicates the preferred buffer scale for this surface. It is
	sent whenever the compositor's preference changes.

	Before receiving this event the preferred buffer scale for this surface
	is 1.

	It is intended that scaling aware clients use this event to scale their
	content and use wl_surface.set_buffer_scale to indicate the scale they
	have rendered with. This allows clients to supply a higher detail
	buffer.

	The compositor shall emit a scale value greater than 0. */
C_WL_EVENT wl_surface_preferred_buffer_scale(struct c_wl_connection *conn, c_wl_object_id wl_surface, c_wl_int factor);

 /* This event indicates the preferred buffer transform for this surface.
	It is sent whenever the compositor's preference changes.

	Before receiving this event the preferred buffer transform for this
	surface is normal.

	Applying this transformation to the surface buffer contents and using
	wl_surface.set_buffer_transform might allow the compositor to use the
	surface buffer more efficiently. */
C_WL_EVENT wl_surface_preferred_buffer_transform(struct c_wl_connection *conn, c_wl_object_id wl_surface, enum wl_output_transform_enum transform);

   /* Deletes the surface and invalidates its object ID. */
C_WL_REQUEST wl_surface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Set a buffer as the content of this surface.

	The new size of the surface is calculated based on the buffer
	size transformed by the inverse buffer_transform and the
	inverse buffer_scale. This means that at commit time the supplied
	buffer size must be an integer multiple of the buffer_scale. If
	that's not the case, an invalid_size error is sent.

	The x and y arguments specify the location of the new pending
	buffer's upper left corner, relative to the current buffer's upper
	left corner, in surface-local coordinates. In other words, the
	x and y, combined with the new surface size define in which
	directions the surface's size changes. Setting anything other than 0
	as x and y arguments is discouraged, and should instead be replaced
	with using the separate wl_surface.offset request.

	When the bound wl_surface version is 5 or higher, passing any
	non-zero x or y is a protocol violation, and will result in an
	'invalid_offset' error being raised. The x and y arguments are ignored
	and do not change the pending state. To achieve equivalent semantics,
	use wl_surface.offset.

	Surface contents are double-buffered state, see wl_surface.commit.

	The initial surface contents are void; there is no content.
	wl_surface.attach assigns the given wl_buffer as the pending
	wl_buffer. wl_surface.commit makes the pending wl_buffer the new
	surface contents, and the size of the surface becomes the size
	calculated from the wl_buffer, as described above. After commit,
	there is no pending buffer until the next attach.

	Committing a pending wl_buffer allows the compositor to read the
	pixels in the wl_buffer. The compositor may access the pixels at
	any time after the wl_surface.commit request. When the compositor
	will not access the pixels anymore, it will send the
	wl_buffer.release event. Only after receiving wl_buffer.release,
	the client may reuse the wl_buffer. A wl_buffer that has been
	attached and then replaced by another attach instead of committed
	will not receive a release event, and is not used by the
	compositor.

	If a pending wl_buffer has been committed to more than one wl_surface,
	the delivery of wl_buffer.release events becomes undefined. A well
	behaved client should not rely on wl_buffer.release events in this
	case. Alternatively, a client could create multiple wl_buffer objects
	from the same backing storage or use a protocol extension providing
	per-commit release notifications.

	Destroying the wl_buffer after wl_buffer.release does not change
	the surface contents. Destroying the wl_buffer before wl_buffer.release
	is allowed as long as the underlying buffer storage isn't re-used (this
	can happen e.g. on client process termination). However, if the client
	destroys the wl_buffer before receiving the wl_buffer.release event and
	mutates the underlying buffer storage, the surface contents become
	undefined immediately.

	If wl_surface.attach is sent with a NULL wl_buffer, the
	following wl_surface.commit will remove the surface content.

	If a pending wl_buffer has been destroyed, the result is not specified.
	Many compositors are known to remove the surface content on the following
	wl_surface.commit, but this behaviour is not universal. Clients seeking to
	maximise compatibility should not destroy pending buffers and should
	ensure that they explicitly remove content from surfaces, even after
	destroying buffers.

    @arg1: c_wl_object_id wl_buffer
    @arg2: c_wl_int x
    @arg3: c_wl_int y
   */
C_WL_REQUEST wl_surface_attach(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request is used to describe the regions where the pending
	buffer is different from the current surface contents, and where
	the surface therefore needs to be repainted. The compositor
	ignores the parts of the damage that fall outside of the surface.

	Damage is double-buffered state, see wl_surface.commit.

	The damage rectangle is specified in surface-local coordinates,
	where x and y specify the upper left corner of the damage rectangle.

	The initial value for pending damage is empty: no damage.
	wl_surface.damage adds pending damage: the new pending damage
	is the union of old pending damage and the given rectangle.

	wl_surface.commit assigns pending damage as the current damage,
	and clears pending damage. The server will clear the current
	damage as it repaints the surface.

	Note! New clients should not use this request. Instead damage can be
	posted with wl_surface.damage_buffer which uses buffer coordinates
	instead of surface coordinates.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
    @arg3: c_wl_int width
    @arg4: c_wl_int height
   */
C_WL_REQUEST wl_surface_damage(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Request a notification when it is a good time to start drawing a new
	frame, by creating a frame callback. This is useful for throttling
	redrawing operations, and driving animations.

	When a client is animating on a wl_surface, it can use the 'frame'
	request to get notified when it is a good time to draw and commit the
	next frame of animation. If the client commits an update earlier than
	that, it is likely that some updates will not make it to the display,
	and the client is wasting resources by drawing too often.

	The frame request will take effect on the next wl_surface.commit.
	The notification will only be posted for one frame unless
	requested again. For a wl_surface, the notifications are posted in
	the order the frame requests were committed.

	The server must send the notifications so that a client
	will not send excessive updates, while still allowing
	the highest possible update rate for clients that wait for the reply
	before drawing again. The server should give some time for the client
	to draw and commit after sending the frame callback events to let it
	hit the next output refresh.

	A server should avoid signaling the frame callbacks if the
	surface is not visible in any way, e.g. the surface is off-screen,
	or completely obscured by other opaque surfaces.

	The object returned by this request will be destroyed by the
	compositor after the callback is fired and as such the client must not
	attempt to use it after that point.

	The callback_data passed in the callback is the current time, in
	milliseconds, with an undefined base.

    @arg1: c_wl_new_id wl_callback
   */
C_WL_REQUEST wl_surface_frame(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request sets the region of the surface that contains
	opaque content.

	The opaque region is an optimization hint for the compositor
	that lets it optimize the redrawing of content behind opaque
	regions.  Setting an opaque region is not required for correct
	behaviour, but marking transparent content as opaque will result
	in repaint artifacts.

	The opaque region is specified in surface-local coordinates.

	The compositor ignores the parts of the opaque region that fall
	outside of the surface.

	Opaque region is double-buffered state, see wl_surface.commit.

	wl_surface.set_opaque_region changes the pending opaque region.
	wl_surface.commit copies the pending region to the current region.
	Otherwise, the pending and current regions are never changed.

	The initial value for an opaque region is empty. Setting the pending
	opaque region has copy semantics, and the wl_region object can be
	destroyed immediately. A NULL wl_region causes the pending opaque
	region to be set to empty.

    @arg1: c_wl_object_id wl_region
   */
C_WL_REQUEST wl_surface_set_opaque_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request sets the region of the surface that can receive
	pointer and touch events.

	Input events happening outside of this region will try the next
	surface in the server surface stack. The compositor ignores the
	parts of the input region that fall outside of the surface.

	The input region is specified in surface-local coordinates.

	Input region is double-buffered state, see wl_surface.commit.

	wl_surface.set_input_region changes the pending input region.
	wl_surface.commit copies the pending region to the current region.
	Otherwise the pending and current regions are never changed,
	except cursor and icon surfaces are special cases, see
	wl_pointer.set_cursor and wl_data_device.start_drag.

	The initial value for an input region is infinite. That means the
	whole surface will accept input. Setting the pending input region
	has copy semantics, and the wl_region object can be destroyed
	immediately. A NULL wl_region causes the input region to be set
	to infinite.

    @arg1: c_wl_object_id wl_region
   */
C_WL_REQUEST wl_surface_set_input_region(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Surface state (input, opaque, and damage regions, attached buffers,
	etc.) is double-buffered. Protocol requests modify the pending state,
	as opposed to the active state in use by the compositor.

	A commit request atomically creates a content update from the pending
	state, even if the pending state has not been touched. The content
	update is placed in a queue until it becomes active. After commit, the
	new pending state is as documented for each related request.

	When the content update is applied, the wl_buffer is applied before all
	other state. This means that all coordinates in double-buffered state
	are relative to the newly attached wl_buffers, except for
	wl_surface.attach itself. If there is no newly attached wl_buffer, the
	coordinates are relative to the previous content update.

	All requests that need a commit to become effective are documented
	to affect double-buffered state.

	Other interfaces may add further double-buffered surface state. */
C_WL_REQUEST wl_surface_commit(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request sets the transformation that the client has already applied
	to the content of the buffer. The accepted values for the transform
	parameter are the values for wl_output.transform.

	The compositor applies the inverse of this transformation whenever it
	uses the buffer contents.

	Buffer transform is double-buffered state, see wl_surface.commit.

	A newly created surface has its buffer transformation set to normal.

	wl_surface.set_buffer_transform changes the pending buffer
	transformation. wl_surface.commit copies the pending buffer
	transformation to the current one. Otherwise, the pending and current
	values are never changed.

	The purpose of this request is to allow clients to render content
	according to the output transform, thus permitting the compositor to
	use certain optimizations even if the display is rotated. Using
	hardware overlays and scanning out a client buffer for fullscreen
	surfaces are examples of such optimizations. Those optimizations are
	highly dependent on the compositor implementation, so the use of this
	request should be considered on a case-by-case basis.

	Note that if the transform value includes 90 or 270 degree rotation,
	the width of the buffer will become the surface height and the height
	of the buffer will become the surface width.

	If transform is not one of the values from the
	wl_output.transform enum the invalid_transform protocol error
	is raised.

    @arg1: enum wl_output_transform_enum transform
   */
C_WL_REQUEST wl_surface_set_buffer_transform(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request sets an optional scaling factor on how the compositor
	interprets the contents of the buffer attached to the window.

	Buffer scale is double-buffered state, see wl_surface.commit.

	A newly created surface has its buffer scale set to 1.

	wl_surface.set_buffer_scale changes the pending buffer scale.
	wl_surface.commit copies the pending buffer scale to the current one.
	Otherwise, the pending and current values are never changed.

	The purpose of this request is to allow clients to supply higher
	resolution buffer data for use on high resolution outputs. It is
	intended that you pick the same buffer scale as the scale of the
	output that the surface is displayed on. This means the compositor
	can avoid scaling when rendering the surface on that output.

	Note that if the scale is larger than 1, then you have to attach
	a buffer that is larger (by a factor of scale in each dimension)
	than the desired surface size.

	If scale is not greater than 0 the invalid_scale protocol error is
	raised.

    @arg1: c_wl_int scale
   */
C_WL_REQUEST wl_surface_set_buffer_scale(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request is used to describe the regions where the pending
	buffer is different from the current surface contents, and where
	the surface therefore needs to be repainted. The compositor
	ignores the parts of the damage that fall outside of the surface.

	Damage is double-buffered state, see wl_surface.commit.

	The damage rectangle is specified in buffer coordinates,
	where x and y specify the upper left corner of the damage rectangle.

	The initial value for pending damage is empty: no damage.
	wl_surface.damage_buffer adds pending damage: the new pending
	damage is the union of old pending damage and the given rectangle.

	wl_surface.commit assigns pending damage as the current damage,
	and clears pending damage. The server will clear the current
	damage as it repaints the surface.

	This request differs from wl_surface.damage in only one way - it
	takes damage in buffer coordinates instead of surface-local
	coordinates. While this generally is more intuitive than surface
	coordinates, it is especially desirable when using wp_viewport
	or when a drawing library (like EGL) is unaware of buffer scale
	and buffer transform.

	Note: Because buffer transformation changes and damage requests may
	be interleaved in the protocol stream, it is impossible to determine
	the actual mapping between surface and buffer damage until
	wl_surface.commit time. Therefore, compositors wishing to take both
	kinds of damage into account will have to accumulate damage from the
	two requests separately and only transform from one to the other
	after receiving the wl_surface.commit.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
    @arg3: c_wl_int width
    @arg4: c_wl_int height
   */
C_WL_REQUEST wl_surface_damage_buffer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* The x and y arguments specify the location of the new pending
	buffer's upper left corner, relative to the current buffer's upper
	left corner, in surface-local coordinates. In other words, the
	x and y, combined with the new surface size define in which
	directions the surface's size changes.

	The exact semantics of wl_surface.offset are role-specific. Refer to
	the documentation of specific roles for more information.

	Surface location offset is double-buffered state, see
	wl_surface.commit.

	This request is semantically equivalent to and the replaces the x and y
	arguments in the wl_surface.attach request in wl_surface versions prior
	to 5. See wl_surface.attach for details.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
   */
C_WL_REQUEST wl_surface_offset(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* This is sent on binding to the seat global or whenever a seat gains
	or loses the pointer, keyboard or touch capabilities.
	The argument is a capability enum containing the complete set of
	capabilities this seat has.

	When the pointer capability is added, a client may create a
	wl_pointer object using the wl_seat.get_pointer request. This object
	will receive pointer events until the capability is removed in the
	future.

	When the pointer capability is removed, a client should destroy the
	wl_pointer objects associated with the seat where the capability was
	removed, using the wl_pointer.release request. No further pointer
	events will be received on these objects.

	In some compositors, if a seat regains the pointer capability and a
	client has a previously obtained wl_pointer object of version 4 or
	less, that object may start sending pointer events again. This
	behavior is considered a misinterpretation of the intended behavior
	and must not be relied upon by the client. wl_pointer objects of
	version 5 or later must not send events if created before the most
	recent event notifying the client of an added pointer capability.

	The above behavior also applies to wl_keyboard and wl_touch with the
	keyboard and touch capabilities, respectively. */
C_WL_EVENT wl_seat_capabilities(struct c_wl_connection *conn, c_wl_object_id wl_seat, enum wl_seat_capability_enum capabilities);

 /* In a multi-seat configuration the seat name can be used by clients to
	help identify which physical devices the seat represents.

	The seat name is a UTF-8 string with no convention defined for its
	contents. Each name is unique among all wl_seat globals. The name is
	only guaranteed to be unique for the current compositor instance.

	The same seat names are used for all clients. Thus, the name can be
	shared across processes to refer to a specific wl_seat global.

	The name event is sent after binding to the seat global, and should be sent
	before announcing capabilities. This event only sent once per seat object,
	and the name does not change over the lifetime of the wl_seat global.

	Compositors may re-use the same seat name if the wl_seat global is
	destroyed and re-created later. */
C_WL_EVENT wl_seat_name(struct c_wl_connection *conn, c_wl_object_id wl_seat, c_wl_string name);

   /* The ID provided will be initialized to the wl_pointer interface
	for this seat.

	This request only takes effect if the seat has the pointer
	capability, or has had the pointer capability in the past.
	It is a protocol violation to issue this request on a seat that has
	never had the pointer capability. The missing_capability error will
	be sent in this case.

    @arg1: c_wl_new_id wl_pointer
   */
C_WL_REQUEST wl_seat_get_pointer(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* The ID provided will be initialized to the wl_keyboard interface
	for this seat.

	This request only takes effect if the seat has the keyboard
	capability, or has had the keyboard capability in the past.
	It is a protocol violation to issue this request on a seat that has
	never had the keyboard capability. The missing_capability error will
	be sent in this case.

    @arg1: c_wl_new_id wl_keyboard
   */
C_WL_REQUEST wl_seat_get_keyboard(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* The ID provided will be initialized to the wl_touch interface
	for this seat.

	This request only takes effect if the seat has the touch
	capability, or has had the touch capability in the past.
	It is a protocol violation to issue this request on a seat that has
	never had the touch capability. The missing_capability error will
	be sent in this case.

    @arg1: c_wl_new_id wl_touch
   */
C_WL_REQUEST wl_seat_get_touch(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Using this request a client can tell the server that it is not going to
	use the seat object anymore. */
C_WL_REQUEST wl_seat_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* Notification that this seat's pointer is focused on a certain
	surface.

	When a seat's focus enters a surface, the pointer image
	is undefined and a client should respond to this event by setting
	an appropriate pointer image with the set_cursor request. */
C_WL_EVENT wl_pointer_enter(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_fixed surface_x, c_wl_fixed surface_y);

 /* Notification that this seat's pointer is no longer focused on
	a certain surface.

	The leave notification is sent before the enter notification
	for the new focus. */
C_WL_EVENT wl_pointer_leave(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_object_id wl_surface);

 /* Notification of pointer location change. The arguments
	surface_x and surface_y are the location relative to the
	focused surface. */
C_WL_EVENT wl_pointer_motion(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, c_wl_fixed surface_x, c_wl_fixed surface_y);

 /* Mouse button click and release notifications.

	The location of the click is given by the last motion or
	enter event.
	The time argument is a timestamp with millisecond
	granularity, with an undefined base.

	The button is a button code as defined in the Linux kernel's
	linux/input-event-codes.h header file, e.g. BTN_LEFT.

	Any 16-bit button code value is reserved for future additions to the
	kernel's event code list. All other button codes above 0xFFFF are
	currently undefined but may be used in future versions of this
	protocol. */
C_WL_EVENT wl_pointer_button(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint serial, c_wl_uint time, c_wl_uint button, enum wl_pointer_button_state_enum state);

 /* Scroll and other axis notifications.

	For scroll events (vertical and horizontal scroll axes), the
	value parameter is the length of a vector along the specified
	axis in a coordinate space identical to those of motion events,
	representing a relative movement along the specified axis.

	For devices that support movements non-parallel to axes multiple
	axis events will be emitted.

	When applicable, for example for touch pads, the server can
	choose to emit scroll events where the motion vector is
	equivalent to a motion event vector.

	When applicable, a client can transform its content relative to the
	scroll distance. */
C_WL_EVENT wl_pointer_axis(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, enum wl_pointer_axis_enum axis, c_wl_fixed value);

 /* Indicates the end of a set of events that logically belong together.
	A client is expected to accumulate the data in all events within the
	frame before proceeding.

	All wl_pointer events before a wl_pointer.frame event belong
	logically together. For example, in a diagonal scroll motion the
	compositor will send an optional wl_pointer.axis_source event, two
	wl_pointer.axis events (horizontal and vertical) and finally a
	wl_pointer.frame event. The client may use this information to
	calculate a diagonal vector for scrolling.

	When multiple wl_pointer.axis events occur within the same frame,
	the motion vector is the combined motion of all events.
	When a wl_pointer.axis and a wl_pointer.axis_stop event occur within
	the same frame, this indicates that axis movement in one axis has
	stopped but continues in the other axis.
	When multiple wl_pointer.axis_stop events occur within the same
	frame, this indicates that these axes stopped in the same instance.

	A wl_pointer.frame event is sent for every logical event group,
	even if the group only contains a single wl_pointer event.
	Specifically, a client may get a sequence: motion, frame, button,
	frame, axis, frame, axis_stop, frame.

	The wl_pointer.enter and wl_pointer.leave events are logical events
	generated by the compositor and not the hardware. These events are
	also grouped by a wl_pointer.frame. When a pointer moves from one
	surface to another, a compositor should group the
	wl_pointer.leave event within the same wl_pointer.frame.
	However, a client must not rely on wl_pointer.leave and
	wl_pointer.enter being in the same wl_pointer.frame.
	Compositor-specific policies may require the wl_pointer.leave and
	wl_pointer.enter event being split across multiple wl_pointer.frame
	groups. */
C_WL_EVENT wl_pointer_frame(struct c_wl_connection *conn, c_wl_object_id wl_pointer);

 /* Source information for scroll and other axes.

	This event does not occur on its own. It is sent before a
	wl_pointer.frame event and carries the source information for
	all events within that frame.

	The source specifies how this event was generated. If the source is
	wl_pointer.axis_source.finger, a wl_pointer.axis_stop event will be
	sent when the user lifts the finger off the device.

	If the source is wl_pointer.axis_source.wheel,
	wl_pointer.axis_source.wheel_tilt or
	wl_pointer.axis_source.continuous, a wl_pointer.axis_stop event may
	or may not be sent. Whether a compositor sends an axis_stop event
	for these sources is hardware-specific and implementation-dependent;
	clients must not rely on receiving an axis_stop event for these
	scroll sources and should treat scroll sequences from these scroll
	sources as unterminated by default.

	This event is optional. If the source is unknown for a particular
	axis event sequence, no event is sent.
	Only one wl_pointer.axis_source event is permitted per frame.

	The order of wl_pointer.axis_discrete and wl_pointer.axis_source is
	not guaranteed. */
C_WL_EVENT wl_pointer_axis_source(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_source_enum axis_source);

 /* Stop notification for scroll and other axes.

	For some wl_pointer.axis_source types, a wl_pointer.axis_stop event
	is sent to notify a client that the axis sequence has terminated.
	This enables the client to implement kinetic scrolling.
	See the wl_pointer.axis_source documentation for information on when
	this event may be generated.

	Any wl_pointer.axis events with the same axis_source after this
	event should be considered as the start of a new axis motion.

	The timestamp is to be interpreted identical to the timestamp in the
	wl_pointer.axis event. The timestamp value may be the same as a
	preceding wl_pointer.axis event. */
C_WL_EVENT wl_pointer_axis_stop(struct c_wl_connection *conn, c_wl_object_id wl_pointer, c_wl_uint time, enum wl_pointer_axis_enum axis);

 /* Discrete step information for scroll and other axes.

	This event carries the axis value of the wl_pointer.axis event in
	discrete steps (e.g. mouse wheel clicks).

	This event is deprecated with wl_pointer version 8 - this event is not
	sent to clients supporting version 8 or later.

	This event does not occur on its own, it is coupled with a
	wl_pointer.axis event that represents this axis value on a
	continuous scale. The protocol guarantees that each axis_discrete
	event is always followed by exactly one axis event with the same
	axis number within the same wl_pointer.frame. Note that the protocol
	allows for other events to occur between the axis_discrete and
	its coupled axis event, including other axis_discrete or axis
	events. A wl_pointer.frame must not contain more than one axis_discrete
	event per axis type.

	This event is optional; continuous scrolling devices
	like two-finger scrolling on touchpads do not have discrete
	steps and do not generate this event.

	The discrete value carries the directional information. e.g. a value
	of -2 is two steps towards the negative direction of this axis.

	The axis number is identical to the axis number in the associated
	axis event.

	The order of wl_pointer.axis_discrete and wl_pointer.axis_source is
	not guaranteed. */
C_WL_EVENT wl_pointer_axis_discrete(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, c_wl_int discrete);

 /* Discrete high-resolution scroll information.

	This event carries high-resolution wheel scroll information,
	with each multiple of 120 representing one logical scroll step
	(a wheel detent). For example, an axis_value120 of 30 is one quarter of
	a logical scroll step in the positive direction, a value120 of
	-240 are two logical scroll steps in the negative direction within the
	same hardware event.
	Clients that rely on discrete scrolling should accumulate the
	value120 to multiples of 120 before processing the event.

	The value120 must not be zero.

	This event replaces the wl_pointer.axis_discrete event in clients
	supporting wl_pointer version 8 or later.

	Where a wl_pointer.axis_source event occurs in the same
	wl_pointer.frame, the axis source applies to this event.

	The order of wl_pointer.axis_value120 and wl_pointer.axis_source is
	not guaranteed. */
C_WL_EVENT wl_pointer_axis_value120(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, c_wl_int value120);

 /* Relative directional information of the entity causing the axis
	motion.

	For a wl_pointer.axis event, the wl_pointer.axis_relative_direction
	event specifies the movement direction of the entity causing the
	wl_pointer.axis event. For example:
	- if a user's fingers on a touchpad move down and this
	  causes a wl_pointer.axis vertical_scroll down event, the physical
	  direction is 'identical'
	- if a user's fingers on a touchpad move down and this causes a
	  wl_pointer.axis vertical_scroll up scroll up event ('natural
	  scrolling'), the physical direction is 'inverted'.

	A client may use this information to adjust scroll motion of
	components. Specifically, enabling natural scrolling causes the
	content to change direction compared to traditional scrolling.
	Some widgets like volume control sliders should usually match the
	physical direction regardless of whether natural scrolling is
	active. This event enables clients to match the scroll direction of
	a widget to the physical direction.

	This event does not occur on its own, it is coupled with a
	wl_pointer.axis event that represents this axis value.
	The protocol guarantees that each axis_relative_direction event is
	always followed by exactly one axis event with the same
	axis number within the same wl_pointer.frame. Note that the protocol
	allows for other events to occur between the axis_relative_direction
	and its coupled axis event.

	The axis number is identical to the axis number in the associated
	axis event.

	The order of wl_pointer.axis_relative_direction,
	wl_pointer.axis_discrete and wl_pointer.axis_source is not
	guaranteed. */
C_WL_EVENT wl_pointer_axis_relative_direction(struct c_wl_connection *conn, c_wl_object_id wl_pointer, enum wl_pointer_axis_enum axis, enum wl_pointer_axis_relative_direction_enum direction);

   /* Set the pointer surface, i.e., the surface that contains the
	pointer image (cursor). This request gives the surface the role
	of a cursor. If the surface already has another role, it raises
	a protocol error.

	The cursor actually changes only if the pointer
	focus for this device is one of the requesting client's surfaces
	or the surface parameter is the current pointer surface. If
	there was a previous surface set with this request it is
	replaced. If surface is NULL, the pointer image is hidden.

	The parameters hotspot_x and hotspot_y define the position of
	the pointer surface relative to the pointer location. Its
	top-left corner is always at (x, y) - (hotspot_x, hotspot_y),
	where (x, y) are the coordinates of the pointer location, in
	surface-local coordinates.

	On wl_surface.offset requests to the pointer surface, hotspot_x
	and hotspot_y are decremented by the x and y parameters
	passed to the request. The offset must be applied by
	wl_surface.commit as usual.

	The hotspot can also be updated by passing the currently set
	pointer surface to this request with new values for hotspot_x
	and hotspot_y.

	The input region is ignored for wl_surfaces with the role of
	a cursor. When the use as a cursor ends, the wl_surface is
	unmapped.

	The serial parameter must match the latest wl_pointer.enter
	serial number sent to the client. Otherwise the request will be
	ignored.

    @arg1: c_wl_uint serial
    @arg2: c_wl_object_id wl_surface
    @arg3: c_wl_int hotspot_x
    @arg4: c_wl_int hotspot_y
   */
C_WL_REQUEST wl_pointer_set_cursor(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Using this request a client can tell the server that it is not going to
	use the pointer object anymore.

	This request destroys the pointer proxy object, so clients must not call
	wl_pointer_destroy() after using this request. */
C_WL_REQUEST wl_pointer_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* This event provides a file descriptor to the client which can be
	memory-mapped in read-only mode to provide a keyboard mapping
	description.

	From version 7 onwards, the fd must be mapped with MAP_PRIVATE by
	the recipient, as MAP_SHARED may fail. */
C_WL_EVENT wl_keyboard_keymap(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, enum wl_keyboard_keymap_format_enum format, c_wl_fd fd, c_wl_uint size);

 /* Notification that this seat's keyboard focus is on a certain
	surface.

	The compositor must send the wl_keyboard.modifiers event after this
	event.

	In the wl_keyboard logical state, this event sets the active surface to
	the surface argument and the keys currently logically down to the keys
	in the keys argument. The compositor must not send this event if the
	wl_keyboard already had an active surface immediately before this event.

	Clients should not use the list of pressed keys to emulate key-press
	events. The order of keys in the list is unspecified. */
C_WL_EVENT wl_keyboard_enter(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_object_id wl_surface, c_wl_array *keys);

 /* Notification that this seat's keyboard focus is no longer on
	a certain surface.

	The leave notification is sent before the enter notification
	for the new focus.

	In the wl_keyboard logical state, this event resets all values to their
	defaults. The compositor must not send this event if the active surface
	of the wl_keyboard was not equal to the surface argument immediately
	before this event. */
C_WL_EVENT wl_keyboard_leave(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_object_id wl_surface);

 /* A key was pressed or released.
	The time argument is a timestamp with millisecond
	granularity, with an undefined base.

	The key is a platform-specific key code that can be interpreted
	by feeding it to the keyboard mapping (see the keymap event).

	If this event produces a change in modifiers, then the resulting
	wl_keyboard.modifiers event must be sent after this event.

	In the wl_keyboard logical state, this event adds the key to the keys
	currently logically down (if the state argument is pressed) or removes
	the key from the keys currently logically down (if the state argument is
	released). The compositor must not send this event if the wl_keyboard
	did not have an active surface immediately before this event. The
	compositor must not send this event if state is pressed (resp. released)
	and the key was already logically down (resp. was not logically down)
	immediately before this event.

	Since version 10, compositors may send key events with the "repeated"
	key state when a wl_keyboard.repeat_info event with a rate argument of
	0 has been received. This allows the compositor to take over the
	responsibility of key repetition. */
C_WL_EVENT wl_keyboard_key(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_uint time, c_wl_uint key, enum wl_keyboard_key_state_enum state);

 /* Notifies clients that the modifier and/or group state has
	changed, and it should update its local state.

	The compositor may send this event without a surface of the client
	having keyboard focus, for example to tie modifier information to
	pointer focus instead. If a modifier event with pressed modifiers is sent
	without a prior enter event, the client can assume the modifier state is
	valid until it receives the next wl_keyboard.modifiers event. In order to
	reset the modifier state again, the compositor can send a
	wl_keyboard.modifiers event with no pressed modifiers.

	In the wl_keyboard logical state, this event updates the modifiers and
	group. */
C_WL_EVENT wl_keyboard_modifiers(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_uint serial, c_wl_uint mods_depressed, c_wl_uint mods_latched, c_wl_uint mods_locked, c_wl_uint group);

 /* Informs the client about the keyboard's repeat rate and delay.

	This event is sent as soon as the wl_keyboard object has been created,
	and is guaranteed to be received by the client before any key press
	event.

	Negative values for either rate or delay are illegal. A rate of zero
	will disable any repeating (regardless of the value of delay).

	This event can be sent later on as well with a new value if necessary,
	so clients should continue listening for the event past the creation
	of wl_keyboard. */
C_WL_EVENT wl_keyboard_repeat_info(struct c_wl_connection *conn, c_wl_object_id wl_keyboard, c_wl_int rate, c_wl_int delay);

C_WL_REQUEST wl_keyboard_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* A new touch point has appeared on the surface. This touch point is
	assigned a unique ID. Future events from this touch point reference
	this ID. The ID ceases to be valid after a touch up event and may be
	reused in the future. */
C_WL_EVENT wl_touch_down(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint serial, c_wl_uint time, c_wl_object_id wl_surface, c_wl_int id, c_wl_fixed x, c_wl_fixed y);

 /* The touch point has disappeared. No further events will be sent for
	this touch point and the touch point's ID is released and may be
	reused in a future touch down event. */
C_WL_EVENT wl_touch_up(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint serial, c_wl_uint time, c_wl_int id);

 /* A touch point has changed coordinates. */
C_WL_EVENT wl_touch_motion(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_uint time, c_wl_int id, c_wl_fixed x, c_wl_fixed y);

 /* Indicates the end of a set of events that logically belong together.
	A client is expected to accumulate the data in all events within the
	frame before proceeding.

	A wl_touch.frame terminates at least one event but otherwise no
	guarantee is provided about the set of events within a frame. A client
	must assume that any state not updated in a frame is unchanged from the
	previously known state. */
C_WL_EVENT wl_touch_frame(struct c_wl_connection *conn, c_wl_object_id wl_touch);

 /* Sent if the compositor decides the touch stream is a global
	gesture. No further events are sent to the clients from that
	particular gesture. Touch cancellation applies to all touch points
	currently active on this client's surface. The client is
	responsible for finalizing the touch points, future touch points on
	this surface may reuse the touch point ID.

	No frame event is required after the cancel event. */
C_WL_EVENT wl_touch_cancel(struct c_wl_connection *conn, c_wl_object_id wl_touch);

 /* Sent when a touchpoint has changed its shape.

	This event does not occur on its own. It is sent before a
	wl_touch.frame event and carries the new shape information for
	any previously reported, or new touch points of that frame.

	Other events describing the touch point such as wl_touch.down,
	wl_touch.motion or wl_touch.orientation may be sent within the
	same wl_touch.frame. A client should treat these events as a single
	logical touch point update. The order of wl_touch.shape,
	wl_touch.orientation and wl_touch.motion is not guaranteed.
	A wl_touch.down event is guaranteed to occur before the first
	wl_touch.shape event for this touch ID but both events may occur within
	the same wl_touch.frame.

	A touchpoint shape is approximated by an ellipse through the major and
	minor axis length. The major axis length describes the longer diameter
	of the ellipse, while the minor axis length describes the shorter
	diameter. Major and minor are orthogonal and both are specified in
	surface-local coordinates. The center of the ellipse is always at the
	touchpoint location as reported by wl_touch.down or wl_touch.move.

	This event is only sent by the compositor if the touch device supports
	shape reports. The client has to make reasonable assumptions about the
	shape if it did not receive this event. */
C_WL_EVENT wl_touch_shape(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_int id, c_wl_fixed major, c_wl_fixed minor);

 /* Sent when a touchpoint has changed its orientation.

	This event does not occur on its own. It is sent before a
	wl_touch.frame event and carries the new shape information for
	any previously reported, or new touch points of that frame.

	Other events describing the touch point such as wl_touch.down,
	wl_touch.motion or wl_touch.shape may be sent within the
	same wl_touch.frame. A client should treat these events as a single
	logical touch point update. The order of wl_touch.shape,
	wl_touch.orientation and wl_touch.motion is not guaranteed.
	A wl_touch.down event is guaranteed to occur before the first
	wl_touch.orientation event for this touch ID but both events may occur
	within the same wl_touch.frame.

	The orientation describes the clockwise angle of a touchpoint's major
	axis to the positive surface y-axis and is normalized to the -180 to
	+180 degree range. The granularity of orientation depends on the touch
	device, some devices only support binary rotation values between 0 and
	90 degrees.

	This event is only sent by the compositor if the touch device supports
	orientation reports. */
C_WL_EVENT wl_touch_orientation(struct c_wl_connection *conn, c_wl_object_id wl_touch, c_wl_int id, c_wl_fixed orientation);

C_WL_REQUEST wl_touch_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


 /* The geometry event describes geometric properties of the output.
	The event is sent when binding to the output object and whenever
	any of the properties change.

	The physical size can be set to zero if it doesn't make sense for this
	output (e.g. for projectors or virtual outputs).

	The geometry event will be followed by a done event (starting from
	version 2).

	Clients should use wl_surface.preferred_buffer_transform instead of the
	transform advertised by this event to find the preferred buffer
	transform to use for a surface.

	Note: wl_output only advertises partial information about the output
	position and identification. Some compositors, for instance those not
	implementing a desktop-style output layout or those exposing virtual
	outputs, might fake this information. Instead of using x and y, clients
	should use xdg_output.logical_position. Instead of using make and model,
	clients should use name and description. */
C_WL_EVENT wl_output_geometry(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_int x, c_wl_int y, c_wl_int physical_width, c_wl_int physical_height, enum wl_output_subpixel_enum subpixel, c_wl_string make, c_wl_string model, enum wl_output_transform_enum transform);

 /* The mode event describes an available mode for the output.

	The event is sent when binding to the output object and there
	will always be one mode, the current mode.  The event is sent
	again if an output changes mode, for the mode that is now
	current.  In other words, the current mode is always the last
	mode that was received with the current flag set.

	Non-current modes are deprecated. A compositor can decide to only
	advertise the current mode and never send other modes. Clients
	should not rely on non-current modes.

	The size of a mode is given in physical hardware units of
	the output device. This is not necessarily the same as
	the output size in the global compositor space. For instance,
	the output may be scaled, as described in wl_output.scale,
	or transformed, as described in wl_output.transform. Clients
	willing to retrieve the output size in the global compositor
	space should use xdg_output.logical_size instead.

	The vertical refresh rate can be set to zero if it doesn't make
	sense for this output (e.g. for virtual outputs).

	The mode event will be followed by a done event (starting from
	version 2).

	Clients should not use the refresh rate to schedule frames. Instead,
	they should use the wl_surface.frame event or the presentation-time
	protocol.

	Note: this information is not always meaningful for all outputs. Some
	compositors, such as those exposing virtual outputs, might fake the
	refresh rate or the size. */
C_WL_EVENT wl_output_mode(struct c_wl_connection *conn, c_wl_object_id wl_output, enum wl_output_mode_enum flags, c_wl_int width, c_wl_int height, c_wl_int refresh);

 /* This event is sent after all other properties have been
	sent after binding to the output object and after any
	other property changes done after that. This allows
	changes to the output properties to be seen as
	atomic, even if they happen via multiple events. */
C_WL_EVENT wl_output_done(struct c_wl_connection *conn, c_wl_object_id wl_output);

 /* This event contains scaling geometry information
	that is not in the geometry event. It may be sent after
	binding the output object or if the output scale changes
	later. The compositor will emit a non-zero, positive
	value for scale. If it is not sent, the client should
	assume a scale of 1.

	A scale larger than 1 means that the compositor will
	automatically scale surface buffers by this amount
	when rendering. This is used for very high resolution
	displays where applications rendering at the native
	resolution would be too small to be legible.

	Clients should use wl_surface.preferred_buffer_scale
	instead of this event to find the preferred buffer
	scale to use for a surface.

	The scale event will be followed by a done event. */
C_WL_EVENT wl_output_scale(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_int factor);

 /* Many compositors will assign user-friendly names to their outputs, show
	them to the user, allow the user to refer to an output, etc. The client
	may wish to know this name as well to offer the user similar behaviors.

	The name is a UTF-8 string with no convention defined for its contents.
	Each name is unique among all wl_output globals. The name is only
	guaranteed to be unique for the compositor instance.

	The same output name is used for all clients for a given wl_output
	global. Thus, the name can be shared across processes to refer to a
	specific wl_output global.

	The name is not guaranteed to be persistent across sessions, thus cannot
	be used to reliably identify an output in e.g. configuration files.

	Examples of names include 'HDMI-A-1', 'WL-1', 'X11-1', etc. However, do
	not assume that the name is a reflection of an underlying DRM connector,
	X11 connection, etc.

	The name event is sent after binding the output object. This event is
	only sent once per output object, and the name does not change over the
	lifetime of the wl_output global.

	Compositors may re-use the same output name if the wl_output global is
	destroyed and re-created later. Compositors should avoid re-using the
	same name if possible.

	The name event will be followed by a done event. */
C_WL_EVENT wl_output_name(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_string name);

 /* Many compositors can produce human-readable descriptions of their
	outputs. The client may wish to know this description as well, e.g. for
	output selection purposes.

	The description is a UTF-8 string with no convention defined for its
	contents. The description is not guaranteed to be unique among all
	wl_output globals. Examples might include 'Foocorp 11" Display' or
	'Virtual X11 output via :1'.

	The description event is sent after binding the output object and
	whenever the description changes. The description is optional, and may
	not be sent at all.

	The description event will be followed by a done event. */
C_WL_EVENT wl_output_description(struct c_wl_connection *conn, c_wl_object_id wl_output, c_wl_string description);

   /* Using this request a client can tell the server that it is not going to
	use the output object anymore. */
C_WL_REQUEST wl_output_release(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* Destroy the region.  This will invalidate the object ID. */
C_WL_REQUEST wl_region_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Add the specified rectangle to the region.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
    @arg3: c_wl_int width
    @arg4: c_wl_int height
   */
C_WL_REQUEST wl_region_add(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Subtract the specified rectangle from the region.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
    @arg3: c_wl_int width
    @arg4: c_wl_int height
   */
C_WL_REQUEST wl_region_subtract(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* Informs the server that the client will not be using this
	protocol object anymore. This does not affect any other
	objects, wl_subsurface objects included. */
C_WL_REQUEST wl_subcompositor_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Create a sub-surface interface for the given surface, and
	associate it with the given parent surface. This turns a
	plain wl_surface into a sub-surface.

	The to-be sub-surface must not already have another role, and it
	must not have an existing wl_subsurface object. Otherwise the
	bad_surface protocol error is raised.

	Adding sub-surfaces to a parent is a double-buffered operation on the
	parent (see wl_surface.commit). The effect of adding a sub-surface
	becomes visible on the next time the state of the parent surface is
	applied.

	The parent surface must not be one of the child surface's descendants,
	and the parent must be different from the child surface, otherwise the
	bad_parent protocol error is raised.

	This request modifies the behaviour of wl_surface.commit request on
	the sub-surface, see the documentation on wl_subsurface interface.

    @arg1: c_wl_new_id wl_subsurface
    @arg2: c_wl_object_id wl_surface
    @arg3: c_wl_object_id wl_surface
   */
C_WL_REQUEST wl_subcompositor_get_subsurface(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


   /* The sub-surface interface is removed from the wl_surface object
	that was turned into a sub-surface with a
	wl_subcompositor.get_subsurface request. The wl_surface's association
	to the parent is deleted. The wl_surface is unmapped immediately. */
C_WL_REQUEST wl_subsurface_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This schedules a sub-surface position change.
	The sub-surface will be moved so that its origin (top left
	corner pixel) will be at the location x, y of the parent surface
	coordinate system. The coordinates are not restricted to the parent
	surface area. Negative values are allowed.

	The scheduled coordinates will take effect whenever the state of the
	parent surface is applied.

	If more than one set_position request is invoked by the client before
	the commit of the parent surface, the position of a new request always
	replaces the scheduled position from any previous request.

	The initial position is 0, 0.

    @arg1: c_wl_int x
    @arg2: c_wl_int y
   */
C_WL_REQUEST wl_subsurface_set_position(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This sub-surface is taken from the stack, and put back just
	above the reference surface, changing the z-order of the sub-surfaces.
	The reference surface must be one of the sibling surfaces, or the
	parent surface. Using any other surface, including this sub-surface,
	will cause a protocol error.

	The z-order is double-buffered. Requests are handled in order and
	applied immediately to a pending state. The final pending state is
	copied to the active state the next time the state of the parent
	surface is applied.

	A new sub-surface is initially added as the top-most in the stack
	of its siblings and parent.

    @arg1: c_wl_object_id wl_surface
   */
C_WL_REQUEST wl_subsurface_place_above(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* The sub-surface is placed just below the reference surface.
	See wl_subsurface.place_above.

    @arg1: c_wl_object_id wl_surface
   */
C_WL_REQUEST wl_subsurface_place_below(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Change the commit behaviour of the sub-surface to synchronized
	mode, also described as the parent dependent mode.

	In synchronized mode, wl_surface.commit on a sub-surface will
	accumulate the committed state in a cache, but the state will
	not be applied and hence will not change the compositor output.
	The cached state is applied to the sub-surface immediately after
	the parent surface's state is applied. This ensures atomic
	updates of the parent and all its synchronized sub-surfaces.
	Applying the cached state will invalidate the cache, so further
	parent surface commits do not (re-)apply old state.

	See wl_subsurface for the recursive effect of this mode. */
C_WL_REQUEST wl_subsurface_set_sync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* Change the commit behaviour of the sub-surface to desynchronized
	mode, also described as independent or freely running mode.

	In desynchronized mode, wl_surface.commit on a sub-surface will
	apply the pending state directly, without caching, as happens
	normally with a wl_surface. Calling wl_surface.commit on the
	parent surface has no effect on the sub-surface's wl_surface
	state. This mode allows a sub-surface to be updated on its own.

	If cached state exists when wl_surface.commit is called in
	desynchronized mode, the pending state is added to the cached
	state, and applied as a whole. This invalidates the cache.

	Note: even if a sub-surface is set to desynchronized, a parent
	sub-surface may override it to behave as synchronized. For details,
	see wl_subsurface.

	If a surface's parent surface behaves as desynchronized, then
	the cached state is applied on set_desync. */
C_WL_REQUEST wl_subsurface_set_desync(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


C_WL_REQUEST wl_fixes_destroy(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);

   /* This request destroys a wl_registry object.

	The client should no longer use the wl_registry after making this
	request.

	The compositor will emit a wl_display.delete_id event with the object ID
	of the registry and will no longer emit any events on the registry. The
	client should re-use the object ID once it receives the
	wl_display.delete_id event.

    @arg1: c_wl_object_id wl_registry
   */
C_WL_REQUEST wl_fixes_destroy_registry(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);


#endif
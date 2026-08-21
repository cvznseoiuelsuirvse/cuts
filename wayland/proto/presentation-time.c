#include <stdint.h>

#include "wayland/server.h"
#include "wayland/display.h"
#include "wayland/proto/presentation-time.h"


C_WL_EVENT wp_presentation_clock_id(struct c_wl_connection *conn, c_wl_object_id wp_presentation, c_wl_uint clk_id) {
  struct c_wl_message msg = {wp_presentation, 0, "u", "clock_id"};
  return c_wl_connection_post(conn, &msg, 1, clk_id);
}
C_WL_INTERFACE_REGISTER(wp_presentation, 2, 2, 0, 
    {"destroy",                wp_presentation_destroy,       0,  {0}},
    {"feedback",               wp_presentation_feedback,      2,  "on"},
)
void wp_presentation_add_listener(struct c_wl_display *display, struct c_wp_presentation_listeners *listeners, void *userdata) {
  c_wl_display_add_interface_listener(display, "wp_presentation", listeners, userdata);
}

C_WL_EVENT wp_presentation_feedback_sync_output(struct c_wl_connection *conn, c_wl_object_id wp_presentation_feedback, c_wl_object_id wl_output) {
  struct c_wl_message msg = {wp_presentation_feedback, 0, "o", "sync_output"};
  return c_wl_connection_post(conn, &msg, 1, wl_output);
}
C_WL_EVENT wp_presentation_feedback_presented(struct c_wl_connection *conn, c_wl_object_id wp_presentation_feedback, c_wl_uint tv_sec_hi, c_wl_uint tv_sec_lo, c_wl_uint tv_nsec, c_wl_uint refresh, c_wl_uint seq_hi, c_wl_uint seq_lo, enum wp_presentation_feedback_kind_enum flags) {
  struct c_wl_message msg = {wp_presentation_feedback, 1, "uuuuuuu", "presented"};
  return c_wl_connection_post(conn, &msg, 7, tv_sec_hi, tv_sec_lo, tv_nsec, refresh, seq_hi, seq_lo, flags);
}
C_WL_EVENT wp_presentation_feedback_discarded(struct c_wl_connection *conn, c_wl_object_id wp_presentation_feedback) {
  struct c_wl_message msg = {wp_presentation_feedback, 2, {0}, "discarded"};
  return c_wl_connection_post(conn, &msg, 0);
}
C_WL_INTERFACE_REGISTER(wp_presentation_feedback, 2, 0, -1, {})


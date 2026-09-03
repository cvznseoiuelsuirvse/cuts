#include "util/callback.h"
#include "util/log.h"

struct c_callback *c_callback_add(c_list *cb_list, void *listeners, void *userdata) {
  struct c_callback cb = {
    .listeners = listeners,
    .userdata = userdata,
  };
  c_log(C_LOG_DEBUG, "registering new callbacks %p with userdata=%p", listeners, userdata);
  return c_list_push(cb_list, &cb, sizeof(cb));
}

void c_callback_remove(c_list **cb_list, struct c_callback *cb) {
  c_list_remove(cb_list, cb);
}

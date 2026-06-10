#ifndef CUTS_XDG_DECORATION_UNSTABLE_V1_H
#define CUTS_XDG_DECORATION_UNSTABLE_V1_H

#include <stdint.h>

#include "wayland/server.h"
#include "wayland/types.h"


enum zxdg_toplevel_decoration_v1_error_enum {
  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_UNCONFIGURED_BUFFER = 0,
  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED = 1,
  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED = 2,
  ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE = 3,
};

 /* These values describe window decoration modes. */
enum zxdg_toplevel_decoration_v1_mode_enum {
  ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE = 1,
  ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE = 2,
};

   /* Destroy the decoration manager. This doesn't destroy objects created
        with the manager. */
C_WL_REQUEST zxdg_decoration_manager_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args);

   /* Create a new decoration object associated with the given toplevel.

        For objects of version 1, creating an xdg_toplevel_decoration from an
        xdg_toplevel which has a buffer attached or committed is a client
        error, and any attempts by a client to attach or manipulate a buffer
        prior to the first xdg_toplevel_decoration.configure event must also be
        treated as errors.

        For objects of version 2 or newer, creating an xdg_toplevel_decoration
        from an xdg_toplevel which has a buffer attached or committed is
        allowed. The initial decoration mode of the surface if a buffer is
        already attached depends on whether a xdg_toplevel_decoration object
        has been associated with the surface or not prior to this request.

        If an xdg_toplevel_decoration was associated with the surface, then
        destroyed without a surface commit, the previous decoration mode is
        retained.

        If no xdg_toplevel_decoration was associated with the surface prior to
        this request, or if a surface commit has been performed after a previous
        xdg_toplevel_decoration object associated with the surface was
        destroyed, the decoration mode is assumed to be client-side.

    @[1] id: c_wl_new_id [[zxdg_toplevel_decoration_v1]]
    @[2] toplevel: c_wl_object_id [[xdg_toplevel]]
   */
C_WL_REQUEST zxdg_decoration_manager_v1_get_toplevel_decoration(struct c_wl_connection *conn, union c_wl_arg *args);


 /* The configure event configures the effective decoration mode. The
        configured state should not be applied immediately. Clients must send an
        ack_configure in response to this event. See xdg_surface.configure and
        xdg_surface.ack_configure for details.

        A configure event can be sent at any time. The specified mode must be
        obeyed by the client. */
C_WL_EVENT zxdg_toplevel_decoration_v1_configure(struct c_wl_connection *conn, c_wl_object_id zxdg_toplevel_decoration_v1, enum zxdg_toplevel_decoration_v1_mode_enum mode);

   /* Switch back to a mode without any server-side decorations at the next
        commit, unless a new xdg_toplevel_decoration is created for the surface
        first. */
C_WL_REQUEST zxdg_toplevel_decoration_v1_destroy(struct c_wl_connection *conn, union c_wl_arg *args);

   /* Set the toplevel surface decoration mode. This informs the compositor
        that the client prefers the provided decoration mode.

        After requesting a decoration mode, the compositor will respond by
        emitting an xdg_surface.configure event. The client should then update
        its content, drawing it without decorations if the received mode is
        server-side decorations. The client must also acknowledge the configure
        when committing the new content (see xdg_surface.ack_configure).

        The compositor can decide not to use the client's mode and enforce a
        different mode instead.

        Clients whose decoration mode depend on the xdg_toplevel state may send
        a set_mode request in response to an xdg_surface.configure event and wait
        for the next xdg_surface.configure event to prevent unwanted state.
        Such clients are responsible for preventing configure loops and must
        make sure not to send multiple successive set_mode requests with the
        same decoration mode.

        If an invalid mode is supplied by the client, the invalid_mode protocol
        error is raised by the compositor.

    @[1] mode: enum zxdg_toplevel_decoration_v1_mode_enum
   */
C_WL_REQUEST zxdg_toplevel_decoration_v1_set_mode(struct c_wl_connection *conn, union c_wl_arg *args);

   /* Unset the toplevel surface decoration mode. This informs the compositor
        that the client doesn't prefer a particular decoration mode.

        This request has the same semantics as set_mode. */
C_WL_REQUEST zxdg_toplevel_decoration_v1_unset_mode(struct c_wl_connection *conn, union c_wl_arg *args);


#endif
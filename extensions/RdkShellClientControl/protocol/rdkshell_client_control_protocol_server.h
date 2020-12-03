/* 
 * Copyright 2020 RDK Management
 */

#ifndef RDKSHELL_CLIENT_CONTROL_SERVER_PROTOCOL_H
#define RDKSHELL_CLIENT_CONTROL_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct rdkshell_client_control;

extern const struct wl_interface rdkshell_client_control_interface;

/**
 * rdkshell_client_control - 
 * @set_client_bounds_and_scale: sets client window bounds and scale
 * @set_client_bounds: sets client window bounds
 * @set_client_scale: sets client window scale
 *
 * It can be used by the home app to control the window of an inactive
 * app.
 */
struct rdkshell_client_control_interface {
	/**
	 * set_client_bounds_and_scale - sets client window bounds and
	 *	scale
	 * @id: Id of the app
	 * @x: the left position of the surface in pixel screen
	 *	coordinates
	 * @y: the top position of the surface in pixel screen
	 *	coordinates
	 * @width: the width of the video surface in pixel screen
	 *	coordinates
	 * @height: the height of the video surface in pixel screen
	 *	coordinates
	 * @sx: horizontal scale factor, fixed point number
	 * @sy: vertical scale factor, fixed point number representing
	 *
	 * Sets client window bounds and scale.
	 */
	void (*set_client_bounds_and_scale)(struct wl_client *client,
					    struct wl_resource *resource,
					    const char *id,
					    int32_t x,
					    int32_t y,
					    uint32_t width,
					    uint32_t height,
					    wl_fixed_t sx,
					    wl_fixed_t sy);
	/**
	 * set_client_bounds - sets client window bounds
	 * @id: Id of the app
	 * @x: the left position of the surface in pixel screen
	 *	coordinates
	 * @y: the top position of the surface in pixel screen
	 *	coordinates
	 * @width: the width of the video surface in pixel screen
	 *	coordinates
	 * @height: the height of the video surface in pixel screen
	 *	coordinates
	 *
	 * Sets client window bounds. Scale of the client window is not
	 * changed.
	 */
	void (*set_client_bounds)(struct wl_client *client,
				  struct wl_resource *resource,
				  const char *id,
				  int32_t x,
				  int32_t y,
				  uint32_t width,
				  uint32_t height);
	/**
	 * set_client_scale - sets client window scale
	 * @id: Id of the app
	 * @sx: horizontal scale factor, fixed point number
	 * @sy: vertical scale factor, fixed point number representing
	 *
	 * Sets client window scale. Bounds of the client window are not
	 * changed.
	 */
	void (*set_client_scale)(struct wl_client *client,
				 struct wl_resource *resource,
				 const char *id,
				 wl_fixed_t sx,
				 wl_fixed_t sy);
};


#ifdef  __cplusplus
}
#endif

#endif

/* 
 * Copyright 2020 RDK Management
 */

#ifndef RDKSHELL_CLIENT_CONTROL_CLIENT_PROTOCOL_H
#define RDKSHELL_CLIENT_CONTROL_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct rdkshell_client_control;

extern const struct wl_interface rdkshell_client_control_interface;

#define RDKSHELL_CLIENT_CONTROL_SET_CLIENT_BOUNDS_AND_SCALE	0
#define RDKSHELL_CLIENT_CONTROL_SET_CLIENT_BOUNDS	1
#define RDKSHELL_CLIENT_CONTROL_SET_CLIENT_SCALE	2

static inline void
rdkshell_client_control_set_user_data(struct rdkshell_client_control *rdkshell_client_control, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) rdkshell_client_control, user_data);
}

static inline void *
rdkshell_client_control_get_user_data(struct rdkshell_client_control *rdkshell_client_control)
{
	return wl_proxy_get_user_data((struct wl_proxy *) rdkshell_client_control);
}

static inline void
rdkshell_client_control_destroy(struct rdkshell_client_control *rdkshell_client_control)
{
	wl_proxy_destroy((struct wl_proxy *) rdkshell_client_control);
}

static inline void
rdkshell_client_control_set_client_bounds_and_scale(struct rdkshell_client_control *rdkshell_client_control, const char *id, int32_t x, int32_t y, uint32_t width, uint32_t height, wl_fixed_t sx, wl_fixed_t sy)
{
	wl_proxy_marshal((struct wl_proxy *) rdkshell_client_control,
			 RDKSHELL_CLIENT_CONTROL_SET_CLIENT_BOUNDS_AND_SCALE, id, x, y, width, height, sx, sy);
}

static inline void
rdkshell_client_control_set_client_bounds(struct rdkshell_client_control *rdkshell_client_control, const char *id, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	wl_proxy_marshal((struct wl_proxy *) rdkshell_client_control,
			 RDKSHELL_CLIENT_CONTROL_SET_CLIENT_BOUNDS, id, x, y, width, height);
}

static inline void
rdkshell_client_control_set_client_scale(struct rdkshell_client_control *rdkshell_client_control, const char *id, wl_fixed_t sx, wl_fixed_t sy)
{
	wl_proxy_marshal((struct wl_proxy *) rdkshell_client_control,
			 RDKSHELL_CLIENT_CONTROL_SET_CLIENT_SCALE, id, sx, sy);
}

#ifdef  __cplusplus
}
#endif

#endif

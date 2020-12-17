/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

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

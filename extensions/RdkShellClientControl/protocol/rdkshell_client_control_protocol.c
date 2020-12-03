/* 
 * Copyright 2020 RDK Management
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"


static const struct wl_interface *types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const struct wl_message rdkshell_client_control_requests[] = {
	{ "set_client_bounds_and_scale", "siiuuff", types + 0 },
	{ "set_client_bounds", "siiuu", types + 0 },
	{ "set_client_scale", "sff", types + 0 },
};

WL_EXPORT const struct wl_interface rdkshell_client_control_interface = {
	"rdkshell_client_control", 1,
	3, rdkshell_client_control_requests,
	0, NULL,
};


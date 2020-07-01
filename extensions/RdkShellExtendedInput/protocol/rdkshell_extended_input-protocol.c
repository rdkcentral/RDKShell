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
};

static const struct wl_message rdkshell_extended_input_events[] = {
	{ "key", "uuuiu", types + 0 },
	{ "touchpad", "uuuiiu", types + 0 },
	{ "slider", "uuuiu", types + 0 },
};

WL_EXPORT const struct wl_interface rdkshell_extended_input_interface = {
	"rdkshell_extended_input", 1,
	0, NULL,
	3, rdkshell_extended_input_events,
};


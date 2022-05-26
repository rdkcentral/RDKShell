/* Generated by wayland-scanner 1.18.0 */

/*
 * Copyright 2022 RDK Management
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"


static const struct wl_interface *rdkshell_shell_types[] = {
	NULL,
	NULL,
};

static const struct wl_message rdkshell_shell_requests[] = {
	{ "set_render_flags", "uu", rdkshell_shell_types + 0 },
	{ "set_visibility_policy", "2u", rdkshell_shell_types + 0 },
	{ "set_suspended", "3", rdkshell_shell_types + 0 },
};

static const struct wl_message rdkshell_shell_events[] = {
	{ "client_state_changed", "u", rdkshell_shell_types + 0 },
	{ "close", "", rdkshell_shell_types + 0 },
};

WL_EXPORT const struct wl_interface rdkshell_shell_interface = {
	"rdkshell_shell", 3,
	3, rdkshell_shell_requests,
	2, rdkshell_shell_events,
};


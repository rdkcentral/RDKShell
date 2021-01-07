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


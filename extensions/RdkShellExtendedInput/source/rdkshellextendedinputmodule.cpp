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

#include "rdkshellextendedinputmodule.h"
#include "rdkshell_extended_input-server-protocol.h"


#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include "../../../compositorcontroller.h"


RdkShellExtendedInputModule::RdkShellExtendedInputModule(std::shared_ptr<RdkShell::RdkCompositor> &client,
                               struct wl_client *wlClient,
                               struct wl_resource *wlRdkShellInput)
    : mClient(client)
    , mListenerTag(-1)
    , mNotifierPipeFd(-1)
    , mNotifierReadPipeFd(-1)
    , mNotifierSource(nullptr)
    , mWaylandThreadId(std::this_thread::get_id())
{
    // create a pipe for communicating between the client input event notifier
    // and our wayland event loop
    int fds[2];
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK | O_DIRECT) < 0)
    {
        std::cout << "Error: failed to create event pipe";
    }
    else
    {
        // store the write end of the pipe
        mNotifierPipeFd = fds[1];

        // add the notification pipe to the display event loop
        wl_event_loop *wlEventLoop = wl_display_get_event_loop(wl_client_get_display(wlClient));
        if (!wlEventLoop)
        {
            std::cout << "Error: failed to get wayland event loop";
        }
        else
        {
            mNotifierSource = wl_event_loop_add_fd(wlEventLoop, fds[0], WL_EVENT_READABLE,
                                                   &RdkShellExtendedInputModule::onWaylandInputEventNotification,
                                                   wlRdkShellInput);
        }

        // although the wl_loop dups a copy of the fd and uses that for poll,
        // it still passes the original fd to the callback, so we have to keep
        // it open
        mNotifierReadPipeFd = fds[0];
    }

    // now can register a listener with the westeros client for state changes
    mListenerTag =
        client->registerInputEventListener(
            std::bind(&RdkShellExtendedInputModule::onWesterosInputEvent, this, std::placeholders::_1));
}


RdkShellExtendedInputModule::~RdkShellExtendedInputModule()
{
    if (std::this_thread::get_id() != mWaylandThreadId)
        std::cout << "Error: destructor called from invalid thread";

    if (mListenerTag > 0)
    {
        std::shared_ptr<RdkShell::RdkCompositor> client = mClient.lock();
        if (client)
        {
            client->unregisterInputEventListener(mListenerTag);
        }
    }

    if (mNotifierSource)
    {
        wl_event_source_remove(mNotifierSource);
        mNotifierSource = nullptr;
    }

    if ((mNotifierPipeFd >= 0) && (close(mNotifierPipeFd) != 0))
    {
        std::cout << "Error: failed to close pipe";
    }
    if ((mNotifierReadPipeFd >= 0) && (close(mNotifierReadPipeFd) != 0))
    {
        std::cout << "Error: failed to close pipe";
    }
}

/*

    Called from the WesterosClient object when an input event should be sent to
    the client.  We need to pass this on to the wayland server so that it can
    send the event to the clients, however wayland is not thread safe and it's
    running in a different thread, so we have to put the event into a pipe which
    will wake the wayland event loop and call onWaylandInputEventNotification(...).

 */
void RdkShellExtendedInputModule::onWesterosInputEvent(const RdkShell::InputEvent &event)
{
    if (TEMP_FAILURE_RETRY(write(mNotifierPipeFd, &event, sizeof(event))) != sizeof(event))
    {
        std::cout << "Error: failed to write to input event to pipe";
    }
}


static inline rdkshell_extended_input_key_state convertKeyState(RdkShell::InputEvent::Details::Key::State state)
{
    switch (state)
    {
        case RdkShell::InputEvent::Details::Key::Pressed:
            return RDKSHELL_EXTENDED_INPUT_KEY_STATE_PRESSED;
        case RdkShell::InputEvent::Details::Key::Released:
            return RDKSHELL_EXTENDED_INPUT_KEY_STATE_RELEASED;
        case RdkShell::InputEvent::Details::Key::VirtualPress:
            return RDKSHELL_EXTENDED_INPUT_KEY_STATE_VIRTUAL_PRESS;
        case RdkShell::InputEvent::Details::Key::VirtualRelease:
            return RDKSHELL_EXTENDED_INPUT_KEY_STATE_VIRTUAL_RELEASE;
    }
}


static inline rdkshell_extended_input_touchpad_state convertTouchpadState(RdkShell::InputEvent::Details::TouchPad::State state)
{
    switch (state)
    {
        case RdkShell::InputEvent::Details::TouchPad::Down:
            return RDKSHELL_EXTENDED_INPUT_TOUCHPAD_STATE_DOWN;
        case RdkShell::InputEvent::Details::TouchPad::Up:
            return RDKSHELL_EXTENDED_INPUT_TOUCHPAD_STATE_UP;
        case RdkShell::InputEvent::Details::TouchPad::Click:
            return RDKSHELL_EXTENDED_INPUT_TOUCHPAD_STATE_CLICK;
    }
}


static inline rdkshell_extended_input_slider_state convertSliderState(RdkShell::InputEvent::Details::Slider::State state)
{
    switch (state)
    {
        case RdkShell::InputEvent::Details::Slider::Down:
            return RDKSHELL_EXTENDED_INPUT_SLIDER_STATE_DOWN;
        case RdkShell::InputEvent::Details::Slider::Up:
            return RDKSHELL_EXTENDED_INPUT_SLIDER_STATE_UP;
     }
}


int RdkShellExtendedInputModule::onWaylandInputEventNotification(int fd, uint32_t mask, void *data)
{
    // attempt to read the new state
    RdkShell::InputEvent event;
    if (TEMP_FAILURE_RETRY(::read(fd, &event, sizeof(event))) != sizeof(event))
    {
        if (errno == EAGAIN)
            return 0;

        std::cout << "Error: failed to read from input event pipe";
        return -1;
    }

    // send the input event out the interface
    wl_resource *resource = reinterpret_cast<wl_resource*>(data);

    std::cout << "RdkShellExtendedInputModule: send rdkshell_extended_input: {" <<
                 " type: 0x" <<  std::hex << std::setw(2) << std::setfill('0') << event.type <<
                 ", code: 0x" << std::hex << std::setw(2) << std::setfill('0') << event.details.key.code <<
                 ", state: " << (event.details.key.state == RdkShell::InputEvent::Details::Key::Pressed ? "Pressed" : "Released") <<
                 ", deviceId (metadata): 0x" << std::hex << std::setw(8) << std::setfill('0') <<  event.deviceId <<
                 " }" << std::endl;

    switch (event.type)
    {
        case RdkShell::InputEvent::KeyEvent:
            rdkshell_extended_input_send_key(resource, 0,
                                event.timestampMs,
                                event.deviceId,
                                event.details.key.code,
                                convertKeyState(event.details.key.state));
            break;

        case RdkShell::InputEvent::TouchPadEvent:
            rdkshell_extended_input_send_touchpad(resource, 0,
                                     event.timestampMs,
                                     event.deviceId,
                                     event.details.touchpad.x,
                                     event.details.touchpad.y,
                                     convertTouchpadState(event.details.touchpad.state));
            break;

        case RdkShell::InputEvent::SliderEvent:
            rdkshell_extended_input_send_slider(resource, 0,
                                   event.timestampMs,
                                   event.deviceId,
                                   event.details.slider.x,
                                   convertSliderState(event.details.slider.state));
            break;

        case RdkShell::InputEvent::InvalidEvent:
            break;
    }

    return 0;
}


static void rdkShellExtendedInputDestroy(struct wl_resource *resource)
{
    auto *rdkShellInput = reinterpret_cast<RdkShellExtendedInputModule*>(wl_resource_get_user_data(resource));

    wl_resource_set_user_data(resource, nullptr);
    delete rdkShellInput;

    // do we need to call the following ?
    // wl_resource_destroy(resource);
}


static void rdkShellExtendedInputBind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    WstCompositor *ctx = reinterpret_cast<WstCompositor*>(data);
    std::string displayName = WstCompositorGetDisplayName(ctx);

    std::shared_ptr<RdkShell::RdkCompositor> rdkCompositor = RdkShell::CompositorController::getCompositor(displayName);


    if (!rdkCompositor)
    {
        std::cout <<"Error: missing rdkCompositor object for compositor instance";
        wl_client_post_no_memory(client);
        return;
    }

    // create the shell interface
    struct wl_resource *resource = wl_resource_create(client, &rdkshell_extended_input_interface,
                                                      std::min<int>(version, 1), id);
    if (!resource)
    {
        std::cout << "Error: failed to create rdkshell_extended_input_interface resource";
        wl_client_post_no_memory(client);
        return;
    }


    // create the context for the shell resource
    RdkShellExtendedInputModule *rdkShellInput = new RdkShellExtendedInputModule(rdkCompositor, client, resource);

    // set the implementation context
    wl_resource_set_implementation(resource, nullptr, rdkShellInput, rdkShellExtendedInputDestroy);
}




extern "C"
{

    bool moduleInit(WstCompositor *ctx, struct wl_display *display)
    {
        std:: cout << "moduleInit called for rdkShellExtendedInput module";


        // register our rdkshell_extended_input interface with wayland
        struct wl_global *shell = wl_global_create(display, &rdkshell_extended_input_interface,
                                                   1, ctx, rdkShellExtendedInputBind);
        if (!shell)
        {
            std::cout << " Error: failed to register rdkshell_extended_input_interface shell interface";
            return false;
        }

        return true;
    }

    void moduleTerm(WstCompositor *ctx)
    {
        std::cout << "moduleTerm called for rdkShellExtendedInput module";

        // wl_global_destroy( shell->wl_simple_shell_global );
    }

}

#include "logger.h"

#include "rdkshellclientcontrolmodule.h"
#include "rdkshell_client_control_protocol_server.h"

#include "../../../compositorcontroller.h"
#include "wayland-server.h"


RdkShellClientControlModule::RdkShellClientControlModule(std::shared_ptr<RdkShell::RdkCompositor> &client)
    : mClient(client)
{
    RdkShell::Logger::log(RdkShell::LogLevel::Information, "RdkShellClientControlModule constructor\n");
}

RdkShellClientControlModule::~RdkShellClientControlModule()
{
    RdkShell::Logger::log(RdkShell::LogLevel::Information, "RdkShellClientControlModule destructor\n");
}

std::shared_ptr<RdkShell::RdkCompositor> RdkShellClientControlModule::client() const
{
    return mClient.lock();
}

void rdkShellClientControlSetClientBoundsAndScale(struct wl_client *client,
                struct wl_resource *resource,
                const char *id,
                int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height,
                wl_fixed_t sx,
				wl_fixed_t sy)
{
    RdkShell::Logger::log(RdkShell::LogLevel::Information,
        "%s appId: %s, x: %d, y: %d, w: %d, h: %d, sx: %f, sy: %f\n",
        __func__, id, x, y, width, height, wl_fixed_to_double(sx), wl_fixed_to_double(sy)); 

    RdkShell::CompositorController::setBounds(id, x, y, width, height);
    RdkShell::CompositorController::setScale(id, wl_fixed_to_double(sx), wl_fixed_to_double(sy));
}

void rdkShellClientControlSetClientBounds(struct wl_client *client,
                struct wl_resource *resource,
                const char *id,
                int32_t x,
                int32_t y,
                uint32_t width,
                uint32_t height)
{
    RdkShell::Logger::log(RdkShell::LogLevel::Information,
        "%s appId: %s, x: %d, y: %d, w: %d, h: %d\n", __func__, id, x, y, width, height); 

    RdkShell::CompositorController::setBounds(id, x, y, width, height);
}

void rdkShellClientControlSetClientScale(struct wl_client *client,
                struct wl_resource *resource,
                const char *id,
                wl_fixed_t sx,
				wl_fixed_t sy)
{
    RdkShell::Logger::log(RdkShell::LogLevel::Information,
        "%s appId: %s, sx: %f, sy: %f\n", __func__, id, wl_fixed_to_double(sx), wl_fixed_to_double(sy)); 

    RdkShell::CompositorController::setScale(id, wl_fixed_to_double(sx), wl_fixed_to_double(sy));
}

struct rdkshell_client_control_interface rdkShellClientControlInterface =
{
    rdkShellClientControlSetClientBoundsAndScale,
    rdkShellClientControlSetClientBounds,
    rdkShellClientControlSetClientScale
};


static void rdkShellClientControlDestroy(struct wl_resource *resource)
{
    auto *clientControlModule = reinterpret_cast<RdkShellClientControlModule*>(wl_resource_get_user_data(resource));

    wl_resource_set_user_data(resource, nullptr);
    delete clientControlModule;

    // do we need to call the following ?
    // wl_resource_destroy(resource);
}

static void rdkShellClientControlBind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    WstCompositor *ctx = reinterpret_cast<WstCompositor*>(data);
    std::string displayName = WstCompositorGetDisplayName(ctx);
    RdkShell::Logger::log(RdkShell::LogLevel::Information, "%s displayName: %s\n", __func__, displayName);

    std::shared_ptr<RdkShell::RdkCompositor> rdkCompositor = RdkShell::CompositorController::getCompositor(displayName);
    if (!rdkCompositor)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Error,
            "Error: missing rdkCompositor object for compositor instance\n");
        wl_client_post_no_memory(client);
        return;
    }

    // create the shell interface
    struct wl_resource *resource = wl_resource_create(client, &rdkshell_client_control_interface,
                                                      std::min<int>(version, 1), id);
    if (!resource)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Error,
            "Error: failed to create rdkshell_client_control_interface resource\n");
        wl_client_post_no_memory(client);
        return;
    }

    // create the context for the shell resource
    RdkShellClientControlModule *clientControlModule = new RdkShellClientControlModule(rdkCompositor);

    // set the implementation context
    wl_resource_set_implementation(resource, &rdkShellClientControlInterface, clientControlModule, rdkShellClientControlDestroy);
}


extern "C"
{

    bool moduleInit(WstCompositor *ctx, struct wl_display *display)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "moduleInit called for rdkShellClientControl module\n");

        struct wl_global *shell = wl_global_create(display, &rdkshell_client_control_interface,
                                                   1, ctx, rdkShellClientControlBind);
        if (!shell)
        {
            RdkShell::Logger::log(RdkShell::LogLevel::Error,
                "Error: failed to register rdkshell_client_control_interface shell interface\n");
            return false;
        }

        return true;
    }

    void moduleTerm(WstCompositor *ctx)
    {
        RdkShell::Logger::log(RdkShell::LogLevel::Information, "moduleTerm called for rdkShellClientControl module\n");
    }

}
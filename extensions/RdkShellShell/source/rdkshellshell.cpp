#include "../../RdkShellLink/rdkshelllink.h"
#include <wayland-server.h>
#include <westeros-compositor.h>
#include "../protocol/rdkshell_shell_protocol_server.h"
#include <stdio.h>

#include "../../../rdkcompositor.h"
#include "../../../compositorcontroller.h"
namespace
{
    std::map<std::string, struct wl_resource*> displayResourceMap;
    std::map<std::string, uint32_t> pendingStateChangeMap;

    class Shell
    {
    public:
        Shell(wl_display *display, std::string displayName, WstCompositor* ctx)
            : info(link(display)), mDisplayName(displayName), mListenerTag(-1), mClient(NULL), mCtx(ctx)
        {
        }

        ~Shell()
        {
            if (mListenerTag > 0)
            {
                if (mClient)
                {
                    mClient->unregisterStateChangeEventListener(mListenerTag);
                }
            }
        }

        void setListener(std::shared_ptr<RdkShell::RdkCompositor> client)
        {
            mClient = client;
            mListenerTag =
                mClient->registerStateChangeEventListener(
                    std::bind(&Shell::onStateChangeEvent, this, std::placeholders::_1));
        }

        void onStateChangeEvent(uint32_t state)
        {
            if (displayResourceMap.find(mDisplayName) != displayResourceMap.end())
            {
                printf("Sending 1 state change event ... [%s] \n", mDisplayName.c_str()); fflush(stdout);
                rdkshell_shell_send_client_state_changed(displayResourceMap[mDisplayName], state);
            }
            else
	    {
                printf("Adding pending state change event ... [%s] \n", mDisplayName.c_str()); fflush(stdout);
                pendingStateChangeMap[mDisplayName] = state;
	    }
        }

        void SetVisibilityPolicy(bool visible)
        {
            std::lock_guard<std::mutex> guard(info->mutex);
            info->visible_by_default = visible;
        }

        void SetOverlayFlag(uint32_t surface_id, bool overlay)
        {
            std::lock_guard<std::mutex> guard(info->mutex);
            info->surface[surface_id].overlay = overlay;
        }

        std::string displayName() { return mDisplayName; }
        WstCompositor* context() { return mCtx; }
    private:
        std::shared_ptr<DisplayInfo> info;
        std::string mDisplayName;
        int mListenerTag;
        std::shared_ptr<RdkShell::RdkCompositor> mClient;
        WstCompositor* mCtx;
    };

    void set_surface_flags(struct wl_client *client, struct wl_resource *resource, uint32_t surface_id, uint32_t flags)
    {
        Shell *shell = (Shell *)wl_resource_get_user_data(resource);
        shell->SetOverlayFlag(surface_id, flags & RDKSHELL_SHELL_RDKSHELL_RENDER_FLAGS_OVERLAY);
    }

    void set_visibility_policy(struct wl_client *client, struct wl_resource *resource, uint32_t policy)
    {
        Shell *shell = (Shell *)wl_resource_get_user_data(resource);
        shell->SetVisibilityPolicy(policy == RDKSHELL_SHELL_VISIBILITY_POLICY_VISIBLE_BY_DEFAULT);
    }

    void set_suspended(struct wl_client *client, struct wl_resource *resource)
    {
        Shell *shell = (Shell *)wl_resource_get_user_data(resource);
        const std::string displayName = WstCompositorGetDisplayName(shell->context());
        std::shared_ptr<RdkShell::RdkCompositor> rdkCompositor = RdkShell::CompositorController::getCompositor(displayName);
        if (NULL != rdkCompositor)
        {
            rdkCompositor->suspendApplication();
        }
    }

    const struct rdkshell_shell_interface ifc = {
        set_surface_flags,
        set_visibility_policy,
        set_suspended,
    };

    void rdkshell_shell_destroy(struct wl_resource *resource)
    {
        Shell *shell = (Shell *)wl_resource_get_user_data(resource);
        displayResourceMap.erase(shell->displayName());
        delete shell;

        // wl_resource_destroy(resource); // Is this needed? Are we called from wl_resource_destroy?
    }

    void rdkshell_shell_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        struct wl_resource *resource = wl_resource_create(client, &rdkshell_shell_interface, std::min<int>(version, 3), id);
        if (!resource)
            return wl_client_post_no_memory(client);

        Shell* shell = reinterpret_cast<Shell*>(data);
        std::string displayName("");
        if (NULL != shell)
        {
            displayName = WstCompositorGetDisplayName(shell->context());
	    
            std::shared_ptr<RdkShell::RdkCompositor> rdkCompositor = RdkShell::CompositorController::getCompositor(displayName);
            if (NULL != rdkCompositor)
            {
                shell->setListener(rdkCompositor);
            }
            displayResourceMap[displayName] = resource;
        }
        wl_resource_set_implementation(resource, &ifc, data, rdkshell_shell_destroy);
        if ((!displayName.empty()) && (pendingStateChangeMap.find(displayName) != pendingStateChangeMap.end()) && (displayResourceMap.find(displayName) != displayResourceMap.end()))
    	{
            printf("Sending pending state change events [%s] [%d]\n", displayName.c_str(), pendingStateChangeMap[displayName]); fflush(stdout);
            rdkshell_shell_send_client_state_changed(displayResourceMap[displayName], pendingStateChangeMap[displayName]);
            pendingStateChangeMap.erase(displayName);
	    }
    }

}

extern "C"
{
    bool moduleInit(WstCompositor *ctx, struct wl_display *display)
    {
        std::string displayName = WstCompositorGetDisplayName(ctx);
        return wl_global_create(display, &rdkshell_shell_interface, 3, new Shell(display, displayName, ctx), rdkshell_shell_bind);
    }

    void moduleTerm(WstCompositor *)
    {
        // do nothing
    }
}

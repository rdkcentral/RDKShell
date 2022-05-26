#include <dlfcn.h>
#include <westeros-render.h>
#include <map>
#include <cstdint>

#include "../RdkShellLink/rdkshelllink.h"
#include "rdkshellrenderer.h"

namespace
{
	class Renderer
	{
	public:
		Renderer(WstRenderer *renderer, void *lib_handle)
			: renderer(renderer), lib_handle(lib_handle), info(link(renderer->display))
		{
			renderer->nativeWindow = this; // NOTE: This is a really bad idea, this variable happens to be unused for now

			renderTerm = renderer->renderTerm;
			updateScene = renderer->updateScene;
			surfaceCreate = renderer->surfaceCreate;
			surfaceDestroy = renderer->surfaceDestroy;
			surfaceSetVisible = renderer->surfaceSetVisible;

			renderer->renderTerm = &Renderer::Terminate;
			renderer->updateScene = &Renderer::UpdateScene;
			renderer->surfaceCreate = &Renderer::CreateSurface;
			renderer->surfaceDestroy = &Renderer::DestroySurface;
			renderer->surfaceSetVisible = &Renderer::SetVisible;
		}

		~Renderer()
		{
			renderer->nativeWindow = nullptr;
			renderTerm(renderer);
			dlclose(lib_handle);
		}

	private:
		void SetExclusiveVisibility(bool overlay)
		{
			std::lock_guard<std::mutex> guard(info->mutex);
			for (const auto &it : surface_map)
			{
				const auto &surface_info = info->surface[it.second];
				surfaceSetVisible(renderer, it.first, surface_info.visible && (surface_info.overlay == overlay));
			}
		}

		bool RegisterSurface(WstRenderSurface *surface)
		{
			std::lock_guard<std::mutex> guard(info->mutex);

			uint32_t id = surface_id++;
			surface_map[surface] = id;

			info->surface[id] = {info->visible_by_default, false};
			return info->visible_by_default;
		}

		void UnregisterSurface(WstRenderSurface *surface)
		{
			std::lock_guard<std::mutex> guard(info->mutex);
			info->surface.erase(surface_map[surface]);
			surface_map.erase(surface);
		}

		bool UpdateVisibility(WstRenderSurface *surface, bool visible)
		{
			std::lock_guard<std::mutex> guard(info->mutex);
			auto &surface_info = info->surface[surface_map[surface]];
			surface_info.visible = visible;

			return !surface_info.overlay;
		}

	private:
		void *lib_handle;
		std::shared_ptr<DisplayInfo> info;
		uint32_t surface_id = 1;
		std::map<WstRenderSurface *, uint32_t> surface_map;

		WstRenderer *renderer;
		WSTMethodRenderTerm renderTerm;
		WSTMethodUpdateScene updateScene;
		WSTMethodSurfaceCreate surfaceCreate;
		WSTMethodSurfaceDestroy surfaceDestroy;
		WSTMethodSurfaceSetVisible surfaceSetVisible;

	private:
		static void Terminate(WstRenderer *renderer)
		{
			Renderer *self = (Renderer *)renderer->nativeWindow;
			delete self;
		}

		static void UpdateScene(WstRenderer *renderer)
		{
			Renderer *self = (Renderer *)renderer->nativeWindow;
			self->SetExclusiveVisibility(renderer->hints & HINT_OVERLAY_ONLY);
			self->updateScene(renderer);
		}

		static WstRenderSurface *CreateSurface(WstRenderer *renderer)
		{
			Renderer *self = (Renderer *)renderer->nativeWindow;
			WstRenderSurface *surface = self->surfaceCreate(renderer);

			bool visible = self->RegisterSurface(surface);
			self->surfaceSetVisible(renderer, surface, visible);
			return surface;
		}

		static void DestroySurface(WstRenderer *renderer, WstRenderSurface *surface)
		{
			Renderer *self = (Renderer *)renderer->nativeWindow;
			self->UnregisterSurface(surface);
			self->surfaceDestroy(renderer, surface);
		}

		static void SetVisible(WstRenderer *renderer, WstRenderSurface *surface, bool visible)
		{
			Renderer *self = (Renderer *)renderer->nativeWindow;
			if (self->UpdateVisibility(surface, visible))
			{
				self->surfaceSetVisible(renderer, surface, visible);
			}
		}
	};

}

extern "C"
{
	int renderer_init(WstRenderer *renderer, int argc, char **argv)
	{
		auto handle = dlopen("/usr/lib/libwesteros_render_embedded.so.0.0.0", RTLD_NOW);
		if (!handle)
			return -1;

		auto real_init = (WSTMethodRenderInit)dlsym(handle, "renderer_init");
		if (!real_init)
		{
			dlclose(handle);
			return -1;
		}

		int rc = real_init(renderer, argc, argv);
		if (rc < 0)
		{
			dlclose(handle);
			return rc;
		}

		new Renderer(renderer, handle);
		return 0;
	}
}

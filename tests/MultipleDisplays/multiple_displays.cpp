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

/* This file contains a test application for client control extension for RDKShell.
*  It is based on westeros-test.cpp. It was stripped out of the features not needed in this test.
*  To enable building it in RDKShell set the RDKSHELL_BUILD_CLIENT_CONTROL_EXTENSION_TEST build 
*  time option.
*
*  Some code is derived from weston and is:
*  Copyright © 2010 Intel Corporation
*  Copyright © 2011 Benjamin Franzke
*  Copyright © 2012-2013 Collabora, Ltd.
*  Licensed under the MIT License
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "wayland-client.h"
#include "wayland-egl.h"


namespace {

const int planeWidth  = 1920;
const int planeHeight = 1080;

int g_running = 0;

long long currentTimeMillis()
{
   long long timeMillis;
   struct timeval tv;

   gettimeofday(&tv, NULL);
   timeMillis = tv.tv_sec * 1000 + tv.tv_usec / 1000;

   return timeMillis;
}


struct Wayland
{
   std::string name;

   wl_display    *display;
   wl_registry   *registry;
   wl_compositor *compositor;
   wl_surface    *surface;
   wl_egl_window *native;
   wl_callback   *frameCallback;

   Wayland(std::string display_name);
   ~Wayland();
};

void registryHandleGlobal(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
   Wayland *wl = (Wayland*) data;

   printf("client_control_extension_test: registry: id %d interface (%s) version %d\n", id, interface, version);

   if (strcmp(interface, wl_compositor_interface.name) == 0) {
      wl->compositor = (struct wl_compositor*) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
      printf("compositor %p\n", wl->compositor);
   }
}

void registryHandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
}

wl_registry_listener registryListener =
{
   registryHandleGlobal,
   registryHandleGlobalRemove
};


struct Egl
{
	Wayland&   wl;

	EGLDisplay display;
	EGLConfig  config;
	EGLSurface surfaceWindow;
	EGLContext context;

	Egl(Wayland& wl);
	~Egl();

	void switch_context();
};


struct Gl
{
	GLuint rotation_uniform;
	GLuint position;
	GLuint color;
	uint32_t speed_div = 10;

	long long startTime, currTime;

	constexpr static const char *vert_shader_text =
	   "uniform mat4 rotation;\n"
	   "attribute vec4 pos;\n"
	   "attribute vec4 color;\n"
	   "varying vec4 v_color;\n"
	   "void main() {\n"
	   "  gl_Position = rotation * pos;\n"
	   "  v_color = color;\n"
	   "}\n";

	constexpr static const char *frag_shader_text =
	   "precision mediump float;\n"
	   "varying vec4 v_color;\n"
	   "void main() {\n"
	   "  gl_FragColor = v_color;\n"
	   "}\n";

	Gl();

	void render();

	static GLuint createShader(GLenum shaderType, const char *shaderSource);
};


struct Application
{
	Wayland wl;
	Egl     egl;
	Gl      gl;

	bool redraw = true;

	Application(std::string name);

	bool step();
	void render();
};

void redraw(void *data, struct wl_callback *callback, uint32_t time)
{
   Application *app = (Application*) data;
   app->redraw = true;

   wl_callback_destroy(callback);
}

wl_callback_listener frameListener =
{
   redraw
};

}


Wayland::Wayland(std::string display_name)
	: name(display_name)
{
	display = wl_display_connect(name.c_str());
	printf("wl_display_connect: display=%p\n", display);
	if (!display) throw std::runtime_error("wl_display_connect failure");

	registry = wl_display_get_registry(display);
	if (!registry) throw std::runtime_error("wl_display_get_registry failure");

	wl_registry_add_listener(registry, &registryListener, this);
	wl_display_roundtrip(display); // NOTE: blocks until all client requests are handled by the server (attaching the registry listener and receiving the current registry state)

	surface = wl_compositor_create_surface(compositor);
	if (!surface) throw std::runtime_error("wl_compositor_create_surface failure");

	native = wl_egl_window_create(surface, planeWidth, planeHeight);
	if (!native) throw std::runtime_error("wl_egl_window_create failure");
}

Wayland::~Wayland()
{
   if (native)     wl_egl_window_destroy(native);
   if (surface)    wl_surface_destroy(surface);
   if (compositor) wl_compositor_destroy(compositor);
   if (registry)   wl_registry_destroy(registry);
   if (display)    wl_display_disconnect(display);
}


Egl::Egl(Wayland& wl)
	: wl(wl)
{
	display = eglGetDisplay(wl.display);
	if (display == EGL_NO_DISPLAY) throw std::runtime_error("eglGetDisplay failure");

	EGLint major, minor;
	if (!eglInitialize(display, &major, &minor)) throw std::runtime_error("eglInitialize failure");
	printf("eglInitiialize: major: %d minor: %d\n", major, minor);

	EGLint configCount;
	if (!eglGetConfigs(display, NULL, 0, &configCount)) throw std::runtime_error("eglGetConfigs failure");
	printf("Number of EGL configurations: %d\n", configCount);


	EGLint attr[] = {
		   EGL_RED_SIZE,        8,
		   EGL_GREEN_SIZE,      8,
		   EGL_BLUE_SIZE,       8,
		   EGL_DEPTH_SIZE,      0,
		   EGL_STENCIL_SIZE,    0,
		   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
		   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		   EGL_NONE
	};

	auto eglConfigs = std::make_unique<EGLConfig[]>(configCount);

	if (!eglChooseConfig(display, attr, eglConfigs.get(), configCount, &configCount)) throw std::runtime_error("eglChooseConfig failure");
	printf("eglChooseConfig: matching configurations: %d\n", configCount);


	int i;
	for(i = 0; i < configCount; ++i)
	{
	  EGLint redSize, greenSize, blueSize, alphaSize, depthSize;
	  eglGetConfigAttrib(display, eglConfigs[i], EGL_RED_SIZE,   &redSize);
	  eglGetConfigAttrib(display, eglConfigs[i], EGL_GREEN_SIZE, &greenSize);
	  eglGetConfigAttrib(display, eglConfigs[i], EGL_BLUE_SIZE,  &blueSize);
	  eglGetConfigAttrib(display, eglConfigs[i], EGL_ALPHA_SIZE, &alphaSize);
	  eglGetConfigAttrib(display, eglConfigs[i], EGL_DEPTH_SIZE, &depthSize);

	  printf("config %d: red: %d green: %d blue: %d alpha: %d depth: %d\n",
			  i, redSize, greenSize, blueSize, alphaSize, depthSize);
	  if ((redSize == 8) &&
		   (greenSize == 8) &&
		   (blueSize == 8) &&
		   (alphaSize == 8) &&
		   (depthSize >= 0))
	  {
		 printf("choosing config %d\n", i);
		 config = eglConfigs[i];
		 break;
	  }
	}

	if (i == configCount) throw std::runtime_error("no suitable configuration available");


	EGLint ctxAttrib[] = {
	   EGL_CONTEXT_CLIENT_VERSION, 2,
	   EGL_NONE
	};

	context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttrib);
	if (context == EGL_NO_CONTEXT) throw std::runtime_error("eglCreateContext failure");

	surfaceWindow = eglCreateWindowSurface(display, config, wl.native, NULL);
	if (surfaceWindow == EGL_NO_SURFACE) throw std::runtime_error("eglCreateWindowSurface failure");

	switch_context();
}

Egl::~Egl()
{
	if (wl.display)
	{
	  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	  eglDestroySurface(display, surfaceWindow);
	  eglTerminate(display);
	  eglReleaseThread();
	}
}

void Egl::switch_context()
{
	eglMakeCurrent(display, surfaceWindow, surfaceWindow, context);
}


Gl::Gl()
{
	startTime = currentTimeMillis();

   GLuint fragment, vertex;
   GLuint program;
   GLint status;

   fragment = createShader(GL_FRAGMENT_SHADER, frag_shader_text);
   vertex = createShader(GL_VERTEX_SHADER, vert_shader_text);

   program = glCreateProgram();
   glAttachShader(program, fragment);
   glAttachShader(program, vertex);
   glLinkProgram(program);

   glGetProgramiv(program, GL_LINK_STATUS, &status);
   if (!status)
   {
	  char log[1000];
	  GLsizei len;
	  glGetProgramInfoLog(program, 1000, &len, log);
	  fprintf(stderr, "Error: linking:\n%*s\n", len, log);
	  return;
   }

   glUseProgram(program);

   position = 0;
   color = 1;

   glBindAttribLocation(program, position, "pos");
   glBindAttribLocation(program, color, "color");
   glLinkProgram(program);

   rotation_uniform = glGetUniformLocation(program, "rotation");
}

void Gl::render()
{
   static const GLfloat verts[3][2] = {
	  { -0.5, -0.5 },
	  {  0.5, -0.5 },
	  {  0,    0.5 }
   };
   static const GLfloat colors[3][4] = {
	  { 1, 0, 0, 1.0 },
	  { 0, 1, 0, 1.0 },
	  { 0, 0, 1, 1.0 }
   };
   GLfloat angle;
   GLfloat rotation[4][4] = {
	  { 1, 0, 0, 0 },
	  { 0, -1, 0, 0 },
	  { 0, 0, 1, 0 },
	  { 0, 0, 0, 1 }
   };

   EGLint rect[4];

   glViewport(0, 0, planeWidth, planeHeight);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   currTime = currentTimeMillis();

   angle = ((currTime - startTime) / speed_div) % 360 * M_PI / 180.0;
   rotation[0][0] =  cos(angle);
   rotation[0][1] =  sin(angle);
   rotation[1][0] = -sin(angle);
   rotation[1][1] =  cos(angle);

   glUniformMatrix4fv(rotation_uniform, 1, GL_FALSE, (GLfloat *) rotation);

   glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, verts);
   glVertexAttribPointer(color, 4, GL_FLOAT, GL_FALSE, 0, colors);
   glEnableVertexAttribArray(position);
   glEnableVertexAttribArray(color);

   glDrawArrays(GL_TRIANGLES, 0, 3);

   glDisableVertexAttribArray(position);
   glDisableVertexAttribArray(color);

   GLenum err = glGetError();
   if (err != GL_NO_ERROR)
   {
	  printf("renderGL: glGetError() = %X\n", err);
   }
}

GLuint Gl::createShader(GLenum shaderType, const char *shaderSource)
{
   GLuint shader = 0;
   GLint shaderStatus;
   GLsizei length;
   char logText[1000];

   shader = glCreateShader(shaderType);
   if (shader)
   {
      glShaderSource(shader, 1, (const char **)&shaderSource, NULL);
      glCompileShader(shader);
      glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderStatus);
      if (!shaderStatus)
      {
         glGetShaderInfoLog(shader, sizeof(logText), &length, logText);
         printf("Error compiling %s shader: %*s\n",
                ((shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment"),
                length,
                logText);
      }
   }

   return shader;
}


Application::Application(std::string name)
	: wl(name), egl(wl), gl()
{
	printf("instantiating app: %s\n", name.c_str());
	render(); // render one frame and setup frame callbacks
}

#include <poll.h>
bool Application::step()
{
	// NOTE: Bad idea if we are handling more than one display on the same thread, blocking call
	// handles wayland input and output, when no messages are available blocks till some are received
	// In this app frame callbacks are set up which will generate events quite frequently and no other events are handled
	if (wl_display_dispatch(wl.display) == -1)
	{
		printf("dispatch fail: %s\n", wl.name);
		return false;
	}

// Alternative, non-blocking, event loop:
// prepare_read returns 0 when queue is empty -> events can be queued up from file
//	while (wl_display_prepare_read(wl.display) != 0)
//		wl_display_dispatch_pending(wl.display);
//
// flush returns bytes written or -1 on error and sets errno (errno = EGAIN or EPIPE should be handled by the client)
//	while (wl_display_flush(wl.display) > 0);
//
// read_events or cancel_read MUST be called, otherwise next call to prepare_read will pause the thread until one of those is called (deadlock on single threaded env)
//	pollfd fds = { wl_display_get_fd(wl.display), POLLIN, 0 };
//	if (poll(&fds, 1, 0) > 0)
//		wl_display_read_events(wl.display);
//	else
//		wl_display_cancel_read(wl.display);

	render();

	return true;
}

void Application::render()
{
	if (!redraw) return;
	redraw = false;

	egl.switch_context(); // Needed if multiple displays are handled from the same thread, otherwise just call this once during initialization
	gl.render();

	wl.frameCallback = wl_surface_frame(wl.surface);
	wl_callback_add_listener(wl.frameCallback, &frameListener, this);

	eglSwapBuffers(egl.display, egl.surfaceWindow);
}


/* #define USE_JSONRPC
 * link the binary with: jsoncpp and jsonrpccpp-client
 * enables automatic display creation and destruction with a simple 3x3 grid positioning
 */

#ifdef USE_JSONRPC
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

struct Thunder
{
	jsonrpc::HttpClient _httpClient;
	jsonrpc::Client rpc;

	Thunder() : _httpClient("http://localhost:9998/jsonrpc"), rpc(_httpClient)
	{
	}

	bool create(std::string client)
	{
		Json::Value params;
		params["client"] = client;
		params["displayName"] = client;

		return call("org.rdk.RDKShell.1.createDisplay", params);
	}

	bool kill(std::string client)
	{
		Json::Value params;
		params["client"] = client;
		params["displayName"] = client;

		return call("org.rdk.RDKShell.1.kill", params);
	}

	bool position(std::string client, int x, int y)
	{
		Json::Value params;
		params["client"] = client;
		params["x"] = x;
		params["y"] = y;
		params["w"] = planeWidth;
		params["h"] = planeHeight;

		return call("org.rdk.RDKShell.1.setBounds", params);
	}

	bool scale(std::string client, double x, double y)
	{
		Json::Value params;
		params["client"] = client;
		params["sx"] = x;
		params["sy"] = y;

		return call("org.rdk.RDKShell.1.setScale", params);
	}

	bool call(std::string method, Json::Value& params)
	{
		try
		{
			auto response = rpc.CallMethod(method, params);
			return response["success"].asBool();
		}
		catch (...)
		{
			return false;
		}
	}
};

static void ensure(bool result)
{
	if (!result) throw std::runtime_error("ensure failed");
}

struct Display
{
	Thunder& thunder;
	std::string name;

	Display(Thunder& thunder, std::string name) : thunder(thunder), name(name)
	{
	    printf("creating display: %s\n", name.c_str());
		ensure(thunder.create(name));
	}

	~Display()
	{
	    printf("killing display: %s\n", name.c_str());
		thunder.kill(name);
	}
};
#endif

static void signalHandler(int signum)
{
   printf("signalHandler: signum %d\n", signum);
   g_running = 0;
}

static void setupSignalHandler()
{
   struct sigaction sigint;
   sigint.sa_handler = signalHandler;
   sigemptyset(&sigint.sa_mask);
   sigint.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sigint, NULL);
}

int main(int argc, char** argv) try
{
	if (argc == 1)
	{
	   printf("error: no apps\n");
	   return 1;
	}

	setupSignalHandler();

#ifdef USE_JSONRPC
   Thunder thunder;
   std::vector<std::unique_ptr<Display>> displays;

   const double _scale = 0.33;
   const int _count = 3;
#endif

   std::vector<std::unique_ptr<Application>> apps;
   for (int i = 1; i < argc; ++i)
   {
	   std::string client_name = argv[i];

#ifdef USE_JSONRPC
	   displays.emplace_back(std::make_unique<Display>(thunder, client_name));

	   const int x = (i - 1) % _count;
	   const int y = (i - 1) / _count;
	   ensure(thunder.position(client_name, x * planeWidth * _scale, y * planeHeight * _scale));
	   ensure(thunder.scale(client_name, _scale, _scale));
#endif

	   apps.emplace_back(std::make_unique<Application>(client_name));
   }

   g_running = 1;
   printf("client_control_extension_test: enter\n");
   while (g_running)
   {
	   for (auto& app : apps) app->step();
   }

   printf("client_control_extension_test: exit\n");

   return 0;
}
catch (std::exception& e)
{
	printf("error: %s\n", e.what());
}

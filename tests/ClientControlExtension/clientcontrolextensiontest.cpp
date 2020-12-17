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

/* This file caontains a test application for client control extension for RDKShell.
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

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "wayland-client.h"
#include "wayland-egl.h"

#include "protocol/rdkshell_client_control_protocol_client.h"

static void registryHandleGlobal(void *data,
                                 struct wl_registry *registry, uint32_t id,
                                 const char *interface, uint32_t version);
static void registryHandleGlobalRemove(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name);

static const struct wl_registry_listener registryListener =
{
   registryHandleGlobal,
   registryHandleGlobalRemove
};

enum class ClientControlExtensionApiType
{
   Api_BoundsAndScale = 0,
   Api_Bounds,
   Api_Scale,
   Api_Count
};

typedef struct _AppCtx
{
   struct wl_display *display;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_surface *surface;
   struct wl_egl_window *native;
   struct wl_callback *frameCallback;
   struct rdkshell_client_control *rdkshellClientControl;

   EGLDisplay eglDisplay;
   EGLConfig eglConfig;
   EGLSurface eglSurfaceWindow;
   EGLContext eglContext;

   int planeWidth;
   int planeHeight;

   struct
   {
      GLuint rotation_uniform;
      GLuint pos;
      GLuint col;
   } gl;
   long long startTime;
   long long currTime;
   bool needRedraw;

   char const *clientName;
   ClientControlExtensionApiType apiToUse;
   int clientPosX;
   int clientPosDeltaX;

   float clientScale;
   float clientDeltaScale;

   float clientWidth;
   float clientHeight;
   float clientDeltaWidth;
   float clientDeltaHeight;

} AppCtx;


static void drawFrame(AppCtx *ctx);
static bool setupEGL(AppCtx *ctx);
static void termEGL(AppCtx *ctx);
static bool createSurface(AppCtx *ctx);
static void resizeSurface(AppCtx *ctx, int dx, int dy, int width, int height);
static void destroySurface(AppCtx *ctx);
static void setupGL(AppCtx *ctx);
static void renderGL(AppCtx *ctx);

int g_running = 0;
int g_log = 0;

static void signalHandler(int signum)
{
   printf("signalHandler: signum %d\n", signum);
   g_running = 0;
}

static long long currentTimeMillis()
{
   long long timeMillis;
   struct timeval tv;

   gettimeofday(&tv, NULL);
   timeMillis = tv.tv_sec * 1000 + tv.tv_usec / 1000;

   return timeMillis;
}

static void registryHandleGlobal(void *data,
                                 struct wl_registry *registry, uint32_t id,
                                 const char *interface, uint32_t version)
{
   AppCtx *ctx = (AppCtx*)data;
   int len;

   printf("client_control_extension_test: registry: id %d interface (%s) version %d\n", id, interface, version);

   len = strlen(interface);
   if ((len == 13) && !strncmp(interface, "wl_compositor", len)) {
      ctx->compositor = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 1);
      printf("compositor %p\n", ctx->compositor);
   }
   else if ((len == strlen("rdkshell_client_control")) && !strncmp(interface, "rdkshell_client_control", len)) {
      ctx->rdkshellClientControl = (struct rdkshell_client_control*)wl_registry_bind(registry, id, &rdkshell_client_control_interface, 1);
      printf("rdkshell_client_control %p\n", ctx->rdkshellClientControl);
   }
}

static void registryHandleGlobalRemove(void *data,
                                       struct wl_registry *registry,
                                       uint32_t name)
{
}

static void showUsage()
{
   printf("usage:\n");
   printf(" client_control_extension_test [options]\n");
   printf("where [options] are:\n");
   printf("  --display <name> : wayland display to connect to\n");
   printf("  --resolution <width> <height> : current display resolution, if not specified 1920x1080 used\n");
   printf("  --client : client name (in lower case) that will be controlled, if not specified YouTube is used\n");
   printf("  --api : 0 - bounds and scale of client app animated, 1 - bounds animated, 2 - scale animated\n");
   printf("  --log : verbose logging\n");
   printf("  -? : show usage\n");
   printf("\n");
}

static void redraw(void *data, struct wl_callback *callback, uint32_t time)
{
   AppCtx *ctx = (AppCtx*)data;

   if (g_log) printf("redraw: time %u\n", time);
   wl_callback_destroy(callback);

   ctx->needRedraw = true;
}

static struct wl_callback_listener frameListener =
{
   redraw
};

static void setupAnimation(AppCtx *ctx)
{
   ctx->clientPosX = 0;
   ctx->clientPosDeltaX = 2;

   ctx->clientScale = 1.0f;
   ctx->clientDeltaScale = 0.01f;

   ctx->clientWidth = ctx->planeWidth;
   ctx->clientHeight = ctx->planeHeight;
   ctx->clientDeltaWidth = 1.0f;
   ctx->clientDeltaHeight = ctx->clientDeltaWidth * ctx->planeHeight / ctx->planeWidth;
}

static void updatePosition(AppCtx *ctx)
{
   if (ctx->clientPosX > ctx->planeWidth || ctx->clientPosX < 0)
      ctx->clientPosDeltaX = -ctx->clientPosDeltaX;
   ctx->clientPosX += ctx->clientPosDeltaX;
}

static void updateSize(AppCtx *ctx)
{
   if (ctx->clientWidth > ctx->planeWidth || ctx->clientWidth < 1)
   {
      ctx->clientDeltaWidth = -ctx->clientDeltaWidth;
      ctx->clientDeltaHeight = -ctx->clientDeltaHeight;
   }
   ctx->clientWidth += ctx->clientDeltaWidth;
   ctx->clientHeight += ctx->clientDeltaHeight;
}

static void updateScale(AppCtx *ctx)
{
   if (ctx->clientScale >= 1.0f || ctx->clientScale < 0.1f)
   {
      ctx->clientDeltaScale = -ctx->clientDeltaScale;
   }
   ctx->clientScale += ctx->clientDeltaScale;
}

static void updateClient(AppCtx *ctx)
{
   if (!ctx->rdkshellClientControl)
      return;

   switch (ctx->apiToUse)
   {
      case ClientControlExtensionApiType::Api_BoundsAndScale:
         updatePosition(ctx);
         updateSize(ctx);
         updateScale(ctx);
         rdkshell_client_control_set_client_bounds_and_scale(ctx->rdkshellClientControl, ctx->clientName,
            ctx->clientPosX, 0, ctx->clientWidth, ctx->clientHeight, wl_fixed_from_double(ctx->clientScale), wl_fixed_from_double(ctx->clientScale));
         break;

      case ClientControlExtensionApiType::Api_Bounds:
         updatePosition(ctx);
         updateSize(ctx);
         rdkshell_client_control_set_client_bounds(ctx->rdkshellClientControl, ctx->clientName,
            ctx->clientPosX, 0, static_cast<int>(ctx->clientWidth), static_cast<int>(ctx->clientHeight));
         break;

      case ClientControlExtensionApiType::Api_Scale:
         updateScale(ctx);
         rdkshell_client_control_set_client_scale(ctx->rdkshellClientControl, ctx->clientName,
            wl_fixed_from_double(ctx->clientScale), wl_fixed_from_double(ctx->clientScale));
         break;
   }
}

static void drawFrame(AppCtx *ctx)
{
   updateClient(ctx);
   renderGL(ctx);

   ctx->frameCallback = wl_surface_frame(ctx->surface);
   wl_callback_add_listener(ctx->frameCallback, &frameListener, ctx);

   eglSwapBuffers(ctx->eglDisplay, ctx->eglSurfaceWindow);
}

int main(int argc, char** argv)
{
   int nRC = 0;
   AppCtx ctx;
   struct sigaction sigint;
   struct wl_display *display = 0;
   struct wl_registry *registry = 0;
   const char *display_name = 0;

   printf("client_control_extension_test\n");

   memset(&ctx, 0, sizeof(AppCtx));
   ctx.planeWidth = 1920;
   ctx.planeHeight = 1080;

   for (int i = 1; i < argc; ++i)
   {
      if (!strcmp((const char*)argv[i], "--display"))
      {
         if (i+1 < argc)
         {
            ++i;
            display_name = argv[i];
         }
      }
      else if (!strcmp((const char*)argv[i], "--resolution"))
      {
         if (i+2 < argc)
         {
            ++i;
            ctx.planeWidth = atoi(argv[i]);
            ++i;
            ctx.planeHeight = atoi(argv[i]);
         }
      }
      else if (!strcmp((const char*)argv[i], "--client"))
      {
         if (i+1 < argc)
         {
            ++i;
            ctx.clientName = argv[i];
         }
      }
      else if (!strcmp((const char*)argv[i], "--log"))
      {
         g_log = true;
      }
      else if (!strcmp((const char*)argv[i], "--api"))
      {
         if (i+1 < argc)
         {
            ++i;
            int v = atoi(argv[i]);
            ctx.apiToUse = (v >= 0 && v < static_cast<int>(ClientControlExtensionApiType::Api_Count))
               ? static_cast<ClientControlExtensionApiType>(v) : ClientControlExtensionApiType::Api_BoundsAndScale;
         }
      }
      else if (!strcmp((const char*)argv[i], "-?"))
      {
         showUsage();
         goto exit;
      }
   }

   if (!ctx.clientName)
   {
      ctx.clientName = "youtube";
   }
   printf("controling client: %s\n", ctx.clientName);

   ctx.startTime = currentTimeMillis();
   
   if (display_name)
   {
      printf("calling wl_display_connect for display name %s\n", display_name);
   }
   else
   {
      printf("calling wl_display_connect for default display\n");
   }
   display = wl_display_connect(display_name);
   printf("wl_display_connect: display=%p\n", display);
   if (!display)
   {
      printf("error: unable to connect to primary display\n");
      nRC = -1;
      goto exit;
   }

   printf("calling wl_display_get_registry\n");
   registry = wl_display_get_registry(display);
   printf("wl_display_get_registry: registry=%p\n", registry);
   if (!registry)
   {
      printf("error: unable to get display registry\n");
      nRC = -2;
      goto exit;
   }

   ctx.display = display;
   ctx.registry = registry;

   wl_registry_add_listener(registry, &registryListener, &ctx);
   
   wl_display_roundtrip(ctx.display);
   
   setupEGL(&ctx);
   createSurface(&ctx);
   setupGL(&ctx);
   setupAnimation(&ctx);

   drawFrame(&ctx);

   sigint.sa_handler = signalHandler;
   sigemptyset(&sigint.sa_mask);
   sigint.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sigint, NULL);

   g_running = 1;
   while (g_running)
   {
      if (wl_display_dispatch(ctx.display) == -1)
      {
         break;
      }

      if (ctx.needRedraw)
      {
         ctx.needRedraw = false;
         drawFrame(&ctx);
      }
   }   

exit:

   printf("client_control_extension_test: exiting...\n");

   if (ctx.rdkshellClientControl)
   {
      rdkshell_client_control_set_client_bounds_and_scale(ctx.rdkshellClientControl, ctx.clientName,
         0, 0, ctx.planeWidth, ctx.planeHeight, wl_fixed_from_double(1.0), wl_fixed_from_double(1.0));
   }

   if (ctx.compositor)
   {
      wl_compositor_destroy(ctx.compositor);
      ctx.compositor = 0;
   }
   
   termEGL(&ctx);

   if (registry)
   {
      wl_registry_destroy(registry);
      registry = 0;
   }
   
   if (display)
   {
      wl_display_disconnect(display);
      display = 0;
   }
   
   printf("client_control_extension_test: exit\n");
      
   return nRC;
}

#define RED_SIZE (8)
#define GREEN_SIZE (8)
#define BLUE_SIZE (8)
#define ALPHA_SIZE (8)
#define DEPTH_SIZE (0)

static bool setupEGL(AppCtx *ctx)
{
   bool result = false;
   EGLint major, minor;
   EGLBoolean b;
   EGLint configCount;
   EGLConfig *eglConfigs = 0;
   EGLint attr[32];
   EGLint redSize, greenSize, blueSize, alphaSize, depthSize;
   EGLint ctxAttrib[3];
   int i;

   /*
    * Get default EGL display
    */
   ctx->eglDisplay = eglGetDisplay((NativeDisplayType)ctx->display);
   printf("eglDisplay=%p\n", ctx->eglDisplay);
   if (ctx->eglDisplay == EGL_NO_DISPLAY)
   {
      printf("error: EGL not available\n");
      goto exit;
   }
    
   /*
    * Initialize display
    */
   b = eglInitialize(ctx->eglDisplay, &major, &minor);
   if (!b)
   {
      printf("error: unable to initialize EGL display\n");
      goto exit;
   }
   printf("eglInitiialize: major: %d minor: %d\n", major, minor);
    
   /*
    * Get number of available configurations
    */
   b = eglGetConfigs(ctx->eglDisplay, NULL, 0, &configCount);
   if (!b)
   {
      printf("error: unable to get count of EGL configurations: %X\n", eglGetError());
      goto exit;
   }
   printf("Number of EGL configurations: %d\n", configCount);
    
   eglConfigs = (EGLConfig*)malloc(configCount*sizeof(EGLConfig));
   if (!eglConfigs)
   {
      printf("error: unable to alloc memory for EGL configurations\n");
      goto exit;
   }
    
   i = 0;
   attr[i++] = EGL_RED_SIZE;
   attr[i++] = RED_SIZE;
   attr[i++] = EGL_GREEN_SIZE;
   attr[i++] = GREEN_SIZE;
   attr[i++] = EGL_BLUE_SIZE;
   attr[i++] = BLUE_SIZE;
   attr[i++] = EGL_DEPTH_SIZE;
   attr[i++] = DEPTH_SIZE;
   attr[i++] = EGL_STENCIL_SIZE;
   attr[i++] = 0;
   attr[i++] = EGL_SURFACE_TYPE;
   attr[i++] = EGL_WINDOW_BIT;
   attr[i++] = EGL_RENDERABLE_TYPE;
   attr[i++] = EGL_OPENGL_ES2_BIT;
   attr[i++] = EGL_NONE;
    
   /*
    * Get a list of configurations that meet or exceed our requirements
    */
   b = eglChooseConfig(ctx->eglDisplay, attr, eglConfigs, configCount, &configCount);
   if (!b)
   {
      printf("error: eglChooseConfig failed: %X\n", eglGetError());
      goto exit;
   }
   printf("eglChooseConfig: matching configurations: %d\n", configCount);
    
   /*
    * Choose a suitable configuration
    */
   for(i = 0; i < configCount; ++i)
   {
      eglGetConfigAttrib(ctx->eglDisplay, eglConfigs[i], EGL_RED_SIZE, &redSize);
      eglGetConfigAttrib(ctx->eglDisplay, eglConfigs[i], EGL_GREEN_SIZE, &greenSize);
      eglGetConfigAttrib(ctx->eglDisplay, eglConfigs[i], EGL_BLUE_SIZE, &blueSize);
      eglGetConfigAttrib(ctx->eglDisplay, eglConfigs[i], EGL_ALPHA_SIZE, &alphaSize);
      eglGetConfigAttrib(ctx->eglDisplay, eglConfigs[i], EGL_DEPTH_SIZE, &depthSize);

      printf("config %d: red: %d green: %d blue: %d alpha: %d depth: %d\n",
              i, redSize, greenSize, blueSize, alphaSize, depthSize);
      if ((redSize == RED_SIZE) &&
           (greenSize == GREEN_SIZE) &&
           (blueSize == BLUE_SIZE) &&
           (alphaSize == ALPHA_SIZE) &&
           (depthSize >= DEPTH_SIZE))
      {
         printf("choosing config %d\n", i);
         break;
      }
   }
   if (i == configCount)
   {
      printf("error: no suitable configuration available\n");
      goto exit;
   }
   ctx->eglConfig = eglConfigs[i];

   ctxAttrib[0] = EGL_CONTEXT_CLIENT_VERSION;
   ctxAttrib[1] = 2; // ES2
   ctxAttrib[2] = EGL_NONE;
    
   /*
    * Create an EGL context
    */
   ctx->eglContext = eglCreateContext(ctx->eglDisplay, ctx->eglConfig, EGL_NO_CONTEXT, ctxAttrib);
   if (ctx->eglContext == EGL_NO_CONTEXT)
   {
      printf("eglCreateContext failed: %X\n", eglGetError());
      goto exit;
   }
   printf("eglCreateContext: eglContext %p\n", ctx->eglContext);

   result = true;
    
exit:

   if (eglConfigs)
   {
      free(eglConfigs);
      eglConfigs = 0;
   }

   return result;       
}

static void termEGL(AppCtx *ctx)
{
   if (ctx->display)
   {
      eglMakeCurrent(ctx->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      
      destroySurface(ctx);
      
      eglTerminate(ctx->eglDisplay);
      eglReleaseThread();
   }
}

static bool createSurface(AppCtx *ctx)
{
   bool result = false;
   EGLBoolean b;

   ctx->surface = wl_compositor_create_surface(ctx->compositor);
   printf("surface=%p\n", ctx->surface);   
   if (!ctx->surface)
   {
      printf("error: unable to create wayland surface\n");
      goto exit;
   }

   ctx->native = wl_egl_window_create(ctx->surface, ctx->planeWidth, ctx->planeHeight);
   if (!ctx->native)
   {
      printf("error: unable to create wl_egl_window\n");
      goto exit;
   }
   printf("wl_egl_window %p\n", ctx->native);
   
   /*
    * Create a window surface
    */
   ctx->eglSurfaceWindow = eglCreateWindowSurface(ctx->eglDisplay,
                                                  ctx->eglConfig,
                                                  (EGLNativeWindowType)ctx->native,
                                                  NULL);
   if (ctx->eglSurfaceWindow == EGL_NO_SURFACE)
   {
      printf("eglCreateWindowSurface: A: error %X\n", eglGetError());
      ctx->eglSurfaceWindow = eglCreateWindowSurface(ctx->eglDisplay,
                                                     ctx->eglConfig,
                                                     (EGLNativeWindowType)NULL,
                                                     NULL);
      if (ctx->eglSurfaceWindow == EGL_NO_SURFACE)
      {
         printf("eglCreateWindowSurface: B: error %X\n", eglGetError());
         goto exit;
      }
   }
   printf("eglCreateWindowSurface: eglSurfaceWindow %p\n", ctx->eglSurfaceWindow);                                         

   /*
    * Establish EGL context for this thread
    */
   b = eglMakeCurrent(ctx->eglDisplay, ctx->eglSurfaceWindow, ctx->eglSurfaceWindow, ctx->eglContext);
   if (!b)
   {
      printf("error: eglMakeCurrent failed: %X\n", eglGetError());
      goto exit;
   }
    
   eglSwapInterval(ctx->eglDisplay, 1);

exit:

   return result;
}

static void destroySurface(AppCtx *ctx)
{
   if (ctx->eglSurfaceWindow)
   {
      eglDestroySurface(ctx->eglDisplay, ctx->eglSurfaceWindow);
      
      wl_egl_window_destroy(ctx->native);   
   }
   if (ctx->surface)
   {
      wl_surface_destroy(ctx->surface);
      ctx->surface = 0;
   }
}

static void resizeSurface(AppCtx *ctx, int dx, int dy, int width, int height)
{
   if (ctx->native)
   {
      wl_egl_window_resize(ctx->native, width, height, dx, dy);
   }
}

static const char *vert_shader_text =
   "uniform mat4 rotation;\n"
   "attribute vec4 pos;\n"
   "attribute vec4 color;\n"
   "varying vec4 v_color;\n"
   "void main() {\n"
   "  gl_Position = rotation * pos;\n"
   "  v_color = color;\n"
   "}\n";

static const char *frag_shader_text =
   "precision mediump float;\n"
   "varying vec4 v_color;\n"
   "void main() {\n"
   "  gl_FragColor = v_color;\n"
   "}\n";

static GLuint createShader(AppCtx *ctx, GLenum shaderType, const char *shaderSource)
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

static void setupGL(AppCtx *ctx)
{
   GLuint frag, vert;
   GLuint program;
   GLint status;

   frag = createShader(ctx, GL_FRAGMENT_SHADER, frag_shader_text);
   vert = createShader(ctx, GL_VERTEX_SHADER, vert_shader_text);

   program = glCreateProgram();
   glAttachShader(program, frag);
   glAttachShader(program, vert);
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

   ctx->gl.pos = 0;
   ctx->gl.col = 1;

   glBindAttribLocation(program, ctx->gl.pos, "pos");
   glBindAttribLocation(program, ctx->gl.col, "color");
   glLinkProgram(program);

   ctx->gl.rotation_uniform = glGetUniformLocation(program, "rotation");
}

static void renderGL(AppCtx *ctx)
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
      { 0, 1, 0, 0 },
      { 0, 0, 1, 0 },
      { 0, 0, 0, 1 }
   };
   static const uint32_t speed_div = 5;
   EGLint rect[4];

   glViewport(0, 0, ctx->planeWidth, ctx->planeHeight);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   ctx->currTime = currentTimeMillis();

   angle = ((ctx->currTime - ctx->startTime) / speed_div) % 360 * M_PI / 180.0;
   rotation[0][0] =  cos(angle);
   rotation[0][2] =  sin(angle);
   rotation[2][0] = -sin(angle);
   rotation[2][2] =  cos(angle);

   glUniformMatrix4fv(ctx->gl.rotation_uniform, 1, GL_FALSE, (GLfloat *) rotation);

   glVertexAttribPointer(ctx->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
   glVertexAttribPointer(ctx->gl.col, 4, GL_FLOAT, GL_FALSE, 0, colors);
   glEnableVertexAttribArray(ctx->gl.pos);
   glEnableVertexAttribArray(ctx->gl.col);

   glDrawArrays(GL_TRIANGLES, 0, 3);

   glDisableVertexAttribArray(ctx->gl.pos);
   glDisableVertexAttribArray(ctx->gl.col);
   
   GLenum err = glGetError();
   if (err != GL_NO_ERROR)
   {
      printf("renderGL: glGetError() = %X\n", err);
   }
}

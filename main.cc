
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <tuple>

#include <cstring>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GL/gl.h>

template <typename T, typename D>
inline auto ptr(T* p, D d) {
  return std::unique_ptr<T, D>(p, d);
}
template <typename W>
inline auto ptr(W* p) {
  return ptr(p, [](W* p) { wl_proxy_destroy(reinterpret_cast<wl_proxy*>(p)); });
}

template <typename W> constexpr wl_interface const* interface_of = nullptr;
template <> constexpr wl_interface const* interface_of<wl_compositor> = &wl_compositor_interface;
template <> constexpr wl_interface const* interface_of<wl_shell>      = &wl_shell_interface;

template <typename W>
inline auto global_bind(wl_display* display, uint32_t version) {
  static_assert(nullptr != interface_of<W>, "please check definition of interface_of definitions");

  using userdata_type = std::tuple<W*, uint32_t>;
  auto registry = ptr(wl_display_get_registry(display));
  wl_registry_listener listener = {
    .global = [](void* data,
  		 wl_registry* registry,
  		 uint32_t name,
  		 char const* interface,
  		 uint32_t version)
    {
      if (0 == std::strcmp(interface, interface_of<W>->name)) {
  	auto& userdata = *reinterpret_cast<userdata_type*>(data);
  	std::get<0>(userdata) =
  	  reinterpret_cast<W*>(wl_registry_bind(registry,
  					     name,
  					     interface_of<W>,
  					     std::min(version, std::get<1>(userdata))));
      }
    },
    .global_remove = [](void* data,
  			wl_registry* registry,
  			uint32_t name)
    {
    },
  };
  userdata_type userdata(nullptr, version);
  wl_registry_add_listener(registry.get(), &listener, &userdata);
  wl_display_roundtrip(display);
  return ptr(std::get<0>(userdata));
}

int main() {
  try {
    auto display = ptr(wl_display_connect(nullptr), wl_display_disconnect);

    auto compositor = global_bind<wl_compositor>(display.get(), 4);
    auto shell      = global_bind<wl_shell>(display.get(), 1);

    auto surface = ptr(wl_compositor_create_surface(compositor.get()));
    auto shell_surface = ptr(wl_shell_get_shell_surface(shell.get(), surface.get()));

    auto egl_display = ptr(eglGetDisplay(display.get()), eglTerminate);
    eglInitialize(egl_display.get(), nullptr, nullptr);

    eglBindAPI(EGL_OPENGL_API);
    EGLint attributes[] = {
      EGL_RED_SIZE,   8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE,  8,
      EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint num_config = 0;
    eglChooseConfig(egl_display.get(), attributes, &config, 1, &num_config);
    auto egl_context = ptr(eglCreateContext(egl_display.get(), config, EGL_NO_CONTEXT, nullptr),
			   [&egl_display](auto p) { eglDestroyContext(egl_display.get(), p); });

    auto egl_window = ptr(wl_egl_window_create(surface.get(), 320, 240), wl_egl_window_destroy);
    auto egl_surface = ptr(eglCreateWindowSurface(egl_display.get(),
						  config, egl_window.get(), nullptr),
			   [&egl_display](auto p) { eglDestroySurface(egl_display.get(), p); });
								      
    eglMakeCurrent(egl_display.get(), egl_surface.get(), egl_surface.get(), egl_context.get());

    wl_shell_surface_listener listener = {
      .ping = [](void*, wl_shell_surface* shell_surface, uint32_t serial) {
	std::cerr << "ping" << std::endl;
	wl_shell_surface_pong(shell_surface, serial);
      },
      .configure = [](void* data, wl_shell_surface*, uint32_t, int32_t width, int32_t height) {
	std::cerr << "configure" << std::endl;
	wl_egl_window_resize(reinterpret_cast<wl_egl_window*>(data), width, height, 0, 0);
      },
      .popup_done = [](void*, wl_shell_surface*) {
      },
    };
    wl_shell_surface_add_listener(shell_surface.get(), &listener, egl_window.get());
    wl_shell_surface_set_toplevel(shell_surface.get());

    for (;;) {
      wl_display_dispatch_pending(display.get());
      glClearColor(0.0, 1.0, 0.0, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
      eglSwapBuffers(egl_display.get(), egl_surface.get());
    }
  }
  catch (std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 0;
}

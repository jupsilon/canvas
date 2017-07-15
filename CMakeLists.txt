
cmake_minimum_required(VERSION 2.6)

add_executable(canvas main.cc)

target_link_libraries(canvas wayland-client wayland-egl EGL GL)

add_custom_target(run
	COMMAND ./canvas
	DEPENDS canvas
	)

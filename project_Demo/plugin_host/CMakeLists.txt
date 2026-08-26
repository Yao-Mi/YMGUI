cmake_minimum_required(VERSION 3.16)
project(plugin_host C)

if(ANDROID AND NOT TARGET SDL2::SDL2)
	if(NOT SDL2_SOURCE_DIR)
		message(FATAL_ERROR "Android standalone build requires -DSDL2_SOURCE_DIR=<SDL2 source>")
	endif()
	set(SDL_STATIC OFF CACHE BOOL "" FORCE)
	set(SDL_SHARED ON CACHE BOOL "" FORCE)
	set(SDL_TEST OFF CACHE BOOL "" FORCE)
	add_subdirectory(${SDL2_SOURCE_DIR} ${CMAKE_BINARY_DIR}/sdl2)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 真动态插件只包含自身代码，不链接 YMGUI；全部 UI 能力从宿主函数表取得。
add_library(system_info MODULE system_info_plugin.c)
add_library(not_a_plugin MODULE not_a_plugin.c)
set_target_properties(system_info PROPERTIES
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)
set_target_properties(not_a_plugin PROPERTIES
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)
if(NOT ANDROID)
	set_target_properties(system_info PROPERTIES PREFIX "")
	set_target_properties(not_a_plugin PROPERTIES PREFIX "")
endif()

ymgui_add_app(plugin_host plugin_host.c)
target_link_libraries(plugin_host ${CMAKE_DL_LIBS})
add_dependencies(plugin_host system_info not_a_plugin)

if(NOT CMAKE_CROSSCOMPILING)
	enable_testing()
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/empty_plugins)
	add_test(NAME plugin_host_dynamic COMMAND plugin_host 8 $<TARGET_FILE_DIR:system_info> selftest)
	set_tests_properties(plugin_host_dynamic PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
	add_test(NAME plugin_host_empty_directory COMMAND plugin_host 1 ${CMAKE_CURRENT_BINARY_DIR}/empty_plugins emptytest)
	set_tests_properties(plugin_host_empty_directory PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
	add_test(NAME plugin_host_direct_close COMMAND plugin_host 1 $<TARGET_FILE_DIR:system_info> closetest)
	set_tests_properties(plugin_host_direct_close PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 5)
endif()

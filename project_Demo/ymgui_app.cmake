# ===========================================================================
# ymgui_app.cmake —— project_Demo 下各"基础验证项目"共用的咬合层。
#
#   用法(每个项目自己的 CMakeLists.txt 里):
#     cmake_minimum_required(VERSION 3.16)
#     project(xxx C)
#     include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)
#     ymgui_add_app(xxx  xxx.c [more.c ...])
#
#   设计:每个项目是"独立工程"——把仓库里的 YMGUI/ 当被 vendored 的外部依赖
#   (从本脚本位置反推仓库根),自己 GLOB 库源码建 ymgui、把 SDL_LCD 建成 sdl_lcd。
#   这样项目间互不牵连,单个项目也能单独 configure/build。样板全收在这里,不必每项目重抄。
#
#   编译输出:约定用 -B build/project_Demo/<项目名> 配置,产物自然落该目录下。
# ===========================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # C99 + GNU 扩展(与主库一致)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

# 仓库根:本脚本在 <repo>/project_Demo/ 下,上跳一级即根
get_filename_component(YMGUI_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(YMGUI_ROOT   ${YMGUI_REPO_ROOT}/YMGUI)
set(YMGUI_SDLDIR ${YMGUI_REPO_ROOT}/SDL_LCD)

# 裸头式 include:9 个可移植层 + SDL_LCD 外壳(全库头文件无重名)
include_directories(
	${YMGUI_ROOT}/CONFIG
	${YMGUI_ROOT}/DEBUG
	${YMGUI_ROOT}/COMMON/GEOM
	${YMGUI_ROOT}/COMMON/MATH
	${YMGUI_ROOT}/OPOBJ
	${YMGUI_ROOT}/CORE
	${YMGUI_ROOT}/GUI
	${YMGUI_ROOT}/WIDGET
	${YMGUI_ROOT}/STATE
	${YMGUI_ROOT}/HAL
	${YMGUI_SDLDIR}
)

# 库本体(平台无关 + HAL 接口),GLOB 各层 .c —— 与主库 CMakeLists 同口径
file(GLOB YMGUI_SRC
	${YMGUI_ROOT}/CONFIG/*.c
	${YMGUI_ROOT}/DEBUG/*.c
	${YMGUI_ROOT}/COMMON/GEOM/*.c
	${YMGUI_ROOT}/COMMON/MATH/*.c
	${YMGUI_ROOT}/OPOBJ/*.c
	${YMGUI_ROOT}/CORE/*.c
	${YMGUI_ROOT}/GUI/*.c
	${YMGUI_ROOT}/WIDGET/*.c
	${YMGUI_ROOT}/STATE/*.c
	${YMGUI_ROOT}/HAL/*.c
)

# 目标可能被多个 include 重复定义?本脚本每个独立工程各自 configure 一次,不会重入。
add_library(ymgui STATIC ${YMGUI_SRC})

# SDL 假 LCD。桌面 Linux 保留 pkg-config，Windows/Android 使用 SDL2 CMake target。
option(YMGUI_SDL_STATIC "Prefer the SDL2 static target when available" OFF)
if(WIN32 OR ANDROID)
	if(NOT TARGET SDL2::SDL2)
		find_package(SDL2 CONFIG REQUIRED)
	endif()
	set(YMGUI_SDL_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
	if(YMGUI_SDL_STATIC AND TARGET SDL2::SDL2-static)
		set(YMGUI_SDL_LIBRARIES SDL2::SDL2-static)
	else()
		set(YMGUI_SDL_LIBRARIES SDL2::SDL2)
	endif()
	if(WIN32 AND TARGET SDL2::SDL2main)
		list(PREPEND YMGUI_SDL_LIBRARIES SDL2::SDL2main)
	endif()
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED sdl2)
	set(YMGUI_SDL_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
	set(YMGUI_SDL_LIBRARIES ${SDL2_LIBRARIES})
endif()
add_library(sdl_lcd STATIC ${YMGUI_SDLDIR}/SDL_LCD.c)
target_include_directories(sdl_lcd PRIVATE ${YMGUI_SDL_INCLUDE_DIRS})
target_link_libraries(sdl_lcd PUBLIC ${YMGUI_SDL_LIBRARIES})

# ymgui_add_app(<名字> <源文件...>):建一个链好 ymgui + sdl_lcd + SDL2 的应用目标
function(ymgui_add_app APP_NAME)
	if(ANDROID)
		add_library(${APP_NAME} SHARED ${ARGN})
	else()
		add_executable(${APP_NAME} ${ARGN})
	endif()
	target_include_directories(${APP_NAME} PRIVATE ${YMGUI_SDL_INCLUDE_DIRS})
	target_link_libraries(${APP_NAME} ymgui sdl_lcd)
	if(NOT WIN32)
		target_link_libraries(${APP_NAME} m)
	endif()
endfunction()

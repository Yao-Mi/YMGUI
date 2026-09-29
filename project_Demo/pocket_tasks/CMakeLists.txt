cmake_minimum_required(VERSION 3.16)
project(pocket_tasks C)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)
ymgui_add_app(pocket_tasks pocket_tasks.c)

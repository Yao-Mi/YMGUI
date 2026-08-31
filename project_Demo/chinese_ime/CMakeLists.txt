cmake_minimum_required(VERSION 3.16)
project(chinese_ime C)

include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)
ymgui_add_app(chinese_ime chinese_ime.c)

set(YMGUI_IME_PHRASES "EXTERNAL" CACHE STRING "Phrase dictionary mode: EXTERNAL, EMBEDDED, or OFF")
set_property(CACHE YMGUI_IME_PHRASES PROPERTY STRINGS EXTERNAL EMBEDDED OFF)
set(YMGUI_IME_PHRASE_BIN "${CMAKE_CURRENT_LIST_DIR}/phrases_rime.bin" CACHE FILEPATH "External phrase dictionary path")
set(YMGUI_IME_CHARS "EXTERNAL" CACHE STRING "Chinese character dictionary mode: EXTERNAL, EMBEDDED, or OFF")
set_property(CACHE YMGUI_IME_CHARS PROPERTY STRINGS EXTERNAL EMBEDDED OFF)
set(YMGUI_IME_CHAR_BIN "${CMAKE_CURRENT_LIST_DIR}/pinyin_gb2312.bin" CACHE FILEPATH "External Chinese character dictionary path")
set(YMGUI_IME_ENGLISH "EXTERNAL" CACHE STRING "English dictionary mode: EXTERNAL or OFF")
set_property(CACHE YMGUI_IME_ENGLISH PROPERTY STRINGS EXTERNAL OFF)
set(YMGUI_IME_ENGLISH_BIN "${CMAKE_CURRENT_LIST_DIR}/english_words.bin" CACHE FILEPATH "External English dictionary path")
string(TOUPPER "${YMGUI_IME_PHRASES}" YMGUI_IME_PHRASES_MODE)
string(TOUPPER "${YMGUI_IME_CHARS}" YMGUI_IME_CHARS_MODE)
string(TOUPPER "${YMGUI_IME_ENGLISH}" YMGUI_IME_ENGLISH_MODE)

target_compile_definitions(chinese_ime PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/tools/gb2312_glyphs.bin")

if(YMGUI_IME_PHRASES_MODE STREQUAL "EXTERNAL")
	target_compile_definitions(chinese_ime PRIVATE
		YMGUI_IME_PHRASES_EXTERNAL=1
		PHRASE_BIN_PATH="${YMGUI_IME_PHRASE_BIN}")
elseif(YMGUI_IME_PHRASES_MODE STREQUAL "EMBEDDED")
	target_compile_definitions(chinese_ime PRIVATE YMGUI_IME_PHRASES_EMBEDDED=1)
elseif(YMGUI_IME_PHRASES_MODE STREQUAL "OFF")
	target_compile_definitions(chinese_ime PRIVATE YMGUI_IME_PHRASES_OFF=1)
else()
	message(FATAL_ERROR "YMGUI_IME_PHRASES must be EXTERNAL, EMBEDDED, or OFF")
endif()

if(YMGUI_IME_CHARS_MODE STREQUAL "EXTERNAL")
	target_compile_definitions(chinese_ime PRIVATE
		YMGUI_IME_CHARS_EXTERNAL=1
		CHAR_BIN_PATH="${YMGUI_IME_CHAR_BIN}")
elseif(YMGUI_IME_CHARS_MODE STREQUAL "EMBEDDED")
	target_compile_definitions(chinese_ime PRIVATE YMGUI_IME_CHARS_EMBEDDED=1)
elseif(YMGUI_IME_CHARS_MODE STREQUAL "OFF")
	target_compile_definitions(chinese_ime PRIVATE YMGUI_IME_CHARS_OFF=1)
else()
	message(FATAL_ERROR "YMGUI_IME_CHARS must be EXTERNAL, EMBEDDED, or OFF")
endif()

if(YMGUI_IME_ENGLISH_MODE STREQUAL "EXTERNAL")
	target_compile_definitions(chinese_ime PRIVATE
		YMGUI_IME_ENGLISH_EXTERNAL=1
		ENGLISH_BIN_PATH="${YMGUI_IME_ENGLISH_BIN}")
elseif(YMGUI_IME_ENGLISH_MODE STREQUAL "OFF")
	target_compile_definitions(chinese_ime PRIVATE YMGUI_IME_ENGLISH_OFF=1)
else()
	message(FATAL_ERROR "YMGUI_IME_ENGLISH must be EXTERNAL or OFF")
endif()

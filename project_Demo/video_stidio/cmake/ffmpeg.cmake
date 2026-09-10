# Prefer a local source tree; otherwise use matching system headers/libraries.
set(STUDIO_FFMPEG_SOURCE "${YMGUI_REPO_ROOT}/extern_lib/FFmpeg" CACHE PATH "FFmpeg source tree (empty for system libraries)")
add_library(studio_ffmpeg INTERFACE)
if(STUDIO_FFMPEG_SOURCE AND EXISTS "${STUDIO_FFMPEG_SOURCE}/configure")
    include(ExternalProject)
    find_program(STUDIO_MAKE make REQUIRED)
    set(STUDIO_FFMPEG_JOBS 4 CACHE STRING "Parallel jobs for the private FFmpeg build")
    set(STUDIO_FFMPEG_BUILD "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg")
    set(STUDIO_FFMPEG_LIBRARIES)
    foreach(lib avformat avcodec swscale avutil)
        list(APPEND STUDIO_FFMPEG_LIBRARIES "${STUDIO_FFMPEG_BUILD}/lib${lib}/lib${lib}.a")
    endforeach()
    ExternalProject_Add(studio_ffmpeg_build
        SOURCE_DIR "${STUDIO_FFMPEG_SOURCE}"
        BINARY_DIR "${STUDIO_FFMPEG_BUILD}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND "${STUDIO_FFMPEG_SOURCE}/configure"
            --disable-everything --disable-programs --disable-doc --disable-network
            --disable-autodetect --disable-x86asm --enable-static --disable-shared --enable-pic
            --enable-avformat --enable-avcodec --enable-swscale --enable-protocol=file
            --enable-demuxer=mov,matroska,avi,image2
            --enable-decoder=h264,hevc,mpeg4,mpeg2video,mjpeg,png,vp8,vp9,rawvideo
            --enable-parser=h264,hevc,mpeg4video,mpegvideo,vp8,vp9 --disable-debug
        BUILD_COMMAND "${STUDIO_MAKE}" "-j${STUDIO_FFMPEG_JOBS}"
        BUILD_ALWAYS TRUE
        BUILD_BYPRODUCTS ${STUDIO_FFMPEG_LIBRARIES}
        INSTALL_COMMAND ""
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_OUTPUT_ON_FAILURE TRUE)
    add_dependencies(studio_ffmpeg studio_ffmpeg_build)
    target_include_directories(studio_ffmpeg INTERFACE "${STUDIO_FFMPEG_SOURCE}" "${STUDIO_FFMPEG_BUILD}")
    target_link_libraries(studio_ffmpeg INTERFACE ${STUDIO_FFMPEG_LIBRARIES} m Threads::Threads)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(STUDIO_AV REQUIRED IMPORTED_TARGET
        libavformat libavcodec libavutil libswscale)
    target_link_libraries(studio_ffmpeg INTERFACE PkgConfig::STUDIO_AV)
endif()

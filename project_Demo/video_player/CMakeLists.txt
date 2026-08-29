cmake_minimum_required(VERSION 3.16)
project(video_player C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 视频播放器(第六个基础验证项目)。催生的库能力:
#   CORE   —— Draw_ImgScaled(定点最近邻缩放 blit 图元,视频帧/缩略图/DPI 图标通用)
#   Image  —— 既有控件扩 NONE/FIT/FILL 缩放模式(不新增控件、不动 Canvas)
# 视频/音频语义全在 app 侧(与分层纪律一致):
#   vp_video —— ffprobe 探参 + ffmpeg(popen)流式解码 rgb565le + 非阻塞读 + 帧环形缓冲背压 + seek 重启
#   mp_audio —— 复用音乐播放器:ffmpeg 整曲 PCM + SDL2 声卡 + 播放主钟(A/V 同步的 master clock)
ymgui_add_app(video_player
	video_player.c
	vp_video.c
	mp_audio.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)+ 内置默认视频(无 argv 时用)
target_compile_definitions(video_player PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/tools/gb2312_glyphs.bin"
	VP_DEFAULT_VIDEO="${CMAKE_CURRENT_LIST_DIR}/卖瓜.mp4")

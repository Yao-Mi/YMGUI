cmake_minimum_required(VERSION 3.16)
project(music_player C)

# 咬合层:把仓库里的 YMGUI/ 当外部依赖引入(建 ymgui + sdl_lcd + SDL2)
include(${CMAKE_CURRENT_LIST_DIR}/../ymgui_app.cmake)

# 音乐播放器(第五个基础验证项目)。催生库控件:
#   Roller   —— 通用居中高亮平滑滚动列表(歌词/时间/日历通用,库不认识"歌词")
#   BarChart —— 通用柱状图(BY_HEIGHT/PER_BAR 配色 × NONE/BAR 顶标;频谱/直方图/统计柱图通用,值由外部喂)
# 音乐语义全在 app 侧(与分层纪律一致):
#   mp_audio    —— ffmpeg(popen)解码 + SDL2 声卡输出 + 播放时钟 + 暂停/seek + 静音兜底
#   mp_lrc      —— .lrc 歌词解析
#   mp_spectrum —— 整数 Goertzel 频段分析(无 libm)
ymgui_add_app(music_player
	music_player.c
	mp_audio.c
	mp_lrc.c
	mp_spectrum.c)

# GB2312 全字库外部 blob 路径(库在 Demo/ 下生成,项目复用同一份)+ 内置默认曲目(无 argv 时用)
target_compile_definitions(music_player PRIVATE
	GB2312_BIN_PATH="${YMGUI_REPO_ROOT}/Demo/gb2312_glyphs.bin"
	MP_DEFAULT_AUDIO="${CMAKE_CURRENT_LIST_DIR}/可能-队长.mp3"
	MP_DEFAULT_LRC="${CMAKE_CURRENT_LIST_DIR}/可能-队长.lrc")

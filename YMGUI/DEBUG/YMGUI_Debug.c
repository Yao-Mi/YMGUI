#include "YMGUI_Debug.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGUI_Debug.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-07-01
  *	@Description: 断言与日志横切层,被所有层调用。发布时可由 YMGUI_ASSERT_DEBUG/DEBUG_MODE 整体关闭
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  * <author> <time> <version > <desc>
  * yaomi 26/07/01 1.0 build this moudle
  *
  *   __  __ ___    ____   __  ___ ____     ______ ______ ______ __  __
  *   \ \/ //   |  / __ \ /  |/  //  _/    /_  __// ____// ____// / / /
  *    \  // /| | / / / // /|_/ / / /       / /  / __/  / /    / /_/ /
  *    / // ___ |/ /_/ // /  / /_/ /       / /  / /___ / /___ / __  /
  *   /_//_/  |_|\____//_/  /_//___/      /_/  /_____/ \____//_/ /_/
  *
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#if YMGUI_ASSERT_DEBUG
/**
  * @brief 断言失败信息输出
  */
void gy_assert_fail_inform(uint8* failfile, uint32 failline)
{
	gy_log_print("[YMGUI assert] file: %s, line: %u\n", (char*)failfile, failline);
}
#endif

#if YMGUI_DEBUG_MODE
/**
  * @brief 提示信息输出(附错误码)
  */
void gy_logout_inform(uint8* tips, GYEVNLOG event)
{
	gy_log_print("[YMGUI log 0x%02X] %s\n", (unsigned)event, (char*)tips);
}
#endif

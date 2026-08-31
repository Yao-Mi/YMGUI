# context_gesture - 上下文手势实验室

这个项目验证 SDL 平台输入到 YMGUI 上下文事件的完整链路：右键短点打开菜单，右键拖动或触摸长按后拖动移动卡片，失焦取消时恢复本次拖动起点。

本项目不新增控件。彩色卡片是基础 `GYOBJ` 配合应用侧 `draw_cb/event_cb` 绘制的；菜单由基础对象挂到 `top_layer` 组成。以前没有看到这种卡片，是因为它不是库里的 `Card` 控件，只是这个应用为展示上下文手势临时定义的视觉和交互。

## 覆盖的能力

| 能力 | 在项目中的作用 |
|---|---|
| `ContextRequested` | 右键短点或长按成立时请求菜单/捕获 |
| `ContextDragging` | 始终派给手势起点卡片，移动卡片位置 |
| `ContextReleased` | 正常结束拖动，保留最终位置 |
| `ContextCancelled` | 失焦或切后台时取消，并恢复手势起点 |
| `top_layer` | 承载菜单和全屏点击遮罩 |

## 构建 / 运行

```sh
cmake -S project_Demo/context_gesture -B build/rgb565/project_Demo/context_gesture
cmake --build build/rgb565/project_Demo/context_gesture
build/rgb565/project_Demo/context_gesture/context_gesture

# 无头运行并自动退出
SDL_VIDEODRIVER=dummy build/rgb565/project_Demo/context_gesture/context_gesture 20
```

设置 `YMGUI_SHOT` 时，应用会自动构造正常释放、取消恢复和菜单三个场景，供批量截图脚本使用。

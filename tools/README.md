# YMGUI Tools

`tools/` 放置生成器、外部字模资源和仓库级自动化脚本，不属于移植到 MCU 的 `YMGUI/` 库本体。

## 字体

```bash
# 生成 GB2312+符号索引和外部 flash blob
python3 tools/gen_font.py \
  YMGUI/CORE/YMGUI_FontDataGB2312.c \
  --cjk --gb2312 --extern \
  --bin tools/gb2312_glyphs.bin

# 快速检查入库文件的大小、哈希和字形数
tools/verify_gb2312.sh

# 真正重生成并逐字节比较，需 python3-pil + fonts-noto-cjk
tools/verify_gb2312.sh --rebuild
```

## 检查与测试

```bash
tools/check_repo.sh                 # 快速静态检查，不编译
tools/test.sh                       # 默认 RGB565 构建 + 35 个 CTest
tools/test.sh --depth 24            # RGB888
tools/test_matrix.sh                # 仓库检查 + RGB565/RGB888
tools/test_matrix.sh --all-depths   # 1/8/16/24 全像素格式
```

`test.sh` 默认把产物放在 `build/verify/rgb<depth>/`，可用 `--build-dir` 覆盖。构建并发默认不超过 8，可通过 `YMGUI_TEST_JOBS=4 tools/test.sh` 覆盖。脚本均以自身位置解析仓库根目录，可从任意工作目录调用。

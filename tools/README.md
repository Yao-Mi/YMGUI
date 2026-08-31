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

## 中文输入法词典

`gen_ime_dict.py` 从 Apache-2.0 的 `rime-pinyin-simp` 生成中文单字/词语数据，并可从 Apache-2.0 的 `wordfreq` 生成带常用度顺序的外挂英文词库：

```bash
curl -L -o /tmp/rime-pinyin-simp.zip \
  https://codeload.github.com/rime/rime-pinyin-simp/zip/refs/heads/master
unzip -p /tmp/rime-pinyin-simp.zip '*/pinyin_simp.dict.yaml' > /tmp/pinyin_simp.dict.yaml
python3 -m pip install wordfreq==3.1.1
python3 tools/gen_ime_dict.py --rime-dict /tmp/pinyin_simp.dict.yaml \
  --english-wordlist large
```

词语模型必须提供明确的分音节拼音和初始频率；生成器不会用单字默认读音拼出词语读音。
项目内的 `project_Demo/chinese_ime/user_phrases.tsv` 会自动作为用户词库合并，格式为 `词语<TAB>分音节拼音<TAB>初始词频`。也可重复使用 `--extra-dict` 合并其他 TSV 词库；不合法行会报告文件和行号并终止生成。

生成结果包括 7291 条单字读音的 `pinyin_gb2312.bin`、47278 条词语的 `phrases_rime.bin` 和 288996 个英文单词的 `english_words.bin`。不传 `--english-wordlist` 时只重建中文数据，避免强制中文生成环境安装 `wordfreq`。英文来源和许可见 `project_Demo/chinese_ime/ENGLISH_DICTIONARY.md`。

`python3 tools/verify_ime_dict.py` 同时校验三份外挂 BIN 的独立魔数、可扩展头、26 个首字母桶、条数、长度、字符串偏移和关键排序回归。应用的 `EXTERNAL` 模式也会在每次启动时执行同类校验。

## 检查与测试

```bash
tools/check_repo.sh                 # 快速静态检查，不编译
tools/test.sh                       # 默认 RGB565 构建 + 35 个 CTest
tools/test.sh --depth 24            # RGB888
tools/test_matrix.sh                # 仓库检查 + RGB565/RGB888
tools/test_matrix.sh --all-depths   # 1/8/16/24 全像素格式
```

`test.sh` 默认把产物放在 `build/verify/rgb<depth>/`，可用 `--build-dir` 覆盖。构建并发默认不超过 8，可通过 `YMGUI_TEST_JOBS=4 tools/test.sh` 覆盖。脚本均以自身位置解析仓库根目录，可从任意工作目录调用。

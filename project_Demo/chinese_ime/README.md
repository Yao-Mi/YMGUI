# chinese_ime

一个面向 YMGUI 的中文拼音输入法小应用：输入拼音或短语缩写后显示候选，点击候选文字或使用实体/屏幕数字键 `1..9` 选择，左右按钮翻页，回车选择首项。

```bash
# 在 YMGUI 仓库根目录执行
cmake -S project_Demo/chinese_ime -B build/rgb565/project_Demo/chinese_ime -DYMGUI_COLOR_DEPTH=16
cmake --build build/rgb565/project_Demo/chinese_ime -j
SDL_VIDEODRIVER=dummy ./build/rgb565/project_Demo/chinese_ime/chinese_ime 30
```

## 外挂字词库

中文单字、中文词语和英文单词是三份互相独立的数据。默认全部使用外挂 BIN；任一文件缺失或损坏时只关闭对应能力，键盘和其他已安装词库继续工作：

| 数据 | 默认文件 | CMake 模式 | 缺失时行为 |
|---|---|---|---|
| 中文单字拼音 | `pinyin_gb2312.bin` | `YMGUI_IME_CHARS=EXTERNAL/EMBEDDED/OFF` | 不显示单字候选，短语仍可用 |
| 中文词语 | `phrases_rime.bin` | `YMGUI_IME_PHRASES=EXTERNAL/EMBEDDED/OFF` | 不显示词语和词语组合，单字仍可用 |
| 英文单词 | `english_words.bin` | `YMGUI_IME_ENGLISH=EXTERNAL/OFF` | 英文键恢复逐字直接上屏 |

例如指定三份外挂文件：

```bash
cmake -S project_Demo/chinese_ime -B build/rgb565/project_Demo/chinese_ime \
  -DYMGUI_COLOR_DEPTH=16 \
  -DYMGUI_IME_CHAR_BIN=/data/pinyin_gb2312.bin \
  -DYMGUI_IME_PHRASE_BIN=/data/phrases_rime.bin \
  -DYMGUI_IME_ENGLISH_BIN=/data/english_words.bin

# 最小键盘：不安装任何候选数据
cmake -S project_Demo/chinese_ime -B build/ime-off \
  -DYMGUI_COLOR_DEPTH=16 \
  -DYMGUI_IME_CHARS=OFF -DYMGUI_IME_PHRASES=OFF -DYMGUI_IME_ENGLISH=OFF

# 无文件系统固件仍可内嵌中文单字和词语；英文大词库保持关闭
cmake -S project_Demo/chinese_ime -B build/ime-embedded \
  -DYMGUI_COLOR_DEPTH=16 \
  -DYMGUI_IME_CHARS=EMBEDDED -DYMGUI_IME_PHRASES=EMBEDDED -DYMGUI_IME_ENGLISH=OFF
```

三种 BIN 都使用独立魔数、定长记录、NUL 字符串区和 27 个桶边界。启动时验证魔数、版本、头长度、记录尺寸、字母桶、文件长度、字符串偏移和内容范围。当前 Demo 为追求查询速度会把已安装 BIN 和运行时指针表载入 RAM，不是逐条磁盘查询；MCU 可将同一校验/索引协议改接 flash 读取。

词语按首个拼音字母分成 26 个逻辑桶，仍保存在同一个文件中；P2 文件头用 27 个偏移描述每个桶的起止位置，桶内按初始词频降序。组合搜索只扫描当前位置字母对应的桶，普通候选在查询状态中保存桶内游标，下一页不会回到桶头重扫。文件头同时记录 `header_size`、`record_size` 和 `bucket_count`，后续可以追加二级前缀或独立缩写索引，而不改变现有词语记录主体。

单字索引也按首拼音字母生成 26 个桶。每次查询只扫描对应桶一次，建立去重并按“用户次数、静态字频”排序的指针表，随后各页直接使用游标读取；组合预测在每个输入位置分别只扫描一次词语桶和单字桶，再把匹配项扩展到该位置的全部束状态，不随束宽重复扫描。英文普通候选按 wordfreq 初始频率沿桶游标懒读取；只有存在用户学习记录时才扫描当前桶收集最多 64 个已学习词并优先排序，不为 288996 个词另建全量候选指针表。

单字和短语候选由 Apache-2.0 的 `rime-pinyin-simp` 词典及项目用户词库生成：`pinyin_gb2312.inc/.bin` 包含同一批 7291 条单字读音；`phrases_rime.inc/.bin` 包含 47278 条经 GB2312 字形过滤的系统词语和 2 条项目用户词语，共 47280 条。不会从单字默认读音猜测词语拼音。英文 `english_words.bin` 来自 Apache-2.0 的 `wordfreq 3.1.1` large 英文表，过滤后为 288996 个词，详细来源见 [ENGLISH_DICTIONARY.md](ENGLISH_DICTIONARY.md)。

短语按音节边界匹配，每个音节可输入一个或多个前缀字母。例如 `我们 = wo'men` 支持 `wm`、`wom`、`wme`、`wome` 和 `women`，但不会把 `wn` 这种跨音节跳字母的任意子序列误判为命中。

当词典没有整句记录时，输入法还会对已有词条和单字做小规模束搜索组合。例如词典中的 `看看 = kan'kan/kk`、`你的 = ni'de/nd` 和单字 `你 = ni/n`，可以组合出 `kkn -> 看看你`，以及 `kknd` 或 `kankannide -> 看看你的`，不需要手写整句特例。组合候选与普通词语进入同一套词频和用户习惯排序。

候选排序会记录本次运行中用户的选择次数；同一词被反复上屏后，会在对应拼音候选中优先显示。统计保存在内存中，重启后清空，避免给裸机应用引入文件系统依赖。

组合区使用“上一页 | 真实组合 TextInput | 下一页”布局，下一行是独立的全宽只读候选文本，格式为 `1 我  2 沃  3 卧`。组合框支持鼠标定位光标和选区；实体或虚拟字母都在当前光标插入或替换选区，`Bksp` 也按当前光标删除，不再强制追加或删除末尾。每页按字体实际像素宽度动态装入最多 9 项，放不下的候选完整移到下一页，不再使用五等分固定槽；前缀候选仍按页懒排序。只要当前输入还能匹配单字拼音（如 `k`、`wo`、`zhong`），就先排完所有匹配单字，再显示词语；`wm`、`nihao` 等不可能是单字拼音的输入直接进入词语候选。

组合缓存为空且不在符号页时，左右翻页键改为 `收起/展开`。`收起` 会隐藏从 Esc 到底部功能行的整个虚拟键盘，保留组合行和候选行供实体键盘使用；`展开` 恢复主键盘。开始输入后两键自动恢复为候选翻页。

实体键盘的字母进入组合缓存；空格、数字和 ASCII 标点会先提交当前组合，再立即上屏，不需再按回车。有候选时 `1..9` 仍优先选择对应候选。实体 Shift/Ctrl/Alt 只在按住期间生效，虚拟 Shift/Ctrl/Alt 保持点动锁存，两种来源复用同一按键状态和回调；实体或虚拟 Caps 都点动翻转。中文模式下主键盘的双字符键显示并输出全角中文标点，英文模式恢复 ASCII；Shift 锁存在中英切换时保留。虚拟或实体 `Ctrl+A/C/X/V/Z/F` 直接作用于上屏 EditView，不再误作用于组合 TextInput。`Tab`、`Bksp` 和 Enter 同样由统一入口处理并显示按下状态；`Bksp` 在组合缓存非空时删缓存，缓存为空时删除上屏 EditView 当前光标前的字符。

底部 `符号` 键打开独立符号层，每页提供 4×9 个符号位，并复用候选栏原有的上一页/下一页浏览当前分类；候选文本区域在此时显示中英文模式、分类和页码。底部保留 `常用/中英/返回/数学/角标/序号/希腊/拼音/箭头` 九个分类键。中文、英文常用符号分别维护，切换中英文会立即换表并回到第一页；七分类共 489 个条目，末页不足 36 项时隐藏空位，只有 `返回` 回到实体键盘式主层。中文组合态点击符号时先上屏首候选，再追加所选符号。

页面上方使用可折行的多行 EditView 作为上屏区，并设置 4px 四边内边距，使首行和边框之间保留空隙。用户可点击定位光标、拖动选字并继续编辑。候选词、英文、空格、换行和符号都在当前光标上屏，有选区时替换选区；不再由应用维护只能末尾追加的字符数组。按回车时有候选则选择首项，没有候选则把 TextInput 中的原始组合串直接上屏并清空缓存。

重新生成词典：

```bash
curl -L -o /tmp/rime-pinyin-simp.zip \
  https://codeload.github.com/rime/rime-pinyin-simp/zip/refs/heads/master
unzip -p /tmp/rime-pinyin-simp.zip '*/pinyin_simp.dict.yaml' > /tmp/pinyin_simp.dict.yaml
python3 -m pip install wordfreq==3.1.1
python3 tools/gen_ime_dict.py --rime-dict /tmp/pinyin_simp.dict.yaml \
  --english-wordlist large
```

要补充词库，编辑 `user_phrases.tsv`，每行填写三个字段并用制表符分隔：

```text
词语<TAB>分音节拼音<TAB>初始词频
看看你的<TAB>kan kan ni de<TAB>50000
嘎达<TAB>ga da<TAB>50000
尼玛<TAB>ni ma<TAB>100000
```

当前仓库已经启用“嘎达”和“尼玛”：输入完整拼音 `gada` / `nima` 或首字母缩写 `gd` / `nm` 都可匹配。随后重新运行上面的生成命令并重新编译。生成器会同步更新内嵌 `.inc` 和外挂 `.bin`；默认用户词库会自动合并，还可重复传入 `--extra-dict path/to/words.tsv` 合并其他文件。生成器会校验拼音音节数、词频范围以及每个字符是否存在于 YMGUI GB2312 字形索引中。

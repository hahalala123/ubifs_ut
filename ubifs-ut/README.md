# UBIFS User-space Unit Test Framework（面试题原型）

让主线 `fs/ubifs/scan.c` 中的 **`ubifs_scan_a_node()`** 在不依赖开发板、
MTD 模拟器或虚拟机的情况下，于用户态直接编译、运行、回归验证。

## 设计要点

- **被测代码零修改**：`src/scan.c` 是主线源码原样拷贝
  （基线：`fs/ubifs/scan.c @ 69050f8d6d075dc01af7a5f2f550a8067510366f`），
  一行未改。改了几行内核代码后，只需重新拷贝该文件并 `make test`，
  即可快速验证行为是否回归——这正是面试题要求解决的问题。
- **桩头文件** `include/ubifs.h`：用用户态实现替代内核基础设施
  （`list_head`、`leXX_to_cpu`、kmalloc 宏、调试宏）。
  on-flash 格式结构体（`struct ubifs_ch`、`ubifs_pad_node`、magic 等）
  与 `ubifs-media.h` **逐字节一致**，因为被测函数解析的就是线上格式。
- **校验语义保真**：`tests/ut_support.c` 中的 `ubifs_check_node()`
  桩复刻了内核版本对扫描路径有意义的检查（头长度 + common header
  CRC32，CRC 算法与内核 `crc32()` 相同），因此
  `SCANNED_A_CORRUPT_NODE` 分支也是真实可达的。

## 目录结构

```
ubifs-ut/
├── include/ubifs.h          # 用户态桩（替代内核 ubifs.h 子集）
├── src/scan.c               # 主线 fs/ubifs/scan.c 原样拷贝【被测代码】
├── tests/
│   ├── ut_support.h/.c      # CRC32、check_node 桩、节点构造辅助、CHECK 宏
│   └── test_scan_a_node.c   # 测试用例（pad node 校验为重点）
├── Makefile                 # make / make test / make verify
├── verify-kernel.sh         # 内核树端到端取证（clone→am→diff→test→基线哈希）
├── INTERVIEW.md             # 面试自述稿（讲解顺序 + 预埋 Q&A）
└── patches/                 # 生成好的内核 patch（新增文件，零侵入）
```

## 运行

```sh
make          # 编译测试二进制（build/ubifs_ut）
make test     # 编译（如需要）并运行测试
# 输出: ubifs_scan_a_node: 22 checks, 0 failures
#       PASS: all tests passed

make verify   # 可选：再注入 3 个变异，验证测试确实能检出回归
# 期望: 3 行 "mutant caught: ..." + "PASS: all mutants caught"
```

已做变异验证（mutation check）：对 `src/scan.c` 分别注入三类典型回归
（对齐掩码 `&7→&3`、边界条件 `>→>=`、padding 字节比较 `==→!=`），
测试均能在 1 秒内准确检出并失败退出。本机 Windows 无 gcc 时，可用便携 TinyCC：

```sh
tools/tcc/tcc.exe -Iinclude -Itests src/scan.c tests/ut_support.c \
    tests/test_scan_a_node.c -o ubifs_ut_test.exe
./ubifs_ut_test.exe
```

## 测试覆盖（对应面试题第 1 问）

重点：**padding node 校验**（挂载/GC 扫描时命中频率最高的分支）：

| 用例 | 覆盖分支 |
|---|---|
| `test_valid_pad_node` | 合法 pad node，返回 node_len + pad_len 总跳过长度 |
| `test_pad_node_overruns_leb` | `offs + node_len + pad_len > leb_size` |
| `test_pad_node_negative_pad_len` | `pad_len` 解释为负（最高位置位） |
| `test_pad_node_misaligned_total` | `(node_len + pad_len) & 7` 未 8 字节对齐 |
| `test_pad_node_36_bytes_is_misaligned` | 36 字节总长度：4 对齐但非 8 对齐，区分 `&7` 与弱校验 |
| `test_pad_node_ends_exactly_at_leb_end` | 边界：`offs+node_len+pad_len == leb_size` 必须接受（检查是 `>` 不是 `>=`） |

其它分支（保证函数整体可回归）：

| 用例 | 覆盖分支 |
|---|---|
| `test_empty_space` | magic 全 0xFF → `SCANNED_EMPTY_SPACE` |
| `test_raw_padding_bytes` | 无头 padding 字节流，8 对齐 → 返回 pad 长度 |
| `test_raw_padding_misaligned` / `test_garbage` | `SCANNED_GARBAGE` |
| `test_len_too_small` | `len < UBIFS_CH_SZ` |
| `test_corrupt_header_crc` | 头 CRC 错 → `SCANNED_A_CORRUPT_NODE` |
| `test_valid_plain_node` | 合法普通节点 → `SCANNED_A_NODE` |
| `test_ubifs_scan_collects_nodes` | 集成：`ubifs_scan()` 扫描手工构造的 LEB |

## 如何变成内核 patch

`patches/` 下的 patch 将本框架以 `tools/testing/ubifs/` 形式加入内核源码树
（纯新增文件，不触碰 `fs/ubifs/` 任何一行）。在任意主线 checkout 上：

```sh
git am patches/0001-ubifs-add-userspace-unit-test-framework.patch
# 或: patch -p1 < patches/0001-ubifs-add-userspace-unit-test-framework.patch
```

一条命令完成全部取证（浅克隆主线 → `git am` → 逐字节 diff → 树上
`make test` → 打印基线 commit 哈希）：

```sh
./verify-kernel.sh          # 默认 ~/linux，可用 LINUX_DIR 指定路径
```

注意：请在 Linux 上运行；Windows 的 `core.autocrlf` 可能让逐字节 diff 误报。

维护方式：每次要测新改动时，`cp fs/ubifs/scan.c src/scan.c` 后 `make test`。

## 扩展到面试题第 2 问（journal replay）

框架对第 2 问同样适用：日志重放（`replay.c`）的核心逻辑是"按 sqnum 决定
同一 inode 多个节点的生效版本"。思路：

1. 用 `ut_make_node()` 构造一串带不同 `sqnum` 的 ino/data/trun 节点；
2. 按 `ubifs_scan()` 的输出格式（`struct ubifs_scan_leb` 链表）组装；
3. 对 replay 的纯决策函数（如排序/截断生效判定）直接喂数据断言。

需要 stub 的面比 scan.c 大（TNC、bud 链表），建议先挑 replay 中
不依赖 TNC 的纯函数切入。

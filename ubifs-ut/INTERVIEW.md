# 面试自述稿：UBIFS 用户态单元测试框架

> 用途：面试时对着这份稿子讲，5~8 分钟。每节给出"讲什么"和"被追问时怎么答"。

---

## 1. 我对题目的理解（30 秒）

题目表面是"写单元测试"，实际考三件事：

1. **能不能在陌生的大型内核子系统里快速定位关键逻辑**——我选了 `fs/ubifs/scan.c`，因为 `ubifs_scan_a_node()` 是 UBIFS 所有介质读取的必经入口：挂载时的 journal replay、GC、调试 dump 全部经过它；
2. **能不能绕开"必须上开发板"的限制做可回归测试**——我的答案是用户态 stub 框架，把未修改的主线 `scan.c` 直接编译进测试二进制；
3. **工程交付规范**——patch 是纯新增文件（`tools/testing/ubifs/`），零侵入，可 `git am`，并明确锁定基线 commit。

## 2. 测试点选型：为什么是 pad node（1 分钟）

`ubifs_scan_a_node()` 的返回值分三类：空空间（-1）、合法节点（-2）、各类异常（0/-3/-4），正数表示裸 padding 字节数。其中 **padding node 校验分支是挂载/GC 扫描时命中频率最高、逻辑最密的决策点**，包含三个拒绝子分支：

- `offs + node_len + pad_len > leb_size`——越出 LEB 末尾；
- `pad_len` 解释为负（最高位置位）——恶意/损坏镜像的边界；
- `(node_len + pad_len) & 7`——UBIFS 所有节点必须 8 字节对齐的格式约束。

题目只要求"可能性最大的 1 点"，我把这个分支做成 6 个用例（3 个拒绝子分支 + 3 个边界：合法、恰好结束在 LEB 末尾、36 字节"4 对齐但非 8 对齐"），其余分支也补齐，共 22 项断言。

## 3. 框架设计取舍（1~2 分钟）

**为什么不用 KUnit / MTD 模拟器：**

| 方案 | 问题 |
|---|---|
| KUnit（内核内） | 需要内核构建+启动环境，UBIFS 无现成 KUnit 设施，stub 面大 |
| nandsim/mtdram + 真实挂载 | 需要 root、MTD 子系统、U-Boot/VM 环境，CI 不友好，单次验证分钟级 |
| **用户态 stub（我的选择）** | `make test` 1 秒完成，任何有 gcc 的机器可跑，CI 直接挂 |

关键设计：**被测代码零修改**。`src/scan.c` 是主线逐字节拷贝，on-flash 结构体（`ubifs_ch`、`ubifs_pad_node`、magic、SCANNED 返回值）与内核定义严格一致；`ubifs_check_node` 的桩复刻了真实语义（头长度 + common header CRC32，CRC 与内核 `crc32()` 同算法），所以 `SCANNED_A_CORRUPT_NODE` 分支真实可达。

## 4. 验证结果（1 分钟，最有说服力的一段）

- `make test`：**22 项断言全过**（x86 Windows/TinyCC 与 ARM 鲲鹏服务器/gcc 双平台一致）；
- `make verify`（变异验证）：故意注入三类典型回归——对齐掩码 `&7→&3`、边界 `>→>=`、padding 字节比较 `==→!=`——**测试 1 秒内全部检出**。这直接回答了"改了函数几行代码能否快速验证"；
- `verify-kernel.sh`：浅克隆主线 → `git am` 应用 patch → `diff` 证明被测文件与主线逐字节一致 → 树上 `make test` → 打印基线 commit。证据链完整可复现。

**已完成的取证（2026-09-21，ARM 服务器实测）：**

- 树基线：`93f51579e7df248780214094418f205253383cc5`（Linux 7.3-rc4，2026-09-20），`git am` 干净应用；
- 文件基线：`fs/ubifs/scan.c @ 69050f8d6d075dc01af7a5f2f550a8067510366f`，与 `src/scan.c` 逐字节一致（`diff` 输出 identical）；
- 树上 `make test`：22 checks, 0 failures。
- 注意点：浅克隆下 `git log -- <路径>` 会误显示边界快照提交（无父提交无法 diff），文件级基线应以 GitHub API 或完整历史为准——这个细节被追问到能答上来是加分项。

## 5. 已知局限与扩展（30 秒，主动说显得诚实）

- 桩假设小端（x86/ARM 满足）；big-endian 需改 `leXX_to_cpu` 宏；
- `ubifs_check_node` 桩只保留头校验，未实现内核的节点类型长度范围表（`c->ranges[]`），如需可补；
- **第 2 问扩展路线已想好**：journal replay（`replay.c`）的核心是"按 sqnum 决定同一 inode 多版本的生效顺序"，可用同一框架构造带不同 sqnum 的 ino/data/trun 节点序列，对 replay 的纯决策函数直接断言——扫描层的 `ubifs_scan()` 集成测试已经演示了 LEB 图像的构造方法。

## 6. 预埋 Q&A

**Q：SCANNED 返回值为什么是 0 和负数？**
A：正数要保留给"裸 padding 字节数"（`ret > 0` 在 `ubifs_scan` 里直接作为跳过长度）。我最初凭印象写成 0~4，调试时发现 `ret > 0` 分支会吞掉节点码，核对内核 `ubifs.h` 才确认是 0 和负数——这个 bug 让我深刻理解了"桩的语义必须与内核逐一对齐"。

**Q：怎么保证测试测的不是你自己改过的 scan.c？**
A：`verify-kernel.sh` 第 3 步 `diff fs/ubifs/scan.c src/scan.c`，逐字节比对，不同则自动更新并提示。

**Q：和 KUnit 比，你的方案缺点是什么？**
A：不跑在真实内核里，无法覆盖与内核调度器/锁/内存管理交互的路径；但换来的是零环境依赖和秒级反馈，对纯解析/校验逻辑（扫描层正是这类）性价比最高。

**Q：如果面试官要求第 2 问现在做？**
A：我会选 replay 中"不依赖 TNC 的纯决策逻辑"切入——比如按 sqnum 排序选取生效版本的判定，输入用 `ut_make_node()` 构造的节点序列，输出断言最终生效版本，1~2 个用例即可覆盖核心正确性。

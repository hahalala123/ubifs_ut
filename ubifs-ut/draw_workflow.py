# -*- coding: utf-8 -*-
"""UBIFS 用户态单元测试框架 · 工作流程示意图"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.executable).parent.parent.parent))
from daimon_runtime import setup_plot
setup_plot()
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle

OUT = Path(sys.argv[0]).parent / 'docs' / '工作流程示意图.png'

fig, ax = plt.subplots(figsize=(15.5, 9.6), dpi=140)
ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis('off')

ax.text(50, 97.8, 'UBIFS 用户态单元测试框架 —— 工作流程（日常回归 / 变异验证 / 主线取证）',
        ha='center', fontsize=16, weight='bold')
ax.text(50, 94.6, '被测代码：主线 fs/ubifs/scan.c 逐字节拷贝，零修改 · 22 项断言秒级反馈',
        ha='center', fontsize=10, color='#555')

def lane(y0, y1, label, color):
    ax.add_patch(FancyBboxPatch((11, y0), 87, y1 - y0,
        boxstyle='round,pad=0.3,rounding_size=1.2',
        fc=color, ec='#999', alpha=.45, lw=1))
    ax.text(9.6, (y0 + y1) / 2, label, ha='right', va='center',
            fontsize=10.5, weight='bold')

def box(x, y, w, h, text, fc='#ffffff', fs=9.3, ec='#444', dashed=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h,
        boxstyle='round,pad=0.25,rounding_size=0.8',
        fc=fc, ec=ec, lw=1.1, linestyle='--' if dashed else '-'))
    ax.text(x + w / 2, y + h / 2, text, ha='center', va='center', fontsize=fs)

def arrow(x0, y0, x1, y1, color='#2563eb', lw=1.8, ls='-', style='-|>'):
    ax.add_patch(FancyArrowPatch((x0, y0), (x1, y1), arrowstyle=style,
        mutation_scale=15, color=color, lw=lw, linestyle=ls, zorder=4))

def badge(x, y, num, color='#ea580c'):
    ax.add_patch(Circle((x, y), 1.75, fc=color, ec='white', lw=1, zorder=6))
    ax.text(x, y, num, ha='center', va='center', fontsize=9.5,
            color='white', weight='bold', zorder=7)

def note(x, y, text, fs=8.6, color='#333', ha='left', weight='normal'):
    ax.text(x, y, text, ha=ha, va='center', fontsize=fs, color=color, weight=weight)

# ---------------- 泳道 ----------------
lane(84, 93, '主线内核树\n（torvalds/linux）', '#fde68a')
lane(62, 80, '测试工程\nubifs-ut', '#bfdbfe')
lane(32, 58, 'make test\n（测试执行）', '#bbf7d0')
lane(16, 28, 'make verify\n（变异验证）', '#fecaca')
lane(1.5, 12.5, 'verify-kernel.sh\n（主线取证）', '#e9d5ff')

# ---------------- 泳道 1：内核树 ----------------
box(13, 86, 30, 5.5, 'fs/ubifs/scan.c\n基线 69050f8d6d07')
box(50, 86, 22, 5.5, '树基线 93f51579\n（Linux 7.3-rc4）')
note(76, 88.75, 'patch 应用目标；测试代码出处', fs=8.3, color='#555')

# ---------------- 泳道 2：测试工程 ----------------
box(13, 70.5, 24, 6.5, 'src/scan.c\n被测代码\n逐字节拷贝，零修改')
box(41, 70.5, 24, 6.5, 'include/ubifs.h\n用户态桩\n（内核原语 + on-flash 格式）')
box(69, 70.5, 24, 6.5, 'tests/\nut_support.c 桩实现\n+ 22 项用例')
box(27, 63, 20, 5, 'gcc 编译\n（-Wall -Wextra）')
box(55, 63, 20, 5, 'build/ubifs_ut\n测试二进制')
arrow(43.2, 73.75, 40.8, 73.75, style='-')
arrow(13, 86, 22, 77.4)
badge(15.5, 82.3, '1')
note(17.5, 82.3, 'cp fs/ubifs/scan.c src/scan.c（唯一维护动作）', fs=8.3)
arrow(33.2, 65.5, 54.6, 65.5)
arrow(65.2, 73.75, 69, 68.4, style='-')

# ---------------- 泳道 3：make test ----------------
box(13, 48, 26, 6.5, 'ubifs_scan_a_node()\n解析节点：magic → CRC →\ntype → pad_len → 对齐')
box(43, 48, 24, 6.5, 'ubifs_check_node 桩\n真实 CRC32 头校验\n（同内核算法）')
box(71, 48, 22, 6.5, '返回 SCANNED_*\n>0 pad字节 / 0 垃圾\n-1 空 / -2 节点 / -3 坏 / -4 pad非法')
arrow(65.2, 65.5, 26, 55)
badge(58, 60.5, '2')
box(13, 36, 26, 6.5, 'CHECK_EQ 断言\n（22 checks）', fc='#ecfdf5')
box(43, 36, 24, 6.5, '重点：pad node 校验\n3 个拒绝子分支 + 3 个边界', fc='#ecfdf5')
box(71, 36, 22, 6.5, 'PASS / FAIL\n退出码 0/1（CI 可判定）', fc='#ecfdf5')
arrow(39.2, 51.25, 42.6, 51.25)
arrow(67.2, 51.25, 70.6, 51.25)
arrow(82, 48, 26, 43)
arrow(39.2, 39.25, 42.6, 39.25)
arrow(67.2, 39.25, 70.6, 39.25)
note(13, 33.4, '反馈周期 < 1 秒；改 scan.c 后 make test 即回归', fs=8.4, color='#555')

# ---------------- 泳道 4：make verify ----------------
box(13, 18.5, 30, 6.5, 'sed 注入 3 个变异\n&7→&3 / >→>= / ==→!=')
box(49, 18.5, 22, 6.5, '分别重编译运行\n期望全部 FAIL')
box(77, 18.5, 17, 6.5, 'mutant caught ×3\n（测试有牙齿）', fc='#fef2f2')
arrow(43.2, 21.75, 48.6, 21.75, color='#dc2626')
arrow(71.2, 21.75, 76.6, 21.75, color='#dc2626')
badge(45.5, 24.6, '3', color='#dc2626')

# ---------------- 泳道 5：verify-kernel.sh ----------------
box(13, 3.5, 17, 6, '浅克隆主线\n--depth 1')
box(34, 3.5, 15, 6, 'git am\n应用 patch')
box(53, 3.5, 15, 6, 'diff 逐字节\n比对 identical')
box(72, 3.5, 12, 6, '树上\nmake test')
box(88, 3.5, 9.5, 6, '基线\n哈希')
arrow(30.2, 6.5, 33.6, 6.5)
arrow(49.2, 6.5, 52.6, 6.5)
arrow(68.2, 6.5, 71.6, 6.5)
arrow(84.2, 6.5, 87.6, 6.5)
badge(93.5, 11.2, '4')
note(13, 14.6, '证据链：patch 可合入 + 被测代码同一 + 测试通过 + 基线可引用（树 93f51579 / 文件 69050f8d）',
     fs=8.4, color='#555')

plt.savefig(OUT, bbox_inches='tight')
print('saved:', OUT)

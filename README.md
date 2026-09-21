# CSAPP Labs

这是《深入理解计算机系统》（CSAPP）的实验练习仓库，主要用于记录 7 个核心实验的实现代码、答案文件和本地/服务器测试结果。

## 实验完成情况

| 实验 | 目录 | 状态 |
| --- | --- | --- |
| Data Lab | `datalab-handout` | 已完成 |
| Bomb Lab | `bomb` | 已完成，答案见 `bomb/answers.txt` |
| Attack Lab | `target1` | 已完成，答案见 `target1/solutions` |
| Cache Lab | `cachelab-handout` | 已完成 |
| Shell Lab | `shlab-handout` | 已完成 |
| Malloc Lab | `malloclab-handout` | 已完成 |
| Proxy Lab | `proxylab-handout` | 已完成 |

## 目录说明

- `datalab-handout/`：位级运算练习，实现 `bits.c` 中的函数。
- `bomb/`：Bomb Lab 二进制拆弹实验，`answers.txt` 保存已验证答案。
- `target1/`：Attack Lab，`solutions/` 保存各阶段 payload。
- `cachelab-handout/`：缓存模拟器与矩阵转置优化实验。
- `shlab-handout/`：Tiny Shell，实现作业控制、信号转发和内置命令。
- `malloclab-handout/`：动态内存分配器实现。
- `proxylab-handout/`：HTTP 代理服务器，实现转发、并发和缓存。

## 测试说明

建议在 Linux 环境或仓库配套的容器环境中运行测试。部分实验依赖 32 位编译环境、`make`、`gcc`、`perl`、`gdb`、`curl` 等工具。

常用验证方式：

```bash
cd datalab-handout && make clean && make btest && ./dlc bits.c && ./btest
cd cachelab-handout && make clean && make && ./test-csim && ./test-trans -M 32 -N 32
cd shlab-handout && make clean && make && ./sdriver.pl -t trace01.txt -s ./tsh -a "-p"
cd malloclab-handout && make clean && make && ./mdriver
cd proxylab-handout && make clean && make && ./driver.sh
```

Bomb Lab 和 Attack Lab 建议在隔离环境中执行，避免误操作影响本机环境。

## 备注

本仓库内容仅用于个人学习和复习 CSAPP 实验流程。各实验目录中保留了原始 handout 文件、实现代码以及必要的答案文件。

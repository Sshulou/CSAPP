# Attack Lab 解题与验证

本目录保存 `target1` 的 5 个 payload。每个 `phase*.txt` 都是给 `hex2raw` 读取的十六进制文本。

目标参数：

- Cookie: `0x59b997fa`
- `getbuf` 缓冲区大小：`0x28`，所以覆盖返回地址前填充 40 字节。
- `touch1`: `0x4017c0`
- `touch2`: `0x4017ec`
- `touch3`: `0x4018fa`

## 在服务器上验证

切换到 `codex` 用户后执行下面这条命令。它会进入和测试时相同的 Podman 容器，把 `phase*.txt` 转成 raw 文件，然后依次验证 5 关。

```bash
export XDG_RUNTIME_DIR=/run/user/1000
mkdir -p /home/codex/attack-work

podman run --rm --network none \
  -v /home/codex/csapp-test-run:/workspace:ro,Z \
  -v /home/codex/attack-work:/work:Z \
  -w /workspace/target1 \
  localhost/csapp-devcontainer:latest \
  bash -lc '
    for n in 1 2 3 4 5; do
      ./hex2raw < solutions/phase$n.txt > /work/phase$n.raw
    done

    for n in 1 2 3; do
      echo "===== phase$n ctarget ====="
      ./ctarget -q -i /work/phase$n.raw
    done

    for n in 4 5; do
      echo "===== phase$n rtarget ====="
      ./rtarget -q -i /work/phase$n.raw
    done
  '
```

看到每关输出 `Touch*`，并且最后有 `PASS`，表示该关成功。第 5 关成功时会看到类似：

```text
Touch3!: You called touch3("59b997fa")
PASS
```

## 每关思路

Phase 1：直接覆盖返回地址，跳转到 `touch1`。

Phase 2：在 `ctarget` 栈上注入代码，将 `%rdi` 设置为 cookie，再跳到 `touch2`。

Phase 3：在 payload 后面放入字符串 `59b997fa`，注入代码把 `%rdi` 指向该字符串，再跳到 `touch3`。

Phase 4：`rtarget` 不能执行栈上代码，用 gadget 链设置 `%rdi = cookie`，再跳到 `touch2`。

Phase 5：用 ROP 链计算栈上字符串地址，把 `%rdi` 指向 `59b997fa`，再跳到 `touch3`。

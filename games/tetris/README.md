# Terminal Tetris

一个基于终端的俄罗斯方块小游戏，实现了基本的掉落、旋转、硬降和消行逻辑，不依赖额外库即可直接在 Linux/macOS 终端编译运行。

## 编译

```bash
gcc -std=c11 -Wall -Wextra -O2 tetris.c -o tetris
```

## 运行

```bash
./tetris
```

## 操作说明

- `a`：向左移动
- `d`：向右移动
- `s`：加速下落
- `w`：顺时针旋转
- `空格`：硬降，直接落到底
- `q`：退出游戏

游戏得分按照每次消行 *100 计分，按 `Ctrl+C` 退出时会自动恢复终端模式。

# RTL 目录使用说明

此目录用于存放被测 CPU 的 Verilog 代码。

## 使用方法

1. 将 CPU 顶层模块 `CPU.sv`（或 `CPU.v`）及其依赖文件放到本目录。
2. 顶层模块名必须为 `CPU`（可通过 `RTL_TOP=<name>` 覆盖）。
3. 运行 `make all` 即可自动编译。

## 支持的文件类型

- `*.sv`（SystemVerilog）
- `*.v`（Verilog）

所有文件会被递归扫描并加入编译。

## CPU 接口要求

详见 `docs/接口规范.md`。

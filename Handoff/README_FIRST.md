# STM32H743 项目交接包

生成时间：2026-08-28（Asia/Shanghai）

请先阅读 `HANDOFF.md`。它记录当前工程基线、硬件接线、CH9350 配置、软件结构、已验证功能、待验证项目和考核遗漏点。

## 包内内容

- `HANDOFF.md`：完整交接说明，以此文件为准。
- `NEXT_SESSION_PROMPT.md`：可直接发给下一次 Codex 对话的上下文。
- `firmware/atk_h743.hex`：交接时重新全量编译生成的固件。
- `build/handoff_rebuild.log`：与交付 HEX 对应的 Keil 全量编译日志。
- `build/atk_h743.map`：同一次构建的链接映射文件。
- `reference/嵌入式组考核(2).pdf`：原始考核要求。
- `reference/CH9350.pdf`：CH9350 芯片资料。
- `reference/CH9350模块说明.png`：用户提供的模块、拨码和接线说明截图。
- `project_source_snapshot.zip`：当前完整工程源码快照，排除 `Output` 中间文件、`.tmp` 和交接文档本身。
- `manifest/SHA256SUMS.txt`：关键文件 SHA-256。
- `manifest/SOURCE_FILE_LIST.txt`：源码快照文件清单。

## 重要约定

当前有效工程是：

```text
E:\STM32Project\STM32H743_5inch_GT911_Test
```

Keil 工程为：

```text
Projects\MDK-ARM\atk_h743.uvprojx
Target: TOUCH
Arm Compiler: 6.24
```

当前目录不是 Git 仓库。下一次修改前先解压并保留本交接包，不要用旧的 stage 或历史快照覆盖当前工程。

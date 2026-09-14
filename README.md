# 宠爱云护 · 智能宠物项圈工作区

这是一个多端产品工作区，不是单一语言仓库。两个主固件、Web/微信端、项目材料和实验归档按职责分区；每个可运行工程仍保留原构建系统的惯例结构。

## 可运行工程

| 路径 | 技术栈 | 入口与验证 |
|---|---|---|
| `apps/careguard-live/` | Vite 8、原生 JavaScript、Tailwind CSS、Node.js、微信小程序 | `npm test`、`npm run build`、`npm run dev` |
| `firmware/esp32-s3/` | C/C++、PlatformIO、Arduino + ESP-IDF component | `pio run -e esp32-s3-devkitc-1` |
| `firmware/stm32f103-keil/` | C、STM32F103、FreeRTOS、STM32 Standard Peripheral Library、Keil uVision 5/ARMCC | `Project.uvprojx`；需 Windows + Keil MDK |

## 目录

```text
.
├── apps/                    # 用户端应用
│   └── careguard-live/      # Web 大屏、Node 代理与微信小程序
├── firmware/                # 嵌入式固件
│   ├── esp32-s3/            # 当前 ESP32-S3 主固件与诊断环境
│   └── stm32f103-keil/      # 早期 STM32F103 + FreeRTOS 原型
├── docs/                    # 商业计划、答辩、报告、技术与赛事资料
├── tools/                   # 可复用的报告生成脚本
├── artifacts/               # 不可再生的实机诊断证据
├── archive/                 # 旧源码快照、提取素材与私人备份
└── audit/directory-refactor # 本次重构的原树、基线与恢复材料
```

## 常用命令

```bash
cd apps/careguard-live
npm ci
npm test
npm run build

cd ../../firmware/esp32-s3
pio run -e esp32-s3-devkitc-1
```

微信小程序从 `apps/careguard-live/WeChatMiniProgram/` 导入微信开发者工具。报告生成脚本位于 `tools/report-generation/integrated-review/`，输入输出均使用工作区相对路径。

## Git 与恢复边界

- 根仓库记录工作区布局、固件、文档和归档。
- `apps/careguard-live/` 源码直接纳入本仓库；`firmware/esp32-s3/components/esp32-camera/` 使用官方 Git 子模块。
- Git recovery bundle 和大型素材仅在原始本地工作区保存，不包含在此轻量远端仓库。
- `archive/private/` 包含可能带凭据的私人备份，只保存在本机，永不进入 Git。

## 维护原则

- 当前 ESP32-S3 源码唯一真源是 `firmware/esp32-s3/src/`；`archive/source-snapshots/` 只用于历史比对。
- `node_modules/`、`.pio/`、Keil `Objects/`/`Listings/`、渲染中间页和 IDE 用户态文件均可再生，不纳入版本控制。
- `captures/` 与 `artifacts/hardware-diagnostics/` 是实机证据，不能按普通构建缓存清理。

## 克隆与本地素材

```bash
git clone --recurse-submodules https://github.com/Zack123Chen/lingji-guard.git
```

远端保存源码和轻量文档，不含单文件超过 10 MiB 的素材、视频、ZIP/bundle 备份、字体及本地编辑器状态。旧完整 Git 历史保留在原工作区的 `main` 分支；轻量发布分支为 `codex/github-source`，推送至远端 `main`。请勿使用 `git push --all` 上传旧素材历史。部分历史报告生成脚本所需的大型模板需从本地材料补充。

STM32 微信提醒脚本从环境变量 `SERVERCHAN_SENDKEY` 读取 Server 酱凭据，运行前需在本机配置，不要把凭据写入源码。

公开仓库额外排除 Office/PDF 报告、申报表和文档图片原件，以避免公开联系方式及签名。需要这些材料时，请从本地工作区单独分享给队友。

内部队友任务包、个人资料、逐人私信、历史操作日志及相机拍摄样张均只在本地保存，未纳入公开版本。

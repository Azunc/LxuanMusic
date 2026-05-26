# LxuanMusic

![Qt](https://img.shields.io/badge/Qt-6.8+-green.svg) ![C++](https://img.shields.io/badge/C++-17-blue.svg) ![License](https://img.shields.io/badge/License-MIT-yellow.svg)

**LxuanMusic** 是一款基于 **Qt 6.8.x + C++17** 开发的桌面本地音乐播放器，定位为一个结构清晰、可演示的毕设级本地音乐播放器工程。

项目采用**分层架构**（View → Controller → Model → DAO → Utils），以 Qt 信号槽驱动状态流转，支持本地音乐库管理、歌单收藏、LRC 歌词同步、桌面歌词、频谱可视化、全局快捷键等完整能力。

---

## 功能特性

| 模块 | 功能 |
|------|------|
| **本地音乐库** | 支持按默认 / 歌手 / 专辑 / 文件夹四种视图浏览与搜索 |
| **音频播放** | 基于 `QMediaPlayer` + `QAudioOutput`，支持播放/暂停/切歌/进度/音量/倍速/循环模式 |
| **歌单系统** | 系统歌单（本地音乐 / 我喜欢 / 播放历史）+ 自定义歌单，支持收藏与批量播放 |
| **LRC 歌词** | 本地 LRC 文件解析与同步高亮滚动，支持桌面歌词悬浮窗（透明度/字号/锁定调节） |
| **频谱可视化** | 通过 `ffmpeg` 解码 PCM + `kiss_fft` 频域分析，绘制动态频谱柱 |
| **全局快捷键** | Windows 原生 API 注册系统级热键（Ctrl+Shift+Space/←/→） |
| **数据持久化** | SQLite 存储歌曲/歌单/历史/队列快照，INI 存储应用配置 |
| **异步批量导入** | `QtConcurrent` 多线程并行提取元数据 + 数据库批量事务写入，不阻塞 UI |

---

## 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17 |
| UI 框架 | Qt 6.8.x (Widgets) |
| 音频播放 | Qt Multimedia (`QMediaPlayer` / `QAudioOutput`) |
| 数据存储 | Qt SQL (SQLite) + `QSettings` (INI) |
| 元数据读取 | TagLib |
| 频谱分析 | `kiss_fft` + `ffmpeg` (外部进程) |
| 构建系统 | qmake (`.pro`) |
| 目标平台 | Windows 11 |

---

## 项目结构

```text
LxuanMusic/
├── controllers/    # 控制层：事件调度，连接 UI 与业务
├── models/         # 业务层：播放引擎、音乐库、歌词、频谱、歌单
├── dao/            # 数据访问层：SQLite / INI 读写封装
├── entity/         # 实体层：Song、Playlist 纯数据对象
├── utils/          # 工具层：文件扫描、元数据提取、LRC 解析、快捷键、主题
├── views/          # 视图层：主窗口、本地音乐、歌单详情、桌面歌词、弹窗
├── styles/         # QSS 主题样式
├── icons/          # 图标资源
├── images/         # 图片资源
├── tools/          # 外部工具（ffmpeg.exe）
└── README.md
```

---

## 快速开始

### 环境要求

- Windows 11
- Qt 6.8.x (MinGW 或 MSVC 套件)
- TagLib 2.x（需本地编译或安装）

### 构建步骤

1. **克隆仓库**
   ```bash
   git clone https://github.com/YourName/LxuanMusic.git
   cd LxuanMusic
   ```

2. **修改 TagLib 路径**
   打开 `LxuanMusic.pro`，将以下路径修改为本机实际路径：
   ```qmake
   INCLUDEPATH += F:/github/taglib-2.2.1/include
   LIBS += -LF:/github/taglib-2.2.1/lib -ltag
   ```

3. **使用 Qt Creator 打开**
   - 双击 `LxuanMusic.pro`
   - 选择 Qt 6.8.x 编译套件
   - 执行 `qmake` → `构建`

4. **运行依赖**
   - 确保运行目录中存在 `tools/ffmpeg.exe`（频谱可视化必需）
   - 确保 TagLib DLL（如 `libtag.dll`）与可执行文件同目录

---

## 使用说明

1. **首次启动**：自动扫描系统默认音乐目录并导入歌曲
2. **导入文件夹**：点击「选择目录」可指定其他音乐文件夹
3. **播放控制**：底部控制栏支持播放/暂停、切歌、进度拖拽、音量调节
4. **桌面歌词**：播放时自动显示悬浮歌词窗，支持锁定/透明度/字号调节
5. **频谱可视化**：点击底部频谱按钮开关动态频谱柱
6. **全局快捷键**：
   - `Ctrl + Shift + Space`：播放/暂停
   - `Ctrl + Shift + ←`：上一首
   - `Ctrl + Shift + →`：下一首

---

## 亮点设计

- **分层架构清晰**：View / Controller / Model / DAO / Utils 职责分离，便于答辩阐述
- **AudioEngine 播放内核独立封装**：避免 UI 与 Qt 多媒体组件强耦合
- **DAO 持久化抽象**：支持自动建表、字段迁移、多表关联、队列快照、歌词缓存
- **异步批量导入**：`QtConcurrent` 并行元数据提取 + 批量事务写入，大数据量不卡 UI
- **频谱可视化**：`ffmpeg` 解码 + `kiss_fft` FFT 频域分析，可演示的多媒体数据处理能力

---

## 文档

| 文档 | 说明 |
|------|------|
| `README.md` | 本文件，项目真实现状与使用说明 |
| `架构设计文档.md` | 目标架构设计、模块边界、关键数据流与重构路线 |
| `database_schema.md` | SQLite 数据库设计文档 |

---

## License

本项目采用 [MIT License](LICENSE) 开源协议。

---

> 本项目为个人毕业设计作品，代码结构优先考虑可演示性与可维护性，欢迎学习交流。

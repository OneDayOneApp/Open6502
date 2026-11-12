# nesearch 在线数据源调研

| 项目 | 内容 |
|---|---|
| 调研日期 | 2026-11-11 |
| 调研范围 | NES / Famicom homebrew、开源游戏、公共 ROM 归档 |
| 网络环境 | 中国大陆默认网络（2026-11-11 实测） |
| 关联文档 | [PRD.md](PRD.md) |
| 结论 | GitHub + Homebrew Hub 为主，PDRoms 为补充，Internet Archive / itch.io 需代理且只做白名单，商业 ROM 站点排除 |

> 本文只评估“搜索与下载源”，不评估模拟器、商业购买渠道和 ROM 修改工具。所有站点内容以调研时点为准，数量和可达性会变化。
>
> **实现状态（2026-11-12）**：GitHub、Homebrew Hub、PDRoms RSS、Internet Archive Provider 已接入；TuxNES `nesmapper.txt` 与 Fogleman 标题截图库已作为元数据源接入。Internet Archive 当前网络超时，UI 显示 offline。

---

## 0. 结论（先看这里）

### 0.1 没有单一站点能覆盖全部

NES 游戏下载源大致分四类：

1. **代码托管/发布平台**：GitHub、Gitee、GitCode、AtomGit。适合 homebrew 源码与 release 资产，API 最好。
2. **homebrew 专门数据库/归档**：Homebrew Hub（NES 数据库）、PDRoms、Internet Archive 的 homebrew 集合。适合“目录 + ROM 文件”。
3. **创作者市场**：itch.io、GameJolt。适合发现新游戏，但下载通常要经过浏览器会话/购买流程，不宜自动抓取。
4. **元数据/校验库**：No-Intro、MAME hash、TOSEC、IGDB、MobyGames、ScreenScraper。只有名称/哈希/截图，没有 ROM。

### 0.2 推荐组合

| 层级 | 来源 | 用途 | 建议 |
|---|---|---|---|
| 主源 A | GitHub `retrobrews/nes-games` | 82 个 homebrew ROM，广度最好 | P0，自动下载 |
| 主源 B | GitHub `nesdev-org/homebrew-db` / Homebrew Hub | 24 个 NES 条目，许可证与哈希最清晰 | P0/P1，作为“license-clean”源 |
| 补充 | PDRoms | 约 115 个 NES 文件，RSS + 结构化页面 | P1，显式开启，提示许可混杂 |
| 大归档 | Internet Archive | 体量最大，但内容混杂、本网络不可达 | P2，collection 白名单 + 代理 |
| 创作者市场 | itch.io、GameJolt | 发现新游戏、跳转作者页 | P2，仅存链接/浏览器打开 |
| 校验 | No-Intro、MAME hash | 哈希、DAT、ROM 版本核对 | P2，只做校验 |
| 排除 | 商业 ROM 站、网盘 | 版权风险、链接不稳定 | 不接入 |

### 0.3 关键发现

- **GitHub 仍是主线**：`retrobrews/nes-games` 有 **82 个 `.nes` + 82 个 `.txt`，共 6,366,496 字节**；`nesdev-org/homebrew-db` 有 **24 个 NES 条目、26 个 `.nes`，共 1,212,832 字节**，且每个条目有 `gameLicense` 字段。
- **Homebrew Hub 是 API 最规范的 NES 源**：`https://hh3.gbdev.io/api` 提供搜索、条目 JSON、统计；当前 NES 共 24 条。但它的 NES 文件下载端点实测 404，需回退到 GitHub raw。
- **PDRoms 是可自动化的补充**：RSS 文件流 + 页面 JSON-LD + 直接 zip 下载；NES 文件列表约 12 页、**约 115 个文件**。许可声明允许 free software / legalised software，但也包含未获明确授权的 fan games，故不宜设为默认。
- **Internet Archive 与 itch.io 在本网络超时**，需要代理；archive.org 内容版权混杂，必须按 collection/identifier 白名单接入。
- **GitHub 的 `nes-homebrew` topic 只有 3 个仓库**，不能靠 topic 发现全部内容；需要“策展仓库 + release 搜索 + 作者仓库”组合。
- **商业 ROM 站全部排除**：技术上可抓，但法律风险和恶意文件风险都高，且与 Open6502 的开源 ROM 策略冲突。

---

## 1. 评估维度

| 维度 | 说明 |
|---|---|
| 内容规模 | NES 相关条目/文件数量、覆盖的 homebrew 年代与类型 |
| 许可清晰度 | 是否逐条给出许可证/作者授权；是否允许再分发 |
| 自动化能力 | 是否有公开 API、RSS、结构化 JSON-LD、稳定直链 |
| 下载方式 | 直链、zip、release asset、浏览器会话、网盘 |
| 国内可达性 | 当前网络实测；区分“站点超时”和“反爬 403” |
| 稳定性 | 站点年龄、维护频率、是否 Cloudflare 挑战 |
| 与 Open6502 适配 | 能否用于回归测试、能否合法放入 `roms/` |
| 接入成本 | Provider 开发量、是否需要 Token/代理/解析 HTML |

---

## 2. 总览矩阵

### 2.1 内容与许可

| 站点 | 类型 | NES 规模 | 许可清晰度 | 建议 |
|---|---|---|---|---|
| GitHub | 代码/发布 | 82 + 24 + 长尾 | 中（逐仓库/逐条目） | P0 |
| Homebrew Hub | 专门数据库 | 24 | 高（`gameLicense` + 部分哈希） | P0/P1 |
| PDRoms | homebrew 归档 | 约 115 个文件 | 低-中（混杂 fan games） | P1，opt-in |
| Internet Archive | 综合归档 | 很大，混杂 | 低（collection 差异大） | P2，白名单 |
| itch.io | 创作者市场 | 大 | 中（逐游戏） | P2，仅链接 |
| GameJolt | 创作者市场 | 小-中 | 中（逐游戏） | P2，仅链接 |
| NESdev | 社区/比赛 | 中 | 中（逐作者） | P2，发现 |
| Gitee / GitCode / AtomGit | 代码镜像 | 取决于镜像 | 中 | P2，传输备选 |
| No-Intro / MAME | 校验数据库 | 商业为主 | 高（元数据） | P2，校验 |
| 商业 ROM 站 | 盗版聚合 | 极大 | 无 | 排除 |

### 2.2 自动化与可达性

| 站点 | API/结构 | 下载方式 | 本机可达 | 角色 |
|---|---|---|---|---|
| GitHub | REST + raw CDN | 直链 / release asset | 好（raw 偶有波动） | 主源 |
| Homebrew Hub | REST `/api/search`、`/api/entry` | 元数据；文件回退 GitHub raw | 好 | 主源/补充 |
| PDRoms | RSS + HTML + JSON-LD | 直接 zip（`?__df=`） | 好（需浏览器 UA） | 补充 |
| Internet Archive | advancedsearch + metadata + download | 直链/zip | 超时（需代理） | 大归档 |
| itch.io | 无公开目录/下载 API | 浏览器会话/购买 | 超时（需代理） | 发现 |
| GameJolt | 有限 API | 页面/直链 | 好（JS 渲染） | 发现 |
| NESdev | 无 API，论坛/ wiki | 附件/外链 | Cloudflare 403 | 发现 |
| ROMhacking.net | 无 API | 补丁，不是完整 ROM | Cloudflare 403 | 排除 |
| Zophar's Domain | 无 API | 补丁/工具 | 超时 | 排除 |
| No-Intro | DAT 下载 | 无 ROM | 好 | 校验 |
| MAME hash | GitHub raw XML | 无 ROM | 好 | 校验 |
| Gitee / GitCode | 仓库 API（部分需登录） | 仓库/raw | 好 | 镜像 |

---

## 3. 第一层：可直接接入

### 3.1 GitHub（主源）

**为什么重要**：homebrew 作者最常用的发布渠道；有稳定 API、raw 直链、release assets；适合做“策展目录 + 长尾发现”。

#### 3.1.1 `retrobrews/nes-games`（广度）

- 地址：<https://github.com/retrobrews/nes-games>
- 内容：**82 个 `.nes` + 82 个同名 `.txt`**，ROM 总计 **6,366,496 字节（约 6.1 MiB）**；`.txt` 通常包含标题、作者、描述、项目链接，部分含许可证全文。
- 目录结构：扁平，`falling.nes` / `falling.txt` 同级。
- 许可：仓库本身**没有 LICENSE**；README 明确写道：

  > “The ROMs listed here have been approved for free distribution on this site/project only. If you want to share it, please contact owner/developer.”

  含义：可以在这个项目/站点免费下载，但不等于可以任意再分发。逐条 `.txt` 可能含 MIT/GPL 等许可证，需要解析并展示。
- 自动化：GitHub tree API 一次拿全量列表；raw 下载不占 API 配额。
- 国内：GitHub API 与 raw 当前可访问；raw 偶有波动。
- 建议：作为 P0 宽目录；下载可以，但**不要默认把 ROM 复制进 Open6502 仓库再分发**，除非逐条确认许可证。

#### 3.1.2 `nesdev-org/homebrew-db`（许可清晰）

- 地址：<https://github.com/nesdev-org/homebrew-db>
- 内容：Homebrew Hub 的 NES 数据库；**24 个 `game.json`、26 个 `.nes`**，总计 **1,212,832 字节**。
- 目录结构：

  ```text
  entries/<slug>/game.json
  entries/<slug>/<rom>.nes
  entries/<slug>/*.png
  ```

- `game.json` 字段（示例 `thwaite`）：

  ```json
  {
    "title": "Thwaite",
    "slug": "thwaite",
    "gameLicense": "GPL-3.0-or-later",
    "developer": "Damian Yerrick",
    "repository": "https://github.com/pinobatch/thwaite-nes",
    "platform": "NES",
    "typetag": "game",
    "files": [
      {"default": true, "filename": "thwaite128.nes", "playable": true}
    ]
  }
  ```

- 许可：数据库仓库为 **GPL-3.0**；每个条目有 `gameLicense`，部分文件带 `sha256`/`md5`/`sha1`。这是目前**最适合 Open6502 回归测试/再分发评估**的源。
- 自动化：可直接 clone 或在运行时读取 tree + raw；无需 API Token。
- 建议：作为 P0/P1 的“license-clean”目录；与 `retrobrews` 合并搜索、按来源标注。

#### 3.1.3 GitHub 长尾

- Topic 覆盖很弱：`nes-homebrew` 只有 **3** 个仓库，`nes-game` 有 **17** 个；搜索 `nes homebrew game` 约 **70** 个仓库（含模拟器/工具，需要过滤）。
- 可行发现方式：
  - 维护 GitHub 策展列表（类似 `retrobrews`）；
  - 搜 releases 中带 `.nes` / `.zip` 的仓库；
  - 从 Homebrew Hub / PDRoms 条目反查 `repository` 字段。
- 不建议只靠 GitHub 搜索做实时目录：速率限制（未认证 60 次/h，搜索 10 次/min）和结果噪声都会影响体验。

#### 3.1.4 GitHub API 要点

```text
GET https://api.github.com/repos/retrobrews/nes-games/git/trees/master?recursive=1
GET https://raw.githubusercontent.com/retrobrews/nes-games/master/falling.nes
GET https://api.github.com/repos/{owner}/{repo}/releases/latest
```

- 未认证 core：60 次/h；带 `GITHUB_TOKEN`：5,000 次/h。
- 搜索 API：10 次/min（未认证）。
- raw 下载不消耗 API 配额，但仍是 GitHub CDN，需限速与重试。
- 建议 1 次 tree + N 次 raw，ETag 缓存，24 h TTL。

### 3.2 Homebrew Hub（API 最规范）

- 前端：<https://hh.gbdev.io>（支持 GB/GBC/GBA/NES，可直接在线玩）
- API：`https://hh3.gbdev.io/api`
- 数据库：<https://github.com/nesdev-org/homebrew-db>
- 当前统计（`/api/stats`）：

  ```json
  {
    "total": 1642,
    "platforms": {"gba": 249, "gbc": 451, "gb": 918, "nes": 24}
  }
  ```

#### API 能力

```text
GET /api/search?platform=NES&page_elements=10
GET /api/entry/thwaite.json
GET /api/entry/<slug>/<filename>        # NES 文件实测 404，需回退 raw
```

- 搜索参数：`platform`、`type`、`developer`、`tags`、`title`、分页。
- **大小写敏感坑**：`platform=NES` 返回 24 条；`platform=nes` 返回 0。
- `title=thwaite` 实测返回 0（可能只对部分平台生效），建议本地缓存后自行搜索。
- 条目 JSON 字段：`title`、`slug`、`gameLicense`、`developer`、`repository`、`platform`、`files`、`screenshots`、`tags`、`date`、`play`。
- 部分文件带哈希，例如 `240p-test-suite` 的 `240pee.nes` 有 `sha256`；可用于严格校验。
- 文件下载：`/api/entry/croom/croom.nes` 返回 404；`https://raw.githubusercontent.com/nesdev-org/homebrew-db/master/entries/croom/croom.nes` 返回 200。实现时应以 raw 为准，或等待官方修复。

#### 评价

| 维度 | 评价 |
|---|---|
| 规模 | 小（NES 24 条），但质量高 |
| 许可 | 最好：逐条 `gameLicense` + 仓库 GPL-3.0 |
| 自动化 | 好：搜索/条目 JSON；文件回退 raw |
| 可达性 | 好：`hh.gbdev.io`、`hh3.gbdev.io` 当前 200 |
| 风险 | 条目少；NES 文件 API 不完整；平台枚举大小写坑 |

**建议**：作为“license-clean”源与 `retrobrews` 并列；搜索合并、来源标注；下载走 raw。

### 3.2.1 TuxNES `nesmapper.txt`（元数据，不是 ROM 源）

- URL：<https://tuxnes.sourceforge.net/nesmapper.txt>
- 内容：`Bigass NES Mapper List 0.1`，标题 + PRG/CHR 大小 + mirroring + mapper 名称/编号。
- 实测：113,122 字节，约 1,589 条可解析行；其中约 1,550+ 条有 mapper 编号，常见 mapper 为 MMC1(1)、MMC3(4)、UNROM(2)、NROM(0)、CNROM(3)、AOROM(7)；mirroring 以 H/V 为主。
- 价值：补全未知 mapper/PRG/CHR/mirroring；尤其适合 Archive.org 或自定义目录里的商业 ROM 条目。
- 限制：没有下载 URL、许可证、哈希、截图；以商业 ROM 标题为主；日版收录不全；作者明确说明 mirroring 不总是可靠；文件较老。
- 接入：`tuxnes_mapper_enabled=true`，抓取后按 `normalize_title` 匹配，仅填充 mapper 为空的条目；数据写入 catalog 缓存。它不会出现在“下载源”搜索过滤中。

### 3.2.2 Fogleman NES Title Screens（元数据，不是 ROM 源）

- URL：<https://www.michaelfogleman.com/static/nes/>
- 内容：630 个 NES 标题截图，页面结构为 `<img src="<sha1>.png"><p>Title</p>`；图片大小约 2–10 KiB，256×240 附近。
- 价值：给 Archive.org / 自定义目录中缺失封面的商业 ROM 补标题截图；按标题归一化匹配（含 `The` 前后缀处理）。
- 限制：没有下载 URL、作者、许可证、mapper、哈希；截图版权归原权利人；标题格式采用 No-Intro 风格 `Title, The`，需要标题归一化。
- 接入：`fogleman_screenshots_enabled=true`，只填充 `screenshot` 为空的条目；不计入游戏搜索来源。

### 3.3 PDRoms（可自动化的补充）

- 地址：<https://pdroms.de>
- 定位：1998 年至今的 homebrew 归档，46 个平台，**2,530 个文件**，首页自称 3,114,871 次下载。
- NES 分类：<https://pdroms.de/files/nintendo-entertainment-system-nes-famicom/>；分类有 **674 条记录**（含新闻），文件列表 **12 页**，按每页 10 条估算约 **115 个 NES 文件**。
- 结构化入口：
  - 文件 RSS：`https://pdroms.de/rss/files/nintendo-entertainment-system-nes-famicom/`
  - 条目页 JSON-LD：`SoftwareApplication`，含 `name`、`author`、`fileSize`、`encodingFormat`、`downloadUrl`、`screenshot`。
  - 下载按钮：`https://pdroms.de/?__df=<token>`，HEAD 实测返回 `200`、`application/octet-stream`、`content-length`、`content-disposition: attachment; filename=...zip`。
  - 实测 zip 内容：`README.md` + 单个 `.nes`（Robo-Ninja Climb 为 `robo-ninja-climb.nes`），需要解压并选择 ROM。
- 没有官方 REST API；WordPress REST 只暴露 posts，文件列表由页面/AJAX 生成。
- 许可声明（节选）：

  > “All downloads on PDRoms are free software or former commercial software legalised for free use by their authors, published under various licenses. Our archive also includes fan games. Please note that these projects are usually developed without the explicit permission of the respective copyright holders.”

  即：大多数是免费/已合法化软件，但**存在未获明确授权的 fan games**；不能一律视为可再分发。
- 可达性：需要浏览器 UA；默认 curl UA 可能 403；下载链接可能绑定会话/IP，需在实现时验证。
- 建议：P1，`--provider pdroms` 显式开启；默认只展示，不自动下载未标注许可证的条目。

### 3.4 Gitee / GitCode / AtomGit（镜像/传输备选）

- Gitee：<https://gitee.com>，200；API `https://gitee.com/api/v5/`，匿名搜索可能返回空/需 Token。
- GitCode：<https://gitcode.com>，200；匿名 API 搜索返回 401，需要登录/Token。
- AtomGit：<https://atomgit.com>，200；有匿名搜索文档，但需按平台文档接入。
- CNB：<https://cnb.cool>，200。
- 定位：**不是内容源**，而是 GitHub 的镜像/加速备选；内容取决于是否有人镜像。
- 风险：镜像可能滞后、缺文件、改动哈希；不能作为完整性基准。
- 建议：P2，仅作为可配置的 transport fallback，不默认。

---

## 4. 第二层：可发现，但不宜自动抓取

### 4.1 Internet Archive

- 地址：<https://archive.org>
- 体量最大，包含软件库、杂志、商业 ROM 集合、homebrew 集合等。
- 已知 API：

  ```text
  GET https://archive.org/advancedsearch.php?q=collection%3A<name>&fl[]=identifier,title&output=json
  GET https://archive.org/metadata/<identifier>
  GET https://archive.org/download/<identifier>/<file>
  ```

- 优点：API 成熟、直链稳定、支持 Range、条目元数据丰富。
- 问题：
  - **版权混杂**：全站搜索会命中商业 ROM 集合，不能作为默认源。
  - **本机网络超时**：`archive.org` 在当前网络 12 s 超时；需要代理。
  - collection/identifier 命名不统一，文件列表可能是 zip/rar/多文件。
- 建议：P2；只允许配置好的 homebrew collection/identifier 白名单；代理可配；不做全站搜索。
- 已实现 Provider：默认 query `(collection:softwarelibrary_nes OR collection:nes-homebrew) AND mediatype:software`，可在 UI Settings 修改；标题优先取 metadata 的 `title/creator/description/licenseurl`，封面先用 `archive.org/services/img/<identifier>`，再由 Fogleman 标题截图覆盖；TuxNES mapper 表补 mapper/PRG/CHR。

### 4.2 itch.io

- 地址：<https://itch.io>
- NES homebrew 最重要的作者发布平台之一；大量免费/付费/“name your own price”游戏。
- 没有公开的目录/下载 API；下载通常需要进入游戏页、完成 claim/支付、再走会话下载。
- 自动抓取会违反 ToS，且付费游戏无法绕过。
- 本机网络超时（需代理）。
- 建议：P2，**只做发现与跳转**：保存 `page_url`，提供 `nesearch open <id>` 调用浏览器；不自动下载。

### 4.3 GameJolt

- 地址：<https://gamejolt.com>
- 有一些 NES/homebrew 项目；站点为 JS 渲染，搜索页 HTML 不含完整列表。
- API 是“按游戏”的开发者 API（分数、奖杯、存档等），不是通用目录/下载 API。
- 本机可达（200），但自动化价值有限。
- 建议：P2，仅链接；不做 Provider。

### 4.4 NESdev / 作者站

- NESdev：<https://www.nesdev.org>，wiki + 论坛 + 年度 compo；ROM 多在外链（GitHub/itch/作者站）或论坛附件。
  - 本机 curl 返回 Cloudflare 403；浏览器可能正常。
  - 无 API，抓取论坛不礼貌且脆弱。
  - 建议：用于人工策展/发现，不做自动源。
- 作者站：
  - SlyDog Studios：<https://www.slydogstudios.org>（200），大量 NES homebrew，直接下载。
  - Mojon Twins：<https://www.mojontwins.com>（200），大量 NES 游戏。
  - 特点：单作者、长尾、页面结构各异；适合人工加入自定义 JSON 目录，不适合做通用 Provider。
- ROMhacking.net：<https://www.romhacking.net>，主要是补丁/翻译，不是完整 ROM；Cloudflare 403。**不作为 ROM 源**。
- Zophar's Domain：<https://www.zophar.net>，补丁/工具为主；本机超时。**不作为 ROM 源**。

---

## 5. 第三层：元数据与校验（不提供 ROM）

| 来源 | 内容 | API/获取 | 用途 | 建议 |
|---|---|---|---|---|
| No-Intro DAT-o-MATIC | ROM 名称、CRC32/MD5/SHA1 | DAT 下载，需按站点流程 | 版本识别、哈希校验 | P2 |
| MAME software list | `hash/nes.xml`，含哈希/年份/厂商 | GitHub raw | 校验、元数据 | P2 |
| TOSEC | DAT/命名规范 | DAT 下载 | 校验/整理 | P2 |
| IGDB | 游戏元数据、封面、类型 | Twitch OAuth API | 补全标题/简介/封面 | P2 |
| MobyGames | 元数据、封面、平台 | 商业 API | 补全元数据 | P2 |
| ScreenScraper | 封面、截图、视频 | API（需开发者账号） | 媒体刮削 | P2 |
| RetroAchievements | 成就、哈希 | API Key | 校验/成就 | P2 |

注意：No-Intro/MAME 的 NES 数据以商业 ROM 为主，**只用于校验，不用于下载**；不要把 DAT 里的商业条目变成下载目录。

---

## 6. 明确排除

### 6.1 商业 ROM 聚合站

包括但不限于：Emuparadise、Vimm's Lair、ROMulation、CoolROM、LoveROMs、RomsMan、EmulatorGames、RetroStic。

排除原因：

- 绝大多数未获权利人授权，属于版权侵权高风险；
- 广告/跳转/下载器可能捆绑恶意软件；
- 站点经常更换域名，自动化维护成本高；
- 与 Open6502 “只使用开源/homebrew ROM”的策略冲突。

### 6.2 网盘/论坛资源

百度网盘、阿里云盘、123pan、蓝奏云、QQ 群、贴吧资源等：

- 来源和许可证不可验证；
- 链接易失效、限速、需要登录；
- 不适合作为可复现的 CI 数据源。

### 6.3 补丁站当作 ROM 源

ROMhacking.net、Zophar's Domain 主要提供补丁/翻译/工具，需要用户自备基础 ROM；不能作为“搜索并下载完整 NES 游戏”的源。

---

## 7. 本机网络实测

测试时间：2026-11-11 21:07–21:20（Asia/Shanghai）；`curl -m 8~20`；仅代表当前网络。

| 站点 | 结果 | 说明 |
|---|---|---|
| `github.com` / `api.github.com` | 200 | 可访问；raw 下载正常 |
| `raw.githubusercontent.com` | 200 | 下载 `falling.nes` 正常 |
| `hh.gbdev.io` / `hh3.gbdev.io` | 200 | API 与前端可访问 |
| `pdroms.de` | 200（浏览器 UA） | 默认 UA 可能 403；下载 zip 正常 |
| `gamejolt.com` | 200 | JS 渲染，目录不易抓取 |
| `gitee.com` | 200 | 镜像平台 |
| `gitcode.com` / `atomgit.com` / `cnb.cool` | 200 | 镜像平台 |
| `datomatic.no-intro.org` | 200 | DAT 元数据 |
| `archive.org` | 超时 | 需代理 |
| `itch.io` | 超时 | 需代理 |
| `www.nesdev.org` | 403 | Cloudflare 挑战，浏览器可能正常 |
| `www.romhacking.net` | 403 | Cloudflare 挑战 |
| `www.zophar.net` | 超时 | 不作为源 |
| `cdn.jsdelivr.net` | 连接重置 | 不适合作为稳定 transport |

> 403 不等于站点在中国不可用，可能只是反爬；超时也不等于站点下线。上表只用于选择“默认可自动化的源”。

---

## 8. 对 nesearch 的建议

### 8.1 分阶段接入

| 阶段 | 接入 | 理由 |
|---|---|---|
| v0.1 P0 | GitHub `retrobrews` + `nesdev-org/homebrew-db` | 一个 GitHub Provider 支持两种目录布局；82 + 24 条，覆盖广度与许可清晰度 |
| v0.1 P1 | 自定义 JSON 目录 | 团队镜像、CI 固定版本、人工策展 |
| v0.1.x P1 | PDRoms | RSS + JSON-LD + zip，量级约 115；需显式开启并提示许可混杂 |
| v0.2 P2 | Internet Archive | collection 白名单 + 代理；不做全站搜索 |
| v0.2 P2 | itch.io / GameJolt | 仅保存链接，`open` 到浏览器；不自动下载 |
| v0.2 P2 | No-Intro / MAME | 只做校验，不做下载 |

### 8.2 Provider 能力模型

建议在 `CatalogProvider` 上增加能力声明，避免把“链接源”当成“下载源”：

```cpp
struct SourceCapabilities {
  bool can_fetch_catalog;   // 能批量拿目录
  bool can_search_remote;   // 能远程搜索
  bool can_download;        // 能直接下载 ROM
  bool has_license;         // 逐条许可证
  bool has_checksums;       // 上游 SHA-256/CRC
  bool requires_auth;       // Token / 登录
  bool requires_proxy;      // 当前网络需要代理
};
```

推荐 Provider：

| Provider | 目录 | 下载 | 许可 | 备注 |
|---|---|---|---|---|
| `github:retrobrews` | 82 | 直链 | 部分 | 默认；README 限制再分发 |
| `github:homebrew-db` | 24 | raw GitHub | 逐条 | license-clean；部分哈希 |
| `pdroms` | ~115 | zip 直链 | 混杂 | opt-in；限速 |
| `archive` | 白名单 | 直链 | 混杂 | 代理 + 白名单 |
| `json` | 自定义 | 任意 | 自定义 | CI/镜像 |
| `itch-link` | 发现 | 浏览器 | 逐条 | 不抓取 |

### 8.3 数据模型补充

```cpp
enum class Redistribution {
  Allowed,       // 明确允许再分发（如 GPL/MIT/CC0）
  DownloadOnly,  // 允许下载，但未明确允许再分发
  Unknown        // 未知，默认警告
};

struct SourceInfo {
  std::string provider;        // "github"
  std::string source_id;       // "retrobrews/nes-games"
  std::string page_url;
  std::string license;         // SPDX 或 "unknown"
  std::string license_url;
  Redistribution redistribution = Redistribution::Unknown;
  std::optional<std::string> sha256;
  std::optional<std::string> md5;
};
```

- `Game` / `Asset` 增加 `SourceInfo`。
- 去重键：优先 `sha256`；没有哈希时用 `(normalized title, developer)`；保留多来源列表，而不是覆盖。
- `search` 结果展示 `source` 与 `license`；`--require-license`（P1）过滤 `Unknown`。

### 8.4 合规策略

1. 默认只索引公开可下载的 homebrew/开源条目；不内置商业 ROM 目录。
2. 逐条展示 `license` 与 `source_url`；未知标 `unknown`，下载前提示。
3. `Redistribution=Allowed` 才建议放入 Open6502 `roms/` 并提交；`DownloadOnly` 仅本地测试。
4. 不绕过 itch.io 购买/claim、不抓取 Cloudflare 挑战页面、不解析网盘。
5. 对 PDRoms 这类“包含 fan games”的归档，默认关闭自动下载，要求显式 `--provider pdroms`。
6. 支持 `blocklist`（P2）与 source 白名单，便于团队合规审查。

### 8.5 实现注意

- **GitHub**：tree 1 次 + raw N 次；ETag/TTL；Token 只发 `api.github.com`。
- **Homebrew Hub**：`platform=NES`（大写）；条目 JSON 用 `gameLicense`；文件 API 404 时回退 raw；哈希存在则严格校验。
- **PDRoms**：先读 RSS 获取条目列表；再解析条目页 JSON-LD；`downloadUrl` 可能带 `?__df=` token，需实测会话/IP 绑定和过期策略；下载前检查 `content-length` 与 `content-type`；zip 内通常含 README + 单个 `.nes`，提取时忽略非 ROM 文件。
- **Archive.org**：只接受配置的 collection/identifier；不要对 `q=nes` 全站搜索。
- **限速**：每个 host 独立令牌桶；GitHub 4 并发、PDRoms 2 并发、Homebrew Hub 4 并发；遵守 `Retry-After`。
- **缓存**：目录缓存按 provider + source + ref 分区；许可证/哈希随条目缓存。

---

## 9. 待确认

1. **是否把 Homebrew Hub / `homebrew-db` 设为并列默认源？** 建议是：它许可证最清晰，但只有 24 条。
2. **PDRoms 是否进入 v0.1？** 建议 P1，且默认关闭自动下载；许可混杂是主要风险。
3. **Internet Archive 是否值得为它做代理支持？** 如果目标用户主要在中国大陆，建议 P2；否则可提前。
4. **itch.io 只做链接是否可接受？** 建议是；自动抓取不可持续且有 ToS 风险。
5. **是否需要 `--require-license` / `Redistribution` 过滤？** 建议 P1，尤其用于 Open6502 的 `roms/`。

---

## 10. 参考

- GitHub `retrobrews/nes-games`：<https://github.com/retrobrews/nes-games>
- GitHub `nesdev-org/homebrew-db`：<https://github.com/nesdev-org/homebrew-db>
- Homebrew Hub：<https://hh.gbdev.io>
- Homebrew Hub API 文档：<https://github.com/gbdev/homebrewhub/blob/master/API.md>
- PDRoms NES 文件：<https://pdroms.de/files/nintendo-entertainment-system-nes-famicom/>
- PDRoms 文件 RSS：<https://pdroms.de/rss/files/nintendo-entertainment-system-nes-famicom/>
- Internet Archive Advanced Search：<https://archive.org/advancedsearch.php>
- Internet Archive Metadata API：<https://archive.org/metadata/>
- No-Intro：<https://www.no-intro.org>
- MAME NES software list：<https://github.com/mamedev/mame/blob/master/hash/nes.xml>
- Open6502 ROM 说明：[../roms/README.md](../roms/README.md)

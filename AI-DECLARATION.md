# AI 使用声明 / AI Usage Declaration

本仓库是 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) 的 fork，用于开发 **AniSwitch** 分支特性：在 Nintendo Switch 上通过 WebDAV/OpenList 播放本地网盘视频，并为视频匹配番剧、加载弹幕（巴哈姆特 / 弹弹play 等多源聚合）。

This repository is a fork of [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili). The fork hosts the **AniSwitch** feature branch: WebDAV/OpenList playback on Nintendo Switch, plus anime matching and danmaku loading (Bahamut / dandanplay and other aggregated sources).

---

## 1. 哪些部分由 AI 参与 / What is AI-assisted

| 范围 | 说明 |
|---|---|
| 代码 | `feature/aniswitch-webdav` 分支上新增的 AniSwitch 相关代码（WebDAV 浏览、片源匹配、弹幕适配、设置页入口等）由 AI 生成初稿 |
| 文档 | 本文件、分支相关说明文档、注释由 AI 起草 |
| 构建脚本 | 针对 fork 的 CI 工作流（`.github/workflows/aniswitch-*.yml`）由 AI 编写 |
| 上游代码 | **未改动**。上游原有代码、资源、子模块保持原样；如需修改，改动会在提交信息中单独说明 |

## 2. 使用什么工具 / Tooling

- 工具：**DeepSeek Harness**（AI 编程代理）
- 协作方式：AI 生成补丁 → 维护者在真机（Nintendo Switch / Atmosphere）上构建、运行、验证 → 合并
- 提交标记：AI 参与的提交在 commit message 末尾带有 trailer

```
AI-Assisted-By: DeepSeek Harness
```

可用 `git log --format='%H %s%n%b' | grep -B1 'AI-Assisted-By'` 列出所有 AI 参与的提交。

## 3. 人工责任 / Human responsibility

- 所有 AI 生成的代码在合并进 `main` 前都经过维护者审查
- 功能正确性以 **真机运行结果** 为准，不以 AI 自述为准
- 涉及网络请求、账号凭据、第三方 API 的代码经过人工复核

## 4. 许可证 / License

本 fork 与其衍生作品继续遵循上游的 **GNU General Public License v3.0**（见 `LICENSE`）。AI 生成的部分同样以 GPLv3 授权发布，不附加任何额外限制。上游版权与作者署名（`xfangfang/wiliwili` 及其贡献者）完整保留。

This fork and its derivative work remain under the upstream **GNU General Public License v3.0**. AI-generated portions are released under the same license with no additional restrictions. Upstream copyright and attribution are preserved in full.

## 5. 与上游的关系 / Relation to upstream

- `main` / `yoga` 等上游分支保持与上游一致，仅用于同步
- AniSwitch 开发在独立分支进行，通过 Pull Request 或独立分发的方式提供
- 本 fork 不代表上游项目立场

## 6. 运行依赖与数据来源 / Runtime dependencies and data sources

**本仓库不包含、不打包、不分发任何弹幕数据，也不内置任何弹幕服务。**

AniSwitch 的弹幕能力完全建立在「使用者自建」的组件上：

| 组件 | 来源 | 本仓库提供什么 |
|---|---|---|
| 弹幕聚合库 | 第三方项目 [danmu_api](https://github.com/huangxd-/danmu_api)（Docker 镜像 `logvar/danmu-api`） | 只提供调用方代码，不分发其数据 |
| `aniswitch-gateway` | 本分支新增（GPLv3 授权） | 源码（`server.js`）与示例配置，不含任何抓取结果 |
| 视频文件 | 使用者自己的 WebDAV / OpenList 存储 | 无 |

因此运行本客户端前，使用者必须自行部署弹幕库与网关（见 `aniswitch-gateway/README.md` 与 `ANISWITCH.md`）。
仓库内的 `config.example.json` 只含占位符，不含任何账号、口令、API 令牌或弹幕内容。

This repository does **not** contain, bundle, or distribute any danmaku (bullet-comment) data, and does not host any danmaku service.
All danmaku capability depends on components the user deploys themselves: the third-party `danmu_api` aggregator and the
`aniswitch-gateway` shipped (as source) in this branch. The example config contains placeholders only — no credentials, tokens,
or danmaku payloads.

## 7. 与上游 wiliwili 的关系（补充）/ Relation to upstream (details)

- 源文件改自 `xfangfang/wiliwili`（基线 v1.6.0，commit `88e5876`）。本分支在上游之上**新增** AniSwitch 相关文件，
  并对少量入口文件（`main.cpp`、`main_activity.*`、`setting_activity.*`、`CMakeLists.txt`、若干 XML）做最小改动，
  改动清单见 `ANISWITCH.md` 的「与上游的差异」。
- 上游的播放链路（mpv + ffmpeg + Tegra NVDEC 硬解）、弹幕渲染引擎、libass、borealis UI 全部原样复用，未做修改。
- 若上游后续更新，本分支改动范围足以逐个 rebase。

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

# AniSwitch

> 源文件改自 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili)（GPLv3），本分支把它改造为**只用来自架 WebDAV 看番**的客户端。
>
> ⚠️ **必须自己架设弹幕库**：本仓库**不包含**任何弹幕数据，也不内置弹幕服务。客户端只会请求你自己部署的
> [danmu_api](https://github.com/huangxd-/danmu_api) + 本仓库 `aniswitch-gateway/` 里的网关。
> 不架设弹幕库 → 视频照常播放，但**没有弹幕**。

## 它是什么样的

- **主页只有一个 AniSwitch 按钮**：原来 wiliwili 的首页/动态/我的三个 tab 全部删除。
- 进去第一层是 **WebDAV 源列表**：`+` 添加、`−` 删除、`X` 编辑、`A` 进入。
- 点进源后是 **目录浏览**：`A` 进子目录、点视频开始播放、`B` 回上一级。
- 播放时从网关拿弹幕，支持暂停/快进/下一集，弹幕与播放器共用 wiliwili 原有的渲染引擎。
- 内置一条 **AniSwitch Gateway** 源（`http://<gateway>:9333`），可以直接播网关配置里那个目录，不走 WebDAV 浏览。

## 按键

| 界面 | 按键 | 行为 |
|---|---|---|
| 主界面 | A | 进入 AniSwitch |
| 主界面 | B | 退出程序 |
| 源列表 | A | 进入选中的源 |
| 源列表 | + | 添加 WebDAV 源（单窗口三个输入框） |
| 源列表 | − | 删除选中的源（内置网关源不可删） |
| 源列表 | X | 编辑选中的源 |
| 源列表 | B | 回主界面 |
| 目录浏览 | A | 进目录 / 播放视频（并把同目录的视频排成播放列表） |
| 目录浏览 | B | 上一级，已在根目录则返回源列表 |
| 播放中 | B | 停止播放并退出播放器 |

## 依赖（必须自己准备）

| 组件 | 作用 | 是否随本仓库分发 |
|---|---|---|
| OpenList / 任意 WebDAV 服务 | 提供视频文件 | 否 |
| [danmu_api](https://github.com/huangxd-/danmu_api)（镜像 `logvar/danmu-api`） | 聚合巴哈姆特/优酷/腾讯/弹弹play/爱一帆等弹幕源 | 否 |
| `aniswitch-gateway/`（本仓库） | 把 OpenList 直链 + 弹幕聚合成客户端要的两个接口 | 是（源码，无数据） |

网关的部署、接口与配置说明见 [`aniswitch-gateway/README.md`](aniswitch-gateway/README.md)。

## 构建

需要 devkitPro（devkitA64 + switch-dev）与下面 4 个包（wiliwili 上游提供）：

```
switch-ffmpeg-7.1-1-any.pkg.tar.zst
switch-libmpv-0.36.0-3-any.pkg.tar.zst
switch-nspmini-48d4fc2-1-any.pkg.tar.xz
hacBrewPack-3.05-1-any.pkg.tar.zst
```

```bash
cmake -B cmake-build-switch -DCMAKE_BUILD_TYPE=Release \
      -DBUILTIN_NSP=OFF -DPLATFORM_SWITCH=ON \
      -DBRLS_UNITY_BUILD=ON -DCMAKE_UNITY_BUILD_BATCH_SIZE=8
make -C cmake-build-switch -j2 wiliwili.nro
```

产物 `cmake-build-switch/wiliwili.nro`，丢到 `sd:/switch/AniSwitch/` 下即可（hbmenu 里显示名与图标由 `CMakeLists.txt`
里的 `nacptool --create "AniSwitch"` 与 `resources/icon/aniswitch.jpg` 决定，图标必须是 **256×256 JPEG**）。

构建注意：

- 必须在 `-j2` 级别的并发下编译，某些环境（如 WSL2）高并发会让 VM 整体重启。
- `BRLS_UNITY_BUILD=ON` 会把多个 cpp 合成一个 TU，**匿名命名空间里不要出现重名函数**（本分支因此加了共享的 `aniswitchLog`）。

## 运行期文件

| 路径 | 内容 |
|---|---|
| `sd:/config/wiliwili/aniswitch_sources.json` | WebDAV 源列表（删除即恢复内置源） |
| `sd:/aniswitch.log` | 运行日志（排查用，可随时删） |
| `sd:/switch/AniSwitch/wiliwili_prev*.nro` | 升级时自动备份的旧版本，程序启动会自行清理 |

## 与上游的差异

新增（全部为本分支新增文件，不改上游逻辑）：

- `wiliwili/source/activity/aniswitch_{activity,browse_activity,list_activity,player_activity,sources,webdav,add_dialog}.cpp`
- `wiliwili/include/activity/` 下同名头文件 + `wiliwili/include/utils/aniswitch_util.hpp`
- `resources/xml/activity/aniswitch_{sources,browse,activity}.xml`、`resources/svg/ico-aniswitch*.svg`、`resources/icon/aniswitch.{jpg,png}`

改动（尽量小）：

- `wiliwili/source/main.cpp`：启动即进 `openAniSwitch()` 的上一级主页（`openMain()`），并在初始化后清理旧备份
- `wiliwili/include/activity/main_activity.hpp` / `source/activity/main_activity.cpp`：主页精简成单个 AniSwitch 按钮
- `wiliwili/resources/xml/activity/main.xml`：同上
- `wiliwili/resources/xml/activity/setting_activity.xml` 等：移除原有的 AniSwitch 设置入口（入口已搬到主页）
- `CMakeLists.txt`：应用名、作者、图标
- 上游的 `mpv`、`ffmpeg`（含 Tegra NVDEC 硬解）、弹幕引擎、`libass` 等全部原样复用

## 已知限制

- **弹幕识别是靠目录名匹配配置的，不做通用自动识别**：新番需要在网关 `config.json` 的 `animes` 里加一条（`dir` 片段 + 各源 `animeId`/集号范围），然后 `/api/verify` 抽查几集确认条数。
- 播放进度不记忆，关掉重进从头发。
- 只支持视频文件的平铺目录，不支持多层聚合播放列表。
- 弹幕暂无过滤（关键词/等级/类型）与透明度设置。

## 许可证

GPLv3，与上游一致。上游版权与署名（`xfangfang/wiliwili` 及其贡献者）完整保留。
AI 参与范围见 [`AI-DECLARATION.md`](AI-DECLARATION.md)。

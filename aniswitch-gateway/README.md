# AniSwitch Gateway

一个零依赖的 Node 小服务，把 **OpenList 的 WebDAV/直链** 和 **自建的弹幕聚合库（danmu_api）** 拼成 Switch 客户端要的两个接口。

> ⚠️ **本仓库不包含任何弹幕数据，也不内置任何弹幕服务。**
> 弹幕来自你自己架设的 [danmu_api](https://github.com/huangxd-/danmu_api)（Docker Hub 镜像 `logvar/danmu-api`）。
> 没有自建弹幕库，这个网关只能返回空列表，客户端也拿不到弹幕。

## 它做什么

```
Switch (AniSwitch.nro)
   │  GET /api/episodes            → 当前目录的视频列表（含每次现取的 OpenList sign 直链）
   │  GET /api/danmaku?dir=&name=&ep= → 合并后的弹幕 JSON（多源去重）
   ▼
AniSwitch Gateway (本目录 server.js)
   │  OpenList  /api/auth/login、/api/fs/list
   │  danmu_api /api/v2/search/anime、/api/v2/bangumi/{id}、/api/v2/comment/{episodeId}
   ▼
OpenList (夸克/阿里云盘等)        danmu_api (巴哈姆特/优酷/腾讯/弹弹play/爱一帆…)
```

## 为什么需要它

- 客户端在 Switch 上，**proxyless、没有浏览器、CORS 无所谓，但需要一个稳定聚合端**：巴哈、优酷、腾讯、弹弹play 的接口各异，聚合与去重放在服务端做一次，比在 4 核 ARM 的掌机上做更划算。
- OpenList 的直链 `sign` 会变，客户端不该长期保存 → 网关每次请求都重新取一遍。
- 番剧与弹幕源的**集号偏移**（例如巴哈只有 1–220、疾风传要从优酷/腾讯补）是人工知识，写在 `config.json` 的 `animes` 里，改完热加载，不用重启容器。

## 部署

1. 先起 danmu_api（细节见其 README），默认端口 9321、默认 token `87654321`：

```bash
docker run -d --name danmu-api -p 9321:9321 \
  -e TOKEN=87654321 -e TZ=Asia/Shanghai \
  -v /mnt/data/danmu-api/config:/app/config \
  -v /mnt/data/danmu-api/.cache:/app/.cache \
  --restart unless-stopped logvar/danmu-api:latest
```

2. 把本目录的 `server.js` 和你的 `config.json` 放到一个目录，起网关（复用 danmu_api 镜像里的 Node，省去装环境）：

```bash
docker run -d --name aniswitch-gw --restart unless-stopped \
  -p 9333:9333 \
  -v /mnt/data/aniswitch-gw:/app \
  --entrypoint node logvar/danmu-api:latest /app/server.js
```

3. 自检：

```bash
curl http://<你的IP>:9333/health
curl http://<你的IP>:9333/api/animes
curl "http://<你的IP>:9333/api/verify?dir=/你的/番剧目录&eps=1,12"
```

`/api/verify` 会跳过缓存、直接报每集弹幕条数，用来确认集号映射对不对。

## 接口

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/health` | `{"code":0,"ok":true}` |
| GET | `/api/episodes` | 默认目录的视频列表 `{code,total,title,episodes:[{ep,name,size,url}]}`，`url` 是带 `sign` 的直链 |
| GET | `/api/danmaku?dir=&name=&ep=` | `{code,count,comments:[{cid,p,m}]}`，`p` 形如 `时间,模式,字号,颜色,,,,,等级` |
| GET | `/api/animes` | 已配置的番剧与规则条数 |
| GET | `/api/verify?dir=&eps=1,12` | 不写缓存，直接返回各集条数 |
| GET | `/api/reload` | 强制重读 `config.json`（平时改文件会自动热加载） |

## config.json

- `openlist`：OpenList 地址与账号，`dir` 是默认目录（客户端也可以走 WebDAV 自己浏览，不走 `dir`）。
- `danmu`：danmu_api 的 `base`，形如 `http://ip:9321/<TOKEN>`。
- `animes[]`：**按目录配置的弹幕规则**，`dir` 用目录名片段匹配，`rules[]` 里 `from`/`to` 限定该源覆盖的集号，`offset` 用于集号偏移。
- `sources[]`：兜底规则，目录匹配不到时使用。

`cacheDir` 下的缓存文件名是 `<番剧tag>-<集号>.json`，默认 30 天有效期，删掉即重建。

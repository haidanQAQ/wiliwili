'use strict';
// AniSwitch Gateway: episode list (OpenList) + merged danmaku (danmu_api)
// Dependency-free Node 18+/22. Config at /app/config.json (hot reload), cache at /cache.
//
// 弹幕按「目录」配置：config.json 里的 animes[] 每条是一个番剧目录，
// 匹配不到时退回顶层 sources[]（老行为，兼容内置网关入口）。
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const CFG_PATH = process.env.ANISWITCH_CONFIG || '/app/config.json';
const PORT = (process.env.ANISWITCH_PORT && parseInt(process.env.ANISWITCH_PORT, 10)) || 9333;

let cfg = null;
let cfgMtime = 0;
let CACHE_DIR = '/cache';

function loadConfig(force) {
  try {
    const st = fs.statSync(CFG_PATH);
    if (!force && cfg && st.mtimeMs === cfgMtime) return cfg;
    const next = JSON.parse(fs.readFileSync(CFG_PATH, 'utf8'));
    cfg = next;
    cfgMtime = st.mtimeMs;
    CACHE_DIR = cfg.cacheDir || '/cache';
    fs.mkdirSync(CACHE_DIR, { recursive: true });
    console.log('[cfg] loaded, animes=' + ((cfg.animes || []).length) + ' sources=' + ((cfg.sources || []).length));
    return cfg;
  } catch (e) {
    if (cfg) {
      console.log('[cfg] reload failed, keep old: ' + e.message);
      return cfg;
    }
    throw e;
  }
}

loadConfig(true);

function TTL_EP() {
  return (cfg.cacheTtlEpisodes || 300) * 1000;
}

let olToken = null;
let olTokenAt = 0;

async function openlistToken(force) {
  const OL = cfg.openlist;
  if (!force && olToken && Date.now() - olTokenAt < 30 * 60 * 1000) return olToken;
  const r = await fetch(OL.base + '/api/auth/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username: OL.user, password: OL.pass }),
  });
  const j = await r.json();
  if (j.code !== 200 || !j.data || !j.data.token) {
    throw new Error('openlist login failed: ' + JSON.stringify(j).slice(0, 200));
  }
  olToken = j.data.token;
  olTokenAt = Date.now();
  return olToken;
}

function epFromName(name) {
  const m = /EP(\d{1,4})/i.exec(name);
  return m ? parseInt(m[1], 10) : null;
}

let epCache = { at: 0, data: null };

async function listEpisodes(force) {
  if (!force && epCache.data && Date.now() - epCache.at < TTL_EP()) return epCache.data;
  const OL = cfg.openlist;
  const token = await openlistToken(false);
  const r = await fetch(OL.base + '/api/fs/list', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: token },
    body: JSON.stringify({ path: OL.dir, password: '', page: 1, per_page: 0, refresh: !!force }),
  });
  const j = await r.json();
  if (j.code !== 200 || !j.data || !j.data.content) {
    throw new Error('openlist list failed: ' + JSON.stringify(j).slice(0, 200));
  }
  const eps = [];
  for (const it of j.data.content) {
    if (!it.is_dir && /\.(mp4|mkv|ts|m2ts)$/i.test(it.name)) {
      const ep = epFromName(it.name);
      if (ep === null) continue;
      const full = OL.dir.replace(/\/$/, '') + '/' + it.name;
      eps.push({
        ep,
        name: it.name,
        size: it.size,
        url: OL.base + '/d/' + encodeURIComponent(full) + '?sign=' + encodeURIComponent(it.sign || ''),
      });
    }
  }
  eps.sort((a, b) => a.ep - b.ep || a.size - b.size);
  epCache = { at: Date.now(), data: eps };
  return eps;
}

// ---- 番剧目录 -> 弹幕规则 ----
// dir 比较用「目录名片段包含」，因此配 "火影忍者" 就能命中
// "/夸克/来自：分享/火影忍者 全集4K日语中字" 这种实际路径。
function dirBase(dirParam) {
  const parts = String(dirParam || '')
    .replace(/\/+$/, '')
    .split('/')
    .filter(Boolean);
  return parts.length ? parts[parts.length - 1] : '';
}

function pickAnime(dirParam, nameParam) {
  const list = cfg.animes || [];
  const full = String(dirParam || '');
  const base = dirBase(full);
  const hit = [];
  for (const a of list) {
    const key = a.dir || a.title || '';
    if (!key) continue;
    if (full.includes(key) || (base && (base.includes(key) || key.includes(base)))) {
      hit.push({ a, w: key.length });
    }
  }
  if (hit.length) {
    hit.sort((x, y) => y.w - x.w);
    return hit[0].a;
  }
  // 名字里带番剧名时也能兜住（有些目录名是「Season 1」这种）
  if (nameParam) {
    for (const a of list) {
      const key = a.dir || a.title || '';
      if (key && String(nameParam).includes(key)) return a;
    }
  }
  return null;
}

function rulesOf(anime) {
  if (anime && Array.isArray(anime.rules) && anime.rules.length) return anime.rules;
  return cfg.sources || [];
}

function cacheTagOf(anime, dirParam) {
  const raw = anime ? (anime.dir || anime.title || 'anime') : ('default:' + dirBase(dirParam));
  return crypto.createHash('sha1').update(raw).digest('hex').slice(0, 10);
}

// ---- danmaku ----
const bangumiCache = new Map(); // animeId -> {at, episodes:[{episodeId,episodeTitle}]}

async function bangumiEpisodes(animeId) {
  const hit = bangumiCache.get(animeId);
  if (hit && Date.now() - hit.at < 6 * 3600 * 1000) return hit.episodes;
  const r = await fetch(cfg.danmu.base + '/api/v2/bangumi/' + animeId);
  const j = await r.json();
  const list = (j.bangumi && j.bangumi.episodes) || [];
  bangumiCache.set(animeId, { at: Date.now(), episodes: list });
  return list;
}

async function comments(eid) {
  const r = await fetch(cfg.danmu.base + '/api/v2/comment/' + eid + '?format=json');
  const j = await r.json();
  return j.comments || [];
}

function parseDanmaku(list) {
  const out = [];
  for (const c of list) {
    const p = String(c.p || '').split(',');
    if (p.length < 4) continue;
    const t = parseFloat(p[0]);
    if (!isFinite(t)) continue;
    out.push({
      t: Math.round(t * 100) / 100,
      type: parseInt(p[1], 10) || 1,
      color: parseInt(p[2], 10) || 16777215,
      text: String(c.m || '').trim(),
    });
  }
  return out;
}

async function mergedDanmaku(ep, anime, dirParam, opts) {
  const tag = cacheTagOf(anime, dirParam);
  const cacheFile = path.join(CACHE_DIR, tag + '_ep' + ep + '.json');
  const noCache = !!(opts && opts.noCache);
  if (!noCache) {
    try {
      const st = fs.statSync(cacheFile);
      if (Date.now() - st.mtimeMs < 30 * 24 * 3600 * 1000) {
        const hit = JSON.parse(fs.readFileSync(cacheFile, 'utf8'));
        hit.cached = true;
        return hit;
      }
    } catch (e) {
      /* miss */
    }
  }

  const rules = rulesOf(anime);
  const jobs = rules.map(async (rule) => {
    if (ep < (rule.from || 1)) return [];
    if (rule.to && ep > rule.to) return [];
    const idx = ep - (rule.offset || 0) - 1;
    if (idx < 0) return [];
    try {
      const eps = await bangumiEpisodes(rule.animeId);
      const item = eps[idx];
      if (!item) return [];
      const raw = await comments(item.episodeId);
      return parseDanmaku(raw);
    } catch (e) {
      return [];
    }
  });
  const parts = await Promise.all(jobs);
  const seen = new Set();
  const merged = [];
  for (const part of parts) {
    for (const d of part) {
      const key = Math.round(d.t) + '|' + d.text;
      if (d.text && !seen.has(key)) {
        seen.add(key);
        merged.push(d);
      }
    }
  }
  merged.sort((a, b) => a.t - b.t);
  const payload = {
    ep,
    count: merged.length,
    anime: anime ? { key: anime.dir || anime.title, rules: rules.length } : null,
    tag,
    items: merged,
  };
  try {
    fs.writeFileSync(cacheFile, JSON.stringify(payload));
  } catch (e) {
    console.log('[cache] write failed: ' + e.message);
  }
  return payload;
}

// ---- http ----
function send(res, code, obj) {
  const body = Buffer.from(JSON.stringify(obj), 'utf8');
  res.writeHead(code, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': body.length,
    'Access-Control-Allow-Origin': '*',
  });
  res.end(body);
}

const server = http.createServer(async (req, res) => {
  loadConfig(false);
  const u = new URL(req.url, 'http://x');
  console.log(
    '[req] ' + (req.socket && req.socket.remoteAddress) + ' ' + req.method + ' ' + req.url +
      ' ua=' + (req.headers['user-agent'] || '-')
  );
  try {
    if (u.pathname === '/api/episodes') {
      const eps = await listEpisodes(u.searchParams.get('refresh') === '1');
      return send(res, 200, { code: 0, total: eps.length, title: cfg.title || 'AniSwitch', episodes: eps });
    }

    if (u.pathname === '/api/danmaku') {
      const ep = parseInt(u.searchParams.get('ep') || '1', 10);
      if (!ep || ep < 1) return send(res, 400, { code: 1, msg: 'bad ep' });
      const dirParam = u.searchParams.get('dir') || '';
      const nameParam = u.searchParams.get('name') || '';
      const anime = pickAnime(dirParam, nameParam);
      const data = await mergedDanmaku(ep, anime, dirParam, { noCache: u.searchParams.get('fresh') === '1' });
      return send(res, 200, { code: 0, ...data });
    }

    // 配了哪些番、命中哪一条
    if (u.pathname === '/api/animes') {
      const list = (cfg.animes || []).map((a) => ({
        dir: a.dir || a.title,
        rules: rulesOf(a).length,
        detail: rulesOf(a),
      }));
      return send(res, 200, { code: 0, total: list.length, animes: list, fallbackSources: (cfg.sources || []).length });
    }

    // 校验某个目录在第 ep 集能拿到多少条弹幕（不进缓存）
    if (u.pathname === '/api/verify') {
      const dirParam = u.searchParams.get('dir') || '';
      const eps = String(u.searchParams.get('eps') || '1,2,100')
        .split(',')
        .map((x) => parseInt(x, 10))
        .filter((x) => x > 0);
      const anime = pickAnime(dirParam, '');
      const out = [];
      for (const ep of eps) {
        const r = await mergedDanmaku(ep, anime, dirParam, { noCache: true });
        out.push({ ep, count: r.count });
      }
      return send(res, 200, { code: 0, matched: anime ? anime.dir || anime.title : null, results: out });
    }

    if (u.pathname === '/api/reload') {
      loadConfig(true);
      return send(res, 200, { code: 0, ok: true, animes: (cfg.animes || []).length });
    }

    if (u.pathname === '/health') return send(res, 200, { code: 0, ok: true });
    return send(res, 404, { code: 404, msg: 'not found' });
  } catch (e) {
    return send(res, 500, { code: 500, msg: String((e && e.message) || e) });
  }
});

server.listen(PORT, '0.0.0.0', () => {
  console.log('aniswitch-gw listening on ' + PORT + ', config=' + CFG_PATH);
});

#pragma once

#include <string>
#include <vector>

#include "activity/aniswitch_sources.hpp"

// 一个 WebDAV 目录项
struct AniSwitchDavEntry {
    std::string name;   // 显示名（已 percent-decode）
    std::string path;   // 服务器绝对路径（已 decode，例如 /dav/夸克/火影/ep01.mp4）
    bool isDir = false;
    long long size = 0;
};

struct AniSwitchDavResult {
    int status = 0;          // HTTP 状态码，0 表示连不上
    std::string error;       // cpr/curl 的错误描述
    std::string origin;      // scheme://host:port
    std::vector<AniSwitchDavEntry> entries;
};

// 列目录（PROPFIND Depth:1）。absPath 为空时用源 URL 自身的路径。
AniSwitchDavResult aniswitchDavList(const AniSwitchSource& source, const std::string& absPath);

// 生成可给 mpv 直接用的直链（带 Basic Auth）
std::string aniswitchDavFileUrl(const AniSwitchSource& source, const AniSwitchDavEntry& entry);

// 生成纯 HTTP 地址（不带账号密码，给 cpr 之类的客户端用）
std::string aniswitchDavPlainUrl(const AniSwitchSource& source, const std::string& absPath);

// 判断是不是能播的视频
bool aniswitchIsVideoFile(const std::string& name);

// 路径 percent-encode / decode（/ 不编码）
std::string aniswitchEncodePath(const std::string& path);
std::string aniswitchDecodeUrl(const std::string& in);

// 从文件名里抠出集号（EP001 / 第001集 / [01]），失败返回 -1
int aniswitchGuessEpisode(const std::string& name);

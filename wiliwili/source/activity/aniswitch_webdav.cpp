//
// AniSwitch: WebDAV 目录读取（PROPFIND）
//
// 用 libcurl 直接发 PROPFIND：cpr 这版没有自定义 method 的封装，
// 而 Switch 上 curl 一定可用（wiliwili 自己就依赖它）。
//

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "activity/aniswitch_webdav.hpp"
#include "utils/aniswitch_util.hpp"

namespace {

const char* PROPFIND_BODY =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<d:propfind xmlns:d=\"DAV:\">"
    "<d:prop><d:displayname/><d:resourcetype/><d:getcontentlength/></d:prop>"
    "</d:propfind>";

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::once_flag curlInitOnce;

void ensureCurlInit() {
    std::call_once(curlInitOnce, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

bool isUnreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string decodePercent(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = in.substr(i + 1, 2);
            char* end = nullptr;
            long v    = std::strtol(hex.c_str(), &end, 16);
            if (end && *end == '\0') {
                out.push_back(static_cast<char>(v));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

// 找到 <prefix:name ...> 或 </prefix:name> 的 '<' 位置
size_t findTag(const std::string& s, size_t from, const std::string& name, bool closing) {
    size_t pos = from;
    while (true) {
        size_t lt = s.find('<', pos);
        if (lt == std::string::npos) return std::string::npos;
        size_t i = lt + 1;
        if (closing) {
            if (i >= s.size() || s[i] != '/') { pos = lt + 1; continue; }
            ++i;
        }
        // 跳过命名空间前缀
        size_t nameStart = i;
        size_t colon     = std::string::npos;
        for (size_t j = i; j < s.size(); ++j) {
            char c = s[j];
            if (c == ':' && colon == std::string::npos) { colon = j; continue; }
            if (c == '>' || c == ' ' || c == '/' || c == '\t' || c == '\r' || c == '\n') {
                std::string tag = colon != std::string::npos && colon > nameStart
                                      ? s.substr(colon + 1, j - colon - 1)
                                      : s.substr(nameStart, j - nameStart);
                if (tag == name) return lt;
                break;
            }
        }
        pos = lt + 1;
    }
}

std::string tagText(const std::string& block, const std::string& name) {
    size_t open = findTag(block, 0, name, false);
    if (open == std::string::npos) return "";
    size_t gt = block.find('>', open);
    if (gt == std::string::npos) return "";
    if (gt > 0 && block[gt - 1] == '/') return "";
    size_t close = findTag(block, gt, name, true);
    if (close == std::string::npos) return "";
    std::string text = block.substr(gt + 1, close - gt - 1);
    // trim
    size_t b = text.find_first_not_of(" \t\r\n");
    size_t e = text.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return text.substr(b, e - b + 1);
}

std::string stripTrailingSlash(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

// http://host:port/dav/x -> scheme://host:port 与 /dav/x
void splitUrl(const std::string& url, std::string& origin, std::string& path) {
    size_t scheme = url.find("://");
    if (scheme == std::string::npos) {
        origin = url;
        path   = "/";
        return;
    }
    size_t slash = url.find('/', scheme + 3);
    if (slash == std::string::npos) {
        origin = url;
        path   = "/";
    } else {
        origin = url.substr(0, slash);
        path   = url.substr(slash);
    }
}

std::string withAuth(const std::string& origin, const AniSwitchSource& source) {
    if (source.user.empty()) return origin;
    size_t scheme = origin.find("://");
    if (scheme == std::string::npos) return origin;
    return origin.substr(0, scheme + 3) + source.user + ":" + source.pass + "@" +
           origin.substr(scheme + 3);
}

}  // namespace

std::string aniswitchEncodePath(const std::string& path) {
    std::string out;
    out.reserve(path.size() * 2);
    for (unsigned char c : path) {
        if (isUnreserved(c) || c == '/') {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string aniswitchDecodeUrl(const std::string& in) { return decodePercent(in); }

std::string aniswitchDavPlainUrl(const AniSwitchSource& source, const std::string& absPath) {
    std::string origin, base;
    splitUrl(source.url, origin, base);
    if (absPath.empty()) return origin + aniswitchEncodePath(base);
    return origin + aniswitchEncodePath(absPath);
}

std::string aniswitchDavFileUrl(const AniSwitchSource& source, const AniSwitchDavEntry& entry) {
    std::string origin, base;
    splitUrl(source.url, origin, base);
    return withAuth(origin, source) + aniswitchEncodePath(entry.path);
}

bool aniswitchIsVideoFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const char* exts[] = {".mp4", ".mkv", ".webm", ".avi", ".mov",
                                 ".m4v", ".ts",  ".flv",  ".wmv", ".mpg"};
    for (auto* e : exts) {
        if (lower.size() > strlen(e) && lower.compare(lower.size() - strlen(e), strlen(e), e) == 0)
            return true;
    }
    return false;
}

int aniswitchGuessEpisode(const std::string& name) {
    // EP001 / ep 12 / 第03集 / [05]
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto readNumber = [&](size_t pos) -> int {
        int value = 0;
        size_t digits = 0;
        while (pos < lower.size() && std::isdigit(static_cast<unsigned char>(lower[pos]))) {
            value = value * 10 + (lower[pos] - '0');
            ++pos;
            ++digits;
            if (digits > 4) break;
        }
        return digits > 0 ? value : -1;
    };

    size_t pos = lower.find("ep");
    while (pos != std::string::npos) {
        size_t p = pos + 2;
        while (p < lower.size() && (lower[p] == ' ' || lower[p] == '-' || lower[p] == '_')) ++p;
        int v = readNumber(p);
        if (v > 0) return v;
        pos = lower.find("ep", pos + 2);
    }

    size_t di = lower.find('第');
    if (di != std::string::npos) {
        int v = readNumber(di + 3);  // 中文「第」在 UTF-8 里占 3 字节
        if (v > 0) return v;
    }

    size_t lb = lower.find('[');
    if (lb != std::string::npos) {
        int v = readNumber(lb + 1);
        if (v > 0) return v;
    }
    return -1;
}

AniSwitchDavResult aniswitchDavList(const AniSwitchSource& source, const std::string& absPathIn) {
    ensureCurlInit();

    AniSwitchDavResult result;

    std::string origin, basePath;
    splitUrl(source.url, origin, basePath);
    result.origin = origin;

    std::string absPath = absPathIn.empty() ? basePath : absPathIn;
    const std::string url = origin + aniswitchEncodePath(absPath);

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }

    std::string body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Depth: 1");
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PROPFIND_BODY);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AniSwitch/0.1");

    if (!source.user.empty()) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERNAME, source.user.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, source.pass.c_str());
    }

    CURLcode cc = curl_easy_perform(curl);
    if (cc != CURLE_OK) {
        result.error = curl_easy_strerror(cc);
    } else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        result.status = static_cast<int>(code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!result.error.empty() || result.status == 0) {
        aniswitchLog("dav: PROPFIND " + url + " failed: " + result.error);
        return result;
    }

    aniswitchLog("dav: PROPFIND " + url + " -> " + std::to_string(result.status) + " bytes=" +
                 std::to_string(body.size()));

    if (result.status != 207 && result.status != 200) {
        result.error = "HTTP " + std::to_string(result.status);
        return result;
    }

    const std::string wantPath = stripTrailingSlash(absPath);

    size_t pos = 0;
    while (true) {
        size_t open = findTag(body, pos, "response", false);
        if (open == std::string::npos) break;
        size_t gt = body.find('>', open);
        if (gt == std::string::npos) break;
        size_t close = findTag(body, gt, "response", true);
        if (close == std::string::npos) break;

        std::string block = body.substr(gt + 1, close - gt - 1);
        pos               = body.find('>', close);
        if (pos == std::string::npos) break;
        pos += 1;

        std::string href = tagText(block, "href");
        if (href.empty()) continue;

        // 完整 URL 只留路径部分
        size_t scheme = href.find("://");
        if (scheme != std::string::npos) {
            size_t slash = href.find('/', scheme + 3);
            href         = slash == std::string::npos ? "/" : href.substr(slash);
        }

        AniSwitchDavEntry entry;
        entry.path  = decodePercent(href);
        entry.isDir = block.find("collection") != std::string::npos;

        std::string trimmed = stripTrailingSlash(entry.path);
        if (trimmed == wantPath || entry.path.empty()) continue;  // 目录自身

        size_t lastSlash = trimmed.find_last_of('/');
        entry.name       = lastSlash == std::string::npos ? trimmed : trimmed.substr(lastSlash + 1);
        if (entry.name.empty()) continue;

        std::string sizeText = tagText(block, "getcontentlength");
        entry.size           = sizeText.empty() ? 0 : std::atoll(sizeText.c_str());

        if (entry.path.back() != '/') entry.path += entry.isDir ? "/" : "";
        result.entries.push_back(std::move(entry));
    }

    aniswitchLog("dav: entries " + std::to_string(result.entries.size()));

    // 目录在前，然后按名字排
    std::sort(result.entries.begin(), result.entries.end(),
              [](const AniSwitchDavEntry& a, const AniSwitchDavEntry& b) {
                  if (a.isDir != b.isDir) return a.isDir;
                  return a.name < b.name;
              });

    return result;
}

//
// AniSwitch: WebDAV 源配置（读写 sdmc:/config/wiliwili/aniswitch_sources.json）
//

#include <cstdio>
#include <sys/stat.h>

#include <nlohmann/json.hpp>

#include "activity/aniswitch_sources.hpp"
#include "utils/aniswitch_util.hpp"

namespace aniswitch {

std::string sourcesPath() { return "sdmc:/config/wiliwili/aniswitch_sources.json"; }

static std::string& gatewayBaseRef() {
    static std::string base;
    return base;
}

const std::string& gatewayBase() {
    std::string& base = gatewayBaseRef();
    if (base.empty()) {
        // 第一次调用：从源列表里找内置网关那条
        std::vector<AniSwitchSource> list = loadSources();
        for (const auto& s : list) {
            if (s.builtin && !s.url.empty()) {
                base = s.url;
                break;
            }
        }
        if (base.empty()) base = ANISWITCH_GW_DEFAULT;
    }
    return base;
}

void setGatewayBase(const std::string& url) { gatewayBaseRef() = url; }

std::vector<AniSwitchSource> defaultSources() {
    AniSwitchSource builtin;
    builtin.name    = "内置网关 · 火影忍者 4K";
    builtin.url     = ANISWITCH_GW_DEFAULT;
    builtin.builtin = true;
    return {builtin};
}

std::vector<AniSwitchSource> loadSources() {
    std::vector<AniSwitchSource> result;

    FILE* fp = fopen(sourcesPath().c_str(), "rb");
    if (!fp) {
        aniswitchLog("sources: not found, use default");
        return defaultSources();
    }

    std::string text;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
    fclose(fp);

    try {
        auto j = nlohmann::json::parse(text);
        for (auto& item : j["sources"]) {
            AniSwitchSource s;
            s.name    = item.value("name", std::string(""));
            s.url     = item.value("url", std::string(""));
            s.user    = item.value("user", std::string(""));
            s.pass    = item.value("pass", std::string(""));
            s.builtin = item.value("builtin", false);
            if (s.name.empty()) s.name = s.url;
            result.push_back(std::move(s));
        }
    } catch (const std::exception& e) {
        aniswitchLog(std::string("sources: parse failed ") + e.what());
    }

    if (result.empty()) result = defaultSources();
    aniswitchLog("sources: loaded " + std::to_string(result.size()));
    return result;
}

void saveSources(const std::vector<AniSwitchSource>& sources) {
    nlohmann::json j;
    j["sources"] = nlohmann::json::array();
    for (const auto& s : sources) {
        j["sources"].push_back({{"name", s.name},
                                {"url", s.url},
                                {"user", s.user},
                                {"pass", s.pass},
                                {"builtin", s.builtin}});
    }

    std::string text = j.dump(2);
    FILE* fp         = fopen(sourcesPath().c_str(), "wb");
    if (!fp) {
        aniswitchLog("sources: save failed (open)");
        return;
    }
    fwrite(text.data(), 1, text.size(), fp);
    fclose(fp);
    aniswitchLog("sources: saved " + std::to_string(sources.size()));
}

}  // namespace aniswitch

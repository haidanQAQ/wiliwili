//
// AniSwitch: 播放任意 HTTP 地址（OpenList 直链）并加载合并弹幕
//

#include <thread>
#include <cmath>

#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_player_activity.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"
#include "view/danmaku_core.hpp"

using namespace brls::literals;

AniSwitchPlayerActivity::AniSwitchPlayerActivity(std::vector<AniSwitchEpisode> list, size_t idx)
    : episodes(std::move(list)), index(idx) {
    brls::Logger::info("AniSwitchPlayer: open index {}", index);
}

AniSwitchPlayerActivity::~AniSwitchPlayerActivity() {
    if (mpvEventSubscribeID) MPV_E->unsubscribe(mpvEventSubscribeID);
}

void AniSwitchPlayerActivity::onContentAvailable() {
    this->video->registerCommonActions(this);

    this->mpvEventSubscribeID = MPV_E->subscribe([this](MpvEventEnum event) {
        if (event == MpvEventEnum::END_OF_FILE) {
            brls::sync([this]() { this->playNext(); });
        }
    });

    this->playCurrent();
    brls::sync([this]() { brls::Application::giveFocus(this->video); });
}

void AniSwitchPlayerActivity::playCurrent() {
    if (this->index >= this->episodes.size()) return;
    const auto& e = this->episodes[this->index];

    char title[256];
    snprintf(title, sizeof(title), "EP%03d  %s", e.ep, e.name.c_str());

    MPVCore::instance().reset();
    this->video->setTitle(title);
    this->video->showOSD(true);
    MPVCore::instance().setUrl(e.url);

    brls::Logger::info("AniSwitchPlayer: play ep {}", e.ep);
    this->loadDanmaku(e.ep);
}

void AniSwitchPlayerActivity::playNext() {
    if (this->index + 1 >= this->episodes.size()) {
        brls::Logger::info("AniSwitchPlayer: no more episodes");
        return;
    }
    this->index++;
    this->playCurrent();
}

void AniSwitchPlayerActivity::loadDanmaku(int ep) {
    std::thread([this, ep]() {
        std::string url = std::string(ANISWITCH_GW_DEFAULT) + "/api/danmaku?ep=" + std::to_string(ep);
        brls::Logger::info("AniSwitchPlayer: GET {}", url);

        cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{60000},
                                   cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});

        std::vector<DanmakuItem> data;
        if (r.status_code != 200) {
            brls::Logger::error("AniSwitchPlayer: danmaku HTTP {}", r.status_code);
        } else {
            try {
                auto j = nlohmann::json::parse(r.text);
                if (j.value("code", -1) == 0) {
                    for (auto& c : j["comments"]) {
                        double t    = c.value("t", 0.0);
                        int mode    = c.value("mode", 1);
                        int color   = c.value("color", 16777215);
                        std::string text = c.value("text", std::string(""));
                        if (text.empty()) continue;

                        char attr[96];
                        // time, type, fontSize, color, , 0, 0, 0, level(>=9 段)
                        snprintf(attr, sizeof(attr), "%.3f,%d,25,%d,,0,0,0,10", t, mode, color);
                        data.emplace_back(text, attr);
                    }
                }
            } catch (const std::exception& ex) {
                brls::Logger::error("AniSwitchPlayer: danmaku parse error {}", ex.what());
            }
        }

        brls::Logger::info("AniSwitchPlayer: {} danmaku for ep {}", data.size(), ep);

        brls::sync([this, data]() {
            DanmakuCore::instance().reset();
            if (!data.empty()) DanmakuCore::instance().loadDanmakuData(data);
        });
    }).detach();
}

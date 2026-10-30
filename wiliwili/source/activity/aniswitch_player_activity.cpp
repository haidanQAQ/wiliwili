//
// AniSwitch: 播放任意 HTTP 地址（OpenList 直链）并加载合并弹幕
//

#include <cstdio>
#include <thread>
#include <cmath>

#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_player_activity.hpp"
#include "activity/aniswitch_sources.hpp"
#include "utils/aniswitch_util.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"
#include "view/danmaku_core.hpp"

using namespace brls::literals;


AniSwitchPlayerActivity::AniSwitchPlayerActivity(std::vector<AniSwitchEpisode> list, size_t idx,
                                                 std::string dir)
    : episodes(std::move(list)), index(idx), dirPath(std::move(dir)) {
    aniswitchLog("== AniSwitchPlayerActivity ctor index=" + std::to_string(idx) + " dir=" + this->dirPath);
}

AniSwitchPlayerActivity::~AniSwitchPlayerActivity() {
    aniswitchLog("== player dtor");
    this->stopPlayback();
}

void AniSwitchPlayerActivity::stopPlayback() {
    if (this->stopped) return;
    this->stopped = true;
    aniswitchLog("player: stopPlayback");

    if (this->mpvSubscribed) {
        MPV_E->unsubscribe(this->mpvEventSubscribeID);
        this->mpvSubscribed = false;
    }
    DanmakuCore::instance().reset();
    if (this->video) this->video->stop();
}

void AniSwitchPlayerActivity::willAppear(bool resetState) {
    aniswitchLog("== player willAppear");
    brls::Activity::willAppear(resetState);
}

void AniSwitchPlayerActivity::willDisappear(bool resetState) {
    aniswitchLog("== player willDisappear");
    this->stopPlayback();
    brls::Activity::willDisappear(resetState);
}

void AniSwitchPlayerActivity::onContentAvailable() {
    aniswitchLog("== player onContentAvailable");
    brls::Activity::onContentAvailable();

    if (!this->video) {
        aniswitchLog("player: video view missing");
        return;
    }

    this->video->registerCommonActions(this);

    this->mpvEventSubscribeID = MPV_E->subscribe([this](MpvEventEnum event) {
        if (event == MpvEventEnum::END_OF_FILE) {
            brls::sync([this]() { this->playNext(); });
        }
    });
    this->mpvSubscribed = true;

    this->playCurrent();

    // B 键：先停掉 mpv 再退出，避免「退出后声音还在后台 + 其他视频播不出来」
    this->video->registerAction(
        "cancel", brls::ControllerButton::BUTTON_B,
        [this](brls::View*) -> bool {
            aniswitchLog("player: B pressed -> stop and pop");
            this->stopPlayback();
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        true);

    brls::sync([this]() { brls::Application::giveFocus(this->video); });
    aniswitchLog("== player onContentAvailable done");
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

    aniswitchLog(std::string("play ep ") + std::to_string(e.ep) + " url=" + e.url.substr(0, 80));
    this->loadDanmaku(e.ep);
}

void AniSwitchPlayerActivity::playNext() {
    if (this->index + 1 >= this->episodes.size()) {
        aniswitchLog("player: no more episodes");
        return;
    }
    this->index++;
    this->playCurrent();
}

void AniSwitchPlayerActivity::loadDanmaku(int ep) {
    // 注意：Switch 上不能用 std::thread（主线程创建即 abort 2162-0004），一律走 brls::async
    brls::async([this, ep]() {
        const std::string name =
            (this->index < this->episodes.size()) ? this->episodes[this->index].name : std::string();
        const std::string url = aniswitch::gatewayBase() + "/api/danmaku?dir=" +
                                aniswitchUrlEncode(this->dirPath) + "&name=" + aniswitchUrlEncode(name) +
                                "&ep=" + std::to_string(ep);
        aniswitchLog("GET " + url);

        std::vector<DanmakuItem> data;
        try {
            cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{90000},
                                       cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});

            if (r.status_code != 200) {
                aniswitchLog("danmaku HTTP " + std::to_string(r.status_code));
            } else {
                auto j = nlohmann::json::parse(r.text);
                if (j.value("code", -1) == 0) {
                    for (auto& c : j["items"]) {
                        double t         = c.value("t", 0.0);
                        int type         = c.value("type", 1);
                        int color        = c.value("color", 16777215);
                        std::string text = c.value("text", std::string(""));
                        if (text.empty()) continue;

                        char attr[128];
                        // DanmakuItem 需要 >=9 段: time,type,fontSize,color,,0,0,0,level
                        snprintf(attr, sizeof(attr), "%.3f,%d,25,%d,,0,0,0,10", t, type, color);
                        data.emplace_back(text, attr);
                    }
                } else {
                    aniswitchLog("danmaku gateway error: " + j.value("msg", std::string("unknown")));
                }
            }
        } catch (const std::exception& ex) {
            aniswitchLog(std::string("danmaku exception: ") + ex.what());
        } catch (...) { aniswitchLog("danmaku exception: unknown"); }

        aniswitchLog("danmaku count " + std::to_string(data.size()) + " for ep " + std::to_string(ep));

        brls::sync([this, data]() {
            DanmakuCore::instance().reset();
            if (!data.empty()) DanmakuCore::instance().loadDanmakuData(data);
        });
    });
}

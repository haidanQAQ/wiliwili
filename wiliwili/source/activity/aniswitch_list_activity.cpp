//
// AniSwitch: 剧集列表（火影 4K，数据来自 AniSwitch Gateway /api/episodes）
//
// 经验：在 Switch(libnx) 上从主线程 `std::thread(...).detach()` 会直接 abort
// （2162-0004，无异常），borealis 的 `brls::async` 正常。所有耗时请求走 brls::async，
// 回调统一用 brls::sync 回主线程。
//

#include <cstdio>
#include <string>
#include <vector>

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/thread.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_list_activity.hpp"
#include "activity/aniswitch_player_activity.hpp"
#include "utils/activity_helper.hpp"
#include "utils/aniswitch_util.hpp"

namespace {

std::string probe(const std::string& url) {
    try {
        cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{6000}, cpr::ConnectTimeout{4000});
        return url + " -> " + std::to_string(r.status_code) + " (" + r.error.message + ")";
    } catch (const std::exception& e) {
        return url + " -> exception " + e.what();
    }
}

}  // namespace

AniSwitchEpisodeCell::AniSwitchEpisodeCell() {
    this->setFocusable(true);
    this->setHeight(88.0f);
    this->setWidthPercentage(100.0f);
    this->setPadding(10, 16, 10, 16);

    this->title = new brls::Label();
    this->title->setFontSize(24);
    this->title->setWidthPercentage(100.0f);
    this->addView(this->title);
}

void AniSwitchEpisodeCell::setEpisode(const AniSwitchEpisode& episode) {
    // 去掉扩展名与体积，只保留剧集名，方便横向 3 列显示
    std::string name = episode.name;
    size_t dot       = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    this->title->setText(name);
}

RecyclingGridItem* AniSwitchEpisodeDataSource::cellForRow(RecyclingGrid* recycler, size_t index) {
    auto* cell = static_cast<AniSwitchEpisodeCell*>(recycler->dequeueReusableCell("AniSwitchEpisodeCell"));
    if (!cell) cell = new AniSwitchEpisodeCell();
    if (index < this->episodes.size()) cell->setEpisode(this->episodes[index]);
    return cell;
}

void AniSwitchEpisodeDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (this->callback) this->callback(index);
}

AniSwitchListActivity::AniSwitchListActivity() { aniswitchLog("== AniSwitchListActivity ctor"); }

AniSwitchListActivity::~AniSwitchListActivity() {
    aniswitchLog("== AniSwitchListActivity dtor");
    this->alive->store(false);
}

brls::View* AniSwitchListActivity::createContentView() {
    return brls::View::createFromXMLResource("activity/aniswitch_activity.xml");
}

void AniSwitchListActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);
    aniswitchLog("== list willAppear");
    if (this->grid) brls::Application::giveFocus(this->grid);
}

void AniSwitchListActivity::onContentAvailable() {
    brls::Activity::onContentAvailable();
    aniswitchLog("== list onContentAvailable");

    this->status = dynamic_cast<brls::Label*>(this->getView("aniswitch/status"));
    this->grid   = dynamic_cast<RecyclingGrid*>(this->getView("aniswitch/grid"));

    if (!this->grid) return;

    this->grid->registerCell("AniSwitchEpisodeCell", []() { return new AniSwitchEpisodeCell(); });
    this->grid->showSkeleton();
    this->grid->setRefreshAction([this]() { this->loadEpisodes(); });

    this->registerAction(
        "返回", brls::ControllerButton::BUTTON_B,
        [](brls::View*) -> bool {
            aniswitchLog("list: B pressed -> pop");
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        true);

    this->loadEpisodes();
}

void AniSwitchListActivity::loadEpisodes() {
    this->showMessage("正在连接 AniSwitch Gateway ...");

    // 10 秒看门狗（brls::delay 定时器，不占用唯一的 async worker）
    const int token = this->ticket->fetch_add(1) + 1;
    aniswitchWatchdog(this->alive, this->ticket, token, 10000, [this]() {
        aniswitchLog("list: timeout 10s");
        this->showMessage("连接超时（10 秒）：" + std::string(ANISWITCH_GW_DEFAULT));
        if (this->grid) this->grid->setError("连接超时");
        aniswitchErrorDialog("连接超时（10 秒，已取消进入）\n" + std::string(ANISWITCH_GW_DEFAULT),
                             this->alive);
    });

    brls::async([this]() {
        const std::string url = std::string(ANISWITCH_GW_DEFAULT) + "/api/episodes";
        aniswitchLog("async: GET " + url);

        long status = 0;
        std::string body;
        std::string err = "unknown";
        try {
            cpr::Response r =
                cpr::Get(cpr::Url{url}, cpr::Timeout{8000}, cpr::ConnectTimeout{5000},
                         cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});
            status = r.status_code;
            body   = r.text;
            err    = r.error.message;
        } catch (const std::exception& e) {
            err = e.what();
        } catch (...) { err = "unexpected"; }

        aniswitchLog("async: status=" + std::to_string(status) + " err=" + err + " bytes=" +
                     std::to_string(body.size()));

        std::vector<AniSwitchEpisode> episodes;
        std::string title;
        std::string errorText;

        if (status != 200) {
            errorText = "Gateway 返回 " + std::to_string(status) + " " + err;
            aniswitchLog("async: " + errorText);
            aniswitchLog("probe1 " + probe(std::string(ANISWITCH_GW_DEFAULT) + "/health"));
            aniswitchLog("probe2 " + probe("http://192.168.31.251:8089/"));
            aniswitchLog("probe3 " + probe("http://192.168.31.251:5244/"));
            aniswitchLog("probe4 " + probe("http://192.168.31.1/"));
        } else {
            try {
                auto j = nlohmann::json::parse(body);
                title  = j.value("title", std::string(""));
                for (auto& item : j["episodes"]) {
                    AniSwitchEpisode e;
                    e.ep   = item.value("ep", 0);
                    e.name = item.value("name", std::string(""));
                    e.size = item.value("size", 0LL);
                    e.url  = item.value("url", std::string(""));
                    if (!e.url.empty()) episodes.push_back(std::move(e));
                }
            } catch (const std::exception& e) {
                errorText = std::string("解析失败: ") + e.what();
                aniswitchLog("async: " + errorText);
            }
        }

        aniswitchLog("async: parsed " + std::to_string(episodes.size()) + " episodes");

        brls::sync([this, episodes, title, errorText]() {
            this->ticket->fetch_add(1);  // 取消 10 秒看门狗
            if (!this->grid) return;

            if (!errorText.empty() || episodes.empty()) {
                std::string msg = errorText.empty() ? "没有读到剧集" : errorText;
                this->showMessage(msg);
                this->grid->setError(msg);
                aniswitchErrorDialog(msg + "\n" + std::string(ANISWITCH_GW_DEFAULT), this->alive);
                return;
            }

            auto* ds = new AniSwitchEpisodeDataSource(episodes);
            ds->setSelectCallback([this](size_t index) { this->openEpisode(index); });
            this->episodes = episodes;
            this->grid->setDataSource(ds);
            this->grid->notifyDataChanged();

            // RecyclingGrid 没有数据时 getDefaultFocus() 返回 nullptr，
            // pushActivity 内部的 giveFocus 会直接跳过 → 手柄光标留在上一级界面。
            brls::Application::giveFocus(this->grid);

            char tip[128];
            snprintf(tip, sizeof(tip), "%s · 共 %zu 集 · A 键播放", title.c_str(), episodes.size());
            this->showMessage(tip);
            aniswitchLog("sync: grid filled");
        });
    });
}

void AniSwitchListActivity::openEpisode(size_t index) {
    aniswitchLog("openEpisode index=" + std::to_string(index));
    if (index >= this->episodes.size()) return;
    Intent::openAniSwitchPlayer(this->episodes, index);
}

void AniSwitchListActivity::showMessage(const std::string& text) {
    if (this->status) this->status->setText(text);
}

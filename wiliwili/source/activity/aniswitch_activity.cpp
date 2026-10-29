//
// AniSwitch: OpenList/WebDAV 视频列表（数据来自 AniSwitch Gateway）
//
#include <cstdio>
#include <string>
#include <thread>

#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/view.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_activity.hpp"
#include "utils/aniswitch_util.hpp"
#include "activity/aniswitch_player_activity.hpp"
#include "view/recycling_grid.hpp"

using namespace brls::literals;

namespace {

brls::View* buildFallbackView(const std::string& message) {
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setWidthPercentage(100);
    box->setHeightPercentage(100);
    box->setPadding(40, 40, 40, 40);

    auto* label = new brls::Label();
    label->setFontSize(22);
    label->setWidthPercentage(100);
    label->setText("AniSwitch 界面加载失败：\n" + message);
    box->addView(label);
    return box;
}

// 一次简单的连通性探测，用于定位「连不上」是哪一环
std::string probeUrl(const std::string& url) {
    try {
        cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{8000},
                                   cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});
        return url + " -> " + std::to_string(r.status_code) + " " + r.error.message;
    } catch (const std::exception& e) {
        return url + " -> exception " + e.what();
    } catch (...) { return url + " -> exception unknown"; }
}
}  // namespace

AniSwitchCell::AniSwitchCell() {
    this->setFocusable(true);
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setHeight(64);
    this->setPadding(4, 12, 4, 12);
    this->setCornerRadius(6);

    this->label = new brls::Label();
    this->label->setFontSize(20);
    this->label->setWidthPercentage(100);
    this->label->setText("");
    this->addView(this->label);
}

void AniSwitchCell::setText(const std::string& text) { this->label->setText(text); }

void AniSwitchCell::prepareForReuse() { this->label->setText(""); }

void AniSwitchCell::cacheForReuse() {}

RecyclingGridItem* AniSwitchDataSource::cellForRow(RecyclingGrid* recycler, size_t index) {
    auto* cell = dynamic_cast<AniSwitchCell*>(recycler->dequeueReusableCell("AniSwitchCell"));

    if (!cell) {
        aniswitchLog("cellForRow: dequeue returned null or wrong type, index=" + std::to_string(index));
        return nullptr;
    }
    if (index >= this->episodes.size()) return cell;

    const auto& e = this->episodes[index];
    char head[32];
    snprintf(head, sizeof(head), "EP%03d", e.ep);
    cell->setText(std::string(head) + "    " + e.name);
    return cell;
}

void AniSwitchDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (index >= this->episodes.size()) return;
    aniswitchLog("select ep index " + std::to_string(index) + " ep=" + std::to_string(this->episodes[index].ep));
    auto* activity = new AniSwitchPlayerActivity(this->episodes, index);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

AniSwitchActivity::AniSwitchActivity() { aniswitchLog("== AniSwitchActivity ctor"); }

brls::View* AniSwitchActivity::createContentView() {
    aniswitchLog("== createContentView");
    try {
        brls::View* view = brls::View::createFromXMLResource("activity/aniswitch_activity.xml");
        aniswitchLog(view ? "xml ok" : "xml returned null");
        if (view) return view;
    } catch (const std::exception& e) {
        aniswitchLog(std::string("xml exception: ") + e.what());
        return buildFallbackView(e.what());
    } catch (...) {
        aniswitchLog("xml exception: unknown");
        return buildFallbackView("unknown exception while inflating xml");
    }
    return buildFallbackView("activity/aniswitch_activity.xml not found");
}

void AniSwitchActivity::onContentAvailable() {
    aniswitchLog("== onContentAvailable");
    brls::Activity::onContentAvailable();

    this->status = dynamic_cast<brls::Label*>(this->getView("aniswitch/status"));
    this->grid   = dynamic_cast<RecyclingGrid*>(this->getView("aniswitch/grid"));
    aniswitchLog(std::string("bind status=") + (this->status ? "ok" : "null") + " grid=" +
                 (this->grid ? "ok" : "null"));

    this->registerAction("返回", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    this->registerAction("刷新", brls::BUTTON_Y, [this](brls::View*) {
        aniswitchLog("action: refresh");
        this->loadEpisodes();
        return true;
    });

    if (!this->grid) {
        this->showMessage("列表组件 aniswitch/grid 未找到");
        return;
    }

    try {
        this->grid->registerCell("AniSwitchCell", []() { return new AniSwitchCell(); });
        this->grid->showSkeleton();
        aniswitchLog("grid ready");
    } catch (const std::exception& e) {
        aniswitchLog(std::string("grid exception: ") + e.what());
        this->showMessage(e.what());
        return;
    }

    this->showMessage("正在连接 AniSwitch Gateway ...");
    this->loadEpisodes();
}

void AniSwitchActivity::showMessage(const std::string& text) {
    aniswitchLog("msg: " + text);
    if (this->status) this->status->setText(text);
}

void AniSwitchActivity::loadEpisodes() {
    aniswitchLog("loadEpisodes: spawn thread");
    std::thread([this]() {
        const std::string base = ANISWITCH_GW_DEFAULT;
        const std::string url  = base + "/api/episodes";
        aniswitchLog("thread: GET " + url);

        std::vector<AniSwitchEpisode> list;
        std::string error;

        try {
            cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{30000},
                                       cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});
            aniswitchLog("thread: status=" + std::to_string(r.status_code) + " err=" + r.error.message +
                         " bytes=" + std::to_string(r.text.size()));

            if (r.status_code != 200) {
                error = "Gateway 返回 " + std::to_string(r.status_code);
                if (!r.error.message.empty()) error += " " + r.error.message;
            } else {
                auto j = nlohmann::json::parse(r.text);
                if (j.value("code", -1) != 0) {
                    error = "Gateway 错误: " + j.value("msg", std::string("unknown"));
                } else {
                    for (auto& item : j["episodes"]) {
                        AniSwitchEpisode e;
                        e.ep   = item.value("ep", 0);
                        e.name = item.value("name", std::string(""));
                        e.size = item.value("size", 0LL);
                        e.url  = item.value("url", std::string(""));
                        if (!e.url.empty()) list.push_back(std::move(e));
                    }
                }
            }
        } catch (const std::exception& e) {
            error = std::string("请求失败: ") + e.what();
        } catch (...) { error = "请求失败: unknown"; }

        aniswitchLog("thread: loaded " + std::to_string(list.size()) + " eps, error=" + error);

        if (!error.empty()) {
            // 连不上时做两次对照探测，帮助我们判断是端口问题还是整机没网
            std::string p1 = probeUrl(base + "/health");
            aniswitchLog("probe1: " + p1);
            std::string p2 = probeUrl("http://192.168.31.251:5244/");
            aniswitchLog("probe2: " + p2);
            error += "\n" + p1 + "\n" + p2;
        }

        aniswitchLog("thread: posting to main loop");
        brls::sync([this, list, error]() {
            aniswitchLog("sync enter, err_empty=" + std::to_string(error.empty()) +
                         " list=" + std::to_string(list.size()));
            try {
            if (!error.empty()) {
                this->showMessage(error);
                aniswitchLog("sync: setError next");
                if (this->grid) this->grid->setError(error);
                aniswitchLog("sync: setError done");
                return;
            }
            this->showMessage("共 " + std::to_string(list.size()) + " 集");
            if (!this->grid) return;
            aniswitchLog("sync: setDataSource next");
            this->grid->setDataSource(new AniSwitchDataSource(list));
            aniswitchLog("sync: notifyDataChanged next");
            this->grid->notifyDataChanged();
            aniswitchLog("sync: grid data set");
            } catch (const std::exception& e) {
                aniswitchLog(std::string("sync exception: ") + e.what());
            } catch (...) { aniswitchLog("sync exception: unknown"); }
        });
        aniswitchLog("thread: posted");
    }).detach();
}

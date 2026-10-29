//
// AniSwitch: OpenList/WebDAV 视频列表（数据来自 AniSwitch Gateway）
//
#include <cstdio>
#include <thread>

#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/view.hpp>
#include <borealis/views/box.hpp>
#include <borealis/views/label.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_activity.hpp"
#include "activity/aniswitch_player_activity.hpp"
#include "view/recycling_grid.hpp"

using namespace brls::literals;

namespace {
// 崩溃排查用：所有关键步骤都写到 SD 卡，便于无 crash report 时定位
void aniswitchLog(const std::string& text) {
    FILE* fp = fopen("sdmc:/aniswitch.log", "a");
    if (!fp) return;
    fputs(text.c_str(), fp);
    fputc('\n', fp);
    fclose(fp);
}

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

    if (!cell) return nullptr;
    if (index >= this->episodes.size()) return cell;

    const auto& e = this->episodes[index];
    char head[32];
    snprintf(head, sizeof(head), "EP%03d", e.ep);
    cell->setText(std::string(head) + "    " + e.name);
    return cell;
}

void AniSwitchDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (index >= this->episodes.size()) return;
    aniswitchLog("select ep index " + std::to_string(index));
    auto* activity = new AniSwitchPlayerActivity(this->episodes, index);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

AniSwitchActivity::AniSwitchActivity() { aniswitchLog("== AniSwitchActivity ctor"); }

brls::View* AniSwitchActivity::onCreateContentView() {
    aniswitchLog("== onCreateContentView");
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

    this->registerAction("返回", brls::BUTTON_B,
                         [](brls::View*) {
                             brls::Application::popActivity();
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
    std::thread([this]() {
        const std::string url = std::string(ANISWITCH_GW_DEFAULT) + "/api/episodes";
        aniswitchLog("GET " + url);

        std::vector<AniSwitchEpisode> list;
        std::string error;

        try {
            cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{30000},
                                       cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});

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

        aniswitchLog("loaded " + std::to_string(list.size()) + " eps, error=" + error);

        brls::sync([this, list, error]() {
            if (!error.empty()) {
                this->showMessage(error);
                if (this->grid) this->grid->setError(error);
                return;
            }
            this->showMessage("共 " + std::to_string(list.size()) + " 集");
            if (!this->grid) return;
            this->grid->setDataSource(new AniSwitchDataSource(list));
            this->grid->notifyDataChanged();
            aniswitchLog("grid data set");
        });
    }).detach();
}

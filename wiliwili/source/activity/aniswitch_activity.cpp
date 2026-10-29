//
// AniSwitch: OpenList/WebDAV 视频列表（数据来自 AniSwitch Gateway）
//

#include <thread>

#include <borealis/core/thread.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "activity/aniswitch_activity.hpp"
#include "activity/aniswitch_player_activity.hpp"
#include "view/recycling_grid.hpp"

using namespace brls::literals;

AniSwitchCell::AniSwitchCell() {
    this->setFocusable(true);
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setHeight(72);
    this->setMargins(4, 8, 4, 8);
    this->setPadding(6, 16, 6, 16);
    this->setCornerRadius(6);

    this->label = new brls::Label();
    this->label->setFontSize(22);
    this->label->setWidth(1000);
    this->label->setText("");
    this->addView(this->label);
}

void AniSwitchCell::setText(const std::string& text) { this->label->setText(text); }

void AniSwitchCell::prepareForReuse() { this->label->setText(""); }

void AniSwitchCell::cacheForReuse() {}

RecyclingGridItem* AniSwitchDataSource::cellForRow(RecyclingGrid* recycler, size_t index) {
    auto* cell = dynamic_cast<AniSwitchCell*>(recycler->dequeueReusableCell("AniSwitchCell"));

    if (index >= this->episodes.size()) return cell;

    const auto& e = this->episodes[index];
    char head[32];
    snprintf(head, sizeof(head), "EP%03d", e.ep);
    std::string text = std::string(head) + "    " + e.name;
    cell->setText(text);
    return cell;
}

void AniSwitchDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (index >= this->episodes.size()) return;
    auto* activity = new AniSwitchPlayerActivity(this->episodes, index);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void AniSwitchActivity::onContentAvailable() {
    brls::Logger::info("AniSwitch: activity content available");

    this->grid->registerCell("AniSwitchCell", []() { return new AniSwitchCell(); });
    this->grid->showSkeleton();
    this->status->setText("正在连接 AniSwitch Gateway ...");

    this->loadEpisodes();
}

void AniSwitchActivity::loadEpisodes() {
    std::thread([this]() {
        std::string url = std::string(ANISWITCH_GW_DEFAULT) + "/api/episodes";
        brls::Logger::info("AniSwitch: GET {}", url);

        std::vector<AniSwitchEpisode> list;
        std::string error;

        cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{30000},
                                   cpr::Header{{"User-Agent", "wiliwili-aniswitch"}});

        if (r.status_code != 200) {
            error = "Gateway 返回 " + std::to_string(r.status_code) + (r.error.message.empty() ? "" : " " + r.error.message);
        } else {
            try {
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
            } catch (const std::exception& ex) {
                error = std::string("解析失败: ") + ex.what();
            }
        }

        brls::sync([this, list, error]() {
            if (!error.empty()) {
                brls::Logger::error("AniSwitch: {}", error);
                this->grid->setError(error);
                this->status->setText(error);
                return;
            }
            brls::Logger::info("AniSwitch: {} episodes", list.size());
            this->status->setText("共 " + std::to_string(list.size()) + " 集");
            this->grid->setDataSource(new AniSwitchDataSource(list));
            this->grid->notifyDataChanged();
        });
    }).detach();
}

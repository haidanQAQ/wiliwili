//
// AniSwitch: WebDAV 目录浏览（PROPFIND 列目录 → 进目录 → 点视频播放）
//
// 和列表页一样：耗时请求走 brls::async，回主线程用 brls::sync。
// Switch 上不能用 std::thread（主线程创建即 abort 2162-0004）。
//

#include <cstdio>
#include <string>
#include <vector>

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/thread.hpp>

#include "activity/aniswitch_browse_activity.hpp"
#include "activity/aniswitch_list_activity.hpp"
#include "utils/activity_helper.hpp"
#include "utils/aniswitch_util.hpp"

namespace {

std::string humanSize(long long bytes) {
    if (bytes <= 0) return "";
    char buf[64];
    if (bytes >= 1024LL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1024.0 / 1024.0 / 1024.0);
    } else if (bytes >= 1024LL * 1024) {
        snprintf(buf, sizeof(buf), "%.0f MB", bytes / 1024.0 / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
    }
    return buf;
}

}  // namespace

AniSwitchDavCell::AniSwitchDavCell() {
    this->setFocusable(true);
    this->setHeight(88.0f);
    this->setWidthPercentage(100.0f);
    this->setPadding(10, 16, 10, 16);
    this->setAxis(brls::Axis::COLUMN);

    this->nameLabel = new brls::Label();
    this->nameLabel->setFontSize(22);
    this->nameLabel->setWidthPercentage(100.0f);
    this->addView(this->nameLabel);

    this->metaLabel = new brls::Label();
    this->metaLabel->setFontSize(16);
    this->metaLabel->setTextColor(nvgRGB(150, 150, 150));
    this->metaLabel->setWidthPercentage(100.0f);
    this->addView(this->metaLabel);
}

void AniSwitchDavCell::setEntry(const AniSwitchDavEntry& entry) {
    this->nameLabel->setText(entry.name);
    if (entry.isDir) {
        this->metaLabel->setText("目录");
    } else if (entry.size > 0) {
        this->metaLabel->setText(humanSize(entry.size));
    } else {
        this->metaLabel->setText("");
    }
}

RecyclingGridItem* AniSwitchDavDataSource::cellForRow(RecyclingGrid* recycler, size_t index) {
    auto* cell = static_cast<AniSwitchDavCell*>(recycler->dequeueReusableCell("AniSwitchDavCell"));
    if (!cell) cell = new AniSwitchDavCell();
    if (index < this->entries.size()) cell->setEntry(this->entries[index]);
    return cell;
}

void AniSwitchDavDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (!this->selectCallback) return;
    this->selectCallback(index);
}

AniSwitchBrowseActivity::AniSwitchBrowseActivity(AniSwitchSource src, std::string p)
    : source(std::move(src)), path(std::move(p)) {
    aniswitchLog("== browse ctor " + this->source.name + " path=" + this->path);
}

AniSwitchBrowseActivity::~AniSwitchBrowseActivity() {
    aniswitchLog("== browse dtor");
    this->alive->store(false);
}

brls::View* AniSwitchBrowseActivity::createContentView() {
    return brls::View::createFromXMLResource("activity/aniswitch_browse.xml");
}

void AniSwitchBrowseActivity::onContentAvailable() {
    aniswitchLog("== browse onContentAvailable");
    brls::Activity::onContentAvailable();

    this->status = dynamic_cast<brls::Label*>(this->getView("browse/status"));
    this->grid   = dynamic_cast<RecyclingGrid*>(this->getView("browse/grid"));

    if (!this->grid || !this->status) {
        aniswitchLog("browse: view missing");
        return;
    }

    this->grid->registerCell("AniSwitchDavCell", []() { return new AniSwitchDavCell(); });
    this->grid->showSkeleton();

    this->registerAction(
        "返回上一级", brls::ControllerButton::BUTTON_B,
        [this](brls::View*) -> bool {
            aniswitchLog("browse: B pressed -> up");
            this->goUp();
            return true;
        },
        false, false, brls::SOUND_BACK);

    this->load();
}

void AniSwitchBrowseActivity::willAppear(bool resetState) {
    aniswitchLog("== browse willAppear");
    brls::Activity::willAppear(resetState);
    brls::sync([this]() {
        if (this->grid) brls::Application::giveFocus(this->grid);
    });
}

void AniSwitchBrowseActivity::showStatus(const std::string& text) {
    if (this->status) this->status->setText(text);
}

void AniSwitchBrowseActivity::load() {
    this->showStatus(this->path);
    this->grid->showSkeleton();

    // 10 秒看门狗（用 brls::delay，不占用唯一的 async worker）
    const int token = this->ticket->fetch_add(1) + 1;
    aniswitchWatchdog(this->alive, this->ticket, token, 10000, [this]() {
        aniswitchLog("browse: timeout 10s");
        this->showStatus("连接超时（10 秒）：" + this->source.url);
        if (this->grid) this->grid->setError("连接超时");
        aniswitchErrorDialog("连接超时（10 秒，已取消进入）\n" + this->source.url, this->alive);
    });

    brls::async([this]() {
        const std::string target = this->path;
        aniswitchLog("browse: loading " + target);

        AniSwitchDavResult result = aniswitchDavList(this->source, target);

        brls::sync([this, result]() {
            this->ticket->fetch_add(1);  // 取消看门狗
            if (result.status <= 0) {
                std::string msg = "连接失败：" + result.error + "\n" + this->source.url;
                this->showStatus(msg);
                if (this->grid) this->grid->setError(result.error);
                aniswitchErrorDialog(msg, this->alive);
                return;
            }
            if (result.status != 207 && result.status != 200) {
                std::string msg =
                    "HTTP " + std::to_string(result.status) + "：" + result.error + "\n" + this->source.url;
                this->showStatus(msg);
                if (this->grid) this->grid->setError(result.error);
                aniswitchErrorDialog(msg, this->alive);
                return;
            }

            this->entries = result.entries;

            auto* ds = new AniSwitchDavDataSource(this->entries);
            ds->setSelectCallback([this](size_t index) { this->enter(index); });
            this->grid->setDataSource(ds);
            this->grid->notifyDataChanged();
            brls::Application::giveFocus(this->grid);

            char tip[320];
            snprintf(tip, sizeof(tip), "%s  ·  %zu 项  ·  B 返回", this->path.c_str(),
                     this->entries.size());
            this->showStatus(tip);
            aniswitchLog("browse: grid filled " + std::to_string(this->entries.size()));
        });
    });
}

void AniSwitchBrowseActivity::enter(size_t index) {
    if (index >= this->entries.size()) return;
    const AniSwitchDavEntry entry = this->entries[index];

    if (entry.isDir) {
        this->history.push_back(this->path);
        this->path = entry.path;
        aniswitchLog("browse: enter dir " + this->path);
        this->load();
        return;
    }

    this->playVideo(index);
}

void AniSwitchBrowseActivity::goUp() {
    if (this->history.empty()) {
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        return;
    }
    this->path = this->history.back();
    this->history.pop_back();
    aniswitchLog("browse: go up to " + this->path);
    this->load();
}

void AniSwitchBrowseActivity::playVideo(size_t index) {
    // 把当前目录里的视频拼成播放列表，方便自动下一集
    std::vector<AniSwitchEpisode> episodes;
    size_t playIndex = 0;

    for (size_t i = 0; i < this->entries.size(); ++i) {
        const auto& e = this->entries[i];
        if (e.isDir || !aniswitchIsVideoFile(e.name)) continue;

        AniSwitchEpisode ep;
        int guess = aniswitchGuessEpisode(e.name);
        ep.ep     = guess > 0 ? guess : static_cast<int>(episodes.size() + 1);
        ep.name   = e.name;
        ep.size   = e.size;
        ep.url    = aniswitchDavFileUrl(this->source, e);
        if (i == index) playIndex = episodes.size();
        episodes.push_back(std::move(ep));
    }

    if (episodes.empty()) {
        this->showStatus("这个文件不是可播放的视频");
        return;
    }

    aniswitchLog("browse: play " + std::to_string(episodes.size()) + " files, index=" +
                 std::to_string(playIndex) + " dir=" + this->path);
    Intent::openAniSwitchPlayer(episodes, playIndex, this->path);
}

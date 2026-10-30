#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/views/label.hpp>

#include "activity/aniswitch_sources.hpp"
#include "activity/aniswitch_webdav.hpp"
#include "view/recycling_grid.hpp"

// AniSwitch: WebDAV 目录浏览
//   A ：进入目录 / 播放视频
//   B ：返回上一级（已在根目录则退出到源列表）
class AniSwitchDavCell : public RecyclingGridItem {
public:
    AniSwitchDavCell();
    ~AniSwitchDavCell() override = default;

    void setEntry(const AniSwitchDavEntry& entry);

private:
    brls::Label* nameLabel = nullptr;
    brls::Label* metaLabel = nullptr;
};

class AniSwitchDavDataSource : public RecyclingGridDataSource {
public:
    explicit AniSwitchDavDataSource(std::vector<AniSwitchDavEntry> entries)
        : entries(std::move(entries)) {}

    size_t getItemCount() override { return this->entries.size(); }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override;

    void onItemSelected(RecyclingGrid* recycler, size_t index) override;

    void clearData() override { this->entries.clear(); }

    void setSelectCallback(std::function<void(size_t)> callback) {
        this->selectCallback = std::move(callback);
    }

private:
    std::vector<AniSwitchDavEntry> entries;
    std::function<void(size_t)> selectCallback;
};

class AniSwitchBrowseActivity : public brls::Activity {
public:
    AniSwitchBrowseActivity(AniSwitchSource source, std::string path);
    ~AniSwitchBrowseActivity() override;

    brls::View* createContentView() override;

    void onContentAvailable() override;

    void willAppear(bool resetState = false) override;

private:
    AniSwitchSource source;
    std::string path;
    std::vector<std::string> history;
    std::vector<AniSwitchDavEntry> entries;

    std::shared_ptr<std::atomic<bool>> alive  = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::atomic<int>> ticket = std::make_shared<std::atomic<int>>(0);

    brls::Label* status = nullptr;
    RecyclingGrid* grid = nullptr;

    void load();
    void enter(size_t index);
    void goUp();
    void showStatus(const std::string& text);
    void playVideo(size_t index);
};

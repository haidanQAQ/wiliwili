//
// Created by fang on 2023/4/17.
//

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <cstdlib>
#include <exception>
#include "utils/aniswitch_util.hpp"
#include "activity/player_activity.hpp"
#include "activity/live_player_activity.hpp"
#include "activity/hint_activity.hpp"
#include "activity/setting_activity.hpp"
#include "activity/search_activity.hpp"
#include "activity/search_activity_tv.hpp"
#include "activity/pgc_index_activity.hpp"
#include "activity/main_activity.hpp"
#include "activity/gallery_activity.hpp"
#include "activity/dlna_activity.hpp"
#include "activity/aniswitch_activity.hpp"
#include "activity/aniswitch_list_activity.hpp"
#include "activity/aniswitch_browse_activity.hpp"
#include "activity/aniswitch_sources.hpp"
#include "activity/aniswitch_player_activity.hpp"
#include "activity/dynamic_activity.hpp"
#include "fragment/mine_collection_video_list.hpp"
#include "fragment/inbox_view.hpp"
#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"

#include "presenter/video_detail.hpp"

void Intent::openAV(const std::string& avid, uint64_t cid, int progress) {
    // av to bv
    // Based on https://github.com/bolanxian/metadata-fetcher/blob/master/src/utils/bv-encode.ts
    constexpr int BASE = 58;
    constexpr int64_t MAX = 1LL << 51;
    constexpr int64_t XOR = 0x1552356C4CDB;
    const std::string table = "FcwAPNKTMug3GV5Lj7EJnHpWsx4tb8haYeviqBz6rkCy12mUSDQX9RdoZf";
    int64_t tmp = std::stoll(avid, nullptr, 10);
    if (tmp < 0 || tmp >= MAX)
        return;
    tmp = (tmp | MAX) ^ XOR;
    std::string x = "0000000000";
    size_t i = 0;
    while (i < x.length()) {
        x[i++] = table[tmp % BASE];
        tmp /= BASE;
    }
    if (tmp > 0)
        return;
    std::string bvid = "BV1000000000";
    const int map[] = {2, 4, 6, 5, 7, 3, 8, 1, 0};
    for (size_t j = 0; j < 9; j++) {
        bvid[j + 3] = x[map[j]];
    }
    openBV(bvid, cid, progress);
}

void Intent::openBV(const std::string& bvid, uint64_t cid, int progress) {
    auto activity = new PlayerActivity(bvid, cid, progress);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openSeasonBySeasonId(uint64_t seasonId, int progress) {
    auto activity = new PlayerSeasonActivity(seasonId, PGC_ID_TYPE::SEASON_ID, progress);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openSeasonByEpId(uint64_t epId, int progress) {
    auto activity = new PlayerSeasonActivity(epId, PGC_ID_TYPE::EP_ID, progress);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openLive(int live, const std::string& name, const std::string& views) {
    auto activity = new LiveActivity(live, name, views);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openCollection(const std::string& mid, const std::string& type) {
    auto collection = new MineCollectionVideoList();
    collection->applyXMLAttribute("type", type);
    collection->applyXMLAttribute("collection", mid);
    auto activity = new brls::Activity(collection);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openSearch(const std::string& key) {
    if (key.empty()) return;
    auto activity = new SearchActivity(key);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openTVSearch() {
    auto activity = new TVSearchActivity();
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openPgcFilter(const std::string& filter) {
    auto activity = new PGCIndexActivity(filter);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openSetting() {
    auto activity = new SettingActivity();
    brls::Application::pushActivity(activity);
}

void Intent::openInbox() {
    auto inbox = new InboxView();
    brls::Application::pushActivity(new brls::Activity(inbox));
}

void Intent::openHint() { brls::Application::pushActivity(new HintActivity()); }

void Intent::openMain() {
    auto activity = new MainActivity();
    brls::Application::pushActivity(activity);
}

void Intent::openGallery(const std::vector<std::string>& data) {
    auto activity = new GalleryActivity(data);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openDLNA() {
    auto activity = new DLNAActivity();
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

void Intent::openAniSwitch() {
    aniswitchCleanupBackups();
    aniswitchLog("openAniSwitch: begin");
    std::set_terminate([]() {
        aniswitchLog("!!! std::terminate called");
        std::exception_ptr ep = std::current_exception();
        if (!ep) {
            aniswitchLog("!!! terminate: no current exception");
        } else {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& ex) {
                aniswitchLog(std::string("!!! terminate what(): ") + ex.what());
            } catch (...) {
                aniswitchLog("!!! terminate: non-std exception");
            }
        }
        fflush(nullptr);
        std::abort();
    });
    try {
        auto activity = new AniSwitchActivity();
        aniswitchLog("openAniSwitch: activity created");
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        aniswitchLog("openAniSwitch: pushActivity returned normally");
    } catch (const std::exception& e) {
        aniswitchLog(std::string("openAniSwitch: push exception: ") + e.what());
    } catch (...) {
        aniswitchLog("openAniSwitch: push exception: unknown");
    }
    aniswitchLog("openAniSwitch: end");
}

// pushActivity 内部会先 blockInputs() 再 createContentView()，一旦后者抛异常
// inputs 就永远锁死（画面还在、手柄全失灵）。这里统一兜底：解锁 + 收拾现场 + 弹提示。
static void aniswitchPushFailed(const std::string& where, const std::string& what) {
    aniswitchLog("pushFailed: " + where + " : " + what);
    brls::Application::unblockInputs();
    brls::sync([where, what]() {
        brls::Dialog* tip =
            new brls::Dialog("无法打开界面\n" + where + "\n" + (what.empty() ? "未知错误" : what));
        tip->addButton("确定", []() {});
        tip->open();
    });
}

void Intent::openAniSwitchList() {
    aniswitchLog("openAniSwitchList: begin");
    AniSwitchListActivity* activity = nullptr;
    bool pushed                     = false;
    try {
        activity = new AniSwitchListActivity();
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        pushed = true;
        aniswitchLog("openAniSwitchList: pushed");
    } catch (const std::exception& e) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchList", e.what());
    } catch (...) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchList", "unknown");
    }
}

void Intent::openAniSwitchPlayer(const std::vector<AniSwitchEpisode>& episodes, size_t index,
                                 const std::string& dirPath) {
    aniswitchLog("openAniSwitchPlayer: index=" + std::to_string(index) + " total=" +
                 std::to_string(episodes.size()) + " dir=" + dirPath);
    AniSwitchPlayerActivity* activity = nullptr;
    bool pushed                       = false;
    try {
        activity = new AniSwitchPlayerActivity(episodes, index, dirPath);
        aniswitchLog("openAniSwitchPlayer: activity created");
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        pushed = true;
        aniswitchLog("openAniSwitchPlayer: pushed");
    } catch (const std::exception& e) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchPlayer", e.what());
    } catch (...) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchPlayer", "unknown");
    }
}


void Intent::openAniSwitchBrowse(const AniSwitchSource& source, const std::string& path) {
    aniswitchLog("openAniSwitchBrowse: " + source.name + " path=" + path);
    AniSwitchBrowseActivity* activity = nullptr;
    bool pushed                       = false;
    try {
        activity = new AniSwitchBrowseActivity(source, path);
        aniswitchLog("openAniSwitchBrowse: activity created");
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        pushed = true;
        aniswitchLog("openAniSwitchBrowse: pushed");
    } catch (const std::exception& e) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchBrowse", e.what());
    } catch (...) {
        if (!pushed) delete activity;
        aniswitchPushFailed("openAniSwitchBrowse", "unknown");
    }
}
void Intent::openActivity(const std::string& id) {
    auto activity = new DynamicActivity(id);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

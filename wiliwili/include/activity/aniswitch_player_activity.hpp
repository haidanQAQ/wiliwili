#pragma once

#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>

#include "activity/aniswitch_list_activity.hpp"
#include "utils/event_helper.hpp"

class VideoView;

// 直接播放任意 HTTP 地址（OpenList 直链），并加载 AniSwitch Gateway 合并后的弹幕
class AniSwitchPlayerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/video_activity.xml");

    AniSwitchPlayerActivity(std::vector<AniSwitchEpisode> list, size_t index,
                            std::string dirPath = std::string());

    void onContentAvailable() override;

    void willAppear(bool resetState = false) override;

    void willDisappear(bool resetState = false) override;

    ~AniSwitchPlayerActivity() override;

private:
    BRLS_BIND(VideoView, video, "video");

    std::vector<AniSwitchEpisode> episodes;
    size_t index = 0;
    // 当前番剧所在目录（WebDAV 绝对路径），网关据此挑选弹幕规则
    std::string dirPath;
    bool mpvSubscribed = false;
    bool stopped       = false;
    MPVEvent::Subscription mpvEventSubscribeID;

    void playCurrent();
    void playNext();
    void loadDanmaku(int ep);
    // 退出播放器时把 mpv 彻底停掉，否则声音会留在后台、之后的视频也播不出来
    void stopPlayback();
};

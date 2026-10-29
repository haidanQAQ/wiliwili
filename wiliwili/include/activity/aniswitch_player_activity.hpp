#pragma once

#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>

#include "activity/aniswitch_activity.hpp"
#include "utils/event_helper.hpp"

class VideoView;

// 直接播放任意 HTTP 地址（OpenList 直链），并加载 AniSwitch Gateway 合并后的弹幕
class AniSwitchPlayerActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/video_activity.xml");

    AniSwitchPlayerActivity(std::vector<AniSwitchEpisode> list, size_t index);

    void onContentAvailable() override;

    ~AniSwitchPlayerActivity() override;

private:
    BRLS_BIND(VideoView, video, "video");

    std::vector<AniSwitchEpisode> episodes;
    size_t index = 0;
    MPVEvent::Subscription mpvEventSubscribeID;

    void playCurrent();
    void playNext();
    void loadDanmaku(int ep);
};

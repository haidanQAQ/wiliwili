#pragma once

//
// AniSwitch: WebDAV 源配置的读写
//
// 存放位置：sdmc:/config/wiliwili/aniswitch_sources.json
// 结构：
// {
//   "sources": [
//     {"name": "内置网关", "url": "http://192.168.1.10:9333", "user": "", "pass": "", "builtin": true},
//     {"name": "我的 OpenList", "url": "http://192.168.1.10:5244/dav", "user": "<用户名>", "pass": "<口令>", "builtin": false}
//   ]
// }
//

#include <string>
#include <vector>

// 内置网关的默认地址：编译期可用 -DANISWITCH_GW_DEFAULT="http://ip:9333" 覆盖，
// 运行期也可以在 sd:/config/wiliwili/aniswitch_sources.json 里改。
#ifndef ANISWITCH_GW_DEFAULT
#define ANISWITCH_GW_DEFAULT "http://192.168.1.10:9333"
#endif

struct AniSwitchSource {
    std::string name;
    std::string url;   // WebDAV 根地址；内置网关则是 http://ip:9333
    std::string user;
    std::string pass;
    bool builtin = false;
};

namespace aniswitch {

std::string sourcesPath();

std::vector<AniSwitchSource> loadSources();

void saveSources(const std::vector<AniSwitchSource>& sources);

// 弹幕网关地址：优先取源列表里 builtin=true 那条的 url，没有则用 ANISWITCH_GW_DEFAULT。
// 播放器拉弹幕时用它，这样换网关不用重编。
const std::string& gatewayBase();
void setGatewayBase(const std::string& url);

// 默认列表：内置网关（火影 4K）+ 用户本地 OpenList 的一个常见默认值留空
std::vector<AniSwitchSource> defaultSources();

}  // namespace aniswitch

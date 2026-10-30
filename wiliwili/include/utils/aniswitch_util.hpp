#pragma once

// AniSwitch 排查日志：直接写 SD 卡，便于在没有 crash report 的情况下定位问题。
// 注意：日志可能在多个线程里同时写（主线程 / 工作线程 / 心跳线程）。
// newlib 的 stdio 在 sdmc devoptab 上不是线程安全的，并发 fopen/fwrite 会造成堆损坏，
// 最终表现为没有任何异常的 std::terminate / abort（Switch 上错误码 2162-0004）。
// 因此这里用全局互斥锁把写日志串行化。
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include <ctime>
#include <dirent.h>
#include <mutex>
#include <string>
#include <unistd.h>

inline void aniswitchLog(const std::string& text) {
    static std::mutex logMutex;
    std::lock_guard<std::mutex> guard(logMutex);

    FILE* fp = fopen("sdmc:/aniswitch.log", "a");
    if (!fp) return;

    std::time_t now = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&now, &tmv);
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d ", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    fputs(stamp, fp);
    fputs(text.c_str(), fp);
    fputc('\n', fp);
    fflush(fp);
    fclose(fp);
}

// 清理装机时留下的旧版本文件（wiliwili_prev*.nro / .bak）。
// hbmenu 会把目录下所有 .nro 都列出来，旧版本留在那里会导致 hbmenu 报错，
// 所以更新脚本先把旧版改名成 .bak，程序启动后再把它删掉。
inline void aniswitchCleanupBackups() {
    const char* dirs[] = {"sdmc:/switch/AniSwitch", "sdmc:/switch/wiliwili"};
    std::string victims[16];
    int count = 0;

    for (const char* dir : dirs) {
        DIR* dp = opendir(dir);
        if (!dp) continue;
        struct dirent* entry;
        while ((entry = readdir(dp)) != nullptr && count < 16) {
            std::string name = entry->d_name;
            if (name.rfind("wiliwili_prev", 0) != 0 && name.rfind("AniSwitch_prev", 0) != 0) continue;
            victims[count++] = std::string(dir) + "/" + name;
        }
        closedir(dp);
    }

    for (int i = 0; i < count; i++) {
        if (remove(victims[i].c_str()) == 0) {
            aniswitchLog("cleanup: removed " + victims[i]);
        }
    }
}

// 规范化用户输入的地址：
//   - 去掉首尾空白
//   - 没有 http:// / https:// 前缀时自动补 http://
//   - 去掉末尾多余的 /
inline std::string aniswitchNormalizeUrl(std::string url) {
    size_t b = url.find_first_not_of(" \t\r\n");
    size_t e = url.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    url = url.substr(b, e - b + 1);

    std::string lower = url;
    for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (lower.rfind("http://", 0) != 0 && lower.rfind("https://", 0) != 0) {
        url = "http://" + url;
    }
    while (url.size() > 8 && url.back() == '/') url.pop_back();
    return url;
}

// 校验地址补全后的形态，返回空字符串表示合法
inline std::string aniswitchCheckUrl(const std::string& url) {
    if (url.empty()) return "地址不能为空";
    size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "地址缺少 http:// 前缀";
    size_t hostStart = scheme + 3;
    if (hostStart >= url.size() || url[hostStart] == '/') return "地址缺少主机名";
    return "";
}

// URL 查询参数用百分号编码（UTF-8 字节逐个转 %XX，保留 unreserved 字符）
inline std::string aniswitchUrlEncode(const std::string& in) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(in.size() * 3);
    for (unsigned char c : in) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

// 「进入源」的超时看门狗。
//
// 血泪教训：borealis 的 brls::async 只有 **一个** worker 线程
// （library/borealis/library/lib/core/thread.cpp 里就一个 task_loop_thread，
//  m_async_tasks 是串行消费的），所以绝不能用 `brls::async` + sleep 循环做计时器
// —— 那样等于把唯一的网络线程霸占 10 秒，真正该发的请求排在后面，
// 表现就是「点了没反应，10 秒后报连接超时，然后请求才成功」。
// brls::delay() 是主线程 performSyncTasks() 里处理的定时任务，不占 worker，正合适。
inline void aniswitchWatchdog(std::shared_ptr<std::atomic<bool>> alive,
                             std::shared_ptr<std::atomic<int>> ticket, int expect, long ms,
                             std::function<void()> onTimeout) {
    brls::delay(ms, [alive, ticket, expect, onTimeout]() {
        if (!alive->load() || ticket->load() != expect) return;
        onTimeout();
    });
}

// 弹一个错误提示，用户点「确定」后延时 300ms 再退出当前界面。
// 不能在同一次回调里先 pop 再 open 新窗口：Application::blockInputs() 是配对调用的，
// 顺序一乱就会把输入永久锁死（表现是「提示框点不了确认，按 Home 报软件已关闭」）。
inline void aniswitchErrorDialog(const std::string& msg, std::shared_ptr<std::atomic<bool>> alive) {
    brls::Dialog* tip = new brls::Dialog(msg);
    tip->addButton("确定", [alive]() {
        brls::delay(300, [alive]() {
            if (!alive->load()) return;
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
        });
    });
    tip->open();
}

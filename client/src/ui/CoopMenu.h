// F9 co-op menu. Discreet overlay drawn with text/rect natives; navigation
// via Win32 keyboard polling (F9 open/close, arrows, Enter, Backspace).
// Never touches the game HUD.
#pragma once

#include "../../shared/coop/Types.h"

#include <string>
#include <vector>

namespace coop {

class CoopContext;

class CoopMenu {
public:
    void Tick(CoopContext& ctx, u64 nowMs);

    // Keyboard hook (SHV thread): toggles open state. F9.
    void OnKey(u32 vk);

    // Chat feed.
    void PushChat(u32 clientId, const std::string& text);

private:
    enum class Screen : u8 {
        MAIN = 0,
        CONNECT,
        HOST,
        PLAYERS,
        SKINS,
        SAVES,
        SETTINGS,
        MISSION,
    };

    void DrawPanel(f32 cx, f32 cy, f32 w, f32 h) const;
    void DrawTextLine(const std::string& text, f32 x, f32 y, f32 scale,
                      u32 r, u32 g, u32 b, u32 a, bool center) const;
    void DrawStatusLine(CoopContext& ctx) const;

    void NavInput(CoopContext& ctx);
    void DrawMain(CoopContext& ctx);
    void DrawConnect(CoopContext& ctx);
    void DrawPlayers(CoopContext& ctx);
    void DrawSkins(CoopContext& ctx);
    void DrawSaves(CoopContext& ctx);
    void DrawSettings(CoopContext& ctx);
    void DrawMission(CoopContext& ctx);

    Screen screen_ = Screen::MAIN;
    bool open_ = false;
    i32 selected_ = 0;
    u64 lastNavMs_ = 0;

    // Connect screen buffer (digit editing via up/down).
    std::string ipBuffer_ = "127.0.0.1";
    std::string nameBuffer_ = "Player";
    i32 connectField_ = 0;
    i32 portBuffer_ = kDefaultTcpPort;

    // Chat.
    struct ChatLine { u32 clientId; std::string text; };
    std::vector<ChatLine> chat_;
    u64 lastChatMs_ = 0;
};

} // namespace coop

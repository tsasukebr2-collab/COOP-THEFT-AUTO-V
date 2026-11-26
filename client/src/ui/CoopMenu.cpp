#include "CoopMenu.h"
#include "../CoopContext.h"
#include "../natives/Natives.h"

#include <windows.h>
#include <cstring>
#include <cstdio>

namespace coop {

namespace {

bool KeyPressedEdge(bool& wasDown, u32 vk) {
    bool down = (GetAsyncKeyState(int(vk)) & 0x8000) != 0;
    bool edge = down && !wasDown;
    wasDown = down;
    return edge;
}
} // namespace

void CoopMenu::OnKey(u32 vk) {
    if (vk == VK_F9) open_ = !open_;
}

void CoopMenu::PushChat(u32 clientId, const std::string& text) {
    chat_.push_back({ clientId, text });
    if (chat_.size() > 8) chat_.erase(chat_.begin());
    lastChatMs_ = GetTickCount64();
}

void CoopMenu::DrawPanel(f32 cx, f32 cy, f32 w, f32 h) const {
    // background
    DRAW_RECT(cx, cy, w, h, 0, 0, 0, 200, 0);
    // top accent
    DRAW_RECT(cx, cy - h / 2.f + 0.008f, w, 0.016f, 255, 140, 0, 255, 0);
}

void CoopMenu::DrawTextLine(const std::string& text, f32 x, f32 y, f32 scale,
                            u32 r, u32 g, u32 b, u32 a, bool center) const {
    SET_TEXT_FONT(0);
    SET_TEXT_SCALE(scale, scale);
    SET_TEXT_COLOUR(int(r), int(g), int(b), int(a));
    SET_TEXT_OUTLINE();
    SET_TEXT_CENTRE(center ? 1 : 0);
    BEGIN_TEXT_COMMAND_DISPLAY_TEXT("STRING");
    ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(text.c_str());
    END_TEXT_COMMAND_DISPLAY_TEXT(x, y, 0);
}

void CoopMenu::Tick(CoopContext& ctx, u64 nowMs) {
    // Navigation polling happens only while open (also allow F9 toggle via
    // SHV keyboard hook registered in dllmain).
    if (open_) {
        static bool wUp=false, wDown=false, wEnter=false, wBack=false, wLeft=false, wRight=false;
        bool up    = KeyPressedEdge(wUp, VK_UP);
        bool down  = KeyPressedEdge(wDown, VK_DOWN);
        bool enter = KeyPressedEdge(wEnter, VK_RETURN);
        bool back  = KeyPressedEdge(wBack, VK_BACK);
        bool left  = KeyPressedEdge(wLeft, VK_LEFT);
        bool right = KeyPressedEdge(wRight, VK_RIGHT);

        if (up)   { if (--selected_ < 0) selected_ = 0; }
        if (down) {
            i32 maxIdx;
            switch (screen_) {
                case Screen::MAIN:   maxIdx = 6; break;
                case Screen::SKINS:  maxIdx = i32(ctx.Skin().Catalog().size()); break;
                case Screen::CONNECT:maxIdx = 2; break;
                default:             maxIdx = 5; break;
            }
            if (++selected_ > maxIdx) selected_ = maxIdx;
        }
        if (back) { screen_ = Screen::MAIN; selected_ = 0; }

        if (screen_ == Screen::MAIN) {
            if (enter) {
                switch (selected_) {
                    case 0: screen_ = Screen::CONNECT; selected_ = 0; break;
                    case 1: ctx.StartSession(ipBuffer_, u16(portBuffer_), nameBuffer_); break;
                    case 2: screen_ = Screen::PLAYERS; selected_ = 0; break;
                    case 3: screen_ = Screen::SKINS; selected_ = 0; break;
                    case 4: screen_ = Screen::SAVES; selected_ = 0; break;
                    case 5: screen_ = Screen::SETTINGS; selected_ = 0; break;
                    case 6: open_ = false; break;
                }
            }
        } else if (screen_ == Screen::CONNECT) {
            if (left)  { if (connectField_ > 0) --connectField_; }
            if (right) { if (connectField_ < 2) ++connectField_; }
            if (up || down) {
                i32 dir = up ? 1 : -1;
                if (connectField_ == 2) {
                    portBuffer_ += dir * 10;
                    if (portBuffer_ < 1) portBuffer_ = 1;
                    if (portBuffer_ > 65535) portBuffer_ = 65535;
                }
            }
            if (enter && ctx.InSession()) { screen_ = Screen::MAIN; }
        } else if (screen_ == Screen::SKINS) {
            if (enter) {
                if (selected_ == 0) {
                    ctx.Skin().RequestOriginal(ctx.LocalSlot());
                } else {
                    const auto& cat = ctx.Skin().Catalog();
                    if (selected_ - 1 < i32(cat.size()))
                        ctx.Skin().RequestLocal(cat[size_t(selected_ - 1)].model);
                }
            }
        }

        DrawMain(ctx);
        switch (screen_) {
            case Screen::CONNECT: DrawConnect(ctx); break;
            case Screen::PLAYERS: DrawPlayers(ctx); break;
            case Screen::SKINS:   DrawSkins(ctx); break;
            case Screen::SAVES:   DrawSaves(ctx); break;
            case Screen::SETTINGS: DrawSettings(ctx); break;
            default: break;
        }
    }

    // Connection status chip (always visible while in session).
    if (ctx.InSession()) DrawStatusLine(ctx);

    // Chat feed (fades after 12 s).
    if (!chat_.empty() && GetTickCount64() - lastChatMs_ > 12000) chat_.clear();
    f32 y = 0.72f;
    for (const auto& line : chat_) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[%u] %s", line.clientId, line.text.c_str());
        DrawTextLine(buf, 0.012f, y, 0.28f, 255, 255, 255, 255, false);
        y += 0.032f;
    }
}

void CoopMenu::DrawMain(CoopContext& ctx) {
    DrawPanel(0.86f, 0.5f, 0.24f, 0.48f);
    DrawTextLine("GTAV CO-OP", 0.86f, 0.30f, 0.42f, 255, 140, 0, 255, true);

    const char* items[] = {
        "Connect to server",
        ctx.InSession() ? "Disconnect" : "Quick connect",
        "Players",
        "Skins (NPC models)",
        "Saves",
        "Settings",
        "Close (F9)",
    };
    f32 y = 0.375f;
    for (i32 i = 0; i < 7; ++i) {
        u32 col = (i == selected_) ? 255 : 160;
        std::string label = items[i];
        if (i == selected_) label = "> " + label;
        DrawTextLine(label, 0.76f, y, 0.30f, col, col, col, 255, false);
        y += 0.045f;
    }
}

void CoopMenu::DrawConnect(CoopContext& ctx) {
    DrawPanel(0.5f, 0.5f, 0.34f, 0.30f);
    DrawTextLine("CONNECT", 0.5f, 0.37f, 0.4f, 255, 140, 0, 255, true);

    const char* fields[3] = { "IP", "Port", "Name" };
    f32 y = 0.44f;
    for (i32 i = 0; i < 3; ++i) {
        char buf[160];
        const char* val = "";
        if (i == 0) val = ipBuffer_.c_str();
        else if (i == 1) {
            static char portbuf[16];
            std::snprintf(portbuf, sizeof(portbuf), "%d", portBuffer_);
            val = portbuf;
        } else val = nameBuffer_.c_str();
        std::snprintf(buf, sizeof(buf), "%s %s: %s",
                      connectField_ == i ? ">" : " ", fields[i], val);
        DrawTextLine(buf, 0.5f, y, 0.32f,
                     connectField_ == i ? 255 : 160,
                     connectField_ == i ? 255 : 160,
                     connectField_ == i ? 255 : 160, 255, true);
        y += 0.06f;
    }
    DrawTextLine(ctx.InSession() ? "IN SESSION" : "LEFT/RIGHT = field  UP/DOWN = edit",
                 0.5f, 0.64f, 0.26f, 150, 150, 150, 255, true);
}

void CoopMenu::DrawPlayers(CoopContext& ctx) {
    (void)ctx;
    DrawPanel(0.5f, 0.5f, 0.30f, 0.40f);
    DrawTextLine("PLAYERS", 0.5f, 0.33f, 0.4f, 255, 140, 0, 255, true);
    // Status + roster rendering is driven by CoopContext state; v1 shows
    // session summary from the net layer.
    DrawTextLine("(session roster)", 0.5f, 0.5f, 0.3f, 200, 200, 200, 255, true);
}

void CoopMenu::DrawSkins(CoopContext& ctx) {
    const auto& catalog = ctx.Skin().Catalog();
    const i32 total = i32(catalog.size()) + 1; // +1 = "Original"
    const std::string& current = ctx.Skin().CurrentModel();

    DrawPanel(0.5f, 0.5f, 0.40f, 0.62f);
    DrawTextLine("SKINS - NPC MODELS", 0.5f, 0.235f, 0.40f, 255, 140, 0, 255, true);

    // Scroll window so the full catalog fits any screen.
    constexpr i32 kWindow = 9;
    i32 first = 0;
    if (selected_ >= kWindow) first = selected_ - kWindow + 1;
    i32 last = first + kWindow;
    if (last > total) { last = total; first = last - kWindow; if (first < 0) first = 0; }

    f32 y = 0.31f;
    for (i32 i = first; i < last; ++i) {
        std::string label;
        char buf[128];
        if (i == 0) {
            label = "Original (protagonist)";
            if (current.empty()) label = "* " + label;
        } else {
            const auto& e = catalog[size_t(i - 1)];
            std::snprintf(buf, sizeof(buf), "%s  [%s]", e.label, e.category);
            label = buf;
            if (current == e.model) label = "* " + label;
        }
        if (i == selected_) label = "> " + label;
        u32 col = (i == selected_) ? 255 : 170;
        DrawTextLine(label, 0.315f, y, 0.30f, col, col, col, 255, false);
        y += 0.042f;
    }

    // Scroll indicator + status + help.
    DrawTextLine(std::to_string(selected_ + 1) + "/" + std::to_string(total),
                 0.66f, 0.31f, 0.26f, 150, 150, 150, 255, false);
    DrawTextLine(ctx.Skin().StatusText(), 0.5f, 0.735f, 0.27f, 220, 220, 220, 255, true);
    DrawTextLine("ENTER = apply   BACKSPACE = back", 0.5f, 0.775f, 0.24f,
                 150, 150, 150, 255, true);
}

void CoopMenu::DrawSaves(CoopContext&) {
    DrawPanel(0.5f, 0.5f, 0.30f, 0.30f);
    DrawTextLine("SHARED SAVE", 0.5f, 0.37f, 0.4f, 255, 140, 0, 255, true);
    DrawTextLine("Autosave runs automatically.", 0.5f, 0.46f, 0.28f, 200, 200, 200, 255, true);
    DrawTextLine("Host pushes after missions.", 0.5f, 0.50f, 0.28f, 200, 200, 200, 255, true);
}

void CoopMenu::DrawSettings(CoopContext&) {
    DrawPanel(0.5f, 0.5f, 0.30f, 0.30f);
    DrawTextLine("SETTINGS", 0.5f, 0.37f, 0.4f, 255, 140, 0, 255, true);
    DrawTextLine("Edit coop_client.ini", 0.5f, 0.48f, 0.28f, 200, 200, 200, 255, true);
}

void CoopMenu::DrawStatusLine(CoopContext& ctx) const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "CO-OP ping %ums", ctx.MenuPing());
    DrawTextLine(buf, 0.012f, 0.012f, 0.25f, 255, 140, 0, 220, false);
}

} // namespace coop

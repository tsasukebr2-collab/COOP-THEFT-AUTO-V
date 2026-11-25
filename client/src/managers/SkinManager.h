// Ped skin selection (NPC models as player skins). PRIORITY menu feature.
//
// Model choice comes from a curated catalog of GAME ASSET NAMES (the same
// public names every SP trainer and the natives documentation use). No hash
// is hardcoded here: every name is resolved at runtime through GET_HASH_KEY,
// and every request is validated with IS_MODEL_IN_CDIMAGE + IS_MODEL_VALID
// before streaming, so a bad name degrades into a visible menu message
// instead of a crash or a wrong-model guess.
//
// Application is non-blocking: REQUEST_MODEL is issued once and Tick polls
// HAS_MODEL_LOADED each frame (the SHV script thread must never busy-wait).
// On load: SET_PLAYER_MODEL + default component variation. The caller
// (CoopContext) re-applies saved weapons afterwards and broadcasts
// MSG_PLAYER_MODEL so remote peers recreate the ped with the new skin.
//
// "Original" restores the current protagonist (player_zero/player_one/
// player_two are public model names, resolved via GET_HASH_KEY too).
#pragma once

#include "../../shared/coop/Types.h"
#include "../natives/ShvCompat.h"

#include <string>
#include <vector>

namespace coop {

class SkinManager {
public:
    struct SkinEntry {
        const char* model;   // game asset name (resolved via GET_HASH_KEY)
        const char* label;   // menu display text
        const char* category;// "Cops" | "Civils" | "Service" | "Story"
    };

    static const std::vector<SkinEntry>& Catalog();

    // Request a catalog model by asset name (menu selection).
    void RequestLocal(const std::string& modelName);

    // Restore the protagonist belonging to `slot` (0=Michael 1=Franklin 2=Trevor
    // per PlayerSlot values in Types.h).
    void RequestOriginal(u8 slot);

    // Script-thread pump: finishes pending model loads.
    void Tick(u64 nowMs);

    // One-shot consume: returns true right after a skin was applied and
    // writes its hash (0 after a restore is still a valid hash value).
    bool ConsumeApplied(u64& outHash);

    // Human-readable status for the menu ("loading...", "applied", "invalid").
    const std::string& StatusText() const { return status_; }

    u64 CurrentHash() const { return currentHash_; }
    const std::string& CurrentModel() const { return currentModel_; }

private:
    enum class Phase : u8 { IDLE, STREAMING };

    void BeginRequest(ScrHash hash, const std::string& name);

    Phase phase_ = Phase::IDLE;
    ScrHash pendingHash_ = 0;
    std::string pendingName_;

    u64 currentHash_ = 0;    // last successfully applied model (0 = protagonist)
    std::string currentModel_; // empty = protagonist
    u64 appliedHash_ = 0;    // consumed by CoopContext to broadcast
    bool hasPending_ = false;
    u64 pendingStartMs_ = 0;
    std::string status_ = "protagonist original";
};

} // namespace coop

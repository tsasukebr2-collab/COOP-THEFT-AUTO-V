#include "SkinManager.h"
#include "../natives/Natives.h"
#include "../../shared/coop/Log.h"

#include <chrono>

namespace coop {

namespace {
u64 NowMs() {
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Curated catalog of game asset names (the public model names SP modding
// uses). Runtime validation (IS_MODEL_IN_CDIMAGE + IS_MODEL_VALID) makes a
// wrong name a menu message, never a crash.
const std::vector<SkinManager::SkinEntry>& BuildCatalog() {
    static const std::vector<SkinManager::SkinEntry> catalog = {
        // ---- Police / emergency ----------------------------------------
        { "s_m_y_cop_01",        "LSPD Officer (M)",        "Cops" },
        { "s_f_y_cop_01",        "LSPD Officer (F)",        "Cops" },
        { "s_m_y_sheriff_01",    "Sheriff (M)",             "Cops" },
        { "s_f_y_sheriff_01",    "Sheriff (F)",             "Cops" },
        { "s_m_y_swat_01",       "SWAT",                    "Cops" },
        { "s_m_y_hwaycop_01",    "Highway Patrol",          "Cops" },
        { "s_m_y_ranger_01",     "Park Ranger (M)",         "Cops" },
        { "s_f_y_ranger_01",     "Park Ranger (F)",         "Cops" },
        { "s_m_m_paramedic_01",  "Paramedic",               "Cops" },
        { "s_m_y_fireman_01",    "Firefighter",             "Cops" },
        // ---- Service ----------------------------------------------------
        { "s_m_m_security_01",   "Security Guard",          "Service" },
        { "s_m_m_autoshop_02",   "Mechanic",                "Service" },
        { "s_m_y_xmech_01",      "LS Customs Mech",         "Service" },
        { "s_m_y_dealer_01",     "Car Dealer",              "Service" },
        { "s_m_y_barman_01",     "Barman",                  "Service" },
        { "s_m_m_gentransport",  "Airport Worker",          "Service" },
        // ---- Civilians --------------------------------------------------
        { "a_m_y_business_01",   "Businessman",             "Civils" },
        { "a_f_y_business_01",   "Businesswoman",           "Civils" },
        { "a_m_m_bevhills_01",   "Rich Old Man",            "Civils" },
        { "a_f_m_bevhills_02",   "Rich Old Woman",          "Civils" },
        { "a_m_y_beach_01",      "Beach Guy",               "Civils" },
        { "a_f_y_beach_01",      "Beach Girl",              "Civils" },
        { "a_m_y_hippy_01",      "Hippy",                   "Civils" },
        { "a_m_y_stwhi_01",      "Street Hipster",          "Civils" },
        // ---- Story characters (ig_) -------------------------------------
        { "ig_bankman",          "Bank Manager",            "Story" },
        { "ig_chef",             "Chef (Trevor's Crew)",    "Story" },
        { "ig_beverly",          "Beverly (Photographer)",  "Story" },
        { "ig_barry",            "Barry (Legal Trouble)",   "Story" },
        { "ig_drfriedlander",    "Dr. Friedlander",         "Story" },
        { "ig_lester",           "Lester",                  "Story" },
        { "ig_lammy",            "Lamar",                   "Story" },
    };
    return catalog;
}

// Protagonist restore: public model names resolved via GET_HASH_KEY.
const char* ModelNameForSlot(u8 slot) {
    switch (PlayerSlot(slot)) {
        case PlayerSlot::MICHAEL:  return "player_zero";
        case PlayerSlot::FRANKLIN: return "player_one";
        case PlayerSlot::TREVOR:   return "player_two";
        default:                   return "player_one";
    }
}
} // namespace

const std::vector<SkinManager::SkinEntry>& SkinManager::Catalog() {
    return BuildCatalog();
}

void SkinManager::BeginRequest(ScrHash hash, const std::string& name) {
    REQUEST_MODEL(hash);
    pendingHash_ = hash;
    pendingName_ = name;
    pendingStartMs_ = NowMs();
    phase_ = Phase::STREAMING;
    status_ = "loading " + name + "...";
}

void SkinManager::RequestLocal(const std::string& modelName) {
    ScrHash h = GET_HASH_KEY(modelName.c_str());
    if (!IS_MODEL_IN_CDIMAGE(h) || !IS_MODEL_VALID(h)) {
        status_ = "invalid model: " + modelName;
        Log::Warn("skin request rejected (%s): not in cdimage / not valid",
                  modelName.c_str());
        return;
    }
    BeginRequest(h, modelName);
}

void SkinManager::RequestOriginal(u8 slot) {
    BeginRequest(GET_HASH_KEY(ModelNameForSlot(slot)), "");
    pendingName_.clear(); // protagonist restore has no asset label
    status_ = "restoring protagonist...";
}

void SkinManager::Tick(u64) {
    if (phase_ != Phase::STREAMING) return;

    if (!HAS_MODEL_LOADED(pendingHash_)) {
        // 5 s timeout: bad stream request must not wedge the menu state.
        if (NowMs() - pendingStartMs_ > 5000) {
            SET_MODEL_AS_NO_LONGER_NEEDED(pendingHash_);
            phase_ = Phase::IDLE;
            status_ = "load timeout";
            Log::Error("skin load timed out");
        }
        return;
    }

    SET_PLAYER_MODEL(PLAYER_ID(), pendingHash_);
    SET_PED_DEFAULT_COMPONENT_VARIATION(PLAYER_PED_ID());
    SET_MODEL_AS_NO_LONGER_NEEDED(pendingHash_);

    currentHash_ = pendingHash_;
    currentModel_ = pendingName_;
    appliedHash_ = pendingHash_;   // consumed by CoopContext (broadcast)
    hasPending_ = true;
    phase_ = Phase::IDLE;
    status_ = currentModel_.empty() ? "protagonist restored"
                                    : "applied: " + currentModel_;
    Log::Info("skin applied: %s", currentModel_.empty() ? "protagonist"
                                                        : currentModel_.c_str());
}

bool SkinManager::ConsumeApplied(u64& outHash) {
    if (!hasPending_) return false;
    hasPending_ = false;
    outHash = appliedHash_;
    return true;
}

} // namespace coop

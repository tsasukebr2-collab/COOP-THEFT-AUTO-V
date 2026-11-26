// Maps persistent co-op GUIDs to local game script handles.
// Prevents duplicate entity creation: a guid seen twice resolves to the
// same local handle; unknown spawns create a local entity once.
#pragma once

#include "../../shared/coop/Types.h"
#include "../natives/ShvCompat.h"   // Scr* entity handles

#include <map>

namespace coop {

class EntityRegistry {
public:
    void Bind(u64 guid, ScrEntity handle, u8 kind) {
        map_[guid] = Entry{ handle, kind };
    }
    void Unbind(u64 guid) { map_.erase(guid); }

    bool Find(u64 guid, ScrEntity& outHandle, u8& kind) const {
        auto it = map_.find(guid);
        if (it == map_.end()) return false;
        outHandle = it->second.handle;
        kind = it->second.kind;
        return true;
    }

    // Reverse lookup (handle -> guid); used when a local entity dies.
    bool FindByHandle(ScrEntity handle, u64& outGuid) const {
        for (const auto& kv : map_) {
            if (kv.second.handle == handle) { outGuid = kv.first; return true; }
        }
        return false;
    }

    void Clear() { map_.clear(); }
    size_t Size() const { return map_.size(); }

private:
    struct Entry { ScrEntity handle; u8 kind; };
    std::map<u64, Entry> map_;
};

} // namespace coop

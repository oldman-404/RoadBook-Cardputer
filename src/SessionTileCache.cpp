#include "SessionTileCache.h"

#include <cstring>

uint32_t SessionTileCache::hashTile(int32_t tileX, int32_t tileY)
{
    const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(tileX)) << 32) |
                         static_cast<uint32_t>(tileY);
    uint64_t mixed = key;
    mixed ^= mixed >> 33;
    mixed *= 0xff51afd7ed558ccdULL;
    mixed ^= mixed >> 33;
    return static_cast<uint32_t>(mixed % kCapacity);
}

void SessionTileCache::clear()
{
    std::memset(slots_, 0, sizeof(slots_));
    count_ = 0;
}

int32_t SessionTileCache::findSlot(int32_t tileX, int32_t tileY) const
{
    uint32_t index = hashTile(tileX, tileY);

    for (uint32_t probe = 0; probe < kCapacity; ++probe) {
        const uint32_t slotIndex = (index + probe) % kCapacity;
        const Slot& slot = slots_[slotIndex];

        if (!slot.occupied) {
            return -1;
        }
        if (slot.x == tileX && slot.y == tileY) {
            return static_cast<int32_t>(slotIndex);
        }
    }

    return -1;
}

bool SessionTileCache::contains(int32_t tileX, int32_t tileY) const
{
    return findSlot(tileX, tileY) >= 0;
}

SessionTileResult SessionTileCache::add(int32_t tileX, int32_t tileY)
{
    if (contains(tileX, tileY)) {
        return SessionTileResult::AlreadyPresent;
    }

    if (count_ >= kCapacity) {
        return SessionTileResult::Full;
    }

    uint32_t index = hashTile(tileX, tileY);

    for (uint32_t probe = 0; probe < kCapacity; ++probe) {
        const uint32_t slotIndex = (index + probe) % kCapacity;
        Slot& slot = slots_[slotIndex];

        if (!slot.occupied) {
            slot.x = tileX;
            slot.y = tileY;
            slot.occupied = 1;
            ++count_;
            return SessionTileResult::Added;
        }
    }

    return SessionTileResult::Full;
}

uint32_t SessionTileCache::count() const
{
    return count_;
}

uint32_t SessionTileCache::capacity() const
{
    return kCapacity;
}

bool SessionTileCache::full() const
{
    return count_ >= kCapacity;
}

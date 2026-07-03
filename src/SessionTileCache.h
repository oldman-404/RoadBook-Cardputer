#pragma once

#include <cstdint>

enum class SessionTileResult {
    Added,
    AlreadyPresent,
    Full,
};

class SessionTileCache {
public:
    void clear();

    SessionTileResult add(int32_t tileX, int32_t tileY);

    bool contains(int32_t tileX, int32_t tileY) const;

    uint32_t count() const;
    uint32_t capacity() const;
    bool full() const;

private:
    struct Slot {
        int32_t x;
        int32_t y;
        uint8_t occupied;
    };

    static constexpr uint32_t kCapacity = 4096;

    static uint32_t hashTile(int32_t tileX, int32_t tileY);
    int32_t findSlot(int32_t tileX, int32_t tileY) const;

    Slot slots_[kCapacity];
    uint32_t count_{0};
};

#pragma once

#include <array>

#include "Core/LocalPlayerContext.hpp"
#include "Input/InputSelectionBookmark.hpp"

struct HotseatFrontendStateSlot {
    bool hasBookmark = false;
    InputSelectionBookmark bookmark{};
};

class HotseatFrontendStateStore {
public:
    static bool isEnabled(const LocalPlayerContext& context) {
        return isLocalHotseatSession(context);
    }

    void clear() {
        for (HotseatFrontendStateSlot& slot : m_slots) {
            slot = HotseatFrontendStateSlot{};
        }
    }

    void storeBookmark(KingdomId kingdom, const InputSelectionBookmark& bookmark) {
        HotseatFrontendStateSlot& slot = m_slots[kingdomIndex(kingdom)];
        slot.hasBookmark = true;
        slot.bookmark = bookmark;
    }

    bool tryLoadBookmark(KingdomId kingdom, InputSelectionBookmark& bookmark) const {
        const HotseatFrontendStateSlot& slot = m_slots[kingdomIndex(kingdom)];
        if (!slot.hasBookmark) {
            return false;
        }

        bookmark = slot.bookmark;
        return true;
    }

private:
    std::array<HotseatFrontendStateSlot, kNumKingdoms> m_slots{};
};
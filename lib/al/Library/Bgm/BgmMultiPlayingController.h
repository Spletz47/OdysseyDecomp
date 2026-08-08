#pragma once

#include <basis/seadTypes.h>
#include <container/seadPtrArray.h>
#include <math/seadRandom.h>

namespace al {
class BgmMultiPlayingController {
public:
    struct PlayParams {
        bool isEnable = false;
        bool unk1 = false;
        bool unk2 = false;
        bool unk3 = false;
        u8 unk4 = 0;
        u8 unk5 = 0;
        const char* name = nullptr;
        void* unk10 = nullptr;
        s32 unk18 = 0;
    };

    BgmMultiPlayingController();

    void use();
    void removeAllMultiBgmPlayingInfo();
    void activate(const PlayParams& params, s32 unk, bool unk2);
    void deactivate(bool unk, s32 unk2);
    void update();
    void tryAddMultiBgmPlayingInfo(const char* name, s32 unk = 0, bool unk2 = false);
    void tryRemoveMultiBgmPlayingInfo(const char* name);
    void enableMultiBgmPlayingInfo(const char* name);
    void disableMultiBgmPlayingInfo(const char* name);

private:
    bool mIsUsed = false;
    bool mIsActive = false;
    const char* mName = nullptr;
    void* _10 = nullptr;
    sead::PtrArrayImpl* mMultiBgmPlayingInfoArray = nullptr;
    bool _20 = false;
    bool _21 = false;
    bool _22 = false;
    bool _23 = false;
    bool _24 = false;
    s32 mLineIndex = -1;
    bool _2c = false;
    sead::Random mRandom;
};
static_assert(sizeof(BgmMultiPlayingController) == 0x40);
}  // namespace al

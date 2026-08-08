#pragma once

#include <basis/seadTypes.h>

namespace al {
class LiveActor;
}  // namespace al

class CityManRhythmInfo {
public:
    struct CurAnimInfo {
        f32 beat = -1.0f;
        s32 animId = -1;
        s32 velocity = -1;
    };

    CityManRhythmInfo(al::LiveActor* actor, const unsigned char* byaml, bool useActorBgm,
                      f32 bgmOffset);

    void update(bool needResync);

    bool isLooping() const;

    void updateAnim();

    bool checkSameBeatAnimInfo(CurAnimInfo& out, s32 offset) const;

    s32 getAnimId(s32 index);
    f32 getAnimBeat(s32 index);

    f32 getPrevBeat() const { return mPrevBeat; }

    f32 getCurAnimBeat() const { return mCurAnimInfo.beat; }

    s32 getCurAnimId() const { return mCurAnimInfo.animId; }

    s32 getNextAnimIndex() const { return mNextAnimIndex; }

    bool isOutOfSync() const { return mIsOutOfSync; }

private:
    bool initAnimInfo(const unsigned char* byaml);

    void resetRhythmInfo(f32 beat);

    al::LiveActor* mActor;
    f32 mBgmOffset;
    f32 mPrevBeat = -1;
    f32 mCurBeat = -1;
    CurAnimInfo* mAnimInfos = nullptr;
    CurAnimInfo mCurAnimInfo;
    s32 mAnimInfoCount = -1;
    s32 mNextAnimIndex = 0;
    bool mIsLooping = false;
    bool mIsOutOfSync = false;
};

static_assert(sizeof(CityManRhythmInfo) == 0x38);

#include "Npc/CityManRhythmInfo.h"

#include "Library/Bgm/BgmLineFunction.h"
#include "Library/LiveActor/LiveActor.h"
#include "Library/Yaml/ByamlIter.h"

#include "MapObj/RhyhtmInfoWatcher.h"

CityManRhythmInfo::CityManRhythmInfo(al::LiveActor* actor, const unsigned char* byaml,
                                     bool useActorBgm, f32 bgmOffset)
    : mActor(actor), mBgmOffset(useActorBgm ? bgmOffset : -0.2f) {
    rs::registerRhyhtmInfoListener(actor);
    initAnimInfo(byaml);
}

// NON_MATCHING: minor instruction scheduling differences in the byaml parsing loop;
// logic is verified equivalent to target.
bool CityManRhythmInfo::initAnimInfo(const unsigned char* byaml) {
    al::ByamlIter iter(byaml);
    mAnimInfoCount = iter.getSize();
    if (mAnimInfoCount < 1)
        return false;

    mAnimInfos = new CurAnimInfo[mAnimInfoCount];

    for (s32 i = 0; i < mAnimInfoCount; i++) {
        al::ByamlIter animIter;
        iter.tryGetIterByIndex(&animIter, i);

        s32 sample = 0;
        animIter.tryGetIntByKey(&sample, "Sample");
        mAnimInfos[i].beat = (f32)sample / 480.0f;

        animIter.tryGetIntByKey(&mAnimInfos[i].animId, "DanceAnimId");

        s32 velocity;
        if (animIter.tryGetIntByKey(&velocity, "Velocity")) {
            mAnimInfos[i].velocity = velocity;
            if (velocity == 100)
                mAnimInfos[i].velocity = -1;
        } else {
            mAnimInfos[i].velocity = -1;
        }
    }

    mNextAnimIndex = 0;
    return true;
}

// NON_MATCHING: compiler hoists the shared -1 constant into a callee-saved register (x21)
// across the whole function; logic is verified equivalent to target, remaining diffs are
// register allocation/scheduling only.
void CityManRhythmInfo::update(bool needResync) {
    mCurBeat = mPrevBeat;
    mIsOutOfSync = false;
    mCurAnimInfo.animId = -1;
    mCurAnimInfo.velocity = -1;

    mPrevBeat = rs::getCurrentBeat(mActor);
    if (mPrevBeat < 0.0f)
        return;

    mIsLooping = rs::isLooping(mActor);

    if (mIsLooping) {
        f32 loopStartBeat = al::getLoopStartBeat(mActor);
        if (mAnimInfoCount < 1)
            return;
        resetRhythmInfo(loopStartBeat);
    } else if (mPrevBeat - mCurBeat > 0.5f) {
        resetRhythmInfo(mPrevBeat);
        mIsOutOfSync = true;
    } else if (needResync) {
        resetRhythmInfo(mPrevBeat);
        mIsOutOfSync = true;
    }

    updateAnim();
}

bool CityManRhythmInfo::isLooping() const {
    return rs::isLooping(mActor);
}

void CityManRhythmInfo::resetRhythmInfo(f32 beat) {
    for (s32 i = 0; i < mAnimInfoCount; i++) {
        if (beat < mAnimInfos[i].beat) {
            s32 index = 0;
            if (i != 0)
                index = i - 1;
            mNextAnimIndex = index;
            return;
        }
    }
}

// NON_MATCHING: minor register allocation/scheduling differences in the pointer-walk loop;
// logic is verified equivalent to target.
void CityManRhythmInfo::updateAnim() {
    s32 count = mAnimInfoCount;
    if (count < 1)
        return;

    s32 index = mNextAnimIndex;
    if (index < 0 || index >= count)
        return;

    CurAnimInfo* info = &mAnimInfos[index];
    s32 pos = index;
    do {
        f32 beat = info->beat;
        if (beat + mBgmOffset <= mPrevBeat) {
            if (pos == count - 1) {
                mCurAnimInfo.beat = beat;
                mCurAnimInfo.animId = info->animId;
                mCurAnimInfo.velocity = info->velocity;
                mNextAnimIndex = -1;
                return;
            }
            if (mPrevBeat < mBgmOffset + info[1].beat) {
                mCurAnimInfo.beat = beat;
                mCurAnimInfo.animId = info->animId;
                mCurAnimInfo.velocity = info->velocity;
                mNextAnimIndex = pos + 1;
                return;
            }
        }
        pos++;
        info++;
    } while (pos < count);
}

bool CityManRhythmInfo::checkSameBeatAnimInfo(CurAnimInfo& out, s32 offset) const {
    s32 lastIndex = mNextAnimIndex;
    if (lastIndex < 1)
        lastIndex = mAnimInfoCount;
    lastIndex -= 1;

    s32 index = lastIndex - offset;
    if (index < 0)
        return false;

    if (mAnimInfos[index].beat != mAnimInfos[lastIndex].beat)
        return false;

    out = mAnimInfos[index];
    return true;
}

s32 CityManRhythmInfo::getAnimId(s32 index) {
    if (-1 < index)
        return mAnimInfos[index].animId;
    return -1;
}

f32 CityManRhythmInfo::getAnimBeat(s32 index) {
    if (-1 < index)
        return mAnimInfos[index].beat;
    return -1;
}

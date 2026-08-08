#pragma once

#include <container/seadPtrArray.h>

namespace al {
class ByamlIter;
class LiveActor;
}

class BgmAnimeSyncDirector;
class BgmSyncTargetActionInfo;

class BgmAnimeSynchronizer {
public:
    BgmAnimeSynchronizer(const char* archiveName, al::LiveActor* actor, s32 maxSyncInfos);

    bool isCreatable(const al::LiveActor* actor);
    static BgmAnimeSynchronizer* tryCreate(al::LiveActor* actor, al::ByamlIter& byaml);

    void registSyncTargetActionInfo(BgmSyncTargetActionInfo* actionInfo);

    void init();

    void trySyncBgm();
    void setSyncChaseRateOffsetMax(f32 a, f32 b, f32 c, f32 d);

private:
    const char* mArchiveName;
    al::LiveActor* mActor;
    s32 mArchiveAndActionIndex;
    bool mIsFirstSync;
    void** mTrackEntries;
    bool* mTrackEnabled;
    bool mHasSyncTriggered;
    sead::PtrArrayImpl* mSyncInfoList;
    BgmSyncTargetActionInfo* mLastSyncTargetActionInfo;
    BgmAnimeSyncDirector* mSyncDirector;
    s32 mCalcedActionIndex;
};

static_assert(sizeof(BgmAnimeSynchronizer) == 0x50);

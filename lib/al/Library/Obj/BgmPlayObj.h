#pragma once

#include "Library/LiveActor/LiveActor.h"

namespace al {
struct ActorInitInfo;
class IUseAudioKeeper;
class BgmDataBase;
class SeResourceInfo;

class BgmPlayObj : public LiveActor {
public:
    struct PlayParams {
        bool isEnable = false;
        const char* name = nullptr;
        void* unk10 = nullptr;
        s32 unk18 = 0;
    };

    BgmPlayObj(const char* name, bool isUseMultiBgmPlayingInfo);

    void init(const ActorInitInfo& info) override;
    void createShape(BgmDataBase* dataBase);
    void init(const ActorInitInfo& info, const char* linkName);
    void init(const ActorInitInfo& info, const char* bgmName, const char* linkName,
              const char* bgmSourceName);
    void initAfterPlacement() override;
    void appear() override;
    void kill() override;
    void stopBgm(s32 unk);
    void movement() override;
    bool isEnableCalcSpeakerParam() const;
    void calc3DParams(bool unk);
    void finalize();
    f32 getDistanceFromSourceToListener();
    bool isPlayable() const;
    void activate(bool unk1, bool unk2, bool unk3);
    void startBgm(bool unk1, bool unk2);
    void activate(const PlayParams& params, bool unk);
    void deactivate(bool unk1, s32 unk2);

    void exeWaitOnSwitch();
    void exeWaitPlayStart();
    void exePlay();

private:
    const char* mBgmName;
    const char* mLinkName = nullptr;
    const char* mBgmSourceName = nullptr;
    const char* mKind = nullptr;
    void* unk20 = nullptr;
    f32 unk28 = 1.0f;
    void* unk30 = nullptr;
    f32 unk40 = 0.0f;
    f32 unk44 = 1.0f;
    void* unk48 = nullptr;
    f32 unk58 = 1.0f;
    void* unk60 = nullptr;
    void* unk68 = nullptr;
    void* unk70 = nullptr;
    void* mResourceCategoryInfo = nullptr;
    void* unk88 = nullptr;
    bool mIsExplicitInit = false;
    s32 mFadeInFrameNum = 0;
    s32 mFadeOutFrameNum = 0;
    s32 mFadeOutFrameNumForCurBgm = 0;
    s32 mStartDelayFrameNum = 0;
    bool mIsRestartBgmFromTop = false;
    bool mIsSkipIntro = false;
    bool mIsSkipIntroIfNotForFirstTime = false;
    f32 mDistanceVolumeMin = 0.0f;
    u8 unkAB = 0;
    bool mHasSwitchStart = false;
    u8 unkAD = 0;
    u8 unkAE = 0;
    bool mIsHighPriority = false;
    void* unkB0 = nullptr;
    SeResourceInfo* mSeResourceInfo = nullptr;
    void* unkC8 = nullptr;
    void* unkD0 = nullptr;
    void* unkD8 = nullptr;
    bool mIsUseMultiBgmPlayingInfo = false;
};

bool tryRegistBgmPlayObj(const IUseAudioKeeper* user, BgmPlayObj* bgmPlayObj);
}  // namespace al

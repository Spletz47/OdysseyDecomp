#pragma once

#include "Library/LiveActor/LiveActor.h"

namespace al {
class BgmMultiPlayingController;
class BgmPlayObj;
}  // namespace al
class BgmAnimeSynchronizer;

class RadioCassette : public al::LiveActor {
public:
    RadioCassette(const char* name);

    void init(const al::ActorInitInfo& info) override;
    void appear() override;
    void control() override;
    void attackSensor(al::HitSensor* self, al::HitSensor* other) override;
    bool receiveMsg(const al::SensorMsg* message, al::HitSensor* other,
                    al::HitSensor* self) override;

    void exeWait();
    void endWait();
    void exeReactionNoise();
    void exeReactionNoiseEnd();
    void exeReactionTuning();
    void exeStartSpecialCollectBgm();

private:
    al::BgmPlayObj* mBgmPlayObj;
    BgmAnimeSynchronizer* mBgmAnimeSynchronizer = nullptr;
    const char* mWaitAnimName = "WaitDance";
    const char* mBehaviorType = nullptr;
    const char* mSpecialCollectBgmName = nullptr;
    bool mIsSpecialCollectBgmCollected = false;
    const char* mReactionActionName = nullptr;
    bool mIsReactionNoiseSePlayed = false;
    s32 mSpecialCollectBgmTimer = -1;
    s32 mReactionNoiseThemeIndex = -1;
    s32 mReactionNoiseVKoopaIndex = -1;
    al::BgmMultiPlayingController* mBgmMultiPlayingController = nullptr;
    bool mIsSwitchMoveOnDone = false;
    s32 mMsgCooldownTimer = 0;
    s32 mMsgCooldownTimerSub = 0;
};

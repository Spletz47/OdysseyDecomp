#include "MapObj/RadioCassette.h"

#include "Library/Base/StringUtil.h"
#include "Library/Bgm/BgmLineFunction.h"
#include "Library/Bgm/BgmMultiPlayingController.h"
#include "Library/LiveActor/ActorActionFunction.h"
#include "Library/LiveActor/ActorInitUtil.h"
#include "Library/LiveActor/ActorResourceFunction.h"
#include "Library/LiveActor/ActorSensorUtil.h"
#include "Library/Math/MathUtil.h"
#include "Library/Nerve/NerveSetupUtil.h"
#include "Library/Nerve/NerveUtil.h"
#include "Library/Obj/BgmPlayObj.h"
#include "Library/Placement/PlacementFunction.h"
#include "Library/Se/SeFunction.h"
#include "Library/Stage/StageSwitchUtil.h"
#include "Library/Yaml/ByamlIter.h"

#include "Audio/BgmAnimeSynchronizer.h"
#include "System/GameDataFunction.h"
#include "System/GameDataHolderAccessor.h"
#include "Util/SensorMsgFunction.h"

namespace {
NERVE_END_IMPL(RadioCassette, Wait);
NERVE_IMPL(RadioCassette, ReactionNoise);
NERVE_IMPL(RadioCassette, ReactionNoiseEnd);
NERVE_IMPL(RadioCassette, ReactionTuning);
NERVE_IMPL(RadioCassette, StartSpecialCollectBgm);

NERVES_MAKE_STRUCT(RadioCassette, Wait, ReactionNoise, ReactionNoiseEnd, ReactionTuning,
                   StartSpecialCollectBgm);

const char* sKoopaThemeNames[] = {
    "KoopaTheme00", "KoopaTheme01", "KoopaTheme02", "KoopaTheme03", "KoopaTheme04", "KoopaTheme05",
};

const char* sNoiseVKoopaNames[] = {
    "NoiseVKoopa00",
    "NoiseVKoopa01",
};

const char* sShopBgmNames[] = {
    "StmRsBgmShop01Radio",
    "StmRsBgmShop02Radio",
    "StmRsBgmShop03Radio",
    "StmRsBgmShop04Radio",
};
}  // namespace

RadioCassette::RadioCassette(const char* name) : al::LiveActor(name) {
    mBgmPlayObj = new al::BgmPlayObj("BGM再生オブジェ", true);
}

void RadioCassette::init(const al::ActorInitInfo& info) {
    al::initActor(this, info);

    mBgmPlayObj->init(info, "３Ｄキューブ音源");
    if (!al::tryRegistBgmPlayObj(this, mBgmPlayObj)) {
        appear();
        return;
    }

    if (!al::isValidStageSwitch(this, "SwitchMoveOn"))
        mIsSwitchMoveOnDone = true;

    al::tryGetStringArg(&mWaitAnimName, info, "WaitAnimName");
    if (!mWaitAnimName)
        mWaitAnimName = "WaitDance";

    const char* specialCollectBgmTypeName = nullptr;
    al::tryGetStringArg(&specialCollectBgmTypeName, info, "SpecialCollectBgmTypeName");
    if (!specialCollectBgmTypeName)
        specialCollectBgmTypeName = "RockJp";

    bool isRockJp = al::isEqualString("RockJp", specialCollectBgmTypeName);
    mSpecialCollectBgmName = isRockJp ? "StmRsBgmEndRockJpRadio" : "StmRsBgmCityScenario03JpRadio";
    mIsSpecialCollectBgmCollected =
        GameDataFunction::isCollectedBgm(GameDataHolderAccessor(this), mSpecialCollectBgmName, nullptr);

    mBgmMultiPlayingController = al::tryAllocBgmMultiPlayingController(this);
    if (!mBgmMultiPlayingController) {
        appear();
        return;
    }

    // NON_MATCHING: original constructs a helper functor object here
    // (operator new(0x10) + a vtable-only object stashed at
    // mBgmMultiPlayingController+0x10); purpose/type unconfirmed.

    al::tryGetStringArg(&mBehaviorType, info, "BehaviorType");
    if (!mBehaviorType)
        mBehaviorType = "Wait";

    // NON_MATCHING: writes mBehaviorType-derived name into
    // mBgmPlayObj's own internal state (offset 0x120); BgmPlayObj isn't
    // decompiled yet so the setter used here is unconfirmed.

    if (al::isEqualString("Shop", mBehaviorType)) {
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo(sShopBgmNames[0]);
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo(sShopBgmNames[1]);
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo(sShopBgmNames[2]);
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo(sShopBgmNames[3]);

        const char* curBgm;
        if (al::isPlayingBgm(this, "Shop"))
            curBgm = al::getCurPlayingBgmResourceName(this);
        else
            curBgm = sShopBgmNames[al::getRandom(0, 4)];
        // NON_MATCHING: curBgm is stashed into mBgmPlayObj's own state
        // (offset 0x128); unconfirmed setter, see above.
        (void)curBgm;
    } else if (al::isEqualString("PlaySandWorldTownBgm", mBehaviorType)) {
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo("StmRsBgmDesertTown");
    } else if (al::isEqualString("PlayCityWorldCafeBgm", mBehaviorType)) {
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo("StmRsBgmCityCafe01Radio");
    }

    if (isRockJp) {
        bool hasRockJp =
            GameDataFunction::isCollectedBgm(GameDataHolderAccessor(this), "StmRsBgmEndRockJpRadio", nullptr);
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo("StmRsBgmEndRockJpRadio", 0, !hasRockJp);

        bool hasCityScenario03 = GameDataFunction::isCollectedBgm(
            GameDataHolderAccessor(this), "StmRsBgmCityScenario03JpRadio", nullptr);
        mBgmMultiPlayingController->tryAddMultiBgmPlayingInfo("StmRsBgmCityScenario03JpRadio", 0,
                                                              !hasCityScenario03);
    }

    al::ByamlIter iter;
    if (al::tryGetActorInitFileIter(&iter, this, "BgmRhythmSyncInfo", nullptr))
        mBgmAnimeSynchronizer = BgmAnimeSynchronizer::tryCreate(this, iter);

    al::offStageSwitch(this, "SwitchDancingOn");
    al::initNerve(this, &NrvRadioCassette.Wait, 0);

    // NON_MATCHING: original calls a virtual through mBgmPlayObj's vtable
    // (offset 0x18) before dispatching to appear(); purpose unconfirmed
    // (likely mBgmPlayObj->initAfterPlacement() or similar).
    appear();
}

void RadioCassette::appear() {}

void RadioCassette::control() {
    if (mMsgCooldownTimerSub > 0)
        mMsgCooldownTimerSub--;
    if (mMsgCooldownTimer > 0)
        mMsgCooldownTimer--;

    if (mSpecialCollectBgmTimer > 0) {
        mSpecialCollectBgmTimer--;
        if (mSpecialCollectBgmTimer == 0) {
            mIsSpecialCollectBgmCollected = true;
            mBgmMultiPlayingController->enableMultiBgmPlayingInfo(mSpecialCollectBgmName);
            al::setNerve(this, &NrvRadioCassette.Wait);
        }
    }

    if (mBgmAnimeSynchronizer)
        mBgmAnimeSynchronizer->trySyncBgm();
}

void RadioCassette::attackSensor(al::HitSensor* self, al::HitSensor* other) {
    if (rs::sendMsgPushToPlayer(other, self))
        return;
    al::sendMsgPush(other, self);
}

// NON_MATCHING: the exact grouping/parenthesization of this condition is a
// best-effort reconstruction from the flattened assembly and needs
// verification against tools/check.
bool RadioCassette::receiveMsg(const al::SensorMsg* message, al::HitSensor* other,
                               al::HitSensor* self) {
    bool isReactingNerve = al::isNerve(this, &NrvRadioCassette.ReactionNoise) ||
                           al::isNerve(this, &NrvRadioCassette.ReactionTuning) ||
                           al::isNerve(this, &NrvRadioCassette.StartSpecialCollectBgm);

    bool isBasicAttack =
        rs::isMsgCapTouchWall(message) || rs::isMsgCapReflect(message) ||
        rs::isMsgHosuiAttackCollide(message) || rs::isMsgSeedAttack(message) ||
        rs::isMsgGrowerAttack(message) || rs::isMsgYoshiTongueAttack(message) ||
        rs::isMsgTankBullet(message) || rs::isMsgGamaneBullet(message) ||
        rs::isMsgGamaneBulletThrough(message) || rs::isMsgGamaneBulletForCoinFlower(message) ||
        rs::isMsgTRexAttack(message) || al::isMsgExplosion(message) ||
        al::isMsgEnemyAttackFire(message) || al::isMsgPlayerFireBallAttack(message);

    bool isMotorcycleAttack = rs::isMsgMotorcycleAttack(message) ||
                              rs::isMsgMotorcycleDashAttack(message) ||
                              rs::isMsgMotorcycleDashCollide(message);

    bool isTrample = rs::checkMsgNpcTrampleReactionAll(message, other, self, false);
    bool isRollingOrHipDrop = rs::isMsgPlayerRollingObjHit(message) || isTrample ||
                              rs::isMsgPlayerAndCapHipDropAll(message);

    bool shouldReact = isBasicAttack || ((!isReactingNerve || isTrample) &&
                                         (isMotorcycleAttack || isRollingOrHipDrop));
    if (!shouldReact)
        return false;

    if (mMsgCooldownTimerSub > 0 || mMsgCooldownTimer > 0) {
        mMsgCooldownTimer = 2;
        return false;
    }

    mReactionActionName = isBasicAttack || isMotorcycleAttack ? "Reaction" : "ReactionCap";

    if (!rs::isMsgPlayerRollingWallHitDown(message) || isReactingNerve) {
        if (GameDataFunction::isGameClear(GameDataHolderAccessor(this))) {
            if (mIsSpecialCollectBgmCollected) {
                if (al::isEqualString("Wait", mBehaviorType))
                    mWaitAnimName = "WaitDance";
                al::setNerve(this, &NrvRadioCassette.ReactionTuning);
            } else {
                al::setNerve(this, &NrvRadioCassette.StartSpecialCollectBgm);
            }
        } else {
            al::setNerve(this, &NrvRadioCassette.ReactionNoise);
        }
    }

    rs::requestHitReactionToAttacker(message, self, other);
    mMsgCooldownTimer = 2;
    mMsgCooldownTimerSub = 8;
    return true;
}

void RadioCassette::exeWait() {
    if (al::isFirstStep(this)) {
        al::tryStartActionIfNotPlaying(this, mWaitAnimName);

        if (al::isValidStageSwitch(this, "SwitchDancingOn") &&
            al::isEqualString("WaitDance", mWaitAnimName))
            al::onStageSwitch(this, "SwitchDancingOn");

        if (!mIsSwitchMoveOnDone && al::isEqualString("WaitDance", mWaitAnimName)) {
            al::onStageSwitch(this, "SwitchMoveOn");
            mIsSwitchMoveOnDone = true;
        }
    }

    // NON_MATCHING: `*(char*)(mBgmMultiPlayingController + 1) == 0` in the
    // original checks an internal BgmMultiPlayingController flag (likely
    // "is active") that isn't decompiled yet.
    if (!al::isEqualString("Wait", mBehaviorType)) {
        al::BgmMultiPlayingController::PlayParams params;
        params.isEnable = true;
        params.name = al::isEqualString("Shop", mBehaviorType) ? "Shop" : "RadioCassette";
        mBgmMultiPlayingController->activate(params, 0, true);
    }
}

void RadioCassette::endWait() {
    if (al::isValidStageSwitch(this, "SwitchDancingOn") &&
        al::isEqualString("WaitDance", mWaitAnimName))
        al::offStageSwitch(this, "SwitchDancingOn");
}

void RadioCassette::exeReactionNoise() {
    if (al::isFirstStep(this)) {
        al::startAction(this, mReactionActionName);
        al::startBgmSituation(this, "RadioCassetteReactionNoise", false, true);
    }

    al::holdSeWithParam(this, "ReactionNoise", 0.4f, "");

    if (al::isStep(this, 30) && !mIsReactionNoiseSePlayed) {
        s32 themeIndex = mReactionNoiseThemeIndex < 0 ? 0 : (mReactionNoiseThemeIndex + 1) % 6;
        al::startSe(this, sKoopaThemeNames[themeIndex]);
        mReactionNoiseThemeIndex = themeIndex;

        s32 vKoopaIndex;
        if (mReactionNoiseVKoopaIndex < 0) {
            vKoopaIndex = 0;
        } else {
            vKoopaIndex = al::getRandom(0, 2);
            if (vKoopaIndex == mReactionNoiseVKoopaIndex)
                vKoopaIndex = (mReactionNoiseVKoopaIndex + 1) % 2;
        }
        al::startSe(this, sNoiseVKoopaNames[vKoopaIndex]);
        mReactionNoiseVKoopaIndex = vKoopaIndex;
        mIsReactionNoiseSePlayed = true;
    }

    if (al::isActionEnd(this) && al::isGreaterEqualStep(this, 0x46))
        al::setNerve(this, &NrvRadioCassette.ReactionNoiseEnd);
}

void RadioCassette::exeReactionNoiseEnd() {
    if (al::isFirstStep(this))
        al::startAction(this, mWaitAnimName);

    f32 rate = 1.0f - static_cast<f32>(al::getNerveStep(this)) / 20.0f;
    if (rate > 0.0f)
        al::holdSeWithParam(this, "ReactionNoise", rate * 0.4f, "");
    else
        al::stopSe(this, "ReactionNoise", 0x14, nullptr);

    if (al::isStep(this, 30))
        mIsReactionNoiseSePlayed = false;

    if (al::isGreaterEqualStep(this, 0x50)) {
        al::endBgmSituation(this, "RadioCassetteReactionNoise", false);
        al::setNerve(this, &NrvRadioCassette.Wait);
        mIsReactionNoiseSePlayed = false;
    }
}

void RadioCassette::exeReactionTuning() {
    if (!mBgmMultiPlayingController)
        return;

    if (al::isFirstStep(this)) {
        al::startAction(this, mReactionActionName);
        // NON_MATCHING: `*(char*)(mBgmMultiPlayingController + 1) != 0` -
        // see exeWait.
        mBgmMultiPlayingController->deactivate(false, 0);
        al::startSe(this, "ReactionTuning");
    }

    if (al::isActionEnd(this) || al::isGreaterEqualStep(this, 0x46)) {
        al::BgmMultiPlayingController::PlayParams params;
        params.isEnable = true;
        params.name = al::isEqualString("Shop", mBehaviorType) ? "Shop" : "RadioCassette";
        mBgmMultiPlayingController->activate(params, 0, false);
        al::setNerve(this, &NrvRadioCassette.Wait);
    }
}

void RadioCassette::exeStartSpecialCollectBgm() {
    if (al::isFirstStep(this)) {
        al::startAction(this, mReactionActionName);
        if (mSpecialCollectBgmTimer <= 0)
            mBgmMultiPlayingController->deactivate(false, 0);
    }

    if (mSpecialCollectBgmTimer <= 0) {
        al::holdSeWithParam(this, "ReactionNoise", 0.4f, "");

        if (al::isGreaterEqualStep(this, 0x78)) {
            al::setNerve(this, &NrvRadioCassette.Wait);
            mSpecialCollectBgmTimer = 0xb4;

            al::BgmMultiPlayingController::PlayParams params;
            params.isEnable = true;
            params.name = mSpecialCollectBgmName;
            mBgmMultiPlayingController->activate(params, 0, false);

            mWaitAnimName = "WaitDance";
        }
    } else if (al::isActionEnd(this) || al::isGreaterEqualStep(this, 0x46)) {
        al::setNerve(this, &NrvRadioCassette.Wait);
    }
}

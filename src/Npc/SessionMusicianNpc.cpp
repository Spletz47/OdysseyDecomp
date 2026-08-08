#include "Npc/SessionMusicianNpc.h"

#include <sstream>

#include "Library/Demo/DemoFunction.h"
#include "Library/Event/EventFlowFunction.h"
#include "Library/Event/EventFlowUtil.h"
#include "Library/Joint/JointControllerKeeper.h"
#include "Library/LiveActor/ActorActionFunction.h"
#include "Library/LiveActor/ActorAnimFunction.h"
#include "Library/LiveActor/ActorClippingFunction.h"
#include "Library/LiveActor/ActorFlagFunction.h"
#include "Library/LiveActor/ActorInitFunction.h"
#include "Library/LiveActor/ActorInitUtil.h"
#include "Library/LiveActor/ActorModelFunction.h"
#include "Library/LiveActor/ActorResourceFunction.h"
#include "Library/LiveActor/ActorSensorUtil.h"
#include "Library/LiveActor/LiveActorFunction.h"
#include "Library/Nerve/NerveSetupUtil.h"
#include "Library/Nerve/NerveUtil.h"
#include "Library/Obj/PartsModel.h"
#include "Library/Placement/PlacementFunction.h"
#include "Library/Stage/StageSwitchUtil.h"
#include "Library/Thread/FunctorV0M.h"
#include "Library/Yaml/ByamlIter.h"

#include "Audio/BgmAnimeSynchronizer.h"
#include "Npc/CityManRhythmInfo.h"
#include "Npc/NpcStateReactionParam.h"
#include "Npc/SessionEventProgress.h"
#include "Npc/SessionMayorNpc.h"
#include "Npc/SessionMusicianLocalFunction.h"
#include "Npc/SessionMusicianType.h"
#include "Npc/SessionMusicianWarpAgent.h"
#include "Npc/TalkNpcCap.h"
#include "Sequence/GameSequenceInfo.h"
#include "System/GameDataFunction.h"
#include "System/GameDataHolderAccessor.h"
#include "Util/NpcActionUtil.h"
#include "Util/NpcAnimUtil.h"
#include "Util/NpcEventFlowUtil.h"
#include "Util/PlayerPuppetFunction.h"
#include "Util/PlayerUtil.h"
#include "Util/SensorMsgFunction.h"

namespace {
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, Wait);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, Reaction);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, WaitNoEventFlow);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, WaitNoEventFlowSabi);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, WarpStart);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, WarpEnd);
NERVE_HOST_TYPE_IMPL(SessionMusicianNpc, Warp);

NERVES_MAKE_NOSTRUCT(HostType, WarpStart, WarpEnd, Warp);
NERVES_MAKE_STRUCT(HostType, Wait, Reaction, WaitNoEventFlow, WaitNoEventFlowSabi);
}  // namespace

static const char* sWaitAnimNames[] = {
    "PlayAtFes1",
    "PlayAtFes2",
};

static const char* sSabiAnimNames[] = {
    "PlayKimeAtFes1",    "PlayKimeAtFes2",     "PlayDrumActionB",
    "PlayGuitarActionB", "PlayTrumpetActionB",
};

const char* getInstrumentArchiveName(const al::ActorInitInfo& actor) {
    const char* objectName = nullptr;
    al::tryGetObjectName(&objectName, actor);

    if (al::isEqualString(objectName, "SessionMusicianDrum"))
        return "Drum";

    if (al::isEqualString(objectName, "SessionMusicianGuitar"))
        return "Guitar";

    if (al::isEqualString(objectName, "SessionMusicianBass"))
        return "Bass";

    if (al::isEqualString(objectName, "SessionMusicianTrumpet"))
        return "Trumpet";

    if (al::isEqualString(objectName, "SessionMusicianVocal"))
        return "Vocal";

    if (al::isEqualString(objectName, "SessionMusicianSaxophone"))
        return "Trumpet";

    if (al::isEqualString(objectName, "SessionMusicianTrombone"))
        return "Trumpet";

    return nullptr;
}

// Thanks german77
static inline const char* getEventTypeEventFlowString(SessionMusicianNpc* npc) {
    if (npc->getEventType() == SessionMusicianNpc::EventType::PowerPlant)
        return "Init";

    if (npc->getEventType() == SessionMusicianNpc::EventType::Wait) {
        SessionEventProgress progress = GameDataFunction::getSessionEventProgress(npc);
        if (progress > SessionEventProgress::Entry)
            return "Wait";
    }

    return "Init";
}

// Thanks german77
static inline void initCamera(SessionMusicianNpc* npc, const al::ActorInitInfo& initInfo,
                              al::EventFlowExecutor* eventFlow,
                              const MusicianCameraParams* params) {
    const char* sTalkEventCameraNames[] = {"TalkDrum", "TalkGuitar", "TalkBass", "TalkTrumpet"};
    SessionMusicianType type = SessionMusicianLocalFunction::getMusicianType(npc);

    if (type.value() <= (u32)SessionMusicianType::Vocal) {
        const char* cameraName = sTalkEventCameraNames[type];
        if (rs::isDefinedEventCamera(eventFlow, cameraName)) {
            rs::initEventCameraFixActorAutoAroundFront2(
                eventFlow, initInfo, cameraName, npc, &params->offset, params->distance->value,
                params->horizontalAngle->value, params->verticalAngle->value);
        }
    }
}

// With help from german77 and GRAnimated
void SessionMusicianNpc::init(const al::ActorInitInfo& initInfo) {
    using SessionMusicianNpcFunctor =
        al::FunctorV0M<SessionMusicianNpc*, void (SessionMusicianNpc::*)()>;

    al::initActorWithArchiveName(this, initInfo, "BandMan", getInstrumentArchiveName(initInfo));

    al::initNerve(this, &NrvHostType.Wait, 8);

    al::tryGetLinksQT(&mMoonGetDemoPlayerPose, &mMoonGetDemoPlayerPos, initInfo,
                      "PlayerPosMoonGetDemo");

    mIsUseBgmTrackMute = al::tryGetBoolArgOrFalse(initInfo, "IsUseBgmTrackMute");

    mNpcStateReaction = NpcStateReaction::create(this, nullptr);
    mNpcStateReaction->setStateReactionParam(new NpcStateReactionParam("Reaction", "ReactionCap"));
    mNpcStateReaction->setEnableCapReaction(true);

    al::initNerveState(this, mNpcStateReaction, &NrvHostType.Reaction, "リアクション");

    mTalkNpcCap = TalkNpcCap::tryCreate(this, initInfo);
    if (mTalkNpcCap != nullptr) {
        al::registerSubActor(this, mTalkNpcCap);
        al::onSyncClippingSubActor(this, mTalkNpcCap);
        al::onSyncAppearSubActor(this, mTalkNpcCap);
        al::onSyncHideSubActor(this, mTalkNpcCap);
        al::onSyncAlphaMaskSubActor(this, mTalkNpcCap);
        al::startVisAnim(this, "DefaultHatOff");
    }

    al::tryGetArg((s32*)&mEventType, initInfo, "EventType");

    {
        const char* animName = nullptr;
        if (al::tryGetStringArg(&animName, initInfo, "MtpAnim") && animName != nullptr &&
            !al::isEqualString(animName, "None"))
            al::tryStartMtpAnimIfExist(this, animName);

        animName = nullptr;
        if (al::tryGetStringArg(&animName, initInfo, "MclAnim") && animName != nullptr &&
            !al::isEqualString(animName, "None"))
            al::tryStartMclAnimIfExist(this, animName);
    }

    if (mEventType == EventType::PowerPlant || mEventType == EventType::Wait)
        SessionMusicianLocalFunction::tryCreateSessionMusicianManager(this);

    if (SessionMusicianLocalFunction::getMusicianType(this).value() <
        (u32)SessionMusicianType::Vocal) {
        auto* params = new MusicianCameraParams;
        params->offset.set(al::findActorParamF32(this, "カメラ/オフセットX")->value,
                           al::findActorParamF32(this, "カメラ/オフセットY")->value,
                           al::findActorParamF32(this, "カメラ/オフセットZ")->value);
        params->distance = al::findActorParamF32(this, "カメラ/距離");
        params->horizontalAngle = al::findActorParamF32(this, "カメラ/水平角度");
        params->verticalAngle = al::findActorParamF32(this, "カメラ/垂直角度");
        mMusicianCameraParams = params;
    }

    const char* instrumentName = getInstrumentArchiveName(initInfo);
    if (instrumentName != nullptr && !al::isEqualString(instrumentName, "Drum") &&
        !al::isEqualString(instrumentName, "Guitar") &&
        !al::isEqualString(instrumentName, "Bass") && !al::isEqualString(instrumentName, "Trumpet"))
        instrumentName = nullptr;

    mTalkNpcParam = rs::initTalkNpcParam(this, instrumentName);

    mWaitAnimName.assign("");
    if (instrumentName != nullptr) {
        std::stringstream ss;
        ss << "Play" << instrumentName << "Action";
        mSabiAnimName = ss.str();
    }

    al::registActorToDemoInfo(this, initInfo);
    al::initJointControllerKeeper(this, 1);
    mNpcJoint = rs::tryCreateAndAppendNpcJointLookAtController(this, mTalkNpcParam);

    if (mEventType == EventType::Ceremony) {
        al::setNerve(this, &NrvHostType.WaitNoEventFlow);
        al::isExistModelResourceYaml(this, "DanceAnimInfo", nullptr);
        mRhythmInfo = new CityManRhythmInfo(
            this, al::getModelResourceYaml(this, "DanceAnimInfo", nullptr), false, 0.0f);
        makeActorAlive();
        return;
    }

    if (al::isEqualString("Drum", getInstrumentArchiveName(initInfo) != nullptr ?
                                      getInstrumentArchiveName(initInfo) :
                                      ""))
        mEventFlowExecutor = rs::initEventFlowSuffix(this, initInfo, "Drum", "SessionMusicianNpc",
                                                     EventType(mEventType).text());
    else
        mEventFlowExecutor =
            rs::initEventFlow(this, initInfo, "SessionMusicianNpc", EventType(mEventType).text());

    al::initEventReceiver(mEventFlowExecutor, this);
    rs::initEventParam(mEventFlowExecutor, mTalkNpcParam, nullptr);

    mLinkedShineIndex = GameDataFunction::tryFindLinkedShineIndex(this, initInfo);

    s32 fanCount = al::calcLinkChildNum(initInfo, "Fan");
    if (fanCount > 0)
        mFanActors.allocBuffer(fanCount, nullptr);
    for (s32 i = 0; i < fanCount; i++) {
        al::LiveActor* fan = al::createLinksActorFromFactory(initInfo, "Fan", i);
        fan->makeActorDead();
        al::registActorToDemoInfo(fan, initInfo);
        mFanActors.pushBack(fan);
    }

    if (mEventType == EventType::Live) {
        rs::startEventFlow(mEventFlowExecutor, "Init");
        makeActorDead();
        return;
    }

    initCamera(this, initInfo, mEventFlowExecutor, mMusicianCameraParams);

    rs::initEventQueryJudge(mEventFlowExecutor, this);

    rs::startEventFlow(mEventFlowExecutor, getEventTypeEventFlowString(this));

    mWarpAgent = new SessionMusicianWarpAgent(this, initInfo);
    SessionMusicianLocalFunction::entryMusicianToManager(this);

    if (!SessionMusicianLocalFunction::isAlreadySessionMember(this) &&
        SessionMusicianLocalFunction::getMusicianType(this) != SessionMusicianType::Vocal) {
        for (s32 i = 0; i < mFanActors.size(); i++)
            mFanActors[i]->appear();

        if (SessionMusicianLocalFunction::isMusicianType(this, SessionMusicianType::Drum)) {
            al::PartsModel* drumSubActor = (al::PartsModel*)al::getSubActor(this, "ドラム");
            drumSubActor->setIsUpdate(false);
        }

        al::listenStageSwitchOnStart(
            this, SessionMusicianNpcFunctor(this, &SessionMusicianNpc::startEvent));
        al::tryOnStageSwitch(this, "SwitchPlayingKeepOn");

        al::ByamlIter bgmIter;
        if (al::tryGetActorInitFileIter(&bgmIter, this, "BgmRhythmSyncInfo", nullptr))
            mBgmSync = BgmAnimeSynchronizer::tryCreate(this, bgmIter);

        makeActorAlive();
        return;
    }
    makeActorDead();
}

void SessionMusicianNpc::startEvent() {
    if ((al::isValidStageSwitch(this, "SwitchRouteGuideKeepOn")) &&
        !(al::isOnStageSwitch(this, "SwitchRouteGuideKeepOn")))
        al::tryOnStageSwitch(this, "SwitchRouteGuideKeepOn");
}

void SessionMusicianNpc::appear() {
    al::LiveActor::appear();
    if (SessionMusicianLocalFunction::isMusicianType(this, SessionMusicianType::Vocal)) {
        al::hideModel(this);
        al::invalidateHitSensors(this);
    }
}

void SessionMusicianNpc::kill() {
    if (mEventType == EventType::Wait && mFanActors.size() > 0)
        for (s32 i = 0; i < mFanActors.size(); i++)
            mFanActors[i]->kill();

    al::tryOffStageSwitch(this, "SwitchRouteGuideKeepOn");
    al::tryOffStageSwitch(this, "SwitchPlayingKeepOn");
    al::LiveActor::kill();
}

void SessionMusicianNpc::attackSensor(al::HitSensor* self, al::HitSensor* other) {
    rs::attackSensorNpcCommon(self, other);
}

// Matched with help from german77
bool SessionMusicianNpc::receiveMsg(const al::SensorMsg* message, al::HitSensor* other,
                                    al::HitSensor* self) {
    if (rs::tryReceiveMsgPlayerDisregard(message, self, mTalkNpcParam))
        return true;
    if (rs::isMsgPlayerDisregardHomingAttack(message))
        return true;
    if (rs::isMsgPlayerDisregardTargetMarker(message))
        return true;

    if (al::isNerve(this, &NrvHostType.Wait) || al::isNerve(this, &NrvHostType.WaitNoEventFlow) ||
        (al::isNerve(this, &NrvHostType.Reaction) && !al::isNewNerve(this))) {
        bool result;
        if (rs::isInvalidTrampleSensor(self, mTalkNpcParam))
            result = mNpcStateReaction->receiveMsgWithoutTrample(message, other, self);
        else
            result = mNpcStateReaction->receiveMsg(message, other, self);

        if (result) {
            al::setNerve(this, &NrvHostType.Reaction);
            return true;
        }
    }

    if (mNpcStateReaction->receiveMsgNoReaction(message, other, self))
        return true;

    if (SessionMusicianLocalFunction::getMusicianType(this) == SessionMusicianType::Vocal) {
        if (al::isMsgBindStart(message))
            return true;

        if (al::isMsgBindInit(message)) {
            mPuppet = rs::startPuppet(self, other);
            return true;
        }
    }

    return rs::isMsgCapAttack(message);
}

// Matched with help from german77
bool SessionMusicianNpc::receiveEvent(const al::EventFlowEventData* event) {
    if (al::isEventName(event, "ShineGet")) {
        al::setNerve(this, &WarpStart);
        return true;
    }

    if (mEventType == EventType::Live && al::isEventName(event, "AppearFan")) {
        if (mFanActors.size() < 1)
            return true;

        for (s32 i = 0; i < mFanActors.size(); i++)
            mFanActors[i]->appear();

        return true;
    }

    if (al::isEventName(event, "Joined")) {
        mIsJoined = true;

        SessionEventProgress newProgress = SessionEventProgress::Entry;

        switch (GameDataFunction::getSessionEventProgress(this)) {
        case SessionEventProgress::Wait1stMusician:
            newProgress = SessionEventProgress::Wait2ndMusician;
            break;
        case SessionEventProgress::Wait2ndMusician:
            newProgress = SessionEventProgress::Wait3rdMusician;
            break;
        case SessionEventProgress::Wait3rdMusician:
            newProgress = SessionEventProgress::Wait4thMusician;
            break;
        case SessionEventProgress::Wait4thMusician:
            newProgress = SessionEventProgress::RequestGoToThePowerPlant;
            break;
        case SessionEventProgress::WaitThePowerPlantWorks:
            newProgress = SessionEventProgress::TheCeremonyIsReady;
            rs::setSceneStatusInvalidSave(this);
            break;
        default:
            goto end;
        }

        GameDataFunction::setSessionEventProgress(this, newProgress);

    end:
        al::tryOffStageSwitch(this, "SwitchRouteGuideKeepOn");
        return true;
    }

    return false;
}

const char* SessionMusicianNpc::judgeQuery(const char* query) const {
    if (al::isEqualString(query, "JudgeMusicianType"))
        return SessionMusicianLocalFunction::getMusicianType(this).text();
    return nullptr;
}

void SessionMusicianNpc::endClipped() {
    mIsNeedRythmResync = true;
    al::LiveActor::endClipped();
}

void SessionMusicianNpc::forceControlForDance() {
    if (mRhythmInfo->getNextAnimIndex() <= 0) {
        mAnimBeatFrameOffset = -1.0f;
        s32 animId = mRhythmInfo->getCurAnimId();
        if (animId < 0)
            return;
        if (animId >= 2) {
            mSabiAnimName = sSabiAnimNames[animId - 2];
            al::setNerve(this, &NrvHostType.WaitNoEventFlowSabi);
        } else {
            mWaitAnimName = sWaitAnimNames[animId];
            al::setNerve(this, &NrvHostType.WaitNoEventFlow);
        }
        return;
    }

    s32 index = mRhythmInfo->getNextAnimIndex() - 1;
    s32 animId = mRhythmInfo->getAnimId(index);
    f32 prevBeat = mRhythmInfo->getPrevBeat();
    f32 animBeat = mRhythmInfo->getAnimBeat(index);
    mAnimBeatFrameOffset = ((prevBeat - animBeat) * 3456.0f) / 204.0f;

    if (animId < 0)
        return;
    if (animId >= 2) {
        mSabiAnimName = sSabiAnimNames[animId - 2];
        al::setNerve(this, &NrvHostType.WaitNoEventFlowSabi);
    } else {
        mWaitAnimName = sWaitAnimNames[animId];
        al::setNerve(this, &NrvHostType.WaitNoEventFlow);
    }
}

void SessionMusicianNpc::control() {
    if (mNpcJoint != nullptr)
        rs::updateNpcJointLookAtController(mNpcJoint);

    if (mRhythmInfo != nullptr) {
        mRhythmInfo->update(mIsNeedRythmResync);

        if (!mRhythmInfo->isOutOfSync() && !mIsNeedRythmResync) {
            controlForDance();
        } else {
            mIsNeedRythmResync = false;
            forceControlForDance();
        }

        if (mBgmSync != nullptr)
            mBgmSync->trySyncBgm();
    }
}

void SessionMusicianNpc::controlForDance() {
    mAnimBeatFrameOffset = -1.0f;

    s32 animId = mRhythmInfo->getCurAnimId();
    if (animId < 0)
        return;

    if (animId >= 2) {
        mSabiAnimName = sSabiAnimNames[animId - 2];
        al::setNerve(this, &NrvHostType.WaitNoEventFlowSabi);
    } else {
        mWaitAnimName = sWaitAnimNames[animId];
        al::setNerve(this, &NrvHostType.WaitNoEventFlow);
    }
}

bool SessionMusicianNpc::isJoined() const {
    if (isDead(this))
        return false;
    return mIsJoined;
}

bool SessionMusicianNpc::isStateWarp() const {
    if (al::isNerve(this, &WarpEnd))
        return false;
    return mIsJoined;
}

void SessionMusicianNpc::doneWarp() {
    al::setNerve(this, &WarpEnd);
}

bool SessionMusicianNpc::isEnableMuteBgmTrack() const {
    if (mEventType == EventType::Live)
        return true;
    if (mEventType == EventType::Ceremony && mIsUseBgmTrackMute)
        return true;
    return false;
}

void SessionMusicianNpc::exeWaitNoEventFlowSabi() {
    if (al::isFirstStep(this)) {
        al::startAction(this, mSabiAnimName.c_str());

        if (mAnimBeatFrameOffset > 0.0f) {
            al::getActionFrameMax(this, mSabiAnimName.c_str());
            al::setActionFrame(this, mAnimBeatFrameOffset);
        }
    }

    if (al::isActionEnd(this))
        al::setNerve(this, &NrvHostType.WaitNoEventFlow);
}

void SessionMusicianNpc::exeWaitNoEventFlow() {
    if (!al::isFirstStep(this))
        return;

    al::validateClipping(this);

    al::startAction(this, mWaitAnimName.c_str());

    if (mAnimBeatFrameOffset > 0.0f)
        al::setActionFrame(this, mAnimBeatFrameOffset);
}

void SessionMusicianNpc::exeWait() {
    if (al::isFirstStep(this))
        al::validateClipping(this);

    rs::updateEventFlow(mEventFlowExecutor);
}

void SessionMusicianNpc::exeWarpStart() {
    if (al::isFirstStep(this)) {
        if (SessionMusicianLocalFunction::getMusicianType(this) == SessionMusicianType::Vocal)
            rs::requestBindPlayer(this, al::getHitSensor(this, "Bind"));

        al::tryOffStageSwitch(this, "SwitchRouteGuideKeepOn");
        al::tryOffStageSwitch(this, "SwitchPlayingKeepOn");
    }

    if (SessionMusicianLocalFunction::getMusicianType(this) != SessionMusicianType::Vocal ||
        !al::isLessStep(this, 0))
        if (mWarpAgent->tryStartWarp())
            al::setNerve(this, &Warp);
}

void SessionMusicianNpc::exeWarp() {}

void SessionMusicianNpc::exeWarpEnd() {
    kill();
}

// With help from german77
void controlBgmTrack(const SessionMusicianNpc* actor, bool isEnd) {
    if (SessionMusicianLocalFunction::isMusicianType(actor, SessionMusicianType::Drum)) {
        isEnd ? SessionMusicianLocalFunction::endPlayingTheDs(actor) :
                SessionMusicianLocalFunction::startPlayingTheDs(actor);
        return;
    }
    if (SessionMusicianLocalFunction::isMusicianType(actor, SessionMusicianType::Bass)) {
        isEnd ? SessionMusicianLocalFunction::endPlayingTheBa(actor) :
                SessionMusicianLocalFunction::startPlayingTheBa(actor);
        return;
    }
    if (SessionMusicianLocalFunction::isMusicianType(actor, SessionMusicianType::Guitar)) {
        isEnd ? SessionMusicianLocalFunction::endPlayingTheGt(actor) :
                SessionMusicianLocalFunction::startPlayingTheGt(actor);
        return;
    }
    if (SessionMusicianLocalFunction::isMusicianType(actor, SessionMusicianType::Trumpet)) {
        isEnd ? SessionMusicianLocalFunction::endPlayingTheTp(actor) :
                SessionMusicianLocalFunction::startPlayingTheTp(actor);
        return;
    }
}

// With help from german77
void SessionMusicianNpc::exeReaction() {
    if (al::isFirstStep(this)) {
        al::invalidateClipping(this);

        if (mEventType == EventType::Live ||
            (mEventType == EventType::Ceremony && mIsUseBgmTrackMute))
            controlBgmTrack(this, true);
    }

    if (mEventFlowExecutor != nullptr &&
        rs::checkEnableStartEventAndCancelReaction(this, mTalkNpcParam)) {
        rs::updateEventFlow(mEventFlowExecutor);

        if (!al::isActionOneTime(this)) {
            al::setNerve(this, &NrvHostType.Wait);
            return;
        }
    }

    if (al::updateNerveState(this)) {
        switch (mEventType) {
        case EventType::Live:
            al::startAction(this, "PlaySeOff");
            al::setNerve(this, &NrvHostType.WaitNoEventFlow);
            break;
        case EventType::Ceremony:
            al::startAction(this, mWaitAnimName.c_str());
            al::setNerve(this, &NrvHostType.WaitNoEventFlow);
            break;

        default:
            al::startAction(this, "PlaySeOn");
            al::setNerve(this, &NrvHostType.Wait);
            break;
        }
    }
}

void SessionMusicianNpc::endReaction() {
    if (mEventType == EventType::Live || (mEventType == EventType::Ceremony && mIsUseBgmTrackMute))
        controlBgmTrack(this, false);
}

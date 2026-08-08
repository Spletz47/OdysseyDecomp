#include "Library/Audio/System/AudioKeeper.h"

#include "Library/LiveActor/ActorAreaFunction.h"

namespace al {

AudioGeneralPurposeAreaChecker::AudioGeneralPurposeAreaChecker(const char* areaName)
    : mAreaName(areaName) {}

void AudioGeneralPurposeAreaChecker::reset() {
    mIsEnteredArea = false;
    mIsExitedArea = false;
    mIsAreaChanged = false;
    unk = nullptr;
    mCurArea = nullptr;
    mPrevArea = nullptr;
}

bool AudioGeneralPurposeAreaChecker::isInArea() const {
    if (mAreaName == nullptr)
        return false;
    if (mPlayerHolder != nullptr)
        return false;
    if (mAreaObjDirector == nullptr)
        return false;

    return al::tryFindAreaObjPlayerOne(mPlayerHolder, mAreaName);
}

}  // namespace al

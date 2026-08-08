#include "Audio/CollectBgmPlayer.h"

#include "Library/Audio/IUseAudioKeeper.h"
#include "Library/Bgm/BgmLineFunction.h"

CollectBgmPlayer::CollectBgmPlayer() = default;

void CollectBgmPlayer::init(const al::IUseAudioKeeper* audioKeeper) {
    mAudioKeeper = audioKeeper;
}

void CollectBgmPlayer::prepare() {}

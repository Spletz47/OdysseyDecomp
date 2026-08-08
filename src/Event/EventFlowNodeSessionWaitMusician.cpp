#include "Event/EventFlowNodeSessionWaitMusician.h"

#include "Library/Event/EventFlowFunction.h"

#include "Npc/SessionMusicianLocalFunction.h"

EventFlowNodeSessionWaitMusician::EventFlowNodeSessionWaitMusician(const char* name) : EventFlowNode(name) {}

void EventFlowNodeSessionWaitMusician::init(const al::EventFlowNodeInitInfo& info) {
    al::initEventFlowNode(this, info);
    al::initEventQuery(this, info);
    mCount = al::getParamIterKeyInt(info, "Count");
}

s32 EventFlowNodeSessionWaitMusician::getNextId() const {
    if (mCount > SessionMusicianLocalFunction::getMemberMusicianNum(getActor()))
        return al::getCaseEventNextId(this, 1);
    return al::getCaseEventNextId(this, 0);
}

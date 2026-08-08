#pragma once

#include "Library/Event/EventFlowNode.h"

class EventFlowNodeSessionWaitMusician : public al::EventFlowNode {
public:
    EventFlowNodeSessionWaitMusician(const char* name);

    void init(const al::EventFlowNodeInitInfo& info) override;
    s32 getNextId() const override;
    void start() override { EventFlowNode::start(); }
private:
    s32 mCount = 0;
};

static_assert(sizeof(EventFlowNodeSessionWaitMusician) == 0x70);

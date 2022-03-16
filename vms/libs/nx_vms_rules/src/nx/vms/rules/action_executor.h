// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "rules_fwd.h"

namespace nx::vms::rules {

class Engine;

/**
 * Action executors are used to execute actions (provided by rules engine in common format)
 * by accessing different components of VMS, which are external for VMS rules engine.
 */
class NX_VMS_RULES_API ActionExecutor: public QObject
{
    Q_OBJECT

public:
    virtual void initialize(Engine* engine) {};
    virtual void execute(const ActionPtr& action) = 0;
};

} // namespace nx::vms::rules

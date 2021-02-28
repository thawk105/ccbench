//
// Created by thawk on 2020/11/10.
//

#pragma once

#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG // NOLINT
#endif

#include "spdlog/spdlog.h"

namespace ccbench {

static inline void setup_spdlog() {

#ifdef NDEBUG
    spdlog::set_level(spdlog::level::info);
#else
    spdlog::set_level(spdlog::level::debug);
#endif

}

} // namespace ccbench

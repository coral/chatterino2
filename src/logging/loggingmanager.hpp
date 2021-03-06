#pragma once

#include "loggingchannel.hpp"

#include <memory>

namespace chatterino {
namespace logging {

void init();
std::shared_ptr<Channel> get(const QString &channelName);

}  // namespace logging
}  // namespace chatterino

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>
#include <string>

namespace nx {
namespace kit {
namespace apple_utils {

std::vector<std::string> getProcessCmdLineArgs();
const std::string getPathToExecutable();

// Get a path to NSApplicationSupportDirectory.
std::string getAppDataLocation();

} // namespace apple_utils
} // namespace kit
} // namespace nx

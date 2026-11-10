#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using VisitCaptions = std::vector<std::string>;
using VisitCaptionsByContext = std::unordered_map<std::string, VisitCaptions>;

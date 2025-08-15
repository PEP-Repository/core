#pragma once

#include <unordered_map>

using VisitCaptions = std::vector<std::string>;
using VisitCaptionsByContext = std::unordered_map<std::string, VisitCaptions>;

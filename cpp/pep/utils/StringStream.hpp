#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace pep {

std::string_view GetUnparsed(std::istringstream& ss);
std::string GetUnparsed(std::istringstream&& ss);

}

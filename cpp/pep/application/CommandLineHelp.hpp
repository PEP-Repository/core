#pragma once

#include <iostream>

namespace pep {
namespace commandline {

void WriteHelpItem(std::ostream& destination, const std::string& entry, const std::string& description);
void WriteHelpItemSupplement(std::ostream& destination, const std::string& text);

}
}

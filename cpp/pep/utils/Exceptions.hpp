#pragma once

#include <exception>
#include <string>

namespace pep {

std::string GetExceptionMessage(std::exception_ptr source);

[[noreturn]] void Terminate(std::exception_ptr source) noexcept;

}

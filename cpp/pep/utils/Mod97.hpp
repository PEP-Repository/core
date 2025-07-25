#pragma once

#include <string>

namespace pep {

class Mod97 {
 public:
  /*!
   * \brief Compute check digits (2 characters) using the MOD 97-10 method (ISO 7064). This is the same method as used to verify IBAN numbers. Spaces and '-' characters are ignored and all alpha characters are converted to uppercase.
   *
   * \param in p_in:...
   * \return std::__cxx11::string
   */
  static std::string ComputeCheckDigits(const std::string& in);

  /*!
   * \brief Verify the check digits in the provided string using the MOD 97-10 method (ISO 7064). The check digits should be the last two characters of the string. Spaces and '-' characters are ignored and all alpha characters are converted to uppercase.
   *
   * \param in p_in:...
   * \return bool
   */
  static bool Verify(const std::string& in);
};
}

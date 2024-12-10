#include <pep/utils/Mod97.hpp>

#include <cctype>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace pep {
// Based on https://rosettacode.org/wiki/IBAN#C.2B.2B
std::string Mod97::ComputeCheckDigits(const std::string& in) {
  std::string working(in);

  // Remove characters '-' and ' ' from input
  boost::algorithm::erase_all(working, "-");
  boost::algorithm::erase_all(working, " ");

  if (!all(working, boost::algorithm::is_alnum())) {
    throw std::runtime_error("Input contains invalid characters");
  }

  // Convert input to all upper case
  boost::algorithm::to_upper(working);

  // Convert all characters to their numeric values
  std::string numberstring;
  for (const auto c: working) {
    if (std::isdigit(c)) {
      numberstring += c  ;
    } else if (std::isupper(c)) {
      numberstring += std::to_string(static_cast<int>(c) - 55);
    }
  }
  // Concatenate the input with '00' to be able to compute the final check digit
  numberstring += "00";

  // A stepwise computation for mod 97 in chunks of 9 at the first time
  // , then in chunks of seven prepended by the last mod 97 operation converted
  // to a string
  size_t segstart = 0;
  size_t step = 9;
  std::string prepended;
  while(segstart  < numberstring.length() - step) { // TODO: fix integer wrap in calculation: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1945#note_22341
    auto number = std::stol(prepended + numberstring.substr(segstart , step)) ;
    int remainder = static_cast<int>(number % 97);
    prepended =  remainder < 10 ? "0" + std::to_string(remainder) : std::to_string(remainder);
    segstart = segstart + step;
    step = 7 ;
  }

  // Compute final check digit
  auto number = 98 - (std::stol(prepended + numberstring.substr(segstart)) % 97);

  if(number < 10) {
    // Prepend '0' to output
    return "0" + std::to_string(number);
  } else {
    return std::to_string(number);
  }
}

bool Mod97::Verify(const std::string& in) {
  // The last two characters of the string should contain the checkdigits
  const size_t CHECK_DIGIT_COUNT = 2;
  auto inLength = in.length();
  if (inLength < CHECK_DIGIT_COUNT) {
    return false;
  }
  auto checkDigitProvided = in.substr(inLength - CHECK_DIGIT_COUNT, CHECK_DIGIT_COUNT);

  auto toCheck = in.substr(0, inLength - CHECK_DIGIT_COUNT);
  std::string checkDigitComputed;
  try {
    checkDigitComputed = ComputeCheckDigits(toCheck);
  }
  catch (const std::out_of_range&) {
    // String too short for our Mod97::ComputeCheckDigits: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1945
    return false;
  }

  return checkDigitComputed == checkDigitProvided;
}
}

#pragma once

#include <pep/rsk-pep/DataTranslationKeys.hpp>
#include <pep/rsk-pep/PseudonymTranslationKeys.hpp>

#include <boost/property_tree/ptree_fwd.hpp>

namespace pep {

[[nodiscard]] PseudonymTranslationKeys ParsePseudonymTranslationKeys(const boost::property_tree::ptree& config);
[[nodiscard]] DataTranslationKeys ParseDataTranslationKeys(const boost::property_tree::ptree& config);

} // namespace pep

#pragma once

#include <pep/utils/BuildFlavor.hpp>
#include <pep/utils/Log.hpp>
#include <pep/crypto/Timestamp.hpp>

#include <map>
#include <unordered_map>
#include <boost/property_tree/ptree_fwd.hpp>

#define PULLCASTOR_LOG(level) LOG("PullCastor", level)

namespace pep {
namespace castor {

/*!
* \brief A map type that depends on the build type: std::unordered_map in release builds for speed; (ordered) std::map in debug builds for ease of item lookup in the debugger.
*/
#if BUILD_HAS_DEBUG_FLAVOR()
  template <typename TKey, typename TValue>
  using UnOrOrderedMap = std::map<TKey, TValue>;
#else
  template <typename TKey, typename TValue>
  using UnOrOrderedMap = std::unordered_map<TKey, TValue>;
#endif

/*!
* \brief Produces a Timestamp instance corresponding with the Date+Time stored by Castor in (the source JSON of) the specified PTree.
* \param datetimeObject The Ptree containing details on the Date+Time.
* \return A Timestamp instance representing the Ptree's Date+Time.
* \remark Defined here instead of in the pep::castor library because it depends on tz (and associated stuff), which we don't want to deploy on every target system.
*/
Timestamp ParseCastorDateTime(const boost::property_tree::ptree& datetimeObject);

}
}

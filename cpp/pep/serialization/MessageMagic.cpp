#include <pep/utils/Log.hpp>
#include <pep/serialization/MessageMagic.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/serialization/SerializeException.hpp>

#include <boost/algorithm/string/erase.hpp>
#include <xxhash.h>
#include <pep/utils/Raw.hpp>

namespace pep {

std::string DescribeMessageMagic(MessageMagic magic) {
  return BasicMessageMagician::DescribeMessageMagic(magic)
    .value_or("<UNKNOWN MESSAGE TYPE: " + std::to_string(magic) + ">");
}

std::string DescribeMessageMagic(std::string_view str) {
  return DescribeMessageMagic(GetMessageMagic(str));
}

MessageMagic CalculateMessageMagic(std::string_view crossPlatformName) {
  return XXH32(crossPlatformName.data(), crossPlatformName.length(), 0xcafebabe);
}

MessageMagic GetMessageMagic(std::string_view str) {
  // Make sure input is long enough to at least read the magic
  if (str.length() < sizeof(MessageMagic)) {
    LOG("GetMessageMagic", severity_level::warning) << "Received a message which is shorter than " << sizeof(MessageMagic) << " bytes";
    throw SerializeException("Invalid message: too short");
  }
  static_assert(sizeof(MessageMagic) == sizeof(std::uint32_t));
  return UnpackUint32BE(str);
}

MessageMagic PopMessageMagic(std::string& str) {
  auto magic = GetMessageMagic(str);
  str.erase(0,sizeof(MessageMagic));
  return magic;
}

std::unordered_map<MessageMagic, std::string>& BasicMessageMagician::Mappings() {
  static std::unordered_map<MessageMagic, std::string> result;
  return result;
}

MessageMagic BasicMessageMagician::RegisterMessageName(const std::string& crossPlatformName) {
  auto result = CalculateMessageMagic(crossPlatformName);
  auto emplaced = Mappings().emplace(result, crossPlatformName);
  if (!emplaced.second) {
    throw std::runtime_error("Duplicate message magic registered for types " + emplaced.first->second + " and " + crossPlatformName);
  }
  return result;
}

std::string_view BasicMessageMagician::SkipMessageMagic(std::string_view szMessage, MessageMagic requiredMagic) {
  auto dwObjectMagic = GetMessageMagic(szMessage);
  if (dwObjectMagic != requiredMagic) {
    LOG("BasicMessageMagician::SkipMessageMagic", severity_level::error) << "Unknown object magic " << dwObjectMagic;
    throw SerializeException("Error parsing message");
  }
  return szMessage.substr(sizeof(MessageMagic));
}

void BasicMessageMagician::WriteMagicTo(std::ostream& destination, MessageMagic magic) {
  WriteBinary(destination, magic);
}

std::optional<std::string> BasicMessageMagician::DescribeMessageMagic(MessageMagic magic) {
  const auto& mappings = Mappings();
  auto pos = mappings.find(magic);
  if (pos == mappings.cend()) {
    return std::nullopt;
  }
  return pos->second;
}

}

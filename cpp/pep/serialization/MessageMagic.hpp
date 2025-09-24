#pragma once

#include <pep/serialization/NormalizedTypeNaming.hpp>
#include <pep/utils/SelfRegistering.hpp>

#include <optional>
#include <ostream>
#include <unordered_map>

namespace pep {

using MessageMagic = uint32_t;

MessageMagic CalculateMessageMagic(const std::string& crossPlatformName);

MessageMagic GetMessageMagic(const std::string_view& str);
MessageMagic PopMessageMagic(std::string& str);

std::string DescribeMessageMagic(const std::string& str);
std::string DescribeMessageMagic(MessageMagic magic);

template <typename TMessage>
struct MessageMagician;

/*!
* \brief Base class for MessageMagician<>. Add non-template methods here to prevent template-induced code bloat.
*/
class BasicMessageMagician {
  template <typename TMessage>
  friend struct MessageMagician;

  template <class TDerived, class TRegistrar, bool REGISTER>
  friend class SelfRegistering;

private:
  template <typename T>
  struct MessageOfMagician;

  template <typename T>
  struct MessageOfMagician<MessageMagician<T>> {
    using type = T;
  };

  static std::unordered_map<MessageMagic, std::string>& Mappings();
  static MessageMagic RegisterMessageName(const std::string& crossPlatformName);

  template <typename TMagician>
  static MessageMagic RegisterType() {
    using MessageType = typename MessageOfMagician<TMagician>::type;
    return RegisterMessageName(GetNormalizedTypeName<MessageType>());
  }

  static void WriteMagicTo(std::ostream& destination, MessageMagic magic);
  static std::string_view SkipMessageMagic(std::string_view szMessage, MessageMagic requiredMagic);

public:
  static std::optional<std::string> DescribeMessageMagic(MessageMagic magic);
};

template <typename TMessage>
struct MessageMagician : public SelfRegistering<MessageMagician<TMessage>, BasicMessageMagician> {
  static inline MessageMagic GetMagic() { return SelfRegistering<MessageMagician<TMessage>, BasicMessageMagician>::REGISTRATION_ID; }
  static inline void WriteMagicTo(std::ostream& destination) { BasicMessageMagician::WriteMagicTo(destination, GetMagic()); }
  static inline std::string_view SkipMessageMagic(std::string_view szMessage) { return BasicMessageMagician::SkipMessageMagic(szMessage, GetMagic()); }
};

}

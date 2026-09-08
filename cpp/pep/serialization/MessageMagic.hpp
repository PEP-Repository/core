#pragma once

#include <pep/utils/NormalizedTypeNaming.hpp>

#include <optional>
#include <ostream>

namespace pep {

using MessageMagic = uint32_t;

MessageMagic CalculateMessageMagic(std::string_view crossPlatformName);

MessageMagic GetMessageMagic(std::string_view str);
MessageMagic PopMessageMagic(std::string& str);

std::string DescribeMessageMagic(std::string_view str);
std::string DescribeMessageMagic(MessageMagic magic);

template <typename TMessage>
struct MessageMagician;

/// \brief Base class for MessageMagician<>. Add non-template methods here to prevent template-induced code bloat.
class BasicMessageMagician {
  template <typename TMessage>
  friend struct MessageMagician;

private:
  static MessageMagic EnsureRegistered(const std::string& crossPlatformName);

  template <typename TMessage>
  static MessageMagic EnsureRegistered() {
    return EnsureRegistered(GetNormalizedTypeName<TMessage>());
  }

  static void WriteMagicTo(std::ostream& destination, MessageMagic magic);
  static std::string_view SkipMessageMagic(std::string_view szMessage, MessageMagic requiredMagic);

public:
  static std::optional<std::string> DescribeMessageMagic(MessageMagic magic);
};

template <typename TMessage>
struct MessageMagician : public BasicMessageMagician {
  static inline MessageMagic GetMagic();
  static inline void WriteMagicTo(std::ostream& destination) { BasicMessageMagician::WriteMagicTo(destination, GetMagic()); }
  static inline std::string_view SkipMessageMagic(std::string_view szMessage) { return BasicMessageMagician::SkipMessageMagic(szMessage, GetMagic()); }
};

template <typename TMessage>
MessageMagic MessageMagician<TMessage>::GetMagic() {
  // Use a static variable to prevent run time overhead when this function is called.
  // Use a function-scoped (as opposed to class-scoped) static so that BasicMessageMagician::EnsureRegistered<> doesn't run at static initialization time,
  // when its assert()ion would presumably wreak havoc.
  static const MessageMagic result = BasicMessageMagician::EnsureRegistered<TMessage>();
  return result;
}

}

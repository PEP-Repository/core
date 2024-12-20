#pragma once

#include <pep/utils/Log.hpp>
#include <pep/serialization/MessageMagic.hpp>
#include <pep/serialization/SerializeException.hpp>

#include <istream>
#include <sstream>
#include <cstdint>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

namespace pep {

template <typename T>
class MessageSerializer {
public:
  MessageSerializer() = default;
  virtual ~MessageSerializer() = default;

  MessageSerializer(const MessageSerializer&) = default;
  MessageSerializer& operator=(const MessageSerializer&) = default;

  virtual void serializeToOstream(std::ostream& destination, T value) const = 0;
  virtual T parseFromIstream(std::istream& source) const = 0;

  virtual std::string toString(T value, bool withMagic = true) const {
    std::stringstream out;
    if (withMagic) {
      MessageMagician<T>::WriteMagicTo(out);
    }
    serializeToOstream(out, std::move(value));
    return out.str();
  }

  virtual T fromString(std::string_view szMessage, bool withMagic = true) const {
    if (withMagic) {
      szMessage = MessageMagician<T>::SkipMessageMagic(szMessage);
    }

    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> in(szMessage.data(), szMessage.size());
    try {
      return parseFromIstream(in);
    }
    catch (const SerializeException& e) {
      LOG("MessageSerializer::fromString", severity_level::error) << "Caught SerializeException: " << e.what();
      throw;
    }
  }
};

}

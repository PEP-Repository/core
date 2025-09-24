#include <gtest/gtest.h>

#include <pep/serialization/Serialization.hpp>
#include <pep/serialization/ErrorSerializer.hpp>

namespace {

class DerivedTestError : public pep::DeserializableDerivedError<DerivedTestError> {
public:
  DerivedTestError(const std::string& description)
    : pep::DeserializableDerivedError<DerivedTestError>(description) {
  }
};

TEST(Error, DeserializesToDerivedType) {
  // Throw a derived error type...
  std::string serialized;
  try {
    throw DerivedTestError("Nothing to see here. Move along.");
  }
  catch (const pep::Error& error) { // ... and catch and serialize it as a base Error instance
    serialized = pep::Serialization::ToString(error);
  }

  ASSERT_EQ(pep::GetMessageMagic(serialized), pep::MessageMagician<pep::Error>::GetMagic()) << "(Type derived from) DeserializableDerivedError<> should be serialized as its base (Error) type";
  ASSERT_THROW(pep::Error::ThrowIfDeserializable(serialized), DerivedTestError) << "Rethrow should produce the original (derived) Error type";
}

}

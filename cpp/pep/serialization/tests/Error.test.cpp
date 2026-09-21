#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>
#include <gtest/gtest.h>

namespace {

struct MoreSpecificError : public pep::Error {
  static const std::string Message;
  explicit MoreSpecificError() : pep::Error(Message) {}
};

const std::string MoreSpecificError::Message = "Created as a MoreSpecificError instance";

} // End anonymous namespace


TEST(Error, DerivedClassesDeserializeToBase) {
  // No serializer exists for MoreSpecificError, so we serialize it as its base pep::Error class.
  // This mimics the behavior of Scheduler::queueNextBatch, which catches (thrown) `pep::Error` instances and
  // (serializes and) sends those base class instances across the network.
  auto serialized = pep::Serialization::ToString<pep::Error>(MoreSpecificError());

  try {
    pep::Error::ThrowIfDeserializable(serialized);
  }
  catch (const MoreSpecificError&) {
    // Demonstrates a limitation instead of specifying a requirement:
    // feel free to remove this test case if/when you (re-)add support for derived type (de)serialization.
    // See https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/2563#note_63843
    FAIL() << "Error class (de)serialization does not support derived type re-instantiation";
  }
  catch (const pep::Error& deserialized) {
    EXPECT_EQ(MoreSpecificError::Message, deserialized.what())
      << "Deserialized (base type) Error instance should have the original (derived) instance's description";
  }
  catch (...) {
    FAIL() << "Error class frontend function (deserialized and) threw an instance of a different type";
  }
}

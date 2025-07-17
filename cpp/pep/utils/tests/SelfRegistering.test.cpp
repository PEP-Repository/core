#include <gtest/gtest.h>

#include <pep/utils/tests/SelfRegistering.test.hpp>

using namespace pep::tests;

namespace {

TEST(SelfRegistering, Works) {
  const auto& registered = TestRegistrar::GetRegisteredTypeTraits();

  ASSERT_GT(registered.size(), 0U) << "No supposedly self-registering type registered";
  ASSERT_TRUE(TestRegistrar::KnowsType("UnknownByRegistrarAndTestFunction"));
  ASSERT_TRUE(TestRegistrar::KnowsType("InheritsFromRegistrar")) << "Self registration doesn't work for (multiple) inheritance from registrar type";
  ASSERT_TRUE(TestRegistrar::KnowsType("RegisteredFromScope")) << "Self registration doesn't work for types in (sub)scopes";

  auto external = std::find_if(registered.cbegin(), registered.cend(), [ownFile = std::string(__FILE__)](const TestRegistrar::RegisteredTraits& candidate) {
    return candidate.constructorFile != ownFile;
    });
  ASSERT_NE(external, registered.cend()) << "Class from a different translation unit wasn't registered";
}

class UnknownByRegistrarAndTestFunction : public TestableSelfRegistering<UnknownByRegistrarAndTestFunction> {
public:
  UnknownByRegistrarAndTestFunction() noexcept : TestableSelfRegistering<UnknownByRegistrarAndTestFunction>(__FILE__) {}
};

class InheritsFromRegistrar : public TestRegistrar, public TestableSelfRegistering<InheritsFromRegistrar> {
public:
  InheritsFromRegistrar() noexcept : TestableSelfRegistering<InheritsFromRegistrar>(__FILE__) {}
};

namespace some::scope {

class RegisteredFromScope : public TestableSelfRegistering<RegisteredFromScope> {
public:
  RegisteredFromScope() noexcept : TestableSelfRegistering<RegisteredFromScope>(__FILE__) {}
};

}

}

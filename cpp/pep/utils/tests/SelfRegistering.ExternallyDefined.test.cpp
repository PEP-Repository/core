#include <pep/utils/tests/SelfRegistering.test.hpp>

// Helper class for SelfRegistering.test: allows verification that self-registration works from a separate translation unit
class ExternallyDefinedSelfRegistering : public TestableSelfRegistering<ExternallyDefinedSelfRegistering> {
public:
  ExternallyDefinedSelfRegistering() noexcept : TestableSelfRegistering<ExternallyDefinedSelfRegistering>(__FILE__) {}
};

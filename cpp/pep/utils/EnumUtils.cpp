#include <pep/utils/EnumUtils.hpp>

namespace {

enum class TestedFlags {
  None = 0,
  First = 0b01,
  Second = 0b10,
  All = 0b11,
};

}

PEP_MARK_AS_FLAG_ENUM_TYPE(TestedFlags);

namespace pep {

// the binary operators are trivial, because they just wrap casts to/from the underlying type
// `operator~` is non-trivial: the implementation needs to mask out bits that would be out-of-range
static_assert(~TestedFlags::None == TestedFlags::All, "operator~ follows bitwise inversion");
static_assert(~TestedFlags::First == TestedFlags::Second, "operator~ follows bitwise inversion");
static_assert(~TestedFlags::Second == TestedFlags::First, "operator~ follows bitwise inversion");
static_assert(~TestedFlags::All == TestedFlags::None, "operator~ follows bitwise inversion");

static_assert(TestFlags(TestedFlags::All, TestedFlags::Second),
    "TestFlags(x, y) tests if all flags that are set in y are also set in x");
static_assert(!TestFlags(TestedFlags::First, TestedFlags::Second),
    "TestFlags(x, y) tests if all flags that are set in y are also set in x");
static_assert(TestFlags(TestedFlags::None, TestedFlags::None),
    "Edge Case: TestFlags(t, T::None) is always true");
static_assert(TestFlags(TestedFlags::All, TestedFlags::None),
    "Edge Case: TestFlags(t, T::None) is always true");

static_assert(FlagsIf(TestedFlags::Second, true) == TestedFlags::Second,
    "FlagsIf(x, true) returns x");
static_assert(FlagsIf(TestedFlags::Second, false) == TestedFlags::None,
    "FlagsIf(x, false) returns None");

}

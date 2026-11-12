#include <pep/storagefacility/PrefixedStrings.hpp>
#include <pep/utils/Generator.hpp>

#include <gtest/gtest.h>

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace pep;

namespace {

template <typename TRange>
constexpr bool CanWritePrefixedStrings = requires(std::ostream& out, TRange&& range) {
  WritePrefixedStrings(out, std::forward<TRange>(range));
};

// WritePrefixedStrings accepts (lvalue and rvalue) ranges of PrefixedStringsEntry, but nothing else
static_assert(CanWritePrefixedStrings<std::vector<PrefixedStringsEntry>&>);
static_assert(CanWritePrefixedStrings<const std::vector<PrefixedStringsEntry>&>);
static_assert(CanWritePrefixedStrings<std::vector<PrefixedStringsEntry>>);
static_assert(CanWritePrefixedStrings<Generator<PrefixedStringsEntry>>);
static_assert(!CanWritePrefixedStrings<std::vector<std::string>>);
static_assert(!CanWritePrefixedStrings<std::vector<PrefixedString>>);

std::string Render(const std::vector<PrefixedStringsEntry>& entries, char separator = '\n') {
  std::ostringstream out;
  WritePrefixedStrings(out, entries, separator);
  return std::move(out).str();
}

Generator<PrefixedStringsEntry> Generate(std::vector<PrefixedStringsEntry> entries) {
  for (auto& entry : entries) {
    co_yield std::move(entry);
  }
}

TEST(PrefixedStrings, EmptyInputWritesNothing) {
  EXPECT_EQ(Render({}), "");
}

TEST(PrefixedStrings, WritesEachStringOnItsOwnLine) {
  EXPECT_EQ(Render({
    PrefixedString{.string = "a"},
    PrefixedStrings{.strings = {"b", "c"}},
    PrefixedString{.string = "d"},
    }), "a\nb\nc\nd\n");
}

TEST(PrefixedStrings, PrependsPrefixesInPushOrder) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "participant/"},
    PushPrefix{.prefix = "column/"},
    PrefixedString{.string = "1.page"},
    PrefixedStrings{.strings = {"2.page", "3.page"}},
    PopPrefix{},
    PopPrefix{},
    }), "participant/column/1.page\nparticipant/column/2.page\nparticipant/column/3.page\n");
}

TEST(PrefixedStrings, DoesNotAddDelimiters) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "a"},
    PushPrefix{.prefix = "b"},
    PrefixedString{.string = "c"},
    PopPrefix{},
    PopPrefix{},
    }), "abc\n");
}

TEST(PrefixedStrings, PopRestoresOuterPrefixes) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "a/"},
    PushPrefix{.prefix = "b/"},
    PrefixedString{.string = "1"},
    PopPrefix{},
    PrefixedString{.string = "2"},
    PushPrefix{.prefix = "c/"},
    PrefixedString{.string = "3"},
    PopPrefix{},
    PopPrefix{},
    PrefixedString{.string = "4"},
    }), "a/b/1\na/2\na/c/3\n4\n");
}

TEST(PrefixedStrings, PrefixesWithoutStringsWriteNothing) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "a/"},
    PushPrefix{.prefix = "b/"},
    PopPrefix{},
    PrefixedStrings{.strings = {}},
    PopPrefix{},
    }), "");
}

TEST(PrefixedStrings, EmptyValues) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "a/"},
    PushPrefix{.prefix = ""},
    PrefixedString{.string = ""},
    PrefixedStrings{.strings = {"", "b"}},
    PopPrefix{},
    PopPrefix{},
    PrefixedString{.string = ""},
    }), "a/\na/\na/b\n\n");
}

TEST(PrefixedStrings, PopWithoutPushThrows) {
  std::ostringstream out;
  EXPECT_THROW(WritePrefixedStrings(out, std::vector<PrefixedStringsEntry>{PrefixedString{.string = "a"}, PopPrefix{}}),
    std::runtime_error);
  EXPECT_EQ(out.str(), "a\n") << "Strings preceding the invalid pop should have been written";

  EXPECT_THROW(Render({PushPrefix{.prefix = "a/"}, PopPrefix{}, PopPrefix{}}), std::runtime_error);
}

TEST(PrefixedStrings, UnpoppedPrefixThrows) {
  std::ostringstream out;
  EXPECT_THROW(WritePrefixedStrings(out, std::vector<PrefixedStringsEntry>{PushPrefix{.prefix = "a/"}, PrefixedString{.string = "1"}}),
    std::runtime_error);
  EXPECT_EQ(out.str(), "a/1\n") << "Strings should have been written before detecting the unpopped prefix";

  EXPECT_THROW(Render({PushPrefix{.prefix = "a/"}, PushPrefix{.prefix = "b/"}, PopPrefix{}}), std::runtime_error);
}

TEST(PrefixedStrings, CustomSeparator) {
  EXPECT_EQ(Render({
    PushPrefix{.prefix = "a/"},
    PrefixedString{.string = "1"},
    PrefixedStrings{.strings = {"2", ""}},
    PopPrefix{},
    }, '\0'), std::string("a/1\0a/2\0a/\0", 11));
  EXPECT_EQ(Render({PrefixedStrings{.strings = {"line\nbreak", "x"}}}, ';'), "line\nbreak;x;")
    << "Strings may contain newlines if they're not the separator";
}

TEST(PrefixedStrings, SeparatorInStringThrows) {
  std::ostringstream out;
  EXPECT_THROW(WritePrefixedStrings(out, std::vector<PrefixedStringsEntry>{PrefixedStrings{.strings = {"a", "b;c"}}}, ';'),
    std::runtime_error);
  EXPECT_EQ(out.str(), "a;") << "Strings preceding the invalid one should have been written";

  EXPECT_THROW(Render({PrefixedString{.string = "a\nb"}}), std::runtime_error);
  EXPECT_THROW(Render({PrefixedString{.string = "\n"}}), std::runtime_error);
}

TEST(PrefixedStrings, SeparatorInPrefixThrows) {
  EXPECT_THROW(Render({PushPrefix{.prefix = "a;"}, PopPrefix{}}, ';'), std::runtime_error)
    << "Prefix should be rejected even if no strings are written under it";
  EXPECT_THROW(Render({PushPrefix{.prefix = "a\n"}, PrefixedString{.string = "b"}, PopPrefix{}}), std::runtime_error);
}

TEST(PrefixedStrings, ConsumesInputRanges) {
  std::ostringstream out;
  WritePrefixedStrings(out, Generate({
    PushPrefix{.prefix = "a/"},
    PrefixedString{.string = "1"},
    PopPrefix{},
    PrefixedStrings{.strings = {"2"}},
    }));
  EXPECT_EQ(out.str(), "a/1\n2\n");
}

}

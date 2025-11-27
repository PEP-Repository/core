#include <pep/utils/TaggedValues.hpp>
#include <gtest/gtest.h>

namespace {

struct FirstNameTag { using type = std::string; };
struct LastNameTag { using type = std::string; };
struct AgeTag { using type = unsigned short; };

template <typename TTag>
void ExpectTaggedValue(const pep::TaggedValues& values, const std::optional<typename TTag::type> expected, const std::optional<std::string>& message = std::nullopt) {
  std::string colon, detail;
  if (message.has_value()) {
    colon = ": ";
    detail = *message;
  }

  if (auto stored = values.get<TTag>()) {
    if (!expected) {
      FAIL() << "Found value '" << *stored << "' but expected none" << colon << detail;
    }
    EXPECT_EQ(*stored, *expected) << detail;
  }
  else if (expected) {
    FAIL() << "Found no value but expected '" << *expected << "': " << colon << detail;
  }
}

}


TEST(TaggedValues, Works) {
  pep::TaggedValues values;

  EXPECT_TRUE(values.empty());
  EXPECT_EQ(0U, values.size());
  ExpectTaggedValue<FirstNameTag>(values, std::nullopt, "Initialized with first name");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Initialized with last name");
  ExpectTaggedValue<AgeTag>(values, std::nullopt, "Initialized with age");

  values.set<FirstNameTag>("Homer");
  EXPECT_FALSE(values.empty());
  EXPECT_EQ(1U, values.size());
  ExpectTaggedValue<FirstNameTag>(values, "Homer");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Setting first name updated last name");
  ExpectTaggedValue<AgeTag>(values, std::nullopt, "Setting first name updated age");

  values.set<LastNameTag>("Simpson");
  EXPECT_EQ(2U, values.size());
  ExpectTaggedValue<FirstNameTag>(values, "Homer", "Setting last name updated first name");
  ExpectTaggedValue<LastNameTag>(values, "Simpson");
  ExpectTaggedValue<AgeTag>(values, std::nullopt, "Setting last name updated age");

  values.set<AgeTag>(46U);
  EXPECT_EQ(3U, values.size());
  ExpectTaggedValue<FirstNameTag>(values, "Homer", "Setting age updated first name");
  ExpectTaggedValue<LastNameTag>(values, "Simpson", "Setting age updated last name");
  ExpectTaggedValue<AgeTag>(values, 46U);

  values.set<FirstNameTag>("Marge");
  EXPECT_EQ(3U, values.size()) << "Overwriting changed the size of the container";
  ExpectTaggedValue<FirstNameTag>(values, "Marge");
  ExpectTaggedValue<LastNameTag>(values, "Simpson", "Overwriting first name updated last name");
  ExpectTaggedValue<AgeTag>(values, 46U, "Overwriting first name updated age");

  if (auto age = values.get<AgeTag>()) {
    --(*age);
  }
  else {
    FAIL() << "Age somehow disappeared";
  }
  EXPECT_EQ(3U, values.size()) << "In-place update changed the size of the container";
  ExpectTaggedValue<FirstNameTag>(values, "Marge", "Incrementing age updated first name");
  ExpectTaggedValue<LastNameTag>(values, "Simpson", "Incrementing age updated last name");
  ExpectTaggedValue<AgeTag>(values, 45U);

  values.unset<LastNameTag>();
  EXPECT_EQ(2U, values.size()) << "Unexpected size after unsetting";
  ExpectTaggedValue<FirstNameTag>(values, "Marge", "Unsetting last name updated first name");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Unsetting last name didn't discard it");
  ExpectTaggedValue<AgeTag>(values, 45U, "Unsetting last name updated age");

  struct FavoriteChildTag { using type = std::string; };
  values.set<FavoriteChildTag>("Lisa");
  EXPECT_EQ(3U, values.size()) << "Unexpected size after adding local tag";
  ExpectTaggedValue<FirstNameTag>(values, "Marge", "Adding local tag updated first name");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Adding local tag updated last name");
  ExpectTaggedValue<AgeTag>(values, 45U, "Adding local tag updated age");
  ExpectTaggedValue<FavoriteChildTag>(values, "Lisa");

  auto copy = values;
  EXPECT_EQ(values.size(), 3U);
  ExpectTaggedValue<FirstNameTag>(values, "Marge", "Copying updated the original");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Copying updated the original");
  ExpectTaggedValue<AgeTag>(values, 45U, "Copying updated the original");
  ExpectTaggedValue<FavoriteChildTag>(values, "Lisa", "Copying updated the original");
  EXPECT_EQ(copy.size(), 3U);
  ExpectTaggedValue<FirstNameTag>(copy, "Marge", "Original value not copied correctly");
  ExpectTaggedValue<LastNameTag>(copy, std::nullopt, "Original value not copied correctly");
  ExpectTaggedValue<AgeTag>(copy, 45U, "Original value not copied correctly");
  ExpectTaggedValue<FavoriteChildTag>(copy, "Lisa", "Original value not copied correctly");

  copy.unset<FavoriteChildTag>();
  EXPECT_EQ(values.size(), 3U);
  ExpectTaggedValue<FirstNameTag>(values, "Marge", "Unsetting a copy value updated the original");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Unsetting a copy value updated the original");
  ExpectTaggedValue<AgeTag>(values, 45U, "Unsetting a copy value updated the original");
  ExpectTaggedValue<FavoriteChildTag>(values, "Lisa", "Unsetting a copy value updated the original");
  EXPECT_EQ(copy.size(), 2U);
  ExpectTaggedValue<FirstNameTag>(copy, "Marge", "Unsetting a copy value updated first name");
  ExpectTaggedValue<LastNameTag>(copy, std::nullopt, "Unsetting a copy value set a different value?!?");
  ExpectTaggedValue<AgeTag>(copy, 45U, "Unsetting a copy value updated age");
  ExpectTaggedValue<FavoriteChildTag>(copy, std::nullopt, "Copy value not unset correctly");

  values.clear();
  EXPECT_EQ(values.size(), 0U);
  ExpectTaggedValue<FirstNameTag>(values, std::nullopt, "Clearing didn't discard first name");
  ExpectTaggedValue<LastNameTag>(values, std::nullopt, "Clearing reintroduced last name?!?");
  ExpectTaggedValue<AgeTag>(values, std::nullopt, "Clearing didn't discard age");
  ExpectTaggedValue<FavoriteChildTag>(values, std::nullopt, "Clearing didn't discard favorite child");
  EXPECT_EQ(copy.size(), 2U) << "Clearing original affected the copy";
  ExpectTaggedValue<FirstNameTag>(copy, "Marge", "Clearing original updated copy's first name");
  ExpectTaggedValue<LastNameTag>(copy, std::nullopt, "Clearing original reintroduced copy's last name?!?");
  ExpectTaggedValue<AgeTag>(copy, 45U, "Clearing original updated copy's age");
  ExpectTaggedValue<FavoriteChildTag>(copy, std::nullopt, "Clearing original reintroduced copy's favorite child?!?");
}

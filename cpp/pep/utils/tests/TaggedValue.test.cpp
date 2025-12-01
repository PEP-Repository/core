#include <pep/utils/TaggedValue.hpp>
#include <gtest/gtest.h>

namespace {

using TaggedFirstName = pep::TaggedValue<std::string, struct FirstNameTag>;
using TaggedLastName  = pep::TaggedValue<std::string, struct LastNameTag>;
using TaggedAge       = pep::TaggedValue<unsigned,    struct AgeTag>;

template <typename TTagged>
void ExpectTaggedValue(const pep::TaggedValues& values, const std::optional<typename TTagged::value_type> expected, const std::optional<std::string>& message = std::nullopt) {
  std::string colon, detail;
  if (message.has_value()) {
    colon = ": ";
    detail = *message;
  }

  if (auto stored = values.get<TTagged>()) {
    if (!expected) {
      FAIL() << "Found value '" << stored->value() << "' but expected none" << colon << detail;
    }
    EXPECT_EQ(stored->value(), *expected) << detail;
  }
  else if (expected) {
    FAIL() << "Found no value but expected '" << *expected << colon << detail;
  }
}

}


TEST(TaggedValues, StorageAndRetrieval) {
  pep::TaggedValues values;

  EXPECT_TRUE(values.empty());
  EXPECT_EQ(0U, values.size());
  ExpectTaggedValue<TaggedFirstName>(values, std::nullopt, "Initialized with first name");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Initialized with last name");
  ExpectTaggedValue<TaggedAge>(values, std::nullopt, "Initialized with age");

  values.add(TaggedFirstName("Homer"));
  EXPECT_FALSE(values.empty());
  EXPECT_EQ(1U, values.size());
  ExpectTaggedValue<TaggedFirstName>(values, "Homer");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Setting first name updated last name");
  ExpectTaggedValue<TaggedAge>(values, std::nullopt, "Setting first name updated age");

  values.add(TaggedLastName("Simpson"));
  EXPECT_EQ(2U, values.size());
  ExpectTaggedValue<TaggedFirstName>(values, "Homer", "Setting last name updated first name");
  ExpectTaggedValue<TaggedLastName>(values, "Simpson");
  ExpectTaggedValue<TaggedAge>(values, std::nullopt, "Setting last name updated age");

  values.add(TaggedAge(46U));
  EXPECT_EQ(3U, values.size());
  ExpectTaggedValue<TaggedFirstName>(values, "Homer", "Setting age updated first name");
  ExpectTaggedValue<TaggedLastName>(values, "Simpson", "Setting age updated last name");
  ExpectTaggedValue<TaggedAge>(values, 46U);

  EXPECT_ANY_THROW(values.add(TaggedFirstName("Marge")));

  values.set(TaggedFirstName("Marge"));
  EXPECT_EQ(3U, values.size()) << "Overwriting changed the size of the container";
  ExpectTaggedValue<TaggedFirstName>(values, "Marge");
  ExpectTaggedValue<TaggedLastName>(values, "Simpson", "Overwriting first name updated last name");
  ExpectTaggedValue<TaggedAge>(values, 46U, "Overwriting first name updated age");

  if (auto age = values.get_value<TaggedAge>()) {
    --(*age);
  }
  else {
    FAIL() << "Age somehow disappeared";
  }
  EXPECT_EQ(3U, values.size()) << "In-place update changed the size of the container";
  ExpectTaggedValue<TaggedFirstName>(values, "Marge", "Incrementing age updated first name");
  ExpectTaggedValue<TaggedLastName>(values, "Simpson", "Incrementing age updated last name");
  ExpectTaggedValue<TaggedAge>(values, 45U);

  values.unset<TaggedLastName>();
  EXPECT_EQ(2U, values.size()) << "Unexpected size after unsetting";
  ExpectTaggedValue<TaggedFirstName>(values, "Marge", "Unsetting last name updated first name");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Unsetting last name didn't discard it");
  ExpectTaggedValue<TaggedAge>(values, 45U, "Unsetting last name updated age");

  using TaggedFavoriteChild = pep::TaggedValue<std::string, struct FavoriteChildTag>;
  values.add(TaggedFavoriteChild("Lisa"));
  EXPECT_EQ(3U, values.size()) << "Unexpected size after adding local tag";
  ExpectTaggedValue<TaggedFirstName>(values, "Marge", "Adding local tag updated first name");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Adding local tag updated last name");
  ExpectTaggedValue<TaggedAge>(values, 45U, "Adding local tag updated age");
  ExpectTaggedValue<TaggedFavoriteChild>(values, "Lisa");

  auto copy = values;
  EXPECT_EQ(values.size(), 3U);
  ExpectTaggedValue<TaggedFirstName>(values, "Marge", "Copying updated the original");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Copying updated the original");
  ExpectTaggedValue<TaggedAge>(values, 45U, "Copying updated the original");
  ExpectTaggedValue<TaggedFavoriteChild>(values, "Lisa", "Copying updated the original");
  EXPECT_EQ(copy.size(), 3U);
  ExpectTaggedValue<TaggedFirstName>(copy, "Marge", "Original value not copied correctly");
  ExpectTaggedValue<TaggedLastName>(copy, std::nullopt, "Original value not copied correctly");
  ExpectTaggedValue<TaggedAge>(copy, 45U, "Original value not copied correctly");
  ExpectTaggedValue<TaggedFavoriteChild>(copy, "Lisa", "Original value not copied correctly");

  copy.unset<TaggedFavoriteChild>();
  EXPECT_EQ(values.size(), 3U);
  ExpectTaggedValue<TaggedFirstName>(values, "Marge", "Unsetting a copy value updated the original");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Unsetting a copy value updated the original");
  ExpectTaggedValue<TaggedAge>(values, 45U, "Unsetting a copy value updated the original");
  ExpectTaggedValue<TaggedFavoriteChild>(values, "Lisa", "Unsetting a copy value updated the original");
  EXPECT_EQ(copy.size(), 2U);
  ExpectTaggedValue<TaggedFirstName>(copy, "Marge", "Unsetting a copy value updated first name");
  ExpectTaggedValue<TaggedLastName>(copy, std::nullopt, "Unsetting a copy value set a different value?!?");
  ExpectTaggedValue<TaggedAge>(copy, 45U, "Unsetting a copy value updated age");
  ExpectTaggedValue<TaggedFavoriteChild>(copy, std::nullopt, "Copy value not unset correctly");

  values.clear();
  EXPECT_EQ(values.size(), 0U);
  ExpectTaggedValue<TaggedFirstName>(values, std::nullopt, "Clearing didn't discard first name");
  ExpectTaggedValue<TaggedLastName>(values, std::nullopt, "Clearing reintroduced last name?!?");
  ExpectTaggedValue<TaggedAge>(values, std::nullopt, "Clearing didn't discard age");
  ExpectTaggedValue<TaggedFavoriteChild>(values, std::nullopt, "Clearing didn't discard favorite child");
  EXPECT_EQ(copy.size(), 2U) << "Clearing original affected the copy";
  ExpectTaggedValue<TaggedFirstName>(copy, "Marge", "Clearing original updated copy's first name");
  ExpectTaggedValue<TaggedLastName>(copy, std::nullopt, "Clearing original reintroduced copy's last name?!?");
  ExpectTaggedValue<TaggedAge>(copy, 45U, "Clearing original updated copy's age");
  ExpectTaggedValue<TaggedFavoriteChild>(copy, std::nullopt, "Clearing original reintroduced copy's favorite child?!?");
}

#pragma once

#include <pep/structure/ShortPseudonyms.hpp>

#include <string>
#include <vector>

namespace pep {

class StudyContexts;

class StudyContext {
  friend class StudyContexts;

private:
  std::string mId;
  bool mIsDefault;

private:
  StudyContext(std::string id, bool isDefault) : mId(std::move(id)), mIsDefault(isDefault) {}

public:
  explicit StudyContext(std::string id) : StudyContext(std::move(id), false) {}

  bool isDefault() const noexcept { return mIsDefault; }
  inline const std::string& getId() const noexcept { return mId; }
  std::string getIdIfNonDefault() const;

  bool matches(const std::string& contexts) const;
  bool matchesShortPseudonym(const pep::ShortPseudonymDefinition& sp) const;

  std::string getAdministeringAssessorColumnName(uint32_t visitNumber) const; // 1-based

  bool operator ==(const StudyContext& other) const;
  inline bool operator !=(const StudyContext& other) const { return !(*this == other); }
};

class StudyContexts {
private:
  std::vector<StudyContext> mItems;

private:
  std::vector<StudyContext>::const_iterator getPositionOf(const StudyContext& context) const;

public:
  StudyContexts() = default;
  explicit StudyContexts(std::vector<StudyContext> items);

  bool contains(const StudyContext& context) const;
  const std::vector<StudyContext>& getItems() const noexcept { return mItems; }

  void add(const StudyContext& context);
  void remove(const StudyContext& context);

  const StudyContext& getById(const std::string& id) const;
  const StudyContext* getDefault() const noexcept;

  std::string toString() const;
  StudyContexts parse(const std::string& value) const;
};

}

#include <pep/structure/StudyContext.hpp>

#include <boost/algorithm/string/split.hpp>

namespace pep {

namespace {
  std::vector<std::string> ContextStringToIds(const std::string& value) {
    std::vector<std::string> result;
    if (!value.empty()) {
      boost::split(result, value, [](char c) {return c == ','; });
    }
    return result;
  }
}

std::string StudyContext::getIdIfNonDefault() const {
  return isDefault() ? std::string() : getId();
}

bool StudyContext::matches(const std::string& contexts) const {
  if (contexts.empty()) {
    return isDefault();
  }
  auto ids = ContextStringToIds(contexts);
  return std::find(ids.cbegin(), ids.cend(), getId()) != ids.cend();
}

bool StudyContext::matchesShortPseudonym(const pep::ShortPseudonymDefinition& sp) const {
  return matches(sp.getStudyContext());
}

std::string StudyContext::getAdministeringAssessorColumnName(uint32_t visitNumber) const {
  assert(visitNumber > 0);

  auto prefix = getIdIfNonDefault();
  if (!prefix.empty()) {
    prefix += ".";
  }
  return prefix + "Visit" + std::to_string(visitNumber) + ".Assessor";
}

bool StudyContext::operator ==(const StudyContext& other) const {
  return (mId == other.mId) && (mIsDefault == other.mIsDefault);
}

std::vector<StudyContext>::const_iterator StudyContexts::getPositionOf(const StudyContext& context) const {
  return std::find(mItems.cbegin(), mItems.cend(), context);
}

StudyContexts::StudyContexts(std::vector<StudyContext> items)
  : mItems(std::move(items)) {
  if (!mItems.empty()) {
    if (getDefault() != nullptr) {
      throw std::runtime_error("Don't specify a default when initializing StudyContexts");
    }
    mItems.front().mIsDefault = true;
  }
  else {
    mItems.push_back(StudyContext(std::string(), true));
  }
}

bool StudyContexts::contains(const StudyContext& context) const {
  return getPositionOf(context) != mItems.cend();
}

void StudyContexts::add(const StudyContext& context) {
  if (contains(context)) {
    throw std::runtime_error("Attempt to add duplicate study context");
  }
  if (context.isDefault() && (getDefault() != nullptr)) {
    throw std::runtime_error("Attempt to add duplicate default study context");
  }
  mItems.push_back(context);
}

void StudyContexts::remove(const StudyContext& context) {
  auto position = getPositionOf(context);
  if (position == mItems.end()) {
    throw std::runtime_error("Study context not found");
  }
  mItems.erase(position);
}

const StudyContext& StudyContexts::getById(const std::string& id) const {
  auto end = mItems.cend();
  auto position = std::find_if(mItems.cbegin(), end, [id](const StudyContext& candidate) { return candidate.getId() == id; });
  if (position == end) {
    throw std::runtime_error("Study context " + id + " not found");
  }
  return *position;
}

const StudyContext* StudyContexts::getDefault() const noexcept {
  auto end = mItems.cend();
  auto position = std::find_if(mItems.cbegin(), end, [](const StudyContext& candidate) { return candidate.isDefault(); });
  if (position == end) {
    return nullptr;
  }
  return &*position;
}

StudyContexts StudyContexts::parse(const std::string& value) const {
  StudyContexts result;

  if (value.empty()) {
    auto defaultContext = getDefault();
    if (defaultContext == nullptr) throw std::runtime_error("No default study context found");
    result.mItems.push_back(*defaultContext);
  }
  else {
    for (auto& id : ContextStringToIds(value)) {
      result.mItems.push_back(getById(id));
    }
  }

  return result;
}

std::string StudyContexts::toString() const {
  std::string result;
  for (const auto& item : mItems) {
    if (!result.empty()) {
      result += ',';
    }
    result += item.getId();
  }
  return result;
}

}

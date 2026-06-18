#include <pep/structure/StudyContext.hpp>

#include <boost/algorithm/string/split.hpp>

namespace pep {

namespace {
  std::vector<std::string> ContextStringToIds(const std::string& value) {
    std::vector<std::string> result;
    if (!value.empty()) {
      boost::split(result, value, std::bind_front(std::equal_to{}, ','));
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
  return (id_ == other.id_) && (isDefault_ == other.isDefault_);
}

std::vector<StudyContext>::const_iterator StudyContexts::getPositionOf(const StudyContext& context) const {
  return std::find(items_.cbegin(), items_.cend(), context);
}

StudyContexts::StudyContexts(std::vector<StudyContext> items)
  : items_(std::move(items)) {
  if (!items_.empty()) {
    if (getDefault() != nullptr) {
      throw std::runtime_error("Don't specify a default when initializing StudyContexts");
    }
    items_.front().isDefault_ = true;
  }
  else {
    items_.push_back(StudyContext(std::string(), true));
  }
}

bool StudyContexts::contains(const StudyContext& context) const {
  return getPositionOf(context) != items_.cend();
}

void StudyContexts::add(const StudyContext& context) {
  if (contains(context)) {
    throw std::runtime_error("Attempt to add duplicate study context");
  }
  if (context.isDefault() && (getDefault() != nullptr)) {
    throw std::runtime_error("Attempt to add duplicate default study context");
  }
  items_.push_back(context);
}

void StudyContexts::remove(const StudyContext& context) {
  auto position = getPositionOf(context);
  if (position == items_.end()) {
    throw std::runtime_error("Study context not found");
  }
  items_.erase(position);
}

const StudyContext& StudyContexts::getById(const std::string& id) const {
  auto end = items_.cend();
  auto position = std::find_if(items_.cbegin(), end, [id](const StudyContext& candidate) { return candidate.getId() == id; });
  if (position == end) {
    throw std::runtime_error("Study context " + id + " not found");
  }
  return *position;
}

const StudyContext* StudyContexts::getDefault() const noexcept {
  auto end = items_.cend();
  auto position = std::find_if(items_.cbegin(), end, [](const StudyContext& candidate) { return candidate.isDefault(); });
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
    result.items_.push_back(*defaultContext);
  }
  else {
    for (auto& id : ContextStringToIds(value)) {
      result.items_.push_back(getById(id));
    }
  }

  return result;
}

std::string StudyContexts::toString() const {
  std::string result;
  for (const auto& item : items_) {
    if (!result.empty()) {
      result += ',';
    }
    result += item.getId();
  }
  return result;
}

}

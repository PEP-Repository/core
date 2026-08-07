#include <pep/structure/StudyContext.hpp>

#include <ranges>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std::ranges;

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
  return contains(ids, getId());
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
  return find(items_, context);
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
  auto position = find(items_, id, &StudyContext::getId);
  if (position == items_.cend()) {
    throw std::runtime_error("Study context " + id + " not found");
  }
  return *position;
}

const StudyContext* StudyContexts::getDefault() const noexcept {
  auto position = find_if(items_, &StudyContext::isDefault);
  if (position == items_.cend()) {
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
  // Not passing the view to boost::algorithm::join directly: it doesn't support C++20 ranges
  return boost::algorithm::join(items_ | views::transform(&StudyContext::getId) | to<std::vector>(), ",");
}

}

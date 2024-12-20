#include <pep/content/ParticipantDeviceHistory.hpp>

#include <algorithm>

#include <boost/property_tree/json_parser.hpp>

namespace pep {

ParticipantDeviceRecord ParticipantDeviceRecord::Deserialize(const boost::property_tree::ptree& source) {
  return ParticipantDeviceRecord(
    source.get<std::string>("type"),
    source.get<std::string>("serial"),
    source.get_optional<std::string>("note").value_or(""),
    source.get<int64_t>("date")
  );
}

ParticipantDeviceRecord ParticipantDeviceRecord::Parse(const std::string& json) {
  boost::property_tree::ptree pepData;
  std::stringstream ss(json);
  boost::property_tree::read_json(ss, pepData);
  return Deserialize(pepData);
}

void ParticipantDeviceRecord::serialize(boost::property_tree::ptree& destination) const {
  destination.put("type", type);
  destination.put("serial", serial);
  if (!note.empty()) {
    destination.put("note", note);
  }
  destination.put("date", time);
}

void ParticipantDeviceHistory::onInvalid(const std::string& reason, bool throwException) {
  if (!mInvalidReason) {
    mInvalidReason = reason;
  }
  if (throwException) {
    throw std::runtime_error(reason);
  }
}

bool ParticipantDeviceHistory::isValid(std::string* invalidReason) const {
  if (mInvalidReason) {
    if (invalidReason != nullptr) {
      *invalidReason = *mInvalidReason;
    }
    return false;
  }

  return true;
}

ParticipantDeviceHistory::ParticipantDeviceHistory(const std::vector<ParticipantDeviceRecord>& records, bool throwIfInvalid)
  : mRecords(records) {
  std::sort(mRecords.begin(), mRecords.end());
  const ParticipantDeviceRecord *active = nullptr;
  std::optional<int64_t> lastTimestamp;
  for (auto i = begin(); i != end(); ++i) {
    if (i->isActive()) {
      if (active != nullptr) {
        onInvalid("Multiple devices active at the same time", throwIfInvalid);
      }
      active = &*i;
    }
    else {
      if (active == nullptr) {
        onInvalid("Participant device deactivation found while no device is active", throwIfInvalid);
      }
      else if (active->serial != i->serial) {
        onInvalid("Participant device deactivation found for device other than the active one", throwIfInvalid);
      }
      active = nullptr;
    }

    if (lastTimestamp) {
      if (i->time == *lastTimestamp) {
        onInvalid("Device (de-)activation records with the same timestamp found", throwIfInvalid);
      }
    }
    lastTimestamp = i->time;
  }
}

const ParticipantDeviceRecord* ParticipantDeviceHistory::getCurrent() const {
  const ParticipantDeviceRecord* result = nullptr;
  if (!mRecords.empty()) {
    result = &mRecords.back();
    if (!result->isActive()) {
      result = nullptr;
    }
  }
  return result;
}

ParticipantDeviceHistory ParticipantDeviceHistory::Parse(const std::string& json, bool throwIfInvalid) {
  boost::property_tree::ptree root;
  std::istringstream source(json);
  boost::property_tree::read_json(source, root);
  const auto& entries = root.get_child("entries");

  std::vector<ParticipantDeviceRecord> records;
  records.reserve(entries.size());
  for (const auto& node : entries) {
    records.push_back(ParticipantDeviceRecord::Deserialize(node.second));
  }

  return ParticipantDeviceHistory(records, throwIfInvalid);
}

std::string ParticipantDeviceHistory::toJson() const {
  boost::property_tree::ptree entries;

  for (auto record : mRecords) {
    boost::property_tree::ptree node;
    record.serialize(node);
    entries.push_back(std::make_pair(std::string(), node));
  }

  // Use a nested ptree to prevent entries from being serialized as a non-array object with unnamed children. See https://stackoverflow.com/a/2114567
  boost::property_tree::ptree root;
  root.push_back(std::make_pair("entries", entries));
  std::ostringstream result;
  boost::property_tree::write_json(result, root);
  return result.str();
}

}

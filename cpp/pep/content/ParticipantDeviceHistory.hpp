#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>
#include <optional>

#include <boost/property_tree/ptree_fwd.hpp>

namespace pep {

struct ParticipantDeviceRecord {
  std::string type;
  std::string serial;
  std::string note;
  int64_t time = 0;

  inline bool isSet() const { return time != 0; }
  inline bool isActive() const { return type == "start"; }

  ParticipantDeviceRecord() = default;
  inline ParticipantDeviceRecord(std::string type, std::string serial, std::string note, int64_t time) :
    type(std::move(type)),
    serial(std::move(serial)),
    note(std::move(note)),
    time(time) {}

  inline bool operator<(const ParticipantDeviceRecord& rhs) const { return std::tie(time, type) < std::tie(rhs.time, rhs.type); }

  void serialize(boost::property_tree::ptree& destination) const;

  static ParticipantDeviceRecord Parse(const std::string& json);
  static ParticipantDeviceRecord Deserialize(const boost::property_tree::ptree& source);
};

class ParticipantDeviceHistory {
private:
  using Records = std::vector<ParticipantDeviceRecord>;

public:
  using const_iterator = Records::const_iterator;

private:
  Records mRecords;
  std::optional<std::string> mInvalidReason;

private:
  void onInvalid(const std::string& reason, bool throwException);

public:
  ParticipantDeviceHistory() noexcept = default;
  ParticipantDeviceHistory(const ParticipantDeviceHistory& value) = default;
  ParticipantDeviceHistory& operator=(const ParticipantDeviceHistory& value) = default;
  ParticipantDeviceHistory(const std::vector<ParticipantDeviceRecord>& records, bool throwIfInvalid = true);

  const ParticipantDeviceRecord* getCurrent() const;

  bool isValid(std::string* invalidReason) const;

  inline size_t size() const { return mRecords.size(); }
  inline const_iterator begin() const { return mRecords.cbegin(); }
  inline const_iterator end() const { return mRecords.cend(); }

  std::string toJson() const;
  static ParticipantDeviceHistory Parse(const std::string& json, bool throwIfInvalid = true);
};

}

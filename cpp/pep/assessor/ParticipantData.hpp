#pragma once

#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/content/ParticipantPersonalia.hpp>

#include <map>

struct ParticipantData {
  std::optional<pep::ParticipantPersonalia> mPersonalia;
  bool mIsTestParticipant = false;
  std::map<std::string, std::string> mShortPseudonyms;
  std::map<std::string, pep::ParticipantDeviceHistory> mParticipantDeviceHistory;
  std::unordered_map<std::string, std::unordered_map<unsigned, unsigned>> mVisitAssessors;
};

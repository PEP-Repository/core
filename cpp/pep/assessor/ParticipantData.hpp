#pragma once

#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/content/ParticipantPersonalia.hpp>

#include <map>

struct ParticipantData {
  std::optional<pep::ParticipantPersonalia> personalia_;
  bool isTestParticipant_ = false;
  std::map<std::string, std::string> shortPseudonyms_;
  std::map<std::string, pep::ParticipantDeviceHistory> participantDeviceHistory_;
  std::unordered_map<std::string, std::unordered_map<unsigned, unsigned>> visitAssessors_;
};

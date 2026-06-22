#pragma once

#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/content/ParticipantPersonalia.hpp>

#include <map>

struct ParticipantData {
  std::optional<pep::ParticipantPersonalia> personalia;
  bool isTestParticipant = false;
  std::map<std::string, std::string> shortPseudonyms;
  std::map<std::string, pep::ParticipantDeviceHistory> participantDeviceHistory;
  std::unordered_map<std::string, std::unordered_map<unsigned, unsigned>> visitAssessors;
};

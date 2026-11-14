#pragma once

#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/application/CommandLineValue.hpp> // for commandline::NamedValues
#include <pep/cli/ColumnQuery.hpp>
#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>

#include <rxcpp/rx-lite.hpp>

#include <memory>
#include <string>
#include <vector>

namespace pep::cli {

struct MultiCellQuery : ColumnQuery {
  static pep::commandline::Parameters Parameters();

  static bool SpecifiesColumns(const pep::commandline::NamedValues& values);
  static bool SpecifiesParticipants(const pep::commandline::NamedValues& values);

  static bool IsNonEmpty(const pep::commandline::NamedValues& values);

  static std::vector<std::string> GetParticipantGroups(const pep::commandline::NamedValues& values);

  struct ParticipantSpecAndPp {
    std::string spec;
    pep::PolymorphicPseudonym pp;
  };

  static rxcpp::observable<ParticipantSpecAndPp> GetPpsForShortPseudonyms(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client);
  static rxcpp::observable<ParticipantSpecAndPp> GetPpsForParticipantSpecs(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client);

  static rxcpp::observable<pep::PolymorphicPseudonym> GetPps(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client);
};

}

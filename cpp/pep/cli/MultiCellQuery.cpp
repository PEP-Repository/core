#include <pep/cli/MultiCellQuery.hpp>
#include <pep/async/RxDistinct.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

pep::commandline::Parameters MultiCellQuery::Parameters() {
  return pep::commandline::Parameters()
    + pep::commandline::Parameter("column", "Columns to include").alias("columns").shorthand('c').value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("column-group", "Column groups to include").alias("column-groups").shorthand('C').value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("participant-group", "Participant groups to include").alias("participant-groups").shorthand('P').value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("participant", "Participants to include").alias("participants").shorthand('p').value(pep::commandline::Value<std::string>().multiple())
    + pep::commandline::Parameter("short-pseudonym", "Short pseudonyms of participants to include").alias("short-pseudonyms").alias("sp").value(pep::commandline::Value<std::string>().multiple());
}

bool MultiCellQuery::SpecifiesColumns(const pep::commandline::NamedValues& values) {
  return values.hasAnyOf({ "column", "column-group" });
}

bool MultiCellQuery::SpecifiesParticipants(const pep::commandline::NamedValues& values) {
  return values.hasAnyOf({ "participant-group", "participant", "short-pseudonym" });
}

bool MultiCellQuery::IsNonEmpty(const pep::commandline::NamedValues& values) {
  return SpecifiesColumns(values) || SpecifiesParticipants(values);
}

std::vector<std::string> MultiCellQuery::GetColumnGroups(const pep::commandline::NamedValues& values) {
  return values.getOptionalMultiple<std::string>("column-group");
}

std::vector<std::string> MultiCellQuery::GetColumns(const pep::commandline::NamedValues& values) {
  return values.getOptionalMultiple<std::string>("column");
}

std::vector<std::string> MultiCellQuery::GetParticipantGroups(const pep::commandline::NamedValues& values) {
  return values.getOptionalMultiple<std::string>("participant-group");
}

rxcpp::observable<MultiCellQuery::ParticipantSpecAndPp> MultiCellQuery::GetPpsForShortPseudonyms(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client) {
  auto sps = pep::MakeSharedCopy(values.getOptionalMultiple<std::string>("short-pseudonym"));
  if (sps->empty()) {
    return rxcpp::observable<>::empty<MultiCellQuery::ParticipantSpecAndPp>();
  }

  return client->findPpsForShortPseudonyms(*sps)
    .flat_map([sps](std::shared_ptr<std::vector<std::optional<pep::PolymorphicPseudonym>>> pps) {
    assert(sps->size() == pps->size());
    return pep::CreateObservable<MultiCellQuery::ParticipantSpecAndPp>([sps, pps](rxcpp::subscriber<MultiCellQuery::ParticipantSpecAndPp> subscriber) {
      for (size_t i = 0U; i < sps->size(); ++i) {
        const auto& pp = (*pps)[i];
        if (pp.has_value()) {
          subscriber.on_next(MultiCellQuery::ParticipantSpecAndPp{ (*sps)[i], *pp });
        }
      }
      subscriber.on_completed();
      });
      });
}

rxcpp::observable<MultiCellQuery::ParticipantSpecAndPp> MultiCellQuery::GetPpsForParticipantSpecs(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client) {
  auto specs = pep::MakeSharedCopy(values.getOptionalMultiple<std::string>("participant"));
  if (specs->empty()) {
    return rxcpp::observable<>::empty<MultiCellQuery::ParticipantSpecAndPp>();
  }

  return client->parsePpsOrIdentities(*specs)
    .flat_map([specs](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> pps) {
    assert(pps->size() == specs->size());
    return rxcpp::observable<>::range(size_t{}, pps->size() - 1U)
      .map([specs, pps](size_t i) { return ParticipantSpecAndPp{ (*specs)[i], (*pps)[i] }; });
      });
}

rxcpp::observable<pep::PolymorphicPseudonym> MultiCellQuery::GetPps(const pep::commandline::NamedValues& values, std::shared_ptr<pep::CoreClient> client) {
  return GetPpsForShortPseudonyms(values, client)
    .concat(GetPpsForParticipantSpecs(values, client))
    .map(std::mem_fn(&ParticipantSpecAndPp::pp))
    .op(pep::RxDistinct());
}

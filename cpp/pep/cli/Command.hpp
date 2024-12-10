#pragma once

#include <pep/cli/CliApplication.hpp>


const static std::string LOG_TAG("Cli");

/*!
 * \brief Utility base for commands supported by the pepcli application.
 * \tparam TParent The parent command type. Should be either CliApplication or a(nother) ChildCommandOf<> specialization.
 */
template <typename TParent>
class ChildCommandOf : public pep::commandline::ChildCommandOf<TParent> {
 protected:
  inline ChildCommandOf(const std::string& name, const std::string& description, TParent& parent)
    : pep::commandline::ChildCommandOf<TParent>(name, description, parent) {
  }

public:
  inline int executeEventLoopFor(std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback) {
    return this->executeEventLoopFor(true, callback);
  }

  inline int executeEventLoopFor(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback) {
    return this->getParent().executeEventLoopFor(ensureEnrolled, callback);
  }
};

#pragma once

#include <pep/cli/CliApplication.hpp>

namespace pep::cli {

/*!
 * \brief Helper base for ChildCommandOf<> class (below).
 * \remark Provides "executeEventLoopFor" methods supporting callbacks that accept (a shared_ptr<> to) either a CoreClient or a (full) Client instance.
 *         Because this class performs the upcast from Client to CoreClient, derived classes that require only the CoreClient interface can
 *         #include CoreClient.hpp instead of having to #include the full Client.hpp.
 */
class ChildCommand {
private:
  int executeEventLoopForClient(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::CoreClient> client)> callback);

protected:
  virtual int executeEventLoopForClient(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback) = 0;

public:
  virtual ~ChildCommand() noexcept = default;

  /*!
   * \brief Provides a Client or CoreClient instance to a callback, and exhausts the observable that the callback returns.
   * \tparam TCallback The callback type, which must be a function-like object that returns an observable<FakeVoid> and accepts a shared_ptr<> to either a CoreClient or a Client instance.
   * \param ensureEnrolled Whether the (Core)Client should be enrolled before providing it to the callback.
   * \param callback The callback function to invoke.
   */
  template <typename TCallback>
  int executeEventLoopFor(bool ensureEnrolled, TCallback callback) {
    constexpr bool CALLBACK_ACCEPTS_CORE_CLIENT = std::is_invocable_v<TCallback, std::shared_ptr<pep::CoreClient>>; // Check if the callback can be invoked with (a shared_ptr to) a CoreClient instance
    using Client = std::conditional_t<CALLBACK_ACCEPTS_CORE_CLIENT, pep::CoreClient, pep::Client>; // If the callback doesn't accept a CoreClient, it must accept a (full) Client instance.
    using Function = std::function<rxcpp::observable<FakeVoid>(std::shared_ptr<Client>)>; // Wrap the callback in a (non-templated) std::function<>.
    return this->executeEventLoopForClient(ensureEnrolled, Function(callback)); // Invoke a method overload depending on the std::function's type (i.e. signature).
  }

  /*!
   * \brief Provides an enrolled Client or CoreClient instance to a callback, and exhausts the observable that the callback returns.
   * \tparam TCallback The callback type, which must be a function-like object that returns an observable<FakeVoid> and accepts a shared_ptr<> to either a CoreClient or a Client instance.
   * \param callback The callback function to invoke.
   */
  template <typename TCallback>
  int executeEventLoopFor(TCallback callback) {
    return this->executeEventLoopFor(true, callback);
  }
};


/*!
 * \brief Utility base for commands supported by the pepcli application.
 * \tparam TParent The parent command type. Should be either CliApplication or a(nother) ChildCommandOf<> specialization.
 */
template <typename TParent>
class ChildCommandOf : public pep::commandline::ChildCommandOf<TParent>, public ChildCommand {
protected:
  inline ChildCommandOf(const std::string& name, const std::string& description, TParent& parent)
    : pep::commandline::ChildCommandOf<TParent>(name, description, parent) {
  }

  int executeEventLoopForClient(bool ensureEnrolled, std::function<rxcpp::observable<pep::FakeVoid>(std::shared_ptr<pep::Client> client)> callback) override {
    return this->getParent().executeEventLoopFor(ensureEnrolled, callback);
  }
};

}

#pragma once

#include <pep/enrollment/KeyComponentMessages.hpp>
#include <pep/server/SigningServer.hpp>

namespace pep {

class EnrollmentServer : public SigningServer {
public:
  class Parameters;

private:
  struct Metrics;

  std::shared_ptr<PseudonymTranslator> mPseudonymTranslator;
  std::shared_ptr<DataTranslator> mDataTranslator;
  std::shared_ptr<Metrics> mMetrics;

  messaging::MessageBatches handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> signedRequest);

protected:
  explicit EnrollmentServer(std::shared_ptr<Parameters> parameters);

  const PseudonymTranslator& pseudonymTranslator() const { return *mPseudonymTranslator; }
  const DataTranslator& dataTranslator() const { return *mDataTranslator; }
};


class EnrollmentServer::Parameters : public SigningServer::Parameters {
public:
  std::shared_ptr<PseudonymTranslator> getPseudonymTranslator() const;
  std::shared_ptr<DataTranslator> getDataTranslator() const;
  void setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator);
  void setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator);

protected:
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);
  void check() const override;

private:
  std::shared_ptr<PseudonymTranslator> pseudonymTranslator;
  std::shared_ptr<DataTranslator> dataTranslator;
};

}

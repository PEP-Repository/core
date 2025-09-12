#include <pep/async/WorkGuard.hpp>
#include <boost/asio/io_context.hpp>

namespace pep {

struct WorkGuard::Implementor {
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> boost;
};

WorkGuard::WorkGuard(boost::asio::io_context& context)
  : mImplementor(std::make_unique<Implementor>(Implementor{ .boost = boost::asio::make_work_guard(context) })) {
}

WorkGuard::~WorkGuard() noexcept = default;

}

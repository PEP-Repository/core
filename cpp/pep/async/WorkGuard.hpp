#pragma once

#include <pep/async/IoContext_fwd.hpp>
#include <boost/noncopyable.hpp>
#include <memory>

namespace pep {

// We don't want to (determine if it's possible to) define copy and/or move operations, so we just make this class noncopyable
class WorkGuard : boost::noncopyable {
private:
  struct Implementor;
  std::unique_ptr<Implementor> mImplementor;

public:
  explicit WorkGuard(boost::asio::io_context& context);
  ~WorkGuard() noexcept; // Can't be defined here because ~Implementor is unknown
};

}

#include <pep/assessor/Async.hpp>

#include <pep/utils/Exceptions.hpp>

#include <QFutureWatcher>

#include <QtConcurrent/QtConcurrent>
class AsyncException : public QException {
private:
  std::exception_ptr mOriginal;

public:
  //NOLINTNEXTLINE(bugprone-throw-keyword-missing)
  explicit AsyncException(std::exception_ptr original) : mOriginal(std::move(original)) {}
  std::exception_ptr toExceptionPtr() const { return mOriginal; }

  QException* clone() const override { return new AsyncException(mOriginal); }
  void raise() const override { throw *this; }
};

void Async::Run(QObject* owner, const std::function<void()>& job, const std::function<void(std::exception_ptr)>& onCompletion) {
  if (owner == nullptr) {
    throw std::runtime_error("Owner of async job cannot be null");
  }
  auto watcher = new QFutureWatcher<void>(owner);
  QObject::connect(watcher, &QFutureWatcher<void>::finished, [watcher, onCompletion]() {
    std::exception_ptr error;
    try {
      watcher->waitForFinished(); // // Raises any exceptions that may have occurred during the job
    }
    catch (const AsyncException& qt) {
      error = qt.toExceptionPtr();
    }
    catch (...) {
      error = std::current_exception();
    }
    watcher->deleteLater();
    onCompletion(std::move(error));
  });
  auto future = QtConcurrent::run([job]() {
    try {
      job();
    }
    catch (const QException&) {
      // Should already be portable across threads to the onCompletion handler
      throw;
    }
    catch (...) {
      // Wrap original exception in a QException to port its message across threads to the onCompletion handler
      throw AsyncException(std::current_exception());
    }
  });
  watcher->setFuture(future);
}

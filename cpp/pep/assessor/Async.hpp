#pragma once

#include <QObject>

#include <exception>
#include <functional>

struct Async {
  static void Run(QObject* owner, const std::function<void()>& job, const std::function<void(std::exception_ptr)>& onCompletion);
};

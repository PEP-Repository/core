#pragma once

#include <exception>
#include <functional>
#include <QObject>

class Async {
public:
  static void Run(QObject* owner, const std::function<void()>& job, const std::function<void(std::exception_ptr)>& onCompletion);
};

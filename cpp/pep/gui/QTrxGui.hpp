#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

rxcpp::observe_on_one_worker ObserveOnGui();

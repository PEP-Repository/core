#pragma once
#include <pep/async/CreateObservable.hpp>

#include <rxcpp/rx-lite.hpp>

#include <functional>

namespace pep
{
  // Given a function that creates an observable<T>, the 'creator',
  // returns an imitator of creator().  The imitator invokes creator()
  // only when it is subscribed to, meaning that the creator isn't called
  // when the imitator isn't used.
  //
  // Calling map on the imitator, as in  RxLazy(...).map([](){...}), doesn't
  // on its own cause a call of the creator (unless, for example, the
  // from the .map resulting observable is subscribed to.) 
  template <typename T, typename Creator>
  rxcpp::observable<T> RxLazy(Creator creator) {

    static_assert(std::is_same_v<
        decltype(creator().as_dynamic()), 
        rxcpp::observable<T>>);
    
    return CreateObservable<T>(
      [creator=std::move(creator)](rxcpp::subscriber<T> s){
      
        creator().subscribe(s);

      });

  }
}


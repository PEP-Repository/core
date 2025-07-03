#include <pep/archiving/PseudonymiseInputFilter.hpp>
#include <pep/archiving/Pseudonymiser.hpp>
#include <pep/archiving/Tar.hpp>

#include <boost/iostreams/filtering_stream.hpp>

namespace pep {

namespace {

const std::streamsize PSEUDONYMISER_BUFFER_SIZE{ 4096 };

}

const std::string& Pseudonymiser::GetDefaultPlaceholder() {
  static const std::string DEFAULT_PLACEHOLDER = "idQE6abTtIA8QspTOBeNshr6pf4Y5y74QGwJ2Pa9"; //Random, so virtually 0 chance of occurring in actual data. Substring will be taken to match length of the short pseudonym
  return DEFAULT_PLACEHOLDER;
}

void Pseudonymiser::pseudonymise(std::istream& in, std::function<void(const char*, const std::streamsize)> writeToDestination) {
  PseudonymiseInputFilter pseudonymiseFilter(mOldValue, mNewValue);
  boost::iostreams::filtering_istream filteringStream;
  filteringStream.push(pseudonymiseFilter);
  filteringStream.push(in);
  char buffer[PSEUDONYMISER_BUFFER_SIZE]{};
  for (;;) {
    auto amount = boost::iostreams::read(filteringStream, buffer, PSEUDONYMISER_BUFFER_SIZE);

    if (amount == boost::iostreams::WOULD_BLOCK) {
      // WOULD_BLOCK is as of yet unimplemented. When it is, we want a heads up.
      throw std::runtime_error("boost::iostreams::WOULD_BLOCK encountered during pseudonymisation. Aborting process.");
    }
    if (amount != EOF) {
      assert(amount >= 0);
      writeToDestination(buffer, amount);
    }
    else {
      break;
    }
  }
}

}

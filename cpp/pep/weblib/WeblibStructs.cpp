#include <pep/weblib/WeblibStructs.hpp>

#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/weblib/EmscriptenMapBinding.hpp>
#include <pep/weblib/EmscriptenVectorBinding.hpp>
#include <pep/weblib/ObservableStream.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <emscripten/bind.h>

using namespace emscripten;
using namespace pep;
using namespace pep::weblib;

// Add Embind serialization for weblib structures

#define PEP_FIELD(r, type, member) \
    .field(BOOST_PP_STRINGIZE(member), &type::member)

#define PEP_BINDINGS(type, ...) \
    EMSCRIPTEN_BINDINGS(type) { \
        value_object<type>(BOOST_PP_STRINGIZE(type)) \
            BOOST_PP_SEQ_FOR_EACH( \
                PEP_FIELD, \
                type, \
                BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__) \
            ); \
    }

EMSCRIPTEN_BINDINGS(optionals) {
  register_optional<decltype(ListQuery::subjectGroups)::value_type>();
  register_optional<decltype(ListQuery::subjects)::value_type>();
  register_optional<decltype(ListQuery::columnGroups)::value_type>();
  register_optional<decltype(ListQuery::columns)::value_type>();
  register_optional<decltype(std::declval<CellEntry>().partialMetadataView())::mapped_type::value_type>();
}

PEP_BINDINGS(ListQuery,
    subjectGroups,
    subjects,
    columnGroups,
    columns
)

PEP_BINDINGS(ColumnGroup,
    name,
    columns
)

PEP_BINDINGS(EnrolledUser,
    userGroup,
    user
)

PEP_BINDINGS(SubjectGroup,
    name
)

PEP_BINDINGS(ParticipantPersonalia,
    firstName,
    middleName,
    lastName,
    dateOfBirth
)

std::unordered_map<std::string, std::optional<val>> CellEntry::partialMetadataView() const {
  using namespace std::ranges;
  return RangeToCollection<decltype(partialMetadataView())>(
    inner->metadata.extra()
    | views::transform([](const auto& entry) {
      const auto& [key, value] = entry;
      std::optional<val> view;
      if (!value.isEncrypted()) {
        std::span plaintext = pep::ConvertBytes<std::uint8_t>(value.plaintext());
        view.emplace(typed_memory_view(plaintext.size(), plaintext.data()));
      }
      return std::pair{key, std::move(view)};
    })
  );
}

std::string CellEntry::id() const {
  return boost::algorithm::hex(inner->id);
}

std::string CellEntry::subjectLocalPseudonym() const {
  assert(inner->accessGroupPseudonym && "accessGroupPseudonym not set");
  return inner->accessGroupPseudonym->text();
}

std::string CellEntry::subjectEncryptedOriginId() const {
  return inner->localPseudonyms->polymorphic.text();
}

const std::string& CellEntry::column() const { return inner->column; }
double CellEntry::fileSize() const { return static_cast<double>(inner->fileSize); }

CellData::CellData(const CellEntry* entry, val contentReadableStream)
      : entry{entry}, contentReadableStream(std::move(contentReadableStream)) {}

val CellData::content() const {
  return *contentReadableStream;
}


// These classes will remain alive in C++ land, unlike the value_object structs above, which get mapped to plain JS objects
EMSCRIPTEN_BINDINGS(weblibStructs) {
  class_<CellEntry>("CellEntry")
    .property("id", &CellEntry::id)
    .property("subjectLocalPseudonym", &CellEntry::subjectLocalPseudonym)
    .property("subjectEncryptedOriginId", &CellEntry::subjectEncryptedOriginId)
    .property("column", &CellEntry::column)
    .property("fileSize", &CellEntry::fileSize)
    .function("partialMetadataView", &CellEntry::partialMetadataView)
  ;
  class_<CellData>("CellData")
    .property("entry", FieldAsConst(&CellData::entry), return_value_policy::reference(), nonnull<ret_val>())
    .property("content", &CellData::content)
  ;
}

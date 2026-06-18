#include <pep/weblib/WeblibStructs.hpp>

#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/weblib/EmscriptenMapBinding.hpp>
#include <pep/weblib/EmscriptenVectorBinding.hpp>
#include <pep/weblib/ObservableStream.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <emscripten/bind.h>

using namespace emscripten;
using namespace pep;
using namespace pep::weblib;

// Add Embind serialization for weblib structures

#define PEP_BINDINGS_IMPL(cur_struct) \
  EMSCRIPTEN_BINDINGS(cur_struct) { \
    value_object<cur_struct>(BOOST_PP_STRINGIZE(cur_struct))

#define PEP_BINDINGS PEP_BINDINGS_IMPL(PEP_CUR_STRUCT)

#define PEP_BINDINGS_END ; }

#define PEP_FIELD(name) .field(#name, &PEP_CUR_STRUCT::name)


EMSCRIPTEN_BINDINGS(optionals) {
  register_optional<decltype(ListQuery::subjectGroups)::value_type>();
  register_optional<decltype(ListQuery::subjects)::value_type>();
  register_optional<decltype(ListQuery::columnGroups)::value_type>();
  register_optional<decltype(ListQuery::columns)::value_type>();
  register_optional<decltype(std::declval<CellEntry>().partialMetadataView())::mapped_type::value_type>();
}

//@formatter:off

#undef PEP_CUR_STRUCT
#define PEP_CUR_STRUCT ListQuery
PEP_BINDINGS
  PEP_FIELD(subjectGroups)
  PEP_FIELD(subjects)
  PEP_FIELD(columnGroups)
  PEP_FIELD(columns)
PEP_BINDINGS_END

#undef PEP_CUR_STRUCT
#define PEP_CUR_STRUCT ColumnGroup
PEP_BINDINGS
  PEP_FIELD(name)
  PEP_FIELD(columns)
PEP_BINDINGS_END

#undef PEP_CUR_STRUCT
#define PEP_CUR_STRUCT EnrolledUser
PEP_BINDINGS
  PEP_FIELD(userGroup)
  PEP_FIELD(user)
PEP_BINDINGS_END

#undef PEP_CUR_STRUCT
#define PEP_CUR_STRUCT SubjectGroup
PEP_BINDINGS
  PEP_FIELD(name)
PEP_BINDINGS_END

#undef PEP_CUR_STRUCT
#define PEP_CUR_STRUCT ParticipantPersonalia
PEP_BINDINGS
  PEP_FIELD(firstName)
  PEP_FIELD(middleName)
  PEP_FIELD(lastName)
  PEP_FIELD(dateOfBirth)
PEP_BINDINGS_END

//@formatter:on

std::unordered_map<std::string, std::optional<val>> CellEntry::partialMetadataView() const {
  using namespace std::ranges;
  return RangeToCollection<decltype(partialMetadataView())>(
    inner->mMetadata.extra()
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
  return boost::algorithm::hex(inner->mId);
}

std::string CellEntry::subjectLocalPseudonym() const {
  assert(inner->mAccessGroupPseudonym && "mAccessGroupPseudonym not set");
  return inner->mAccessGroupPseudonym->text();
}

std::string CellEntry::subjectEncryptedOriginId() const {
  return inner->mLocalPseudonyms->mPolymorphic.text();
}

const std::string& CellEntry::column() const { return inner->mColumn; }
double CellEntry::fileSize() const { return static_cast<double>(inner->mFileSize); }

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

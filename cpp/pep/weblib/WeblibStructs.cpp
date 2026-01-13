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

#include <rxcpp/operators/rx-map.hpp>

#include <type_traits>

using namespace emscripten;
using namespace pep;
using namespace pep::weblib;

// Add Embind serialization for weblib structures

#undef BINDINGS_IMPL
#define BINDINGS_IMPL(cur_struct) \
  EMSCRIPTEN_BINDINGS(cur_struct) { \
    value_object<cur_struct>(BOOST_PP_STRINGIZE(cur_struct))

#undef BINDINGS
#define BINDINGS BINDINGS_IMPL(CUR_STRUCT)

#undef BINDINGS_END
#define BINDINGS_END ; }

#undef FIELD
#define FIELD(name) .field(#name, &CUR_STRUCT::name)


EMSCRIPTEN_BINDINGS(optionals) {
  register_optional<decltype(ListQuery::subjectGroups)::value_type>();
  register_optional<decltype(ListQuery::subjects)::value_type>();
  register_optional<decltype(ListQuery::columnGroups)::value_type>();
  register_optional<decltype(ListQuery::columns)::value_type>();
  register_optional<decltype(std::declval<CellEntry>().partialMetadataView())::mapped_type::value_type>();
}

//@formatter:off

#undef CUR_STRUCT
#define CUR_STRUCT ListQuery
BINDINGS
  FIELD(subjectGroups)
  FIELD(subjects)
  FIELD(columnGroups)
  FIELD(columns)
BINDINGS_END

#undef CUR_STRUCT
#define CUR_STRUCT ColumnGroup
BINDINGS
  FIELD(name)
  FIELD(columns)
BINDINGS_END

#undef CUR_STRUCT
#define CUR_STRUCT EnrolledUser
BINDINGS
  FIELD(userGroup)
  FIELD(user)
BINDINGS_END

#undef CUR_STRUCT
#define CUR_STRUCT SubjectGroup
BINDINGS
  FIELD(name)
BINDINGS_END


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
std::uint64_t CellEntry::fileSize() const { return inner->mFileSize; }

CellData::CellData(const CellEntry* entry, val contentReadableStream)
      : entry{entry}, contentReadableStream(std::move(contentReadableStream)) {}

val CellData::content() const {
  return *contentReadableStream;
}


EMSCRIPTEN_BINDINGS(CellEntry) {
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

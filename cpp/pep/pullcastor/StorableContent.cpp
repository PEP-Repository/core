#include <pep/async/RxGetOne.hpp>
#include <pep/pullcastor/StorableContent.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep {
namespace castor {

namespace {

//! \return Whether 2 ptrees are equal, not taking into account the order of the keys
bool PtreesEqual(const boost::property_tree::ptree& p1, const boost::property_tree::ptree& p2) {
  if (p1.data() != p2.data()) {
    return false;
  }
  if (p1.size() != p2.size()) {
    return false;
  }
  for (auto it1 = p1.ordered_begin(), it2 = p2.ordered_begin(); it1 != p1.not_found(); it1++, it2++) {
    if (it1->first != it2->first) {
      return false;
    }
    if (!PtreesEqual(it1->second, it2->second)) {
      return false;
    }
  }
  return true;
}

/*!
  * \brief Cell content that stores JSON data.
  * \remark Provides better comparison to existing (string) data than simple CellData does, preventing unnecessary updates being sent to PEP.
  */
class JsonCellContent : public CellContent, public SharedConstructor<JsonCellContent> {
private:
  mutable std::optional<std::string> mValue;
  std::shared_ptr<boost::property_tree::ptree> mStructure;

public:
  using SharedConstructor<JsonCellContent>::Create;

  explicit JsonCellContent(std::shared_ptr<boost::property_tree::ptree> structure)
    : mStructure(structure) {
  }

  /*!
  * \brief Produces JSON corresponding with this instance's Ptree structure.
  * \return A string containing JSON data.
  */
  const std::string& getValue() const {
    if (!mValue.has_value()) {
      assert(mStructure != nullptr);
      std::stringstream stream;
      boost::property_tree::write_json(stream, *mStructure);
      mValue = stream.str();
    }
    return *mValue;
  }

  /*!
    * \brief Produces (an observable emitting) the raw (binary) data in the cell.
    * \return (An observable emitting) the cell's raw, binary data.
    */
  rxcpp::observable<std::string> getData() const override {
    return rxcpp::observable<>::just(this->getValue());
  }

  /*!
    * \brief Produces (an observable emitting) data to store if the cell should contain this instance's data, but currently contains the specified data.
    * \param existing The data currently stored in PEP.
    * \return (An observable emitting) this instance's data if it doesn't match the existing data.
    * \remark An update won't be required if named Ptree nodes are ordered differently in existing data.
    */
  rxcpp::observable<std::string> getDataToStore(const std::string& existing) const override {
    boost::property_tree::ptree tree;
    std::stringstream content(existing);
    try {
      boost::property_tree::read_json(content, tree);
    }
    catch (const boost::property_tree::json_parser_error&) {
      // The existing data is apparently not JSON and (therefore) needs to be updated to our JSON content
      return rxcpp::observable<>::just(this->getValue());
    }
    if (PtreesEqual(*mStructure, tree)) {
      return rxcpp::observable<>::empty<std::string>();
    }
    return rxcpp::observable<>::just(this->getValue());
  }

};

}

StorableColumnContent::StorableColumnContent(const std::string& column, std::shared_ptr<CellContent> content, const std::string& fileExtension)
  : mColumn(column), mContent(content), mFileExtension(fileExtension) {
}

rxcpp::observable<std::shared_ptr<StorableColumnContent>> StorableColumnContent::CreateJson(
  const std::string& column,
  rxcpp::observable<std::shared_ptr<FieldValue>> fvs,
  std::shared_ptr<boost::property_tree::ptree> reports) {
  assert(reports != nullptr);

  return FieldValue::Aggregate(fvs)
    .op(RxGetOne("CRF tree"))
    .map([column, reports](std::shared_ptr<boost::property_tree::ptree> crf) {
    auto tree = std::make_shared<boost::property_tree::ptree>();
    tree->put_child("crf", *crf); // JSON node is called "crf" regardless of the type of data we're pulling (i.e. study or survey)
    tree->put_child("reports", *reports); // JSON node is called "reports" because Castor's repeating data used to be named that way

    auto json = JsonCellContent::Create(tree);
    return StorableColumnContent::Create(column, json, ".json");
    });
}

StorableCellContent::StorableCellContent(const ColumnBoundParticipantId& cbpId, const std::string& column, std::shared_ptr<const CellContent> content, const std::string& fileExtension)
  : mCbpId(cbpId), mColumn(column), mContent(content), mFileExtension(fileExtension) {
}

}
}

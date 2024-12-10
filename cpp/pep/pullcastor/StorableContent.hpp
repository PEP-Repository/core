#pragma once

#include <pep/pullcastor/CellContent.hpp>
#include <pep/pullcastor/FieldValue.hpp>
#include <pep/pullcastor/ColumnBoundParticipantId.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Cell content associated with a column name.
  */
class StorableColumnContent : public std::enable_shared_from_this<StorableColumnContent>, public SharedConstructor<StorableColumnContent> {
  friend class SharedConstructor<StorableColumnContent>;

private:
  std::string mColumn;
  std::shared_ptr<CellContent> mContent;
  std::string mFileExtension;

  StorableColumnContent(const std::string& column, std::shared_ptr<CellContent> content, const std::string& fileExtension);

public:
  /*!
    * \brief Produces the column name.
    * \return The column name.
    */
  inline const std::string& getColumn() const noexcept { return mColumn; }

  /*!
    * \brief Produces the cell content.
    * \return A CellContent instance.
    */
  inline std::shared_ptr<const CellContent> getContent() const noexcept { return mContent; }

  /*!
    * \brief Produces the file extension for this column content.
    * \return The file extension for this column content.
    */
  inline const std::string& getFileExtension() const noexcept { return mFileExtension; }

  /*!
    * \brief Produces (an observable emitting a single) StorableColumnContent with JsonCellContent containing the specified FieldValue values.
    * \param column The name of the column.
    * \param fvs The field values to collect into the JSON's "crf" node.
    * \param reports The Ptree containing the data for the JSON's "reports" node.
    * \return (An observable emitting a single) StorableColumnContent encapsulating a JsonCellContent instance.
    */
  static rxcpp::observable<std::shared_ptr<StorableColumnContent>> CreateJson(
    const std::string& column,
    rxcpp::observable<std::shared_ptr<FieldValue>> fvs,
    std::shared_ptr<boost::property_tree::ptree> reports);
};


/*!
  * \brief Cell content associated with a column name and a Castor participant ID.
  */
class StorableCellContent : public SharedConstructor<StorableCellContent> {
  friend class SharedConstructor<StorableCellContent>;

private:
  ColumnBoundParticipantId mCbpId;
  std::string mColumn;
  std::shared_ptr<const CellContent> mContent;
  std::string mFileExtension;

  StorableCellContent(const ColumnBoundParticipantId& cbrId, const std::string& column, std::shared_ptr<const CellContent> content, const std::string& fileExtension);

public:
  /*!
    * \brief Produces the column-bound participant ID.
    * \return The Castor participant ID associated with the PEP (SP) column that the ID applies to.
    */
  inline const ColumnBoundParticipantId& getColumnBoundParticipantId() const noexcept { return mCbpId; }

  /*!
    * \brief Produces the column name.
    * \return The column name.
    */
  inline const std::string& getColumn() const noexcept { return mColumn; }

  /*!
    * \brief Produces the cell content.
    * \return A CellContent instance.
    */
  inline std::shared_ptr<const CellContent> getContent() const noexcept { return mContent; }

  /*!
    * \brief Produces the file extension for this content.
    * \return The file extension for this content.
    */
  inline const std::string& getFileExtension() const noexcept { return mFileExtension; }
};

}
}

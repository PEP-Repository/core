#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Base class for PEP cell content.
  */
class CellContent : public std::enable_shared_from_this<CellContent> {
public:
  explicit CellContent() = default;
  CellContent(const CellContent& other) = default;
  CellContent(CellContent&& other) = default;
  CellContent& operator=(const CellContent& other) = default;
  CellContent& operator=(CellContent&& other) = default;
  virtual ~CellContent() = default;

  /*!
    * \brief Produces (an observable emitting) the raw (binary) data in the cell.
    * \return (An observable emitting) the cell's raw, binary data.
    */
  virtual rxcpp::observable<std::string> getData() const = 0;

  /*!
    * \brief Produces (an observable emitting) data to store if the cell should contain this instance's data, but currently contains the specified data.
    * \param existing The data currently stored in PEP.
    * \return (An observable emitting) this instance's data if it doesn't match the existing data.
    */
  virtual rxcpp::observable<std::string> getDataToStore(const std::string& existing) const;

  /*!
    * \brief Creates a CellContent instance for the specified EnumerateAndRetrieveResult.
    * \param client The client that produced the EnumerateAndRetrieveResult.
    * \param ticket The ticket used to produce the EnumerateAndRetrieveResult.
    * \param ear The EnumerateAndRetrieveResult containing cell data retrieved from PEP.
    * \return (An observable emitting) this instance's data if it doesn't match the existing data.
    */
  static std::shared_ptr<CellContent> Create(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear);
};


/*!
  * \brief Cell content whose data are available during construction.
  * \remark Contrast with LazyCellContent (below).
  */
class PreloadedCellContent : public CellContent, public SharedConstructor<PreloadedCellContent> {
  friend class CellContent;
  friend class SharedConstructor<PreloadedCellContent>;

private:
  std::string mValue;

  explicit PreloadedCellContent(const std::string& value);

  /*!
    * \brief Creates a PreloadedCellContent instance if the data are available.
    * \param ear An EnumerateAndRetrieveResult that may or may not contain the cell data.
    * \return A PreloadedCellContent instance if data available, or a nullptr if not.
    */
  static std::shared_ptr<PreloadedCellContent> TryCreate(const EnumerateAndRetrieveResult& ear);

public:
  using SharedConstructor<PreloadedCellContent>::Create;

  /*!
    * \brief Preloads the data for the specified PEP cell and returns (an observable emitting) a corresponding PreloadedCellContent.
    * \param client The client that produced the EnumerateAndRetrieveResult.
    * \param ticket The ticket used to produce the EnumerateAndRetrieveResult.
    * \param ear The EnumerateAndRetrieveResult containing cell data retrieved from PEP.
    * \return (An observable emitting) a PreloadedCellContent for the specified EnumerateAndRetrieveResult.
    */
  static rxcpp::observable<std::shared_ptr<PreloadedCellContent>> Load(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear);

  /*!
    * \brief Produces (an observable emitting) the raw (binary) data in the cell.
    * \remark Use this method only when dealing with the CellData base class. If you already (know you) have a PreloadedCellContent, use the faster getValue() method (below) instead.
    */
  rxcpp::observable<std::string> getData() const override;

  /*!
    * \brief Produces (an observable emitting) the raw (binary) data in the cell.
    * \return (An observable emitting) the cell's raw, binary data.
    */
  inline const std::string& getValue() const noexcept { return mValue; }
};


/*!
  * \brief Cell content whose data are not available immediately and must still be retrieved from PEP.
  * \remark Contrast with PreloadedCellContent (above).
  */
class LazyCellContent : public CellContent, private SharedConstructor<LazyCellContent> {
  friend class CellContent;
  friend class SharedConstructor<LazyCellContent>;
  using SharedConstructor<LazyCellContent>::Create;

private:
  std::shared_ptr<RxCache<std::string>> mData;

  LazyCellContent(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, std::shared_ptr<EnumerateResult> entry);

public:
  /*!
    * \brief Produces (an observable emitting) the raw (binary) data in the cell.
    * \return (An observable emitting) the cell's raw, binary data.
    */
  rxcpp::observable<std::string> getData() const override;
};

}
}

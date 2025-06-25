#include <pep/assessor/participantselector.hpp>

#include <pep/assessor/ui_participantselector.h>

#include <pep/assessor/InputValidationTooltip.hpp>

#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <cassert>
#include <boost/algorithm/string/join.hpp>

namespace {

QRegularExpression GetPseudonymsRegex(const std::vector<pep::PseudonymFormat>& formats) {
  if (formats.empty()) {
    throw std::runtime_error("Input validation not possible: no pseudonym format specified");
  }
  std::vector<std::string> entries;
  entries.reserve(formats.size());
  std::transform(formats.cbegin(), formats.cend(), std::back_inserter(entries), [](const pep::PseudonymFormat& format) {return format.getRegexPattern(); });
  auto pattern = "^(" + boost::algorithm::join(entries, "|") + ")$";
  return QRegularExpression(QString::fromStdString(pattern));
}

}

/*! \brief Select participant
 *
 * This function manages the validation of input from the user and emits the validated SID or short pseudonym
 *
 * \param parent The parent object of this one. Needed by the Qt framework.
 */
ParticipantSelector::ParticipantSelector(QWidget* parent, const pep::GlobalConfiguration& config) :
  QWidget(parent),
  ui(new Ui::ParticipantSelector) {
  ui->setupUi(this);
  ui->retranslateUi(this);

  ui->sidInput->setAttribute(Qt::WA_MacShowFocusRect, false);

  // See #1784: ensure we can enter IDs produced by the (primary) generated format...
  auto maxLength = *config.getGeneratedParticipantIdentifierFormat().getLength();
  // ... and ensure we can also enter other formats.
  for (const auto& format : config.getParticipantIdentifierFormats()) {
    auto length = format.getLength();
    if (length.has_value()) { // Only process generated formats: we don't want to determine the (max) data length allowed by a regex
      maxLength = std::max(maxLength, *length);
    }
  }
  ui->sidInput->setMaxLength(static_cast<int>(maxLength));

  const auto& sps = config.getShortPseudonyms();
  std::vector<pep::PseudonymFormat> spFormats;
  spFormats.reserve(sps.size());
  std::transform(sps.cbegin(), sps.cend(), std::back_inserter(spFormats), [](const pep::ShortPseudonymDefinition& definition) {return pep::PseudonymFormat(definition.getPrefix(), definition.getLength()); });

  ui->sidInput->setValidator(new QRegularExpressionValidator(GetPseudonymsRegex(config.getParticipantIdentifierFormats()), ui->sidInput));
  SetInputValidationTooltip(ui->sidInput, tr("participant-id-tooltip"));
  if (spFormats.empty()) {
    ui->shortPseudonymInput->setEnabled(false);
  }
  else {
    ui->shortPseudonymInput->setValidator(new QRegularExpressionValidator(GetPseudonymsRegex(spFormats), ui->shortPseudonymInput));
    SetInputValidationTooltip(ui->shortPseudonymInput, tr("participant-short-pseudonym-tooltip"));
  }

  QObject::connect(ui->sidInput, &QLineEdit::textChanged, this, [this]() {
    ui->openParticipantButton->setEnabled(ui->sidInput->hasAcceptableInput());
  });
  QObject::connect(ui->shortPseudonymInput, &QLineEdit::textChanged, this, [this]() {
    ui->findShortPseudonymButton->setEnabled(ui->shortPseudonymInput->hasAcceptableInput());
    SetInputValidationTooltip(ui->shortPseudonymInput, tr("participant-short-pseudonym-tooltip"));
  });

  QObject::connect(ui->cancelButton, &QPushButton::clicked, this, [this]() {
    emit cancelled();
  });
  QObject::connect(ui->openParticipantButton, &QPushButton::clicked, this, [this]() {
    if (ui->sidInput->hasAcceptableInput()) {
      //Do normal SID lookup
      emit participantSidSelected(ui->sidInput->text().toUpper().toStdString());
    }
  });
  QObject::connect(ui->findShortPseudonymButton, &QPushButton::clicked, this, [this]() {
    if (ui->shortPseudonymInput->hasAcceptableInput()) {
      //Do Short pseudonym lookup
      emit participantShortPseudonymSelected(ui->shortPseudonymInput->text().toUpper().toStdString());
    }
  });
  QObject::connect(ui->sidInput, &QLineEdit::returnPressed, ui->openParticipantButton, &QPushButton::click);
  QObject::connect(ui->shortPseudonymInput, &QLineEdit::returnPressed, ui->findShortPseudonymButton, &QPushButton::click);
}

/*! \brief Destructor
 *
 * Clears out the UI object.
 */
ParticipantSelector::~ParticipantSelector() {
  delete ui;
}

/*! \brief Set UI focus to the SID input
 */
void ParticipantSelector::doFocus() {
  ui->sidInput->setFocus();
}


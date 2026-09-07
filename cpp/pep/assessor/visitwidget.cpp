#include <pep/assessor/UserRole.hpp>
#include <pep/assessor/assessorwidget.hpp>
#include <pep/assessor/visitwidget.hpp>

#include <pep/assessor/ui_visitwidget.h>

#include <QDialog>
#include <QLabel>


VisitWidget::VisitWidget(const std::vector<pep::AssessorDefinition>& assessors, pep::UserRole& currentPepRole_, pep::StudyContext& studyContext, QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::VisitWidget),
  assessors_(assessors),
  currentPepRole_(currentPepRole_),
  studyContext_(studyContext)
{
  ui_->setupUi(this);
  ui_->retranslateUi(this);

  QObject::connect(ui_->edit_assessor, &QPushButton::clicked, this, &VisitWidget::openEditAssessor);

  //initializes a data castor button bar
  dataCastorButtons_ = new ButtonBar(this);
  ui_->data_castorButtonBar_layout->addWidget(dataCastorButtons_);

  //initializes a print button bar and adds buttons
  printButtons_ = new ButtonBar(this);
  ui_->print_buttonBar_layout->addWidget(printButtons_);

  printButtons_->addButton(tr("print-summary"), std::bind(&VisitWidget::printSummary, this), true);
  printStickersButton_ = printButtons_->addButton(tr("print-stickers"), std::bind(&VisitWidget::printAllStickers, this), true);
  printOneStickerButton_ = printButtons_->addButton(tr("print-one-sticker"), std::bind(&VisitWidget::printSingleSticker, this), true);
  printButtons_->addButton(tr("locate-bartender"), std::bind(&VisitWidget::locateBartender, this), true);
}

VisitWidget::~VisitWidget()
{
  delete ui_;
}

void VisitWidget::disablePrinting() {
  printButtons_->setEnabled(false); 
}

void VisitWidget::disableAssessorSelection() {
  ui_->edit_assessor->setEnabled(false);
}

QLabel& VisitWidget::getPseudonymButtonCaption() {
  return *ui_->data_gathering_header;
}

ButtonBar& VisitWidget::getPseudonymButtonBar() {
  return *dataCastorButtons_;
}

QSpacerItem& VisitWidget::getPseudonymButtonSpacer() {
  return *ui_->verticalSpacer_1;
}

QLabel& VisitWidget::getPseudonymCaption() {
  return *ui_->pseudonyms_header;
}

QLabel& VisitWidget::getPseudonymLabel() {
  return *ui_->pseudonymLabel;
}

QSpacerItem& VisitWidget::getPseudonymSpacerForOtherVisits() {
  return *ui_->verticalSpacer_pseudonyms_other_visits;
}

QLabel& VisitWidget::getPseudonymCaptionForOtherVisits() {
  return *ui_->pseudonyms_header_other_visits;
}

QLabel& VisitWidget::getPseudonymLabelForOtherVisits() {
  return *ui_->pseudonymLabel_other_visits;
}

QPushButton& VisitWidget::getPrintAllButton() {
  return *printStickersButton_;
}

QPushButton& VisitWidget::getPrintOneButton() {
  return *printOneStickerButton_;
}

void VisitWidget::setCurrentAssessor(const std::optional<unsigned>& id) {
  currentAssessorId_ = id;
  if(id.has_value()) {
    auto position = std::find_if(assessors_.cbegin(), assessors_.cend(),
                                               [&id](const pep::AssessorDefinition &candidate) { return candidate.id == id; });
    if (position == assessors_.cend()) {
      ui_->currentAssessorLabel->setText(tr("<assessor %1>").arg(QString::number(*id)));
    }
    else {
      size_t index = static_cast<size_t>(position - assessors_.cbegin());
      ui_->currentAssessorLabel->setText(
        QString::fromStdString(assessors_.at(index).name));
    }
  }
  else
    ui_->currentAssessorLabel->setText(tr("<none/unspecified>"));

  //ui_->retranslateUi(this);
}

void VisitWidget::openEditAssessor() {
  if (!currentPepRole_.canEditVisitAdministeringAssessor()) {
    return;
  }

  QDialog* editVisitAssessor = new QDialog(this);
  editVisitAssessor->setModal(true);

  QVBoxLayout* layoutVisitAssessor = new QVBoxLayout(this);
  auto editor = new AssessorWidget();

  editor->setAssessors(assessors_, studyContext_);
  editor->setCurrentAssessor(currentAssessorId_);

  QObject::connect(editor, &AssessorWidget::updateIssued, this, &VisitWidget::updateVisitAssessor);

  layoutVisitAssessor->addWidget(editor);
  editVisitAssessor->setLayout(layoutVisitAssessor);
  layoutVisitAssessor->setSizeConstraint(QLayout::SetFixedSize);

  editVisitAssessor->show();
}

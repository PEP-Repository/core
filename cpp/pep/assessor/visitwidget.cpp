#include <pep/assessor/UserRole.hpp>
#include <pep/assessor/assessorwidget.hpp>
#include <pep/assessor/visitwidget.hpp>

#include <pep/assessor/ui_visitwidget.h>

#include <QDialog>
#include <QLabel>


VisitWidget::VisitWidget(const std::vector<pep::AssessorDefinition>& assessors, pep::UserRole& currentPEPRole, pep::StudyContext& studyContext, QWidget *parent) :
  QWidget(parent),
  ui(new Ui::VisitWidget),
  assessors(assessors),
  currentPEPRole(currentPEPRole),
  studyContext(studyContext)
{
  ui->setupUi(this);
  ui->retranslateUi(this);

  QObject::connect(ui->edit_assessor, &QPushButton::clicked, this, &VisitWidget::openEditAssessor);

  //initializes a data castor button bar
  data_castor_buttons = new ButtonBar(this);
  ui->data_castorButtonBar_layout->addWidget(data_castor_buttons);

  //initializes a print button bar and adds buttons
  print_buttons = new ButtonBar(this);
  ui->print_buttonBar_layout->addWidget(print_buttons);

  print_buttons->addButton(tr("print-summary"), std::bind(&VisitWidget::printSummary, this), true);
  print_stickers_button = print_buttons->addButton(tr("print-stickers"), std::bind(&VisitWidget::printAllStickers, this), true);
  print_oneSticker_button = print_buttons->addButton(tr("print-one-sticker"), std::bind(&VisitWidget::printSingleSticker, this), true);
  print_buttons->addButton(tr("locate-bartender"), std::bind(&VisitWidget::locateBartender, this), true);
}

VisitWidget::~VisitWidget()
{
  delete ui;
}

void VisitWidget::disablePrinting() {
  print_buttons->setEnabled(false); 
}

void VisitWidget::disableAssessorSelection() {
  ui->edit_assessor->setEnabled(false);
}

QLabel& VisitWidget::getPseudonymButtonCaption() {
  return *ui->data_gathering_header;
}

ButtonBar& VisitWidget::getPseudonymButtonBar() {
  return *data_castor_buttons;
}

QSpacerItem& VisitWidget::getPseudonymButtonSpacer() {
  return *ui->verticalSpacer_1;
}

QLabel& VisitWidget::getPseudonymCaption() {
  return *ui->pseudonyms_header;
}

QLabel& VisitWidget::getPseudonymLabel() {
  return *ui->pseudonymLabel;
}

QSpacerItem& VisitWidget::getPseudonymSpacerForOtherVisits() {
  return *ui->verticalSpacer_pseudonyms_other_visits;
}

QLabel& VisitWidget::getPseudonymCaptionForOtherVisits() {
  return *ui->pseudonyms_header_other_visits;
}

QLabel& VisitWidget::getPseudonymLabelForOtherVisits() {
  return *ui->pseudonymLabel_other_visits;
}

QPushButton& VisitWidget::getPrintAllButton() {
  return *print_stickers_button;
}

QPushButton& VisitWidget::getPrintOneButton() {
  return *print_oneSticker_button;
}

void VisitWidget::setCurrentAssessor(const std::optional<unsigned>& id) {
  currentAssessorId = id;
  if(id.has_value()) {
    auto position = std::find_if(assessors.cbegin(), assessors.cend(),
                                               [&id](const pep::AssessorDefinition &candidate) { return candidate.id == id; });
    if (position == assessors.cend()) {
      ui->currentAssessorLabel->setText(tr("<assessor %1>").arg(QString::number(*id)));
    }
    else {
      size_t index = static_cast<size_t>(position - assessors.cbegin());
      ui->currentAssessorLabel->setText(
        QString::fromStdString(assessors.at(index).name));
    }
  }
  else
    ui->currentAssessorLabel->setText(tr("<none/unspecified>"));

  //ui->retranslateUi(this);
}

void VisitWidget::openEditAssessor() {
  if (!currentPEPRole.canEditVisitAdministeringAssessor()) {
    return;
  }

  QDialog* editVisitAssessor = new QDialog(this);
  editVisitAssessor->setModal(true);

  QVBoxLayout* layoutVisitAssessor = new QVBoxLayout(this);
  auto editor = new AssessorWidget();

  editor->setAssessors(assessors, studyContext);
  editor->setCurrentAssessor(currentAssessorId);

  QObject::connect(editor, &AssessorWidget::updateIssued, this, &VisitWidget::updateVisitAssessor);

  layoutVisitAssessor->addWidget(editor);
  editVisitAssessor->setLayout(layoutVisitAssessor);
  layoutVisitAssessor->setSizeConstraint(QLayout::SetFixedSize);

  editVisitAssessor->show();
}

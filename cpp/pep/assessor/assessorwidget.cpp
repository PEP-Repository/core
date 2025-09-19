#include <pep/assessor/assessorwidget.hpp>

#include <pep/assessor/ui_assessorwidget.h>

AssessorWidget::AssessorWidget(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::AssessorWidget)
{
  ui->setupUi(this);

  QObject::connect(ui->assessorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAssessorComboIndexChanged(int)));
  QObject::connect(ui->cancelButton, &QAbstractButton::clicked, this, &AssessorWidget::closeWidget);
  QObject::connect(ui->updateButton, &QAbstractButton::clicked, [this]() {
    ui->updateButton->setEnabled(false);
    QString id;
    auto index = ui->assessorComboBox->currentIndex() - 1; // The first item is "<none/unspecified>"
    if (index >= 0) {
      id = QString::number(mAssessors[static_cast<size_t>(index)].id);
    }
    emit updateIssued(id);
    closeWidget();
  });
}

AssessorWidget::~AssessorWidget()
{
  delete ui;
}

void AssessorWidget::setAssessors(const std::vector<pep::AssessorDefinition>& assessors, const pep::StudyContext& studyContext) {
  if (!mAssessors.empty()) {
    throw std::runtime_error("Can only set assessors once");
  }
  std::copy_if(assessors.cbegin(), assessors.cend(), std::back_inserter(mAssessors), [&studyContext](const pep::AssessorDefinition& candidate) {return candidate.matchesStudyContext(studyContext); });
  std::sort(mAssessors.begin(), mAssessors.end(), [](const pep::AssessorDefinition& lhs, const pep::AssessorDefinition& rhs) {return strcmp(lhs.name.c_str(), rhs.name.c_str()) < 0; });

  auto enable = !mAssessors.empty();
  ui->assessorComboBox->setEnabled(enable);
  ui->updateButton->setEnabled(enable);

  while (ui->assessorComboBox->count() > 0) {
    ui->assessorComboBox->removeItem(0);
  }
  if (enable) {
    ui->assessorComboBox->addItem(tr("<none/unspecified>"));
    for (const auto& assessor : mAssessors) {
      ui->assessorComboBox->addItem(QString::fromStdString(assessor.name));
    }
    ui->assessorComboBox->setCurrentIndex(0);
    enableDisableUpdateButton();
  }
}

void AssessorWidget::setCurrentAssessor(const std::optional<unsigned int>& id) {
  int index = 0; // Select the <none/unspecified> entry by default

  if (id.has_value()) {
    auto position = std::find_if(mAssessors.cbegin(), mAssessors.cend(), [&id](const pep::AssessorDefinition& candidate) {return candidate.id == id; });
    if (position == mAssessors.cend()) {
      /* Assessor was previously selected and stored but
       * - either removed from GlobalConfiguration,
       * - or study context was removed from assessor.
       */
      index = static_cast<int>(mAssessors.size());
      ui->assessorComboBox->addItem(tr("<assessor %1>").arg(QString::number(*id)));
    }
    else {
      // Remove any <assessor with ID '%1'> entry that may have been added previously
      index = ui->assessorComboBox->count() - 1;
      while (index > static_cast<int>(mAssessors.size())) {
        ui->assessorComboBox->removeItem(index);
        index = ui->assessorComboBox->count() - 1;
      }

      index = static_cast<int>(position - mAssessors.cbegin());
    }

    ++index; // UI index is one greater than data index due to <none/unspecified> entry
  }

  ui->assessorComboBox->setCurrentIndex(index);
  mStoredAssessorIndex = index;
  ui->updateButton->setEnabled(false);
}

void AssessorWidget::onAssessorComboIndexChanged(int newIndex) {
  enableDisableUpdateButton();
}

void AssessorWidget::enableDisableUpdateButton() {
  ui->updateButton->setEnabled(mStoredAssessorIndex != ui->assessorComboBox->currentIndex());
}

void AssessorWidget::closeWidget() {
  this->parent()->deleteLater();
}
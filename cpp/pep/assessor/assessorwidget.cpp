#include <pep/assessor/assessorwidget.hpp>

#include <pep/assessor/ui_assessorwidget.h>

AssessorWidget::AssessorWidget(QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::AssessorWidget)
{
  ui_->setupUi(this);

  QObject::connect(ui_->assessorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAssessorComboIndexChanged(int)));
  QObject::connect(ui_->cancelButton, &QAbstractButton::clicked, this, &AssessorWidget::closeWidget);
  QObject::connect(ui_->updateButton, &QAbstractButton::clicked, [this]() {
    ui_->updateButton->setEnabled(false);
    QString id;
    auto index = ui_->assessorComboBox->currentIndex() - 1; // The first item is "<none/unspecified>"
    if (index >= 0) {
      id = QString::number(assessors_[static_cast<size_t>(index)].id);
    }
    emit updateIssued(id);
    closeWidget();
  });
}

AssessorWidget::~AssessorWidget()
{
  delete ui_;
}

void AssessorWidget::setAssessors(const std::vector<pep::AssessorDefinition>& assessors, const pep::StudyContext& studyContext) {
  if (!assessors_.empty()) {
    throw std::runtime_error("Can only set assessors once");
  }
  std::copy_if(assessors.cbegin(), assessors.cend(), std::back_inserter(assessors_), [&studyContext](const pep::AssessorDefinition& candidate) {return candidate.matchesStudyContext(studyContext); });
  std::sort(assessors_.begin(), assessors_.end(), [](const pep::AssessorDefinition& lhs, const pep::AssessorDefinition& rhs) {return strcmp(lhs.name.c_str(), rhs.name.c_str()) < 0; });

  auto enable = !assessors_.empty();
  ui_->assessorComboBox->setEnabled(enable);
  ui_->updateButton->setEnabled(enable);

  while (ui_->assessorComboBox->count() > 0) {
    ui_->assessorComboBox->removeItem(0);
  }
  if (enable) {
    ui_->assessorComboBox->addItem(tr("<none/unspecified>"));
    for (const auto& assessor : assessors_) {
      ui_->assessorComboBox->addItem(QString::fromStdString(assessor.name));
    }
    ui_->assessorComboBox->setCurrentIndex(0);
    enableDisableUpdateButton();
  }
}

void AssessorWidget::setCurrentAssessor(const std::optional<unsigned int>& id) {
  int index = 0; // Select the <none/unspecified> entry by default

  if (id.has_value()) {
    auto position = std::find_if(assessors_.cbegin(), assessors_.cend(), [&id](const pep::AssessorDefinition& candidate) {return candidate.id == id; });
    if (position == assessors_.cend()) {
      /* Assessor was previously selected and stored but
       * - either removed from GlobalConfiguration,
       * - or study context was removed from assessor.
       */
      index = static_cast<int>(assessors_.size());
      ui_->assessorComboBox->addItem(tr("<assessor %1>").arg(QString::number(*id)));
    }
    else {
      // Remove any <assessor with ID '%1'> entry that may have been added previously
      index = ui_->assessorComboBox->count() - 1;
      while (index > static_cast<int>(assessors_.size())) {
        ui_->assessorComboBox->removeItem(index);
        index = ui_->assessorComboBox->count() - 1;
      }

      index = static_cast<int>(position - assessors_.cbegin());
    }

    ++index; // UI index is one greater than data index due to <none/unspecified> entry
  }

  ui_->assessorComboBox->setCurrentIndex(index);
  storedAssessorIndex_ = index;
  ui_->updateButton->setEnabled(false);
}

void AssessorWidget::onAssessorComboIndexChanged(int newIndex) {
  enableDisableUpdateButton();
}

void AssessorWidget::enableDisableUpdateButton() {
  ui_->updateButton->setEnabled(storedAssessorIndex_ != ui_->assessorComboBox->currentIndex());
}

void AssessorWidget::closeWidget() {
  this->parent()->deleteLater();
}
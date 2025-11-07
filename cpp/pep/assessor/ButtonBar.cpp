#include <pep/assessor/ButtonBar.hpp>

ButtonBar::ButtonBar(QWidget* parent) {
  flowlayout = new FlowLayout();
  flowlayout->setContentsMargins(0, 0, 0, 0);
  this->setLayout(flowlayout);
}

// Adds a button to the button bar and puts the button in a vector that is labeled with the description of the button
QPushButton* ButtonBar::addButton(const QString& description, ButtonClickSlot slot, bool button_enabled = true) {
  auto button = new QPushButton(this);
  button->setStyleSheet(
      "QWidget {\n"
      "  border: 0.05em solid #CA0B5E;\n"
      "  border-radius: 0.25em;\n"
      "  color: #CA0B5E;\n"
      "  padding: 0.5em;\n"
      "  font-size: 13pt;\n"
      "  outline: none;\n"
      "}\n"
      "QWidget:focus {\n"
      "  border: 0.1em solid #CA0B5E;\n"
      "}\n"
      "QWidget:hover {\n"
      "  background-color: rgba(202,11,94,0.8);\n"
      "  color: white;\n"
      "}\n"
      "QWidget:disabled {\n"
      "  color: grey;\n"
      "  border-color: grey;\n"
      "}\n");
  button->setText(description);
  flowlayout->addWidget(button);

  if (button_enabled) {
    QObject::connect(button, &QPushButton::clicked, slot);
    enabled_buttons.push_back(button);
  }
  else
    button->setEnabled(false);

  return button;
}

// enables or disables all the buttons in the button bar
void ButtonBar::setEnabled(bool enable) {
  for(QPushButton* button : enabled_buttons)
    button->setEnabled(enable);
}

// a helperfunction to make void ClearLayout(QLayout *layout) a recursive function
void ButtonBar::clear() {
  ClearLayout(flowlayout);
}

// Adapted (ahem!) from https://stackoverflow.com/a/4857631
void ButtonBar::ClearLayout(QLayout *layout) {
  for (QLayoutItem *item{}; (item = layout->takeAt(0));)
  {
    if (item->layout())
    {
      ClearLayout(item->layout());
      delete item->layout();
    }
    if (item->widget())
    {
      delete item->widget();
    }
    delete item;
  }
}

#include <pep/assessor/InputValidationTooltip.hpp>

#include <QStyle>
#include <QToolTip>

void SetInputValidationTooltip(QLineEdit* widget, QString text) {
  QObject::connect(widget, &QLineEdit::textChanged, [widget, text]() {
    if (!widget->hasAcceptableInput()) {
      widget->setProperty("error", true);
      QPoint coords = widget->mapToGlobal(QPoint());
      coords.ry() += widget->height();
      //QToolTip::setFont(*MainWindow::tooltipFont);
      QToolTip::showText(coords, text);
    }
    else {
      widget->setProperty("error", false);
      QToolTip::hideText();
    }
    // see https://wiki.qt.io/Dynamic_Properties_and_Stylesheets
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
  });
}


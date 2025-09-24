#pragma once

#include <QTimeZone>
#include <QWidget>

namespace Ui {
class DateTimeEditor;
}

class DateTimeEditor : public QWidget
{
  Q_OBJECT

public:
  explicit DateTimeEditor(QWidget *parent = nullptr);
  ~DateTimeEditor() override;

  QDateTime getValue() const;
  void setValue(const QDateTime& value);

signals:
  void valueChanged();

private:
  Ui::DateTimeEditor *ui;
};

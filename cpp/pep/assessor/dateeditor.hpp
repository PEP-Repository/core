#pragma once

#include <QWidget>

namespace Ui {
class DateEditor;
}

class DateEditor : public QWidget
{
  Q_OBJECT

public:
  explicit DateEditor(QWidget *parent = nullptr);
  ~DateEditor() override;

  QDate getValue() const;
  void setValue(const QDate& value);

  bool hasAcceptableInput() const;

signals:
  void valueChanged();

private:
  Ui::DateEditor *ui;
};

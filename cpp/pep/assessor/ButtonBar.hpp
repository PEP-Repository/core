#pragma once

#include <pep/assessor/FlowLayout.hpp>
#include <QPushButton>
#include <vector>
class ButtonBar : public QWidget {
  Q_OBJECT

public:
  using ButtonClickSlot = std::function<void()>;

  explicit ButtonBar(QWidget* parent = nullptr);
  QPushButton* addButton(const QString& description, ButtonClickSlot slot, bool button_enabled);
  /// \brief Enables or disables the buttons in the bar.
  void setButtonsEnabled(bool enable);
  void clear();

private:
  static void ClearLayout(QLayout *layout);
  FlowLayout* flowlayout_;
  std::vector<QPushButton*> enabledButtons_;
};

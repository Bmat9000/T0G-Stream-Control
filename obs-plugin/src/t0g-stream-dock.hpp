#pragma once
#include <QDockWidget>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class T0GStreamDock final : public QDockWidget {
public:
    explicit T0GStreamDock(QWidget *parent = nullptr);
private:
    void updateReadyState();
    QLineEdit *titleEdit{};
    QLineEdit *gameEdit{};
    QComboBox *audienceBox{};
    QCheckBox *twitchEnabled{};
    QCheckBox *tiktokEnabled{};
    QLabel *twitchStatus{};
    QLabel *tiktokStatus{};
    QPushButton *updateButton{};
    QPushButton *goLiveButton{};
};

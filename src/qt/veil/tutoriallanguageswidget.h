// Copyright (c) 2019 The Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TUTORIALLANGUAGESWIDGET_H
#define TUTORIALLANGUAGESWIDGET_H

#include <QWidget>

namespace Ui {
class TutorialLanguagesWidget;
}

class TutorialLanguagesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TutorialLanguagesWidget(QWidget *parent = nullptr);
    std::string GetLanguageSelection();
    ~TutorialLanguagesWidget();

private:
    Ui::TutorialLanguagesWidget *ui;
    QString strLanguageSelection;
    QWidget* parent;

private Q_SLOTS:
    void buttonClicked(QString str);
};

#endif // TUTORIALLANGUAGESWIDGET_H

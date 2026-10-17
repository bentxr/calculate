#pragma once

#include <QDialog>

// The third-party notices and licences that travel with the program, one per row, the selected one's text beside
// (wide) or under (narrow) the list: the plotting library's (its NOTICE, then each component's licence) and the screen
// typeface's.
class LicencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit LicencesDialog(QWidget* parent);
};

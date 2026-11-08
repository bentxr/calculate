#pragma once

#include <QDialog>

// The third-party notices and licences that travel with the program, one per row, the selected one's text beside
// (wide) or under (narrow) the list: the screen typeface's, then Qt's (the LGPL-3.0 and the GPL-3.0 it adds its
// permissions to).
class LicencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit LicencesDialog(QWidget* parent);
};

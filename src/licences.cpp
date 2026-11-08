#include "licences.hpp"

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QListWidget>
#include <QPlainTextEdit>

LicencesDialog::LicencesDialog(QWidget* parent) : QDialog(parent) {
    setObjectName("licencesDialog");
    setWindowTitle(tr("Third-party licences"));
    setAttribute(Qt::WA_DeleteOnClose);
    const bool narrow = parent->width() < parent->height();
    auto* list = new QListWidget(this);
    list->setObjectName("licenceList");
    auto* text = new QPlainTextEdit(this);
    text->setObjectName("licenceText");
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    QStringList texts;
    QFile ofl(QStringLiteral(":/fonts/JetBrainsMono-OFL.txt"));
    if (ofl.open(QIODevice::ReadOnly)) {
        list->addItem(QStringLiteral("JetBrains Mono: OFL.txt"));
        texts << QString::fromUtf8(ofl.readAll());
    }
    // Qt, which the program links under the LGPL-3.0 (the GPL-3.0 with additional permissions)
    for (const char* name : {"LGPL-3.0-only.txt", "GPL-3.0-only.txt"}) {
        QFile file(QStringLiteral(":/licences/") + QLatin1String(name));
        if (!file.open(QIODevice::ReadOnly)) continue;
        list->addItem(QStringLiteral("Qt: ") + QLatin1String(name));
        texts << QString::fromUtf8(file.readAll());
    }
    connect(list, &QListWidget::currentRowChanged, text, [text, texts](int row) {
        text->setPlainText(row >= 0 && row < texts.size() ? texts[row] : QString());
    });
    auto* panes = new QBoxLayout(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    panes->addWidget(list, 1);
    panes->addWidget(text, narrow ? 2 : 3);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(panes, 1);
    layout->addWidget(buttons);
    list->setCurrentRow(0);
    // Over the window, inside it: the whole of it on a phone.
    const QRect area = parent->geometry();
    const QSize size = narrow ? area.size() : area.size().boundedTo(QSize(900, 640));
    setGeometry(QRect(area.center() - QPoint(size.width() / 2, size.height() / 2), size) & area);
}

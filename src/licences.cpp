#include "licences.hpp"

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QDirIterator>
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
    // NOTICE first (it names every component), then each licence it names, in path order
    const QString root = QStringLiteral(":/licences/");
    QStringList paths;
    for (QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories); it.hasNext();) paths << it.next().mid(root.size());
    paths.sort();
    paths.removeAll(QStringLiteral("NOTICE"));
    paths.prepend(QStringLiteral("NOTICE"));
    QStringList texts;
    for (const QString& path : paths) {
        QFile file(root + path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        list->addItem(path);
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

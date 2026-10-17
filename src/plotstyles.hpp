#pragma once

#include <QByteArray>
#include <QDialog>
#include <QImage>
#include <QMap>
#include <QStringList>

// The style chooser (D9.8): a tile for "default" and each of matplotlib's bundled styles, its thumbnail the figure
// drawn by the engine in that style (drawn one after another, so the dialog answers meanwhile). A click takes it.
class PlotStylesDialog : public QDialog {
    Q_OBJECT

public:
    PlotStylesDialog(const QByteArray& figure, QWidget* parent);

    QString chosen() const { return chosen_; }  // "default" or a style's name
    bool finished() const { return next_ >= names_.size(); }  // every thumbnail drawn
    QSize thumbnailSize() const { return QSize(160, 120); }
    QImage thumbnail(const QString& name) const { return thumbnails_.value(name); }

private:
    void drawNext();
    void retranslate();

    QByteArray figure_;
    QStringList names_;
    int next_ = 0;
    QMap<QString, QImage> thumbnails_;
    QString chosen_;
};

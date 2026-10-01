#include "typechooser.hpp"

#include "presenter.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QListView>
#include <QPainter>
#include <QStyledItemDelegate>

#include <cmath>

namespace {

constexpr int pad = 6;
constexpr int gap = 4;
constexpr int barHeight = 5;

QFont smallFont(const QFont& font) {
    QFont f = font;
    f.setPixelSize(qRound(QFontInfo(font).pixelSize() * 0.85));
    return f;
}

// One type per row: the name and its significant digits, a bar, and the C++ details in small print.
class TypeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        const QFontMetrics m(option.font);
        return QSize(m.horizontalAdvance('x') * 46, 2 * pad + m.height() + 2 * gap + barHeight + QFontMetrics(smallFont(option.font)).height());
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem o = option;
        initStyleOption(&o, index);
        o.text.clear();
        o.widget->style()->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);

        const bool selected = option.state & QStyle::State_Selected;
        const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        const QRect r = option.rect.adjusted(pad, pad, -pad, -pad);
        const QFontMetrics m(option.font);
        painter->save();
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(text);
        painter->drawText(r.left(), r.top() + m.ascent(), index.data(Qt::DisplayRole).toString());
        painter->setFont(option.font);
        const QString digits = index.data(TypeChooser::DigitsRole).toString();
        painter->drawText(r.right() - m.horizontalAdvance(digits), r.top() + m.ascent(), digits);

        const QRect track(r.left(), r.top() + m.height() + gap, r.width(), barHeight);
        painter->fillRect(track, option.palette.color(QPalette::Mid));
        QRect filled = track;
        filled.setWidth(qRound(track.width() * precision(index)));
        painter->fillRect(filled, option.palette.color(selected ? QPalette::HighlightedText : QPalette::Highlight));

        painter->setFont(smallFont(option.font));
        painter->drawText(r.left(), track.bottom() + gap + QFontMetrics(smallFont(option.font)).ascent(),
                          index.data(TypeChooser::DetailRole).toString());
        painter->restore();
    }

private:
    // The share of the bar: significant digits on a logarithmic scale against the most precise type;
    // Exact (no rounding at all) fills it.
    static qreal precision(const QModelIndex& index) {
        const int digits = index.data(TypeChooser::PrecisionRole).toInt();
        if (digits < 0) return 1;
        int most = 1;
        for (int row = 0; row < index.model()->rowCount(); ++row)
            most = qMax(most, index.model()->index(row, 0).data(TypeChooser::PrecisionRole).toInt());
        return std::log(1.0 + digits) / std::log(1.0 + most);
    }
};

}  // namespace

TypeChooser::TypeChooser(QWidget* parent) : QComboBox(parent) {
    auto* list = new QListView(this);
    list->setItemDelegate(new TypeDelegate(list));
    setView(list);
    for (const calculate_core::TypeInfo& t : calculate_core::numberTypes()) {
        addItem({});
        setItemData(count() - 1, t.type == calculate_core::NumberType::Exact ? -1 : t.decimalDigits, PrecisionRole);
    }
    setCurrentIndex(static_cast<int>(calculate_core::NumberType::Double));
    retranslate();
}

void TypeChooser::retranslate() {
    const std::vector<calculate_core::TypeInfo> types = calculate_core::numberTypes();
    for (int i = 0; i < count(); ++i) {
        const calculate_core::TypeInfo& t = types[static_cast<std::size_t>(i)];
        setItemText(i, view::shortTypeName(t));
        setItemData(i, view::typeLabel(t), Qt::ToolTipRole);
        setItemData(i, view::typeDigits(t), DigitsRole);
        setItemData(i, view::typeDetail(t), DetailRole);
    }
    // As wide as the widest row, so no detail is cut off.
    QFont bold = view()->font();
    bold.setBold(true);
    int widest = 0;
    for (int i = 0; i < count(); ++i) {
        widest = qMax(widest, QFontMetrics(smallFont(view()->font())).horizontalAdvance(itemData(i, DetailRole).toString()));
        widest = qMax(widest, QFontMetrics(bold).horizontalAdvance(itemText(i) + "    ")
                                  + QFontMetrics(view()->font()).horizontalAdvance(itemData(i, DigitsRole).toString()));
    }
    view()->setMinimumWidth(widest + 2 * pad + view()->style()->pixelMetric(QStyle::PM_ScrollBarExtent));
}

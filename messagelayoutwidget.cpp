#include "messagelayoutwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace {

const int HeaderHeight = 24;    // row with the bit numbers
const int ByteColumnWidth = 56; // column with the byte numbers
const int CellWidth = 48;
const int CellHeight = 28;

// Motorola signals run through the bits in "big endian" order: byte 0 bit 7
// first, then byte 0 bit 6 ... byte 1 bit 7 ... In that order a Motorola
// signal's bits are consecutive. This converts a bit number to its position in
// that order (and back: applying it twice gives the original number).
int toBigEndianOrder(int bit)
{
    return (bit / 8) * 8 + 7 - bit % 8;
}

// The startBit at which a signal must be placed so that the bit at position
// `index` in its signalBits() list lands on `bit`. -1 if that is impossible.
int startBitFor(int bit, int index, Signal::ByteOrder byteOrder)
{
    if (byteOrder == Signal::ByteOrder::Intel) {
        const int start = bit - index;
        return start >= 0 ? start : -1;
    }
    const int position = toBigEndianOrder(bit) - index;
    return position >= 0 ? toBigEndianOrder(position) : -1;
}

} // namespace

MessageLayoutWidget::MessageLayoutWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true); // to show the hand cursor over signals
}

void MessageLayoutWidget::setLayoutData(int lengthInBytes, const QVector<SignalMapping> &mappings,
                                        const QHash<int, Signal> &signalsById)
{
    m_lengthInBytes = lengthInBytes;
    m_mappings = mappings;
    m_signalsById = signalsById;
    m_dragIndex = -1;

    setMinimumSize(sizeHint());
    updateGeometry();
    update();
}

QSize MessageLayoutWidget::sizeHint() const
{
    return QSize(ByteColumnWidth + 8 * CellWidth + 1, HeaderHeight + m_lengthInBytes * CellHeight + 1);
}

QColor MessageLayoutWidget::colorFor(int mappingIndex)
{
    // Light colours that differ clearly from their neighbours; black text stays readable.
    return QColor::fromHsv((mappingIndex * 67 + 200) % 360, 80, 245);
}

int MessageLayoutWidget::bitAt(const QPoint &pos) const
{
    if (pos.x() < ByteColumnWidth || pos.x() >= ByteColumnWidth + 8 * CellWidth
        || pos.y() < HeaderHeight || pos.y() >= HeaderHeight + m_lengthInBytes * CellHeight)
        return -1;

    const int column = (pos.x() - ByteColumnWidth) / CellWidth;
    const int row = (pos.y() - HeaderHeight) / CellHeight;
    return row * 8 + (7 - column);
}

QRect MessageLayoutWidget::cellRect(int bit) const
{
    const int row = bit / 8;
    const int column = 7 - bit % 8;
    return QRect(ByteColumnWidth + column * CellWidth, HeaderHeight + row * CellHeight,
                 CellWidth, CellHeight);
}

QVector<int> MessageLayoutWidget::bitsOf(int mappingIndex, int startBit) const
{
    const Signal signal = m_signalsById.value(m_mappings.at(mappingIndex).signalId);
    return signalBits(startBit, signal.length, signal.byteOrder);
}

bool MessageLayoutWidget::fits(int mappingIndex, int startBit) const
{
    if (startBit < 0)
        return false;

    QVector<SignalMapping> moved = m_mappings;
    moved[mappingIndex].startBit = startBit;
    return checkSignalLayout(m_lengthInBytes, moved, m_signalsById).isEmpty();
}

void MessageLayoutWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    painter.setPen(palette().color(QPalette::Text));

    // Only the rows that need repainting: a UDP message can have thousands.
    const int firstRow = qMax(0, (event->rect().top() - HeaderHeight) / CellHeight);
    const int lastRow = qMin(m_lengthInBytes - 1, (event->rect().bottom() - HeaderHeight) / CellHeight);

    // Header and byte numbers.
    painter.drawText(QRect(0, 0, ByteColumnWidth, HeaderHeight), Qt::AlignCenter, "Byte");
    for (int column = 0; column < 8; ++column) {
        painter.drawText(QRect(ByteColumnWidth + column * CellWidth, 0, CellWidth, HeaderHeight),
                         Qt::AlignCenter, QString::number(7 - column));
    }
    for (int row = firstRow; row <= lastRow; ++row) {
        painter.drawText(QRect(0, HeaderHeight + row * CellHeight, ByteColumnWidth, CellHeight),
                         Qt::AlignCenter, QString::number(row));
    }

    // One signal: fill its cells and remember where to write its name (once per
    // byte it touches). Names are written last so the grid lines do not cross them.
    QVector<QPair<QRect, QString>> names;
    auto drawSignal = [&](int mappingIndex, int startBit, QColor color) {
        const QString name = m_signalsById.value(m_mappings.at(mappingIndex).signalId).name;
        QHash<int, QRect> rowRects; // row -> area of this signal in that row
        for (int bit : bitsOf(mappingIndex, startBit)) {
            if (bit < 0 || bit >= m_lengthInBytes * 8)
                continue; // does not fit (e.g. the signal was made longer later)
            const QRect cell = cellRect(bit);
            painter.fillRect(cell, color);
            rowRects[bit / 8] = rowRects.value(bit / 8, cell).united(cell);
        }
        for (const QRect &area : rowRects)
            names.append({area, name});
    };

    for (int i = 0; i < m_mappings.size(); ++i) {
        QColor color = colorFor(i);
        if (i == m_dragIndex)
            color.setAlpha(90); // the original place, faded while the signal is dragged
        drawSignal(i, m_mappings.at(i).startBit, color);
    }
    if (m_dragIndex >= 0 && m_dragStartBit >= 0)
        drawSignal(m_dragIndex, m_dragStartBit, colorFor(m_dragIndex).darker(115));

    // Grid lines on top.
    painter.setPen(palette().color(QPalette::Mid));
    for (int row = firstRow; row <= lastRow + 1 && row <= m_lengthInBytes; ++row) {
        const int y = HeaderHeight + row * CellHeight;
        painter.drawLine(ByteColumnWidth, y, ByteColumnWidth + 8 * CellWidth, y);
    }
    for (int column = 0; column <= 8; ++column) {
        const int x = ByteColumnWidth + column * CellWidth;
        painter.drawLine(x, HeaderHeight, x, HeaderHeight + m_lengthInBytes * CellHeight);
    }

    painter.setPen(Qt::black);
    for (const auto &name : names) {
        const QRect &area = name.first;
        painter.drawText(area.adjusted(3, 0, -3, 0), Qt::AlignCenter,
                         painter.fontMetrics().elidedText(name.second, Qt::ElideRight, area.width() - 6));
    }
}

void MessageLayoutWidget::mousePressEvent(QMouseEvent *event)
{
    const int bit = bitAt(event->pos());
    if (event->button() != Qt::LeftButton || bit < 0)
        return;

    // Find the signal under the mouse and which of its bits was grabbed.
    for (int i = 0; i < m_mappings.size(); ++i) {
        const int index = bitsOf(i, m_mappings.at(i).startBit).indexOf(bit);
        if (index >= 0) {
            m_dragIndex = i;
            m_dragGrabbedBit = index;
            m_dragStartBit = m_mappings.at(i).startBit;
            setCursor(Qt::ClosedHandCursor);
            update();
            return;
        }
    }
}

void MessageLayoutWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int bit = bitAt(event->pos());

    if (m_dragIndex < 0) {
        // Not dragging: show an open hand over signals.
        bool overSignal = false;
        for (int i = 0; i < m_mappings.size() && bit >= 0 && !overSignal; ++i)
            overSignal = bitsOf(i, m_mappings.at(i).startBit).contains(bit);
        setCursor(overSignal ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }

    const Signal::ByteOrder byteOrder = m_signalsById.value(m_mappings.at(m_dragIndex).signalId).byteOrder;
    const int startBit = bit < 0 ? -1 : startBitFor(bit, m_dragGrabbedBit, byteOrder);
    m_dragStartBit = fits(m_dragIndex, startBit) ? startBit : -1;
    setCursor(m_dragStartBit >= 0 ? Qt::ClosedHandCursor : Qt::ForbiddenCursor);
    update();
}

void MessageLayoutWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_dragIndex < 0)
        return;

    const int index = m_dragIndex;
    const int startBit = m_dragStartBit;
    m_dragIndex = -1;
    setCursor(Qt::OpenHandCursor);
    update();

    // Dropped where it does not fit: it simply stays where it was.
    if (startBit >= 0 && startBit != m_mappings.at(index).startBit)
        emit signalMoved(index, startBit);
}

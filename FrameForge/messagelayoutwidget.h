#ifndef MESSAGELAYOUTWIDGET_H
#define MESSAGELAYOUTWIDGET_H

#include "ffcore/message.h"

#include <QHash>
#include <QVector>
#include <QWidget>

using namespace ff;

// Draws a message payload as a grid (one row per byte, bit 7 on the left,
// bit 0 on the right) with every placed signal as a coloured block.
// A signal can be dragged with the mouse to a new position; when dropped
// somewhere it fits without overlapping another signal, signalMoved() is emitted.
//
// The widget only shows what it is given with setLayoutData(); the owner
// (MessageDialog) applies the move and calls setLayoutData() again.
// With setReadOnly(true) it only shows the layout (used in the message view).
class MessageLayoutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MessageLayoutWidget(QWidget *parent = nullptr);

    void setLayoutData(int lengthInBytes, const QVector<SignalMapping> &mappings,
                       const QHash<int, Signal> &signalsById);

    void setReadOnly(bool readOnly);

    QSize sizeHint() const override;

    // Colour used for the mapping at this position; also used by the dialog's signal list.
    static QColor colorFor(int mappingIndex);

signals:
    void signalMoved(int mappingIndex, int newStartBit);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int bitAt(const QPoint &pos) const; // -1 outside the grid
    QRect cellRect(int bit) const;
    QVector<int> bitsOf(int mappingIndex, int startBit) const;
    bool fits(int mappingIndex, int startBit) const;

    bool m_readOnly = false;
    int m_lengthInBytes = 0;
    QVector<SignalMapping> m_mappings;
    QHash<int, Signal> m_signalsById;

    // While dragging: which signal, which of its bits was grabbed, where it would land.
    int m_dragIndex = -1;
    int m_dragGrabbedBit = 0; // position in signalBits() order
    int m_dragStartBit = -1;  // -1: no valid position under the mouse
};

#endif // MESSAGELAYOUTWIDGET_H

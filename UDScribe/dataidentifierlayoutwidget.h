#ifndef DATAIDENTIFIERLAYOUTWIDGET_H
#define DATAIDENTIFIERLAYOUTWIDGET_H

#include "usccore/dataidentifier.h"

#include <QHash>
#include <QVector>
#include <QWidget>

using namespace usc;

// Draws a DID's data record as a grid (one row per byte, bit 7 on the left,
// bit 0 on the right) with every placed data type as a coloured block.
// A data type can be dragged with the mouse to a new position; when dropped
// somewhere it fits without overlapping another data type, dataTypeMoved() is emitted.
//
// The widget only shows what it is given with setLayoutData(); the owner
// (DataIdentifierDialog) applies the move and calls setLayoutData() again.
// With setReadOnly(true) it only shows the layout (used on the DID's page).
class DataIdentifierLayoutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataIdentifierLayoutWidget(QWidget *parent = nullptr);

    void setLayoutData(int lengthInBytes, const QVector<DataTypeMapping> &mappings,
                       const QHash<int, DataType> &dataTypesById);

    void setReadOnly(bool readOnly);

    QSize sizeHint() const override;

    // Colour used for the mapping at this position; also used by the dialog's data type list.
    static QColor colorFor(int mappingIndex);

signals:
    void dataTypeMoved(int mappingIndex, int newStartBit);

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
    QVector<DataTypeMapping> m_mappings;
    QHash<int, DataType> m_dataTypesById;

    // While dragging: which data type, which of its bits was grabbed, where it would land.
    int m_dragIndex = -1;
    int m_dragGrabbedBit = 0; // position in dataTypeBits() order
    int m_dragStartBit = -1;  // -1: no valid position under the mouse
};

#endif // DATAIDENTIFIERLAYOUTWIDGET_H

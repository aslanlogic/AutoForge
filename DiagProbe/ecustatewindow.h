#ifndef ECUSTATEWINDOW_H
#define ECUSTATEWINDOW_H

#include <QWidget>

class QLabel;
class Tester;

// What the tester knows about the ECU right now: its session, the unlocked
// security levels, how long until S3 sends it back to the default session, and
// the last response.
class EcuStateWindow : public QWidget
{
    Q_OBJECT

public:
    explicit EcuStateWindow(Tester *tester, QWidget *parent = nullptr);

private:
    void refresh();

    Tester *m_tester;
    QLabel *m_measurement;
    QLabel *m_session;
    QLabel *m_security;
    QLabel *m_s3;
    QLabel *m_lastResponse;
};

#endif // ECUSTATEWINDOW_H

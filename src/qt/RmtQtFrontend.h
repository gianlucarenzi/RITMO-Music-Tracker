// RmtQtFrontend.h - Qt frontend of RITMO
//
// The tracker GUI is CRmtView, CMainFrame and CRmtDoc, compiled against the
// CompatTypes.h layer: RmtQtBridge (RmtQtFrontend.cpp) implements IRmtHost for
// it and owns those objects, RmtViewWidget shows the view and passes it
// keyboard, mouse and paint events, RmtMainWindow is the frame.
// Only Qt types here: CompatTypes.h stays out of the moc'ed header.

#pragma once

#include <QMainWindow>
#include <QWidget>

#include <memory>

class RmtQtBridge;

class RmtViewWidget : public QWidget {
    Q_OBJECT
public:
    RmtViewWidget(RmtQtBridge* bridge, QWidget* parent);

protected:
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    bool focusNextPrevChild(bool) override { return false; } // Tab belongs to the tracker

private:
    RmtQtBridge* m_bridge;
};

class RmtMainWindow : public QMainWindow {
    Q_OBJECT
public:
    RmtMainWindow();
    ~RmtMainWindow() override;

    // CView::OnInitialUpdate() and the file given on the command line
    void Start(const QString& songFile);

    // The interface size of this session only (--scale): the configuration keeps the one of the options
    void OverrideScaling(int percent);

    // The toolbars are shown and hidden by the View menu (g_view), not by
    // the right-click menu of QMainWindow
    QMenu* createPopupMenu() override { return nullptr; }

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    std::unique_ptr<RmtQtBridge> m_bridge;
};

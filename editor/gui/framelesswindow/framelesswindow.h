/*
###############################################################################
#                                                                             #
# The MIT License                                                             #
#                                                                             #
# Copyright (C) 2017 by Juergen Skrotzky (JorgenVikingGod@gmail.com)          #
#               >> https://github.com/Jorgen-VikingGod                        #
#                                                                             #
# Sources: https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle  #
#                                                                             #
###############################################################################
*/

#ifndef FRAMELESSWINDOW_H
#define FRAMELESSWINDOW_H

#include <QWidget>
#include <QMainWindow>

namespace Ui {
class FramelessWindow;
}

class FramelessWindow : public QWidget {
  Q_OBJECT

 public:
  explicit FramelessWindow(QWidget *parent = Q_NULLPTR);
  virtual ~FramelessWindow();
  void setContent(QMainWindow *w);
  void enableShadow(bool on_off);
  void init();

 private:
  bool leftBorderHit(const QPoint &pos);
  bool rightBorderHit(const QPoint &pos);
  bool topBorderHit(const QPoint &pos);
  bool bottomBorderHit(const QPoint &pos);
  void styleWindow(bool bActive, bool bNoState);

 public slots:
  void setWindowTitle(const QString &text);
  void setWindowIcon(const QIcon &ico);

 private slots:
  void on_applicationStateChanged(Qt::ApplicationState state);
  void on_minimizeButton_clicked();
  void on_restoreButton_clicked();
  void on_maximizeButton_clicked();
  void on_closeButton_clicked();
  void on_windowTitlebar_doubleClicked();

 protected:
  void closeEvent(QCloseEvent* event) override;
  void changeEvent(QEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;
private:
  void checkBorderDragging(QMouseEvent *event);

 private:
  Ui::FramelessWindow *ui;
  QMainWindow* mContentWindow = nullptr;
  QRect m_StartGeometry;
  const quint8 CONST_DRAG_BORDER_SIZE = 15;
  bool m_bMousePressed = false;
  bool m_bDragTop = false;
  bool m_bDragLeft = false;
  bool m_bDragRight = false;
  bool m_bDragBottom = false;
  bool m_bEnableShadowEffect = false;
};

#endif  // FRAMELESSWINDOW_H

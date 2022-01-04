#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>

namespace Ui {
class MainWindow;
}

class ImageCamera;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected slots:
    void showImg(QPixmap &img);

private:
    Ui::MainWindow *ui;
    ImageCamera* c;
};

#endif // MAINWINDOW_H

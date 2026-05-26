#ifndef VOLUMEPOPUP_H
#define VOLUMEPOPUP_H
#include <QSlider>
#include <QLabel>
#include <QPropertyAnimation>
#include <QObject>
#include <QLayout>
#include <QPainter>

//音量条
class VolumePopup: public QWidget
{
    Q_OBJECT
public:
    explicit VolumePopup(QWidget *parent = nullptr);

signals:
    void volumeChanged(int value);
protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QSlider *slider;
    QLabel *label;
    QPropertyAnimation *fadeAnim; // 动画
    void initStyle();
};
#endif // VOLUMEPOPUP_H

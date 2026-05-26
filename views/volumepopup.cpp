#include "volumepopup.h"

VolumePopup::VolumePopup(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);  // 弹出类型，无边框
    setAttribute(Qt::WA_TranslucentBackground);          // 背景透明，使用paintEvent绘制白底圆角
    setFixedSize(40, 160);                               // 整个popup窗口大小

    // 滑条与标签
    slider = new QSlider(Qt::Vertical, this);
    slider->setRange(0, 100);
    slider->setValue(9);    // 音量条音量显示数值，实际修改去model层。
    //slider->setInvertedAppearance(false); // ✅ 最下面是0，最上面是100
    slider->setFixedWidth(20);           // 整个滑条的宽度
    slider->setFixedHeight(120);         // 滑条的高度（你可以修改这个数值改变滑条高度）

    label = new QLabel("9%", this);
    label->setAlignment(Qt::AlignCenter);
    //滑块下的文字样式。
    //label->setStyleSheet("color: #00CC65; font-weight: bold; font-size: 14px;");

    // 布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(slider, 1, Qt::AlignCenter);
    layout->addWidget(label);

    // 滑条值变化时更新标签和发射信号
    connect(slider, &QSlider::valueChanged, this, [=](int value){
        label->setText(QString::number(value) + "%");
        emit volumeChanged(value);
    });

    // 滑条样式
    initStyle();

    // popup淡入动画
    fadeAnim = new QPropertyAnimation(this, "windowOpacity", this);
    fadeAnim->setDuration(150);

    // 背景圆角设置（整个窗口）
    setStyleSheet(R"(
        background-color:#FFFFFF;  /* 白色背景 */
        border-radius:8px;        /* 窗口圆角 */
    )");
}

void VolumePopup::initStyle()
{
    slider->setFixedWidth(20);   // 滑条整体宽度
    slider->setFixedHeight(120); // 滑条高度
    slider->setInvertedAppearance(false);   // 保持滑块位置从0到100从下到上
    slider->setInvertedControls(false);     // 鼠标操作正常

//     slider->setStyleSheet(R"(    QSlider {
//         background: transparent;  /* 背景透明，用paintEvent画 */
//     }
//     QSlider::groove:vertical {
//         background: #FFFFFF;       /* 滑槽白色 */
//         border: 1px solid #E0E0E0;
//         border-radius: 6px;
//         width: 6px;                /* 滑槽宽度 */
//         margin: 4px 0;  /* ✅ 顶部和底部各留 8px 空间给滑块 */
//     }
//     QSlider::handle:vertical {
//         background: #00CC65;       /* 滑块颜色 */
//         border: none;
//         height: 16px;              /* 滑块高度，可改 */
//         width: 16px;               /* 滑块宽度，可改 */
//         margin: -5px -5px;         /* 滑块超出滑槽的偏移，让它居中 */
//         border-radius: 8px;        /* 滑块圆角 */
//     }
// QSlider::sub-page:vertical {
//         /* 当 0 在底部时，sub-page 是底部到滑块的区域 */
//         /* 保持滑槽的背景色 (例如白色或透明) */
//         background: transparent;
//         border-radius: 6px;
//     }

//     QSlider::add-page:vertical {
//         /* add-page 是滑块到最大值 (顶部) 的区域 */
//         /* 设置为已填充颜色 */
//         background: #00CC65;
//         border-radius: 6px;
//     })");

}


void VolumePopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 画白色圆角矩形背景
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 4, 4); // 10像素圆角，可修改
}

void VolumePopup::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    fadeAnim->stop();
    fadeAnim->setStartValue(0.0);
    fadeAnim->setEndValue(1.0);
    fadeAnim->start();
}

void VolumePopup::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    setWindowOpacity(1.0);
}

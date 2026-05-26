/***************************************************
 *  @file      main.cpp
 *  @brief     XXXX Function
 *  // 本项目使用了 kiss_fft 开源库进行快速离散傅里叶变换（FFT）计算
 *  // kiss_fft 开源库的原始作者：Mark Borgerding
 *  // 开源协议：BSD 3-Clause License
 *  // 原始仓库：https://github.com/mborgerding/kissfft
 *  // 当前使用版本：kissfft-131.2.0
 *  @author    un
 *  @date      2026/05/12
 *  @history
 ****************************************************/
#include "views/mainwindow.h"

#include "dao/dbdao.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  if (!DbDao::instance()->initDb())
  {
    qWarning() << "[DB Warning] 默认SQLite数据库初始化失败，将继续启动界面，但持久化能力不可用";
  }

  MainWindow w;
  w.show();
  return a.exec();
}

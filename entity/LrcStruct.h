#ifndef LRCSTRUCT_H
#define LRCSTRUCT_H

#include <QString>
#include <QList>

// 歌词行结构体
struct LyricLine {
    qint64 timestamp; // 毫秒
    QString text;
};

// 歌词元信息结构体
struct LrcMetaData {
    QString title;  // [ti:标题]
    QString artist; // [ar:歌手]
    QString album;  // [al:专辑]
};

#endif // LRCSTRUCT_H

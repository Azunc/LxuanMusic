/***************************************************
 *  @file      lrcparser.h
 *  @brief     lrc歌词解析工具：把lrc文本解析成「时间戳+歌词内容」的结构化列表
 *
 *  @author    un
 *  @date      2026/04/13
 *  @history
 ****************************************************/
#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QObject>
#include "./entity/LrcStruct.h"

class LrcParser : public QObject
{
    Q_OBJECT
public:
    explicit LrcParser(QObject *parent = nullptr);
    // 异步加载并解析歌词文件
    void loadAsync(const QString &filePath);

signals:
    // 解析完成信号，返回行列表和元信息
    void parsingFinished(const QList<LyricLine> &lyrics, const LrcMetaData &metaData);

private:
    // 实际的解析逻辑
    QPair<QList<LyricLine>, LrcMetaData> parseContent(const QString &content);
};

#endif // LRCPARSER_H

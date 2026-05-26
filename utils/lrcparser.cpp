#include "LrcParser.h"
#include <QFile>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>

LrcParser::LrcParser(QObject *parent) : QObject(parent) {}

void LrcParser::loadAsync(const QString &filePath)
{
    // 使用 QtConcurrent 在子线程读取文件并解析，防止卡顿UI
    QFuture<QPair<QList<LyricLine>, LrcMetaData>> future = QtConcurrent::run([filePath]() {
        QPair<QList<LyricLine>, LrcMetaData> result;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return result;
        }
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QRegularExpression timeRegex("\\[(\\d{2}):(\\d{2})\\.(\\d{2,3})\\](.*)");
        QRegularExpression metaRegex("\\[(\\w+):(.*)\\]");
        const QStringList lines = content.split('\n');

        for (const QString &line : lines) {
            QRegularExpressionMatch metaMatch = metaRegex.match(line);
            if (metaMatch.hasMatch() && !line.contains(timeRegex)) {
                QString tag = metaMatch.captured(1);
                QString val = metaMatch.captured(2).trimmed();
                if (tag == "ti") result.second.title = val;
                else if (tag == "ar") result.second.artist = val;
                else if (tag == "al") result.second.album = val;
                continue;
            }

            QRegularExpressionMatch timeMatch = timeRegex.match(line);
            if (timeMatch.hasMatch()) {
                int min = timeMatch.captured(1).toInt();
                int sec = timeMatch.captured(2).toInt();
                int ms = timeMatch.captured(3).toInt();
                if (timeMatch.captured(3).length() == 2) ms *= 10;

                qint64 timestamp = min * 60000 + sec * 1000 + ms;
                QString text = timeMatch.captured(4).trimmed();
                if (!text.isEmpty()) {
                    result.first.append({timestamp, text});
                }
            }
        }
        std::sort(result.first.begin(), result.first.end(), [](const LyricLine &a, const LyricLine &b){
            return a.timestamp < b.timestamp;
        });
        return result;
    });

    // 监听子线程完成，发送信号
    auto *watcher = new QFutureWatcher<QPair<QList<LyricLine>, LrcMetaData>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher](){
        auto result = watcher->result();
        emit parsingFinished(result.first, result.second);
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

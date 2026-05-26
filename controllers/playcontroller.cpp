#include "playcontroller.h"

PlayController::PlayController(QObject *parent)
    : QObject(parent), m_audioEngine(AudioEngine::instance())
{
  connect(m_audioEngine, &AudioEngine::sigProgressChanged, this, &PlayController::sigProgressChanged);
  connect(m_audioEngine, &AudioEngine::sigSourceChanged, this, &PlayController::sigSourceChanged);
  connect(m_audioEngine, &AudioEngine::sigPlayError, this, &PlayController::sigPlayError);
  connect(m_audioEngine, &AudioEngine::sigMetaDataChanged, this, &PlayController::sigMetaDataChanged);
  connect(m_audioEngine, &AudioEngine::sigPlayPause, this, &PlayController::sigPlayPause);
}

void PlayController::setPlayQueue(const QList<QUrl> &queue, int startIndex, bool autoPlay)
{
  m_audioEngine->setPlayQueue(queue, startIndex, autoPlay);
}

void PlayController::playAt(int index)
{
  const QList<QUrl> queue = m_audioEngine->playQueue();
  if (index < 0 || index >= queue.size())
  {
    return;
  }
  m_audioEngine->play(queue.at(index));
}

void PlayController::play(const QUrl &mediaUrl)
{
  m_audioEngine->play(mediaUrl);
}

void PlayController::playPause()
{
  m_audioEngine->playPause();
}

void PlayController::next()
{
  m_audioEngine->next();
}

void PlayController::previous()
{
  m_audioEngine->previous();
}

void PlayController::setPosition(qint64 position)
{
  m_audioEngine->setPosition(position);
}

void PlayController::setVolume(int volume)
{
  m_audioEngine->setVolume(volume);
}

QList<QUrl> PlayController::playQueue() const
{
  return m_audioEngine->playQueue();
}

int PlayController::currentIndex() const
{
  return m_audioEngine->currentIndex();
}

qint64 PlayController::currentPosition() const
{
  return m_audioEngine->currentPosition();
}

qint64 PlayController::currentDuration() const
{
  return m_audioEngine->currentDuration();
}

QUrl PlayController::currentSource() const
{
  return m_audioEngine->currentSource();
}

QMediaPlayer::PlaybackState PlayController::playbackState() const
{
  return m_audioEngine->isPlaying();
}

void PlayController::setLoopMode(LoopMode mode)
{
  m_audioEngine->setLoopMode(mode);
}

LoopMode PlayController::loopMode() const
{
  return m_audioEngine->loopMode();
}

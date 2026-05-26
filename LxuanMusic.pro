QT       += core gui widgets multimedia multimediawidgets sql network concurrent
# Qt模块说明：
# core/gui/widgets 基础UI必需
# multimedia/multimediawidgets 音频播放、频谱绘制必需
# sql 数据库存储必需
# network 在线搜索/歌词功能预留（当前版本未使用）
# concurrent 多线程处理预留（当前版本未使用）
TARGET = MusicPlayer
TEMPLATE = app
# 配置C++标准，适配Qt6
CONFIG += c++17 warn_on
CONFIG += debug_and_release
win32:LIBS += -lpsapi

# ============================================================================
# TagLib 依赖配置（请根据本机实际安装路径修改）
# ============================================================================
# 示例路径（Windows）：
# INCLUDEPATH += F:/github/taglib-2.2.1/include
# LIBS += -LF:/github/taglib-2.2.1/lib -ltag
# TagLib DLL 路径（用于打包复制）
# TAGLIB_DLL = F:/github/taglib-2.2.1/build/bin/libtag.dll
# ============================================================================
INCLUDEPATH += F:/github/taglib-2.2.1/include
LIBS += -LF:/github/taglib-2.2.1/lib -ltag
TAGLIB_DLL = F:/github/taglib-2.2.1/build/bin/libtag.dll

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    entity/playlist.cpp \
    entity/song.cpp \
    main.cpp \
    dao/cachedao.cpp \
    dao/configdao.cpp \
    dao/dbdao.cpp \
    models/audioengine.cpp \
    models/librarymodel.cpp \
    models/lyricmodel.cpp \
    models/playlistmodel.cpp \
    models/visualizermodel.cpp\
    controllers/librarycontroller.cpp \
    controllers/lyriccontroller.cpp \
    controllers/playcontroller.cpp \
    controllers/settingcontroller.cpp \
    utils/filescanner.cpp \
    utils/hotkeymanager.cpp \
    utils/kiss_fft.c \
    utils/lrcparser.cpp \
    utils/metadataextractor.cpp \
    utils/thememanager.cpp \
    views/collapsiblegroup.cpp \
    views/mainwindow.cpp \
    views/lyricwidget.cpp \
    views/playlistitem.cpp \
    views/playlistsongitem.cpp \
    views/playlistdetailwidget.cpp \
    views/playlistpopup.cpp \
    views/sidebarwidget.cpp \
    views/volumepopup.cpp \
    views/localmusicwidget.cpp \
    views/addtoplaylistpopup.cpp

# --------------------------- 头文件配置 ---------------------------
HEADERS += \
    dao/cachedao.h \
    dao/configdao.h \
    dao/dbdao.h \
    entity/LrcStruct.h \
    entity/playlist.h \
    entity/song.h \
    models/audioengine.h \
    models/librarymodel.h \
    models/lyricmodel.h \
    models/playlistmodel.h \
    models/visualizermodel.h \
    controllers/librarycontroller.h \
    controllers/lyriccontroller.h \
    controllers/playcontroller.h \
    controllers/settingcontroller.h \
    utils/_kiss_fft_guts.h \
    utils/filescanner.h \
    utils/hotkeymanager.h \
    utils/kiss_fft.h \
    utils/kiss_fft_log.h \
    utils/lrcparser.h \
    utils/metadataextractor.h \
    utils/thememanager.h \
    views/collapsiblegroup.h \
    views/mainwindow.h \
    views/lyricwidget.h \
    views/playlistitem.h \
    views/playlistsongitem.h \
    views/playlistdetailwidget.h \
    views/playlistpopup.h \
    views/sidebarwidget.h \
    views/volumepopup.h \
    views/localmusicwidget.h \
    views/addtoplaylistpopup.h

FORMS += \
    views/mainwindow.ui \
    views/lyricwidget.ui

# --------------------------- 资源文件配置 ---------------------------
# 你的图标、QSS主题都放在这个资源文件里
RESOURCES += \
 QtMusic.qrc
RESOURCE_BASE = $$PWD/resource # 指定资源根目录为resource，自动去掉路径里的resource前缀
# Default rules for deployment.


qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
 tools/ffmpeg.exe

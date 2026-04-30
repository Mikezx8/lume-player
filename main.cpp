#include <QApplication>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QMediaContent>
#include <QFileDialog>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QGraphicsOpacityEffect>
#include <QShowEvent>
#include <QFrame>
#include <QPropertyAnimation>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFont>
#include <QBrush>
#include <QProcess>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QMap>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QQueue>
#include <QSet>
#include <QCollator>
#include <QCursor>
#include <QCommandLineParser>
#include <QShortcut>
#include <QDBusConnection>
#include <QDBusMessage>

struct Theme {
    QColor accent  { "#ff6b6b" };
    QColor accent2 { "#ff6b6b" };
};

static QString ft(qint64 ms) {
    if (ms < 0) ms = 0;
    const qint64 s = ms / 1000;
    return QString("%1:%2")
        .arg(s / 60, 2, 10, QChar('0'))
        .arg(s % 60, 2, 10, QChar('0'));
}

static Theme loadTheme() {
    Theme t;
    QFile f(QDir::homePath() + "/.lumeconf/theme.json");
    if (!f.open(QIODevice::ReadOnly))
        return t;

    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    for (const auto &k : {"accent", "accent-purple", "accent-color", "accentColor"}) {
        if (o.contains(k)) {
            t.accent = t.accent2 = QColor(o[k].toString());
            break;
        }
    }
    if (o.contains("accent2"))
        t.accent2 = QColor(o["accent2"].toString());

    return t;
}


static QString expandTemplate(const QString &templ,
                              const QString &currentPath,
                              const QString &currentDir,
                              const QString &currentName,
                              const QString &stateText,
                              qint64 positionMs) {
    QString out = templ;
    out.replace("%f", currentPath);
    out.replace("%d", currentDir);
    out.replace("%n", currentName);
    out.replace("%s", stateText);
    out.replace("%p", QString::number(positionMs));
    return out;
}

static QPushButton *mkBtn(const QString &text, bool accent, QWidget *parent, const Theme &theme) {
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(34);
    b->setFocusPolicy(Qt::NoFocus);

    if (accent) {
        const QString ac  = theme.accent.name();
        const QString ac2 = theme.accent2.name();
        const QString ach = theme.accent.lighter(115).name();
        b->setStyleSheet(QString(
            "QPushButton{color:white;border:none;border-radius:17px;"
            "padding:4px 22px;font-size:15px;font-weight:bold;"
            "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2);}"
            "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %3,stop:1 %1);}"
            "QPushButton:pressed{background:%2;}"
        ).arg(ac, ac2, ach));
    } else {
        b->setStyleSheet(
            "QPushButton{color:rgba(255,255,255,210);background:rgba(255,255,255,8);"
            "border:1px solid rgba(255,255,255,16);border-radius:10px;"
            "padding:4px 14px;font-size:13px;min-width:0;}"
            "QPushButton:hover{background:rgba(255,255,255,18);"
            "border:1px solid rgba(255,255,255,30);color:white;}"
            "QPushButton:pressed{background:rgba(255,255,255,28);}"
        );
    }
    return b;
}

static QLabel *mkTimeLbl(const QString &s, QWidget *parent) {
    auto *l = new QLabel(s, parent);
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet(
        "color:rgba(255,255,255,165);font-size:11px;"
        "font-family:'Courier New',monospace;min-width:38px;"
        "background:transparent;"
    );
    return l;
}

class SeekSlider : public QSlider {
public:
    explicit SeekSlider(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent) {
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override {
        if (ev->button() == Qt::LeftButton) {
            if (orientation() == Qt::Horizontal) {
                int value = QStyle::sliderValueFromPosition(
                    minimum(), maximum(), ev->pos().x(), width());
                setValue(value);
            } else {
                int value = QStyle::sliderValueFromPosition(
                    minimum(), maximum(), ev->pos().y(), height(), true);
                setValue(value);
            }
            ev->accept();
        }
        QSlider::mousePressEvent(ev);
    }
};

class ThumbnailCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailCache(QObject *parent = nullptr) : QObject(parent) {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
        QDir().mkpath(cacheDir);
    }

    bool hasCached(const QString &filePath) const {
        QString cachePath = getCachePath(filePath);
        return QFile::exists(cachePath);
    }

    QPixmap getCached(const QString &filePath) const {
        QString cachePath = getCachePath(filePath);
        QPixmap pixmap;
        pixmap.load(cachePath);
        return pixmap;
    }

    void cacheThumbnail(const QString &filePath, const QPixmap &thumbnail) {
        QString cachePath = getCachePath(filePath);
        thumbnail.save(cachePath, "PNG");
    }

private:
    QString cacheDir;

    QString getCachePath(const QString &filePath) const {
        QFileInfo fi(filePath);
        QString hash = QString::number(qHash(fi.absoluteFilePath()), 16);
        return cacheDir + "/" + hash + ".png";
    }
};

class ThumbnailExtractor : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailExtractor(ThumbnailCache *cache, QObject *parent = nullptr) 
        : QObject(parent), thumbnailCache(cache) {
        ffmpegPath = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpegPath.isEmpty()) {
            ffmpegPath = QStandardPaths::findExecutable("ffmpeg", 
                QStringList() << "/usr/local/bin" << "/opt/homebrew/bin" << "/usr/bin");
        }
        maxConcurrent = 2;
    }

    void extractAsync(const QString &filePath, const QSize &size) {
        if (processingSet.contains(filePath))
            return;
        
        if (thumbnailCache->hasCached(filePath)) {
            QPixmap cached = thumbnailCache->getCached(filePath);
            memoryCache[filePath] = cached;
            emit thumbnailReady(filePath, cached);
            return;
        }
        
        if (memoryCache.contains(filePath)) {
            emit thumbnailReady(filePath, memoryCache[filePath]);
            return;
        }
            
        if (ffmpegPath.isEmpty()) {
            emit thumbnailReady(filePath, QPixmap());
            return;
        }

        queue.enqueue({filePath, size});
        processingSet.insert(filePath);
        processQueue();
    }

    bool hasCached(const QString &filePath) const {
        return memoryCache.contains(filePath) || thumbnailCache->hasCached(filePath);
    }

    QPixmap getCached(const QString &filePath) const {
        if (memoryCache.contains(filePath))
            return memoryCache[filePath];
        if (thumbnailCache->hasCached(filePath))
            return thumbnailCache->getCached(filePath);
        return QPixmap();
    }

signals:
    void thumbnailReady(const QString &filePath, const QPixmap &thumbnail);

private:
    struct QueueItem {
        QString filePath;
        QSize size;
    };

    QString ffmpegPath;
    QQueue<QueueItem> queue;
    QSet<QString> processingSet;
    QMap<QString, QPixmap> memoryCache;
    ThumbnailCache *thumbnailCache;
    int maxConcurrent;
    int activeJobs = 0;

    void processQueue() {
        while (!queue.isEmpty() && activeJobs < maxConcurrent) {
            QueueItem item = queue.dequeue();
            activeJobs++;
            startExtraction(item.filePath, item.size);
        }
    }

    void startExtraction(const QString &filePath, const QSize &size) {
        QTemporaryFile *tempFile = new QTemporaryFile(QDir::tempPath() + "/lume_thumb_XXXXXX.png", this);
        if (!tempFile->open()) {
            delete tempFile;
            onJobFinished(filePath, QPixmap());
            return;
        }
        
        QString tempPath = tempFile->fileName();
        tempFile->close();
        delete tempFile;

        QProcess *process = new QProcess(this);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process, filePath, tempPath, size](int exitCode, QProcess::ExitStatus) {
            process->deleteLater();
            
            QPixmap result;
            if (exitCode == 0 && QFile::exists(tempPath)) {
                QImage img(tempPath);
                QFile::remove(tempPath);
                
                if (!img.isNull()) {
                    QImage scaled = img.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    result = QPixmap::fromImage(scaled);
                    memoryCache[filePath] = result;
                    thumbnailCache->cacheThumbnail(filePath, result);
                }
            } else {
                QFile::remove(tempPath);
            }
            
            onJobFinished(filePath, result);
        });

        QStringList args;
        args << "-y" << "-ss" << "00:00:02" << "-i" << filePath 
             << "-vframes" << "1" << "-q:v" << "2"
             << "-s" << QString("%1x%2").arg(size.width() * 2).arg(size.height() * 2)
             << tempPath;

        process->start(ffmpegPath, args);
    }

    void onJobFinished(const QString &filePath, const QPixmap &result) {
        processingSet.remove(filePath);
        activeJobs--;
        emit thumbnailReady(filePath, result);
        processQueue();
    }
};

class OverlayPanel : public QWidget {
public:
    explicit OverlayPanel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMouseTracking(true);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);

        QPainterPath path;
        path.addRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), 24, 24);

        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, QColor(22, 22, 30, 242));
        g.setColorAt(1, QColor(10, 10, 18, 248));

        p.fillPath(path, g);
        p.setPen(QPen(QColor(255, 255, 255, 24), 1.0));
        p.drawPath(path);
    }
};

class EndScreen : public QWidget {
    Q_OBJECT
public:
    explicit EndScreen(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMouseTracking(true);
        
        auto *layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignCenter);
        layout->setSpacing(20);
        
        titleLabel = new QLabel("Finished", this);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("color:white;font-size:24px;font-weight:bold;background:transparent;");
        
        auto *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(15);
        btnLayout->setAlignment(Qt::AlignCenter);
        
        prevBtn = new QPushButton("⏮ Previous", this);
        replayBtn = new QPushButton("🔄 Replay", this);
        nextBtn = new QPushButton("Next ⏭", this);
        
        QString btnStyle = 
            "QPushButton{color:white;background:rgba(255,255,255,15);"
            "border:1px solid rgba(255,255,255,25);border-radius:12px;"
            "padding:10px 20px;font-size:14px;font-weight:bold;}"
            "QPushButton:hover{background:rgba(255,255,255,25);border-color:rgba(255,255,255,40);}";
        
        prevBtn->setStyleSheet(btnStyle);
        replayBtn->setStyleSheet(btnStyle);
        nextBtn->setStyleSheet(btnStyle);
        
        prevBtn->setCursor(Qt::PointingHandCursor);
        replayBtn->setCursor(Qt::PointingHandCursor);
        nextBtn->setCursor(Qt::PointingHandCursor);
        
        prevBtn->setFocusPolicy(Qt::NoFocus);
        replayBtn->setFocusPolicy(Qt::NoFocus);
        nextBtn->setFocusPolicy(Qt::NoFocus);
        
        btnLayout->addWidget(prevBtn);
        btnLayout->addWidget(replayBtn);
        btnLayout->addWidget(nextBtn);
        
        layout->addWidget(titleLabel);
        layout->addLayout(btnLayout);
        
        hide();
    }
    
    QPushButton *prevBtn;
    QPushButton *replayBtn;
    QPushButton *nextBtn;
    QLabel *titleLabel;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(0, 0, 0, 180));
    }
};

class PlaylistDrawer : public QWidget {
public:
    explicit PlaylistDrawer(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMouseTracking(true);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(18, 16, 18, 18);
        root->setSpacing(12);

        auto *head = new QHBoxLayout();
        head->setSpacing(10);

        titleLbl = new QLabel("Playlist", this);
        titleLbl->setStyleSheet(
            "color:white;font-size:16px;font-weight:700;"
            "background:transparent;"
        );

        autoPlayNext = new QPushButton("Auto: Off", this);
        autoPlayNext->setCheckable(true);
        autoPlayNext->setCursor(Qt::PointingHandCursor);
        autoPlayNext->setFocusPolicy(Qt::NoFocus);
        autoPlayNext->setStyleSheet(
            "QPushButton{color:rgba(255,255,255,150);background:rgba(255,255,255,8);"
            "border:1px solid rgba(255,255,255,16);border-radius:8px;"
            "padding:4px 10px;font-size:11px;}"
            "QPushButton:checked{color:white;background:rgba(255,107,107,60);"
            "border:1px solid rgba(255,107,107,120);}"
        );
        connect(autoPlayNext, &QPushButton::toggled, this, [this](bool checked) {
            autoPlayNext->setText(checked ? "Auto: On" : "Auto: Off");
        });

        closeBtn = new QPushButton("×", this);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setFixedSize(30, 30);
        closeBtn->setStyleSheet(
            "QPushButton{color:white;background:rgba(255,255,255,10);"
            "border:1px solid rgba(255,255,255,18);border-radius:15px;font-size:18px;}"
            "QPushButton:hover{background:rgba(255,255,255,18);}"
        );

        head->addWidget(titleLbl);
        head->addStretch(1);
        head->addWidget(autoPlayNext);
        head->addWidget(closeBtn);

        list = new QListWidget(this);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setSpacing(4);
        list->setFrameShape(QFrame::NoFrame);
        list->setIconSize(QSize(80, 45));
        list->setFocusPolicy(Qt::NoFocus);
        list->setStyleSheet(
            "QListWidget{background:transparent;border:none;outline:none;}"
            "QListWidget::item{padding:10px 12px;border-radius:12px;"
            "background:rgba(255,255,255,6);color:rgba(255,255,255,190);}"
            "QListWidget::item:selected{background:rgba(255,255,255,20);color:white;}"
            "QListWidget::item:hover{background:rgba(255,255,255,14);color:white;}"
        );

        root->addLayout(head);
        root->addWidget(list, 1);
    }

    QListWidget *list = nullptr;
    QPushButton *closeBtn = nullptr;
    QPushButton *autoPlayNext = nullptr;
    QLabel *titleLbl = nullptr;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);

        QPainterPath path;
        path.addRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), 22, 22);

        QLinearGradient g(0, 0, width(), 0);
        g.setColorAt(0, QColor(12, 12, 18, 248));
        g.setColorAt(1, QColor(18, 18, 26, 248));

        p.fillPath(path, g);
        p.setPen(QPen(QColor(255, 255, 255, 18), 1.0));
        p.drawPath(path);
    }
};

class PlayerWindow : public QWidget {
    Q_OBJECT
public:
    explicit PlayerWindow(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        theme = loadTheme();

        thumbnailCache = new ThumbnailCache(this);
        thumbnailExtractor = new ThumbnailExtractor(thumbnailCache, this);
        connect(thumbnailExtractor, &ThumbnailExtractor::thumbnailReady,
                this, &PlayerWindow::onThumbnailReady);

        setWindowTitle("Lume");
        resize(1280, 720);
        setMinimumSize(640, 360);
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("QWidget{background:rgb(8,8,12);}");
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        canvas = new QWidget(this);
        canvas->setAttribute(Qt::WA_StyledBackground, true);
        canvas->setMouseTracking(true);
        canvas->setStyleSheet("background:rgb(10,10,14);");
        canvas->setCursor(Qt::ArrowCursor);
        root->addWidget(canvas, 1);

        scene = new QGraphicsScene(this);

        view = new QGraphicsView(scene, canvas);
        view->setFrameShape(QFrame::NoFrame);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        view->setStyleSheet("background:black;");
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
        view->viewport()->setCursor(Qt::ArrowCursor);
        view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

        videoItem = new QGraphicsVideoItem;
        scene->addItem(videoItem);

        player = new QMediaPlayer(this);
        player->setVideoOutput(videoItem);

        endScreen = new EndScreen(canvas);
        endScreen->hide();
        
        connect(endScreen->replayBtn, &QPushButton::clicked, this, [this] {
            endScreen->hide();
            // Don't reload media, just seek to start and play
            qint64 duration = player->duration();
            player->setPosition(0);
            player->play();
        });
        
        connect(endScreen->nextBtn, &QPushButton::clicked, this, [this] {
            endScreen->hide();
            playNextFile();
        });
        
        connect(endScreen->prevBtn, &QPushButton::clicked, this, [this] {
            endScreen->hide();
            playPreviousFile();
        });

        overlay = new OverlayPanel(canvas);
        overlay->hide();

        overlayFx = new QGraphicsOpacityEffect(overlay);
        overlayFx->setOpacity(0.0);
        overlay->setGraphicsEffect(overlayFx);

        auto *ov = new QVBoxLayout(overlay);
        ov->setContentsMargins(22, 14, 22, 16);
        ov->setSpacing(10);

        auto *progressRow = new QHBoxLayout();
        progressRow->setSpacing(10);

        timeLbl = mkTimeLbl("00:00", overlay);
        seek = new SeekSlider(Qt::Horizontal, overlay);
        seek->setRange(0, 0);
        seek->setMouseTracking(true);
        durLbl = mkTimeLbl("00:00", overlay);

        progressRow->addWidget(timeLbl);
        progressRow->addWidget(seek, 1);
        progressRow->addWidget(durLbl);

        auto *btnRow = new QHBoxLayout();
        btnRow->setSpacing(6);

        openBtn    = mkBtn("Open", false, overlay, theme);
        prevFileBtn = mkBtn("⏮", false, overlay, theme);
        skipBackBtn = mkBtn("-10s", false, overlay, theme);
        playBtn    = mkBtn("▶", true, overlay, theme);
        skipFwdBtn  = mkBtn("+10s", false, overlay, theme);
        nextFileBtn = mkBtn("⏭", false, overlay, theme);
        stopBtn    = mkBtn("■", false, overlay, theme);
        listBtn    = mkBtn("☰", false, overlay, theme);
        dbusBtn    = mkBtn("DBus", false, overlay, theme);
        execBtn    = mkBtn("Exec", false, overlay, theme);
        volBtn     = mkBtn("🔊", false, overlay, theme);
        fullBtn    = mkBtn("⛶", false, overlay, theme);

        volSlider = new SeekSlider(Qt::Horizontal, overlay);
        volSlider->setRange(0, 100);
        volSlider->setValue(80);
        volSlider->setFixedWidth(72);
        volSlider->setMouseTracking(true);

        statusLbl = new QLabel("Ready", overlay);
        statusLbl->setStyleSheet(
            "color:rgba(255,255,255,60);font-size:11px;"
            "font-style:italic;background:transparent;"
        );

        btnRow->addWidget(openBtn);
        btnRow->addSpacing(4);
        btnRow->addWidget(prevFileBtn);
        btnRow->addWidget(skipBackBtn);
        btnRow->addWidget(stopBtn);
        btnRow->addWidget(playBtn);
        btnRow->addWidget(skipFwdBtn);
        btnRow->addWidget(nextFileBtn);
        btnRow->addWidget(listBtn);
        btnRow->addWidget(dbusBtn);
        btnRow->addWidget(execBtn);
        btnRow->addSpacing(4);
        btnRow->addWidget(volBtn);
        btnRow->addWidget(volSlider);
        btnRow->addStretch(1);
        btnRow->addWidget(statusLbl);
        btnRow->addStretch(1);
        btnRow->addWidget(fullBtn);

        ov->addLayout(progressRow);
        ov->addLayout(btnRow);

        styleSliders();

        playlistDrawer = new PlaylistDrawer(canvas);
        playlistDrawer->hide();

        connect(playlistDrawer->closeBtn, &QPushButton::clicked, this, [this] {
            hidePlaylistDrawer();
        });

        connect(playlistDrawer->list, &QListWidget::itemDoubleClicked,
                this, [this](QListWidgetItem *item) {
            if (!item)
                return;
            bool isDir = item->data(Qt::UserRole + 2).toBool();
            if (isDir) {
                currentMediaDir = item->data(Qt::UserRole).toString();
                reloadPlaylist(currentMediaDir);
            } else {
                const QString path = item->data(Qt::UserRole).toString();
                if (!path.isEmpty())
                    openMediaFile(path, true);
            }
        });

        view->lower();
        endScreen->raise();
        overlay->raise();
        playlistDrawer->raise();

        canvas->installEventFilter(this);
        view->viewport()->installEventFilter(this);
        this->installEventFilter(this);

        hideTimer = new QTimer(this);
        hideTimer->setSingleShot(true);
        connect(hideTimer, &QTimer::timeout, this, [this] {
            if (player->state() == QMediaPlayer::PlayingState)
                maybeHideOverlay();
        });

        cursorHideTimer = new QTimer(this);
        cursorHideTimer->setSingleShot(true);
        cursorHideTimer->setInterval(2000);
        connect(cursorHideTimer, &QTimer::timeout, this, [this] {
            if (player->state() == QMediaPlayer::PlayingState && 
                !playlistOpen && !endScreen->isVisible()) {
                canvas->setCursor(Qt::BlankCursor);
                view->viewport()->setCursor(Qt::BlankCursor);
            }
        });

        connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
            seek->setRange(0, d > 0 ? int(d) : 0);
            durLbl->setText(ft(d));
        });

        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
            if (!seek->isSliderDown()) {
                seek->blockSignals(true);
                seek->setValue(int(p));
                seek->blockSignals(false);
                timeLbl->setText(ft(p));
            }
        });

        connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                if (playlistDrawer->autoPlayNext->isChecked()) {
                    playNextFile();
                } else {
                    endScreen->show();
                    endScreen->raise();
                    canvas->setCursor(Qt::ArrowCursor);
                    view->viewport()->setCursor(Qt::ArrowCursor);
                }
            }
        });

        connect(player,
                static_cast<void(QMediaPlayer::*)(QMediaPlayer::State)>(&QMediaPlayer::stateChanged),
                this,
                [this](QMediaPlayer::State s) {
            if (s == QMediaPlayer::PlayingState) {
                playBtn->setText("⏸");
                statusLbl->setText("Playing");
                endScreen->hide();
                showOverlay(true);
                cursorHideTimer->start();
            } else if (s == QMediaPlayer::PausedState) {
                playBtn->setText("▶");
                statusLbl->setText("Paused");
                hideTimer->stop();
                cursorHideTimer->stop();
                canvas->setCursor(Qt::ArrowCursor);
                view->viewport()->setCursor(Qt::ArrowCursor);
            } else {
                playBtn->setText("▶");
                statusLbl->setText("Stopped");
                hideTimer->stop();
                cursorHideTimer->stop();
                canvas->setCursor(Qt::ArrowCursor);
                view->viewport()->setCursor(Qt::ArrowCursor);
            }
            refreshPlaylistHighlight();
        });

        connect(player,
                static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
                this,
                [this](QMediaPlayer::Error) {
            statusLbl->setText(player->errorString());
        });

        connect(openBtn, &QPushButton::clicked, this, [this] { doOpen(); });
        connect(prevFileBtn, &QPushButton::clicked, this, [this] { playPreviousFile(); });
        connect(nextFileBtn, &QPushButton::clicked, this, [this] { playNextFile(); });
        connect(skipBackBtn, &QPushButton::clicked, this, [this] { skipBack(10); });
        connect(skipFwdBtn, &QPushButton::clicked, this, [this] { skipForward(10); });
        connect(playBtn, &QPushButton::clicked, this, [this] { doPlay(); });
        connect(stopBtn, &QPushButton::clicked, this, [this] {
            player->stop();
            seek->setValue(0);
            playBtn->setText("▶");
            statusLbl->setText("Stopped");
            hideTimer->stop();
            hideOverlayNow();
            refreshPlaylistHighlight();
        });
        connect(listBtn, &QPushButton::clicked, this, [this] {
            togglePlaylistDrawer();
        });
        connect(dbusBtn, &QPushButton::clicked, this, [this] { sendDbusShortcut(); });
        connect(execBtn, &QPushButton::clicked, this, [this] { runExecShortcut(); });
        connect(fullBtn, &QPushButton::clicked, this, [this] { doFull(); });

        connect(volBtn, &QPushButton::clicked, this, [this] {
            if (player->volume() > 0) {
                savedVol = player->volume();
                player->setVolume(0);
                volSlider->setValue(0);
                volBtn->setText("🔇");
            } else {
                player->setVolume(savedVol);
                volSlider->setValue(savedVol);
                volBtn->setText("🔊");
            }
        });

        connect(volSlider, &QSlider::valueChanged, this, [this](int v) {
            player->setVolume(v);
            volBtn->setText(v == 0 ? "🔇" : (v < 50 ? "🔉" : "🔊"));
        });

        connect(seek, &QSlider::sliderMoved, this, [this](int v) {
            player->setPosition(v);
            timeLbl->setText(ft(v));
            if (player->state() == QMediaPlayer::PlayingState)
                showOverlay(true);
        });

        connect(seek, &QSlider::valueChanged, this, [this](int v) {
            if (!seek->isSliderDown()) {
                player->setPosition(v);
                timeLbl->setText(ft(v));
            }
        });

        auto *dbusShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this);
        connect(dbusShortcut, &QShortcut::activated, this, [this] {
            sendDbusShortcut();
        });

        auto *execShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+E")), this);
        connect(execShortcut, &QShortcut::activated, this, [this] {
            runExecShortcut();
        });

        player->setVolume(80);
        cursorHideTimer->start();
    }

    void skipBack(int seconds) {
        qint64 newPos = player->position() - (seconds * 1000);
        if (newPos < 0) newPos = 0;
        player->setPosition(newPos);
    }

    void skipForward(int seconds) {
        qint64 newPos = player->position() + (seconds * 1000);
        if (newPos > player->duration()) newPos = player->duration();
        player->setPosition(newPos);
    }

    void openPathFromArgument(const QString &arg) {
        QString path = arg.trimmed();
        if (path.isEmpty())
            return;

        if (path.startsWith(QStringLiteral("file://")))
            path = QUrl(path).toLocalFile();
        else if (QUrl(path).isLocalFile())
            path = QUrl(path).toLocalFile();

        QFileInfo fi(path);
        if (!fi.exists()) {
            if (statusLbl)
                statusLbl->setText("Path not found");
            return;
        }

        if (fi.isDir()) {
            currentMediaDir = fi.absoluteFilePath();
            reloadPlaylist(currentMediaDir);
            showOverlay(true);
            if (statusLbl)
                statusLbl->setText("Folder loaded");
            return;
        }

        openMediaFile(fi.absoluteFilePath(), true);
        showOverlay(true);
    }

    void sendDbusShortcut() {
        const QString service = qEnvironmentVariable("LUME_MEDIA_DBUS_SERVICE",
                                                    "org.lume.Player");
        const QString path = qEnvironmentVariable("LUME_MEDIA_DBUS_PATH",
                                                  "/org/lume/Player");
        const QString iface = qEnvironmentVariable("LUME_MEDIA_DBUS_INTERFACE",
                                                   "org.lume.Player");
        const QString method = qEnvironmentVariable("LUME_MEDIA_DBUS_METHOD",
                                                    "Open");

        QString stateText = "stopped";
        if (player->state() == QMediaPlayer::PlayingState)
            stateText = "playing";
        else if (player->state() == QMediaPlayer::PausedState)
            stateText = "paused";

        QDBusMessage msg = QDBusMessage::createMethodCall(service, path, iface, method);
        msg << currentMediaPath
            << currentMediaDir
            << QFileInfo(currentMediaPath).fileName()
            << stateText
            << player->position();

        const bool sent = QDBusConnection::sessionBus().send(msg);
        if (statusLbl)
            statusLbl->setText(sent ? "DBus sent" : "DBus failed");
    }

    void runExecShortcut() {
        QString cmd = qEnvironmentVariable("LUME_MEDIA_EXEC_CMD").trimmed();

        const QString currentName = QFileInfo(currentMediaPath).fileName();
        const QString stateText = (player->state() == QMediaPlayer::PlayingState)
                                    ? "playing"
                                    : (player->state() == QMediaPlayer::PausedState)
                                        ? "paused"
                                        : "stopped";

        if (cmd.isEmpty()) {
            if (currentMediaPath.isEmpty()) {
                if (statusLbl)
                    statusLbl->setText("No media selected");
                return;
            }
            QProcess::startDetached("xdg-open", { currentMediaPath });
            if (statusLbl)
                statusLbl->setText("Exec: xdg-open");
            return;
        }

        cmd = expandTemplate(cmd, currentMediaPath, currentMediaDir, currentName, stateText, player->position());
        QProcess::startDetached("/bin/sh", { "-lc", cmd });
        if (statusLbl)
            statusLbl->setText("Exec sent");
    }

protected:
    void showEvent(QShowEvent *e) override {
        QWidget::showEvent(e);
        repositionAll();
    }

    void resizeEvent(QResizeEvent *e) override {
        QWidget::resizeEvent(e);
        repositionAll();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(8, 8, 12));
    }

void keyPressEvent(QKeyEvent *event) override {
    switch (event->key()) {
        case Qt::Key_Left:
            if (event->modifiers() & Qt::AltModifier) {
                playPreviousFile();
            } else {
                skipBack(10);
            }
            break;
        case Qt::Key_Right:
            if (event->modifiers() & Qt::AltModifier) {
                playNextFile();
            } else {
                skipForward(10);
            }
            break;
        case Qt::Key_R:
            // Replay last 2 seconds
            skipBack(3);
            break;
        case Qt::Key_Up:
            player->setVolume(qMin(player->volume() + 5, 100));
            volSlider->setValue(player->volume());
            break;
        case Qt::Key_Down:
            player->setVolume(qMax(player->volume() - 5, 0));
            volSlider->setValue(player->volume());
            break;
        case Qt::Key_Space:
            doPlay();
            break;
        case Qt::Key_Escape:
            if (isFullScreen()) {
                showNormal();
                fullBtn->setText("⛶");
                repositionAll();
            }
            break;
        case Qt::Key_F:
        case Qt::Key_F11:
            doFull();
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj == canvas || obj == view->viewport() || obj == this) {
            if (ev->type() == QEvent::MouseMove) {
                canvas->setCursor(Qt::ArrowCursor);
                view->viewport()->setCursor(Qt::ArrowCursor);
                cursorHideTimer->start();
                
                if (player->state() == QMediaPlayer::PlayingState && !playlistOpen)
                    showOverlay(true);
            }
            
            if (obj == view->viewport()) {
                if (ev->type() == QEvent::MouseButtonPress && !endScreen->isVisible()) {
                    auto *me = static_cast<QMouseEvent *>(ev);
                    if (me->button() == Qt::LeftButton) {
                        doPlay();
                        return true;
                    }
                }
                if (ev->type() == QEvent::MouseButtonDblClick) {
                    doFull();
                    return true;
                }
            }
        }

        return QWidget::eventFilter(obj, ev);
    }

private slots:
    void onThumbnailReady(const QString &filePath, const QPixmap &thumbnail) {
        if (thumbnail.isNull())
            return;
            
        if (playlistDrawer && playlistDrawer->list) {
            for (int i = 0; i < playlistDrawer->list->count(); ++i) {
                QListWidgetItem *item = playlistDrawer->list->item(i);
                if (item && item->data(Qt::UserRole).toString() == filePath) {
                    item->setIcon(QIcon(thumbnail));
                    break;
                }
            }
        }
    }

private:
    Theme theme;
    ThumbnailCache *thumbnailCache = nullptr;
    ThumbnailExtractor *thumbnailExtractor = nullptr;

    QWidget *canvas = nullptr;
    QGraphicsView *view = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsVideoItem *videoItem = nullptr;
    QMediaPlayer *player = nullptr;

    OverlayPanel *overlay = nullptr;
    QGraphicsOpacityEffect *overlayFx = nullptr;
    QVariantAnimation *overlayAnim = nullptr;

    PlaylistDrawer *playlistDrawer = nullptr;
    QPropertyAnimation *playlistAnim = nullptr;
    bool playlistOpen = false;

    EndScreen *endScreen = nullptr;

    QLabel *timeLbl = nullptr;
    QLabel *durLbl = nullptr;
    QLabel *statusLbl = nullptr;

    QPushButton *openBtn = nullptr;
    QPushButton *prevFileBtn = nullptr;
    QPushButton *skipBackBtn = nullptr;
    QPushButton *playBtn = nullptr;
    QPushButton *skipFwdBtn = nullptr;
    QPushButton *nextFileBtn = nullptr;
    QPushButton *stopBtn = nullptr;
    QPushButton *listBtn = nullptr;
    QPushButton *dbusBtn = nullptr;
    QPushButton *execBtn = nullptr;
    QPushButton *volBtn = nullptr;
    QPushButton *fullBtn = nullptr;

    SeekSlider *seek = nullptr;
    SeekSlider *volSlider = nullptr;
    QTimer *hideTimer = nullptr;
    QTimer *cursorHideTimer = nullptr;

    int savedVol = 80;
    bool hasMedia = false;
    QString currentMediaPath;
    QString currentMediaDir;

    void repositionAll() {
        repositionVideo();
        repositionOverlay();
        repositionPlaylistDrawer();
        repositionEndScreen();
    }

    void repositionEndScreen() {
        if (endScreen && canvas) {
            endScreen->setGeometry(canvas->rect());
            endScreen->raise();
        }
    }

    QStringList getPlaylistFilesInOrder() {
        QStringList files;
        if (playlistDrawer && playlistDrawer->list) {
            for (int i = 0; i < playlistDrawer->list->count(); ++i) {
                QListWidgetItem *item = playlistDrawer->list->item(i);
                if (item && !item->data(Qt::UserRole + 2).toBool()) {
                    files.append(item->data(Qt::UserRole).toString());
                }
            }
        }
        return files;
    }

    int getCurrentFileIndex() {
        QStringList files = getPlaylistFilesInOrder();
        return files.indexOf(QFileInfo(currentMediaPath).absoluteFilePath());
    }

    void playNextFile() {
        QStringList files = getPlaylistFilesInOrder();
        int currentIdx = getCurrentFileIndex();
        
        if (currentIdx >= 0 && currentIdx < files.size() - 1) {
            openMediaFile(files[currentIdx + 1], true);
        }
    }

    void playPreviousFile() {
        if (player->position() > 3000) {
            player->setPosition(0);
            if (player->state() != QMediaPlayer::PlayingState)
                player->play();
        } else {
            QStringList files = getPlaylistFilesInOrder();
            int currentIdx = getCurrentFileIndex();
            
            if (currentIdx > 0) {
                openMediaFile(files[currentIdx - 1], true);
            } else if (currentIdx == 0) {
                player->setPosition(0);
                if (player->state() != QMediaPlayer::PlayingState)
                    player->play();
            }
        }
    }

    void styleSliders() {
        const QString ac  = theme.accent.name();
        const QString ac2 = theme.accent2.name();
        const QString ach = theme.accent.lighter(115).name();

        seek->setStyleSheet(QString(
            "QSlider{background:transparent;}"
            "QSlider::groove:horizontal{height:4px;background:rgba(255,255,255,22);border-radius:2px;}"
            "QSlider::sub-page:horizontal{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:1 %1);border-radius:2px;}"
            "QSlider::handle:horizontal{width:14px;height:14px;margin:-5px 0;border-radius:7px;background:%1;border:2px solid white;}"
            "QSlider::handle:horizontal:hover{width:16px;height:16px;margin:-6px 0;border-radius:8px;background:%3;}"
        ).arg(ac, ac2, ach));

        volSlider->setStyleSheet(
            "QSlider{background:transparent;}"
            "QSlider::groove:horizontal{height:3px;background:rgba(255,255,255,20);border-radius:2px;}"
            "QSlider::sub-page:horizontal{background:rgba(255,255,255,55);border-radius:2px;}"
            "QSlider::handle:horizontal{width:10px;height:10px;margin:-4px 0;border-radius:5px;background:white;}"
        );
    }

    void repositionVideo() {
        if (!scene || !view || !videoItem)
            return;

        view->setGeometry(canvas->rect());
        scene->setSceneRect(QRectF(QPointF(0, 0), canvas->size()));
        videoItem->setSize(canvas->size());
    }

    int drawerWidth() const {
        if (!canvas)
            return 300;
        return qBound(240, canvas->width() / 3, 340);
    }

    void repositionOverlay() {
        if (!overlay || !canvas)
            return;

        const int margin = 24;
        const int barH = 106;
        const int barW = qMin(canvas->width() - margin * 2, 880);
        const int barX = (canvas->width() - barW) / 2;
        const int barY = canvas->height() - barH - margin;

        overlay->setGeometry(barX, barY, barW, barH);
    }

    void repositionPlaylistDrawer() {
        if (!playlistDrawer || !canvas)
            return;

        const int w = drawerWidth();
        const QRect shownRect(0, 0, w, canvas->height());
        const QRect hiddenRect(-w, 0, w, canvas->height());

        if (playlistOpen && !playlistAnim)
            playlistDrawer->setGeometry(shownRect);
        else if (!playlistOpen && !playlistAnim)
            playlistDrawer->setGeometry(hiddenRect);
    }

    void showOverlay(bool autoHide) {
        if (!overlay || !overlayFx)
            return;

        if (overlayAnim) {
            disconnect(overlayAnim, nullptr, this, nullptr);
            overlayAnim->stop();
            overlayAnim->deleteLater();
            overlayAnim = nullptr;
        }

        overlay->show();
        overlay->raise();
        
        if (playlistOpen && playlistDrawer->isVisible())
            playlistDrawer->raise();
        if (endScreen->isVisible())
            endScreen->raise();

        if (overlayFx->opacity() >= 0.98) {
            if (autoHide)
                hideTimer->start(3000);
            return;
        }

        auto *a = new QVariantAnimation(this);
        overlayAnim = a;
        a->setStartValue(overlayFx->opacity());
        a->setEndValue(1.0);
        a->setDuration(180);
        a->setEasingCurve(QEasingCurve::InOutQuad);

        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            if (overlayFx)
                overlayFx->setOpacity(v.toDouble());
        });
        connect(a, &QVariantAnimation::finished, this, [this, a, autoHide] {
            if (overlayAnim == a)
                overlayAnim = nullptr;
            a->deleteLater();
            if (autoHide)
                hideTimer->start(3000);
        });

        a->start();
    }

    void hideOverlayNow() {
        if (!overlay || !overlayFx)
            return;

        if (overlayAnim) {
            disconnect(overlayAnim, nullptr, this, nullptr);
            overlayAnim->stop();
            overlayAnim->deleteLater();
            overlayAnim = nullptr;
        }

        if (!overlay->isVisible()) {
            overlayFx->setOpacity(0.0);
            return;
        }

        auto *a = new QVariantAnimation(this);
        overlayAnim = a;
        a->setStartValue(overlayFx->opacity());
        a->setEndValue(0.0);
        a->setDuration(190);
        a->setEasingCurve(QEasingCurve::InOutQuad);

        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            if (overlayFx)
                overlayFx->setOpacity(v.toDouble());
        });
        connect(a, &QVariantAnimation::finished, this, [this, a] {
            if (overlayAnim == a)
                overlayAnim = nullptr;
            a->deleteLater();
            if (overlay) {
                overlay->hide();
            }
        });

        a->start();
    }

    void maybeHideOverlay() {
        if (!overlay || !canvas)
            return;

        const QPoint g = QCursor::pos();
        const QPoint inCanvas = canvas->mapFromGlobal(g);
        const QPoint inOverlay = overlay->mapFromGlobal(g);
        const QPoint inDrawer = playlistDrawer->mapFromGlobal(g);

        if (!canvas->rect().contains(inCanvas) ||
            (!overlay->rect().contains(inOverlay) && 
             !(playlistOpen && playlistDrawer->isVisible() && 
               playlistDrawer->rect().contains(inDrawer)))) {
            hideOverlayNow();
            return;
        }

        showOverlay(false);
    }

    void togglePlaylistDrawer() {
        if (playlistOpen)
            hidePlaylistDrawer();
        else
            showPlaylistDrawer();
    }

    void showPlaylistDrawer() {
        if (!playlistDrawer || !canvas)
            return;

        if (playlistAnim) {
            QPropertyAnimation *oldAnim = playlistAnim;
            playlistAnim = nullptr;
            disconnect(oldAnim, nullptr, this, nullptr);
            oldAnim->stop();
            oldAnim->deleteLater();
        }

        if (currentMediaDir.isEmpty())
            currentMediaDir = QDir::homePath();

        reloadPlaylist(currentMediaDir);
        playlistDrawer->show();
        playlistDrawer->raise();

        const int w = drawerWidth();
        const QRect startRect(-w, 0, w, canvas->height());
        const QRect endRect(0, 0, w, canvas->height());

        playlistDrawer->setGeometry(startRect);

        playlistAnim = new QPropertyAnimation(playlistDrawer, "geometry", this);
        playlistAnim->setDuration(220);
        playlistAnim->setEasingCurve(QEasingCurve::OutCubic);
        playlistAnim->setStartValue(startRect);
        playlistAnim->setEndValue(endRect);
        connect(playlistAnim, &QPropertyAnimation::finished, this, [this] {
            playlistAnim = nullptr;
            if (playlistDrawer) {
                playlistDrawer->raise();
                playlistDrawer->setFocus();
            }
        });
        playlistAnim->start();

        playlistOpen = true;
        refreshPlaylistHighlight();
    }

    void hidePlaylistDrawer() {
        if (!playlistDrawer || !canvas)
            return;

        if (!playlistOpen && !playlistDrawer->isVisible())
            return;

        if (playlistAnim) {
            QPropertyAnimation *oldAnim = playlistAnim;
            playlistAnim = nullptr;
            disconnect(oldAnim, nullptr, this, nullptr);
            oldAnim->stop();
            oldAnim->deleteLater();
        }

        const int w = drawerWidth();
        const QRect startRect = playlistDrawer->geometry();
        const QRect endRect(-w, 0, w, canvas->height());

        playlistAnim = new QPropertyAnimation(playlistDrawer, "geometry", this);
        playlistAnim->setDuration(220);
        playlistAnim->setEasingCurve(QEasingCurve::OutCubic);
        playlistAnim->setStartValue(startRect);
        playlistAnim->setEndValue(endRect);
        connect(playlistAnim, &QPropertyAnimation::finished, this, [this] {
            playlistAnim = nullptr;
            if (playlistDrawer)
                playlistDrawer->hide();
        });
        playlistAnim->start();

        playlistOpen = false;
    }

    void reloadPlaylist(const QString &dirPath) {
        if (!playlistDrawer || !playlistDrawer->list)
            return;

        playlistDrawer->list->clear();

        QDir d(dirPath.isEmpty() ? QDir::homePath() : dirPath);
        const QStringList filters = {
            "*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.m4v",
            "*.mp3", "*.wav", "*.flac", "*.m4a", "*.ogg"
        };

        QFileInfoList allEntries = d.entryInfoList(filters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        
        std::sort(allEntries.begin(), allEntries.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
            if (a.isDir() && !b.isDir())
                return true;
            if (!a.isDir() && b.isDir())
                return false;
            return collator.compare(a.fileName(), b.fileName()) < 0;
        });

        for (const QFileInfo &fi : allEntries) {
            if (fi.isDir()) {
                auto *item = new QListWidgetItem("📁 " + fi.fileName());
                item->setData(Qt::UserRole, fi.absoluteFilePath());
                item->setData(Qt::UserRole + 1, fi.fileName());
                item->setData(Qt::UserRole + 2, true);
                item->setForeground(QColor(200, 200, 255, 200));
                playlistDrawer->list->addItem(item);
            } else {
                auto *item = new QListWidgetItem(fi.fileName());
                item->setData(Qt::UserRole, fi.absoluteFilePath());
                item->setData(Qt::UserRole + 1, fi.fileName());
                item->setData(Qt::UserRole + 2, false);
                
                QString absPath = fi.absoluteFilePath();
                if (thumbnailExtractor->hasCached(absPath)) {
                    item->setIcon(QIcon(thumbnailExtractor->getCached(absPath)));
                } else {
                    thumbnailExtractor->extractAsync(absPath, QSize(80, 45));
                }
                
                playlistDrawer->list->addItem(item);
            }
        }

        refreshPlaylistHighlight();
    }

    void refreshPlaylistHighlight() {
        if (!playlistDrawer || !playlistDrawer->list)
            return;

        for (int i = 0; i < playlistDrawer->list->count(); ++i) {
            QListWidgetItem *item = playlistDrawer->list->item(i);
            if (!item) continue;
            
            bool isDir = item->data(Qt::UserRole + 2).toBool();
            if (isDir) continue;
            
            const QString path = item->data(Qt::UserRole).toString();
            const QString base  = item->data(Qt::UserRole + 1).toString();

            const bool current = !currentMediaPath.isEmpty() &&
                                 QFileInfo(path).absoluteFilePath() == QFileInfo(currentMediaPath).absoluteFilePath();

            item->setText((current ? QStringLiteral("▶ ") : QString()) + base);

            QFont f = item->font();
            f.setBold(current);
            item->setFont(f);

            item->setForeground(current ? QBrush(Qt::white)
                                        : QBrush(QColor(255, 255, 255, 190)));
            item->setBackground(current ? QBrush(QColor(255, 255, 255, 22))
                                        : QBrush(Qt::transparent));

            if (current)
                playlistDrawer->list->setCurrentItem(item);
        }
    }

    void openMediaFile(const QString &f, bool autoplay) {
        if (f.isEmpty())
            return;

        player->stop();
        player->setMedia(QMediaContent(QUrl::fromLocalFile(f)));

        currentMediaPath = QFileInfo(f).absoluteFilePath();
        currentMediaDir = QFileInfo(f).absolutePath();
        hasMedia = true;

        setWindowTitle("Lume – " + QFileInfo(f).fileName());
        reloadPlaylist(currentMediaDir);
        refreshPlaylistHighlight();

        if (autoplay)
            player->play();
    }

    void doPlay() {
        if (!hasMedia) {
            doOpen();
            return;
        }

        if (player->state() == QMediaPlayer::PlayingState)
            player->pause();
        else
            player->play();
    }

    void doFull() {
        if (isFullScreen()) {
            showNormal();
            fullBtn->setText("⛶");
        } else {
            showFullScreen();
            fullBtn->setText("⊡");
        }

        repositionAll();

        if (player->state() == QMediaPlayer::PlayingState)
            showOverlay(true);
        else
            showOverlay(false);
    }

    void doOpen() {
        const QString f = QFileDialog::getOpenFileName(
            this,
            "Open Media",
            QString(),
            "Media (*.mp4 *.mkv *.avi *.mov *.webm *.m4v *.mp3 *.wav *.flac *.m4a *.ogg);;All (*)"
        );
        if (f.isEmpty())
            return;

        openMediaFile(f, true);
        showOverlay(true);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Lume");

    QCommandLineParser parser;
    parser.setApplicationDescription("Lume media player");
    parser.addHelpOption();
    parser.process(app);

    const QStringList positional = parser.positionalArguments();

    PlayerWindow w;
    w.show();

    if (!positional.isEmpty()) {
        const QString firstArg = positional.first();
        QTimer::singleShot(0, &w, [&w, firstArg] {
            w.openPathFromArgument(firstArg);
        });
    }

    return app.exec();
}

#include "main.moc"
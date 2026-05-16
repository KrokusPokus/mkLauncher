#include "mainwindow.h"
#include "filepropertiesdialog.h"
#include "helpers.h"
#include "searchworker.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QtConcurrent>
#include <QDateTime>
#include <QDBusConnection>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QObject>

#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSize>
#include <QStandardPaths>
#include <QStringList>
#include <QStringView>

#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <qwindow.h>
#include <algorithm> // Für std::reverse
#include <utility> // Für std::as_const

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#define WM_MY_LAUNCHER_RESTORE (WM_APP + 123)
#endif


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Note on other flags:
    //    Qt::Tool  Advantage: no taskbar icon. Problem: Window can't be minimized. Window can't be hidden.
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    setWindowTitle("mkLauncher");
    setWindowIcon(QIcon(":/icons/res/app.ico"));

    this->setStyleSheet(
            "QMainWindow { background-color: #222222; }"
            "QHeaderView::section { background-color: #222222; color: #ffffff; }"
        );

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    m_centralWidget->setMinimumWidth(745);

    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(3, 3, 3, 3);     // margins: distance between edge of layout box to contents
    m_mainLayout->setSpacing(3);                      // spacing: distance between elements within layout

    // --------------------------------------------------------------------
    // Container for editbox controls

    m_topControlsContainerWidget = new QWidget();
    m_topControlsHBoxLayout = new QHBoxLayout(m_topControlsContainerWidget);
    m_topControlsHBoxLayout->setContentsMargins(7, 5, 7, 5);
    m_topControlsHBoxLayout->setSpacing(1);

    /*
    NOTE:
        padding: distance from text to border
        margin: distance from border to other elements in layout
    */
    m_LineEdit1 = new QLineEdit();
    m_LineEdit1->setStyleSheet("QLineEdit { border: 1px solid #a09beb; border-top-left-radius: 5px; border-bottom-left-radius: 5px; padding: 4px; padding-left: 7px; padding-bottom: 5px; background-color: #1a1a1a; color: #ffffff;}");
    m_topControlsHBoxLayout->addWidget(m_LineEdit1, 8); // second parameter is stretch factor

    m_LineEdit2 = new QLineEdit();
    m_LineEdit2->setStyleSheet("QLineEdit { border: 1px solid #a09beb; border-top-right-radius: 5px; border-bottom-right-radius: 5px; padding: 4px; padding-left: 7px; padding-bottom: 5px; background-color: #1a1a1a; color: #ffffff;}");
    m_topControlsHBoxLayout->addWidget(m_LineEdit2, 1);

    if (m_settings.showPlaceholderText == true) {
        m_LineEdit1->setPlaceholderText(tr("(search terms)"));
        m_LineEdit2->setPlaceholderText(tr("(action)"));
    }

    // --------------------------------------------------------------------
    // ListView

    m_tableWidget = new Custom_QTableWidget();
    m_tableWidget->setItemDelegate(new CutDelegate(this));
    m_tableWidget->setEditTriggers(QAbstractItemView::EditKeyPressed);    // QAbstractItemView::NoEditTriggers

    m_tableWidget->setStyleSheet(
        /* Haupt-Styling für die Tabelle */
        "QTableWidget {"
        "    border: none;"
        "    background-color: #2e2e2e;"
        "    color: #ffffff;"
        "}"
        // Aktive Auswahl
        "QTableWidget::item:selected:active {"
        "    background-color: #6b69d6;"
        "    color: white;"
        "}"
        // Inaktive Auswahl (wenn die InputBox den Fokus hat)
        "QTableWidget::item:selected:!active {"
        "    background-color: #505050;"
        "    color: white;"
        "}"
        /* Styling für die Ecke unten rechts */
        "QAbstractScrollArea::corner {"
        "    background: #1b1b1b;"
        "    border: none;"
        "}"
        );

    m_tableWidget->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    width: 17px;"
        "    margin: 5px 0px 5px 0px;"
        "}"

        "QScrollBar::handle:vertical {"
        "    background: #3f3f3f;"
        "    min-height: 20px;"
        "    margin-left: 5px;"
        "    margin-right: 5px;"
        "    border-radius: 3px;"
        "}"

        "QScrollBar::handle:vertical:hover {"
        "    background: #6966f7;"
        "}"

        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background: none;"
        "}"

        "QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    height: 5px;"
        "    subcontrol-position: top;"
        "    subcontrol-origin: margin;"
        "}"

        "QScrollBar::add-line:vertical {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    height: 5px;"
        "    subcontrol-position: bottom;"
        "    subcontrol-origin: margin;"
        "}"
        );

    m_tableWidget->horizontalScrollBar()->setStyleSheet(
        "QScrollBar:horizontal {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    height: 17px;"
        "    margin: 0px 5px 0px 5px;"
        "}"

        "QScrollBar::handle:horizontal {"
        "    background: #3f3f3f;"
        "    min-width: 20px;"
        "    margin-top: 5px;"
        "    margin-bottom: 5px;"
        "    border-radius: 3px;"
        "}"

        "QScrollBar::handle:horizontal:hover {"
        "    background: #6966f7;"
        "}"

        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "    background: none;"
        "}"

        "QScrollBar::sub-line:horizontal {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    width: 5px;"
        "    subcontrol-position: left;"
        "    subcontrol-origin: margin;"
        "}"

        "QScrollBar::add-line:horizontal {"
        "    border: none;"
        "    background: #1b1b1b;"
        "    width: 5px;"
        "    subcontrol-position: right;"
        "    subcontrol-origin: margin;"
        "}"
        );

    m_tableLineHeight = 18;

    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->verticalHeader()->setMinimumSectionSize(0);
    m_tableWidget->verticalHeader()->setDefaultSectionSize(m_tableLineHeight);
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({tr("Name"), tr("Folder"), tr("Size"), tr("Changed"), tr("Type"), tr("Rating")});

    m_tableWidget->horizontalHeaderItem(eColName)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableWidget->horizontalHeaderItem(eColPath)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableWidget->horizontalHeaderItem(eColSize)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_tableWidget->horizontalHeaderItem(eColDate)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableWidget->horizontalHeaderItem(eColType)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableWidget->horizontalHeaderItem(eColQuality)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    m_tableWidget->horizontalHeader()->setSectionsMovable(true);
    m_tableWidget->horizontalHeader()->setHighlightSections(false);

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    m_tableWidget->setAlternatingRowColors(m_settings.alternatingRowColors);
    m_tableWidget->setShowGrid(m_settings.showGrid);

    m_tableWidget->setMinimumHeight(m_tableLineHeight * 14);

    m_bHeaderVisible = false;
    updateColumns();   // will hide the header for now and update column sizes

    // --------------------------------------------------------------------

    m_mainLayout->addWidget(m_topControlsContainerWidget);
    m_mainLayout->addWidget(m_tableWidget);

    m_tableWidget->hide();
    m_mainLayout->activate();

    // --------------------------------------------------------------------
    // Context menu Actions (m_tableWidget)

    m_actionListViewOpenFiles = new QAction(tr("Open"), this);
    //m_actionListViewOpenFiles->setShortcut(QKeySequence("Ctrl+O"));
    //m_actionListViewOpenFiles->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewOpenFiles);
    connect(m_actionListViewOpenFiles, &QAction::triggered, this, &MainWindow::action_ListViewOpenFiles);

    m_actionListViewEditFiles = new QAction(tr("Edit"), this);
    m_actionListViewEditFiles->setShortcut(QKeySequence("Ctrl+E"));
    m_actionListViewEditFiles->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewEditFiles);
    connect(m_actionListViewEditFiles, &QAction::triggered, this, &MainWindow::action_ListViewEditFiles);

    m_actionListViewBrowseToFile = new QAction(tr("Show in folder"),this);
    m_actionListViewBrowseToFile->setIcon(QIcon::fromTheme("folder-open"));
    m_actionListViewBrowseToFile->setShortcut(QKeySequence("Ctrl+L"));
    m_actionListViewBrowseToFile->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewBrowseToFile);
    connect(m_actionListViewBrowseToFile, &QAction::triggered, this, &MainWindow::action_ListViewBrowseToFile);

    m_actionListViewCopyPaths = new QAction(tr("Copy Path"), this);
    m_actionListViewCopyPaths->setIcon(QIcon::fromTheme("edit-copy-path"));
    m_actionListViewCopyPaths->setShortcut(QKeySequence("Ctrl+Shift+C"));
    m_actionListViewCopyPaths->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewCopyPaths);
    connect(m_actionListViewCopyPaths, &QAction::triggered, this, &MainWindow::action_ListViewCopyPaths);

    m_actionListViewCutFiles = new QAction(tr("Cut"), this);
    m_actionListViewCutFiles->setIcon(QIcon::fromTheme("edit-cut"));
    m_actionListViewCutFiles->setShortcut(QKeySequence("Ctrl+X"));
    m_actionListViewCutFiles->setShortcutContext(Qt::WidgetShortcut);
    m_tableWidget->addAction(m_actionListViewCutFiles);
    connect(m_actionListViewCutFiles, &QAction::triggered, this, &MainWindow::action_ListViewCutFiles);

    m_actionListViewCopyFiles = new QAction(tr("Copy"), this);
    m_actionListViewCopyFiles->setIcon(QIcon::fromTheme("edit-copy"));
    m_actionListViewCopyFiles->setShortcut(QKeySequence("Ctrl+C"));
    m_actionListViewCopyFiles->setShortcutContext(Qt::WidgetShortcut);
    m_tableWidget->addAction(m_actionListViewCopyFiles);
    connect(m_actionListViewCopyFiles, &QAction::triggered, this, &MainWindow::action_ListViewCopyFiles);

    m_actionListViewDeleteFiles = new QAction(tr("Delete"), this);
    m_actionListViewDeleteFiles->setIcon(QIcon::fromTheme("edit-delete"));
    m_actionListViewDeleteFiles->setShortcut(QKeySequence::Delete);
    m_actionListViewDeleteFiles->setShortcutContext(Qt::WidgetShortcut);
    m_tableWidget->addAction(m_actionListViewDeleteFiles);
    connect(m_actionListViewDeleteFiles, &QAction::triggered, this, [this]() { action_ListViewDeleteFiles(true); });

    m_actionListViewRenameFiles = new QAction(tr("Rename"), this);
    m_actionListViewRenameFiles->setIcon(QIcon::fromTheme("edit-rename"));
    m_actionListViewRenameFiles->setShortcut(QKeySequence(Qt::Key_F2));
    m_actionListViewRenameFiles->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewRenameFiles);
    connect(m_actionListViewRenameFiles, &QAction::triggered, this, &MainWindow::action_ListViewRenameFiles);

    m_actionListViewFileProperties = new QAction(tr("Properties"), this);
    m_actionListViewFileProperties->setIcon(QIcon::fromTheme("document-properties"));
    m_actionListViewFileProperties->setShortcut(QKeySequence("Ctrl+I"));
    m_actionListViewFileProperties->setShortcutContext(Qt::WindowShortcut);
    m_tableWidget->addAction(m_actionListViewFileProperties);
    connect(m_actionListViewFileProperties, &QAction::triggered, this, &MainWindow::action_ListViewFileProperties);

    if (m_settings.showIconsInMenu == false) {
        m_actionListViewOpenFiles->setIconVisibleInMenu(false);
        m_actionListViewEditFiles->setIconVisibleInMenu(false);
        m_actionListViewBrowseToFile->setIconVisibleInMenu(false);
        m_actionListViewCopyPaths->setIconVisibleInMenu(false);
        m_actionListViewCutFiles->setIconVisibleInMenu(false);
        m_actionListViewCopyFiles->setIconVisibleInMenu(false);
        m_actionListViewDeleteFiles->setIconVisibleInMenu(false);
        m_actionListViewRenameFiles->setIconVisibleInMenu(false);
        m_actionListViewFileProperties->setIconVisibleInMenu(false);
    }

    if (m_settings.showShortcutsInMenu == false) {
        m_actionListViewOpenFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewEditFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewBrowseToFile->setShortcutVisibleInContextMenu(false);
        m_actionListViewCopyPaths->setShortcutVisibleInContextMenu(false);
        m_actionListViewCutFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewCopyFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewDeleteFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewRenameFiles->setShortcutVisibleInContextMenu(false);
        m_actionListViewFileProperties->setShortcutVisibleInContextMenu(false);
    }

    // --------------------------------------------------------------------

#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    loadMimeCache();

    if (QDBusConnection::sessionBus().registerService("org.mkLauncher")) {
        QDBusConnection::sessionBus().registerObject("/Main", this, QDBusConnection::ExportAllSlots);
    }
#endif

    // --------------------------------------------------------------------
    // Shortcuts: Whole Window

    QShortcut *WindowShortcutN = new QShortcut(QKeySequence("Ctrl+N"), this);
    WindowShortcutN->setContext(Qt::WindowShortcut);
    connect(WindowShortcutN, &QShortcut::activated, this, &MainWindow::action_EditSettingsFile);

    QShortcut *WindowShortcutH = new QShortcut(QKeySequence("Ctrl+H"), this);
    WindowShortcutH->setContext(Qt::WindowShortcut);
    connect(WindowShortcutH, &QShortcut::activated, this, &MainWindow::onToggleListViewHeader);

    QShortcut *WindowShortcutCtrlBackSpace = new QShortcut(QKeySequence("Ctrl+Backspace"), this);
    WindowShortcutCtrlBackSpace->setContext(Qt::WindowShortcut);
    connect(WindowShortcutCtrlBackSpace, &QShortcut::activated, this, [this]() { m_tableWidget->setRowCount(0); m_LineEdit2->clear(); m_LineEdit1->clear(); m_LineEdit1->setFocus(); });

    // --------------------------------------------------------------------

    m_LineEdit1->installEventFilter(this);
    m_LineEdit2->installEventFilter(this);
    m_tableWidget->installEventFilter(this);

    // --------------------------------------------------------------------

    m_timerUpdateIcons = new QTimer(this);
    m_timerUpdateIcons->setSingleShot(true);
    connect(m_timerUpdateIcons, &QTimer::timeout, this, &MainWindow::onTimedUpdateIcons);

    connect(m_LineEdit1, &QLineEdit::textChanged, this, &MainWindow::handleTextChange);

    connect(m_tableWidget, &Custom_QTableWidget::itemChanged, this, &MainWindow::onItemChanged);
    connect(m_tableWidget, &Custom_QTableWidget::customContextMenuRequested, this, &MainWindow::onShowContextMenu);
    connect(m_tableWidget, &Custom_QTableWidget::itemDoubleClicked, this, &MainWindow::onListViewItemDoubleClicked);
    connect(m_tableWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onVerticalBarScrollChange);
    connect(m_tableWidget->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onListViewHeaderClicked);

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::onClipboardChanged);
}

MainWindow::~MainWindow() = default;

void MainWindow::onVerticalBarScrollChange() {
    m_timerUpdateIcons->start(100);
}

void MainWindow::onListViewHeaderClicked() {
    m_timerUpdateIcons->start(100);
}

void MainWindow::onToggleListViewHeader() {
    m_bHeaderVisible = !m_bHeaderVisible;
    updateColumns();
}

void MainWindow::updateColumns() {
    if (m_bHeaderVisible) {
        m_tableWidget->horizontalHeader()->setVisible(true);

        m_tableWidget->setColumnHidden(eColSize, false);
        m_tableWidget->setColumnHidden(eColDate, false);
        m_tableWidget->setColumnHidden(eColType, false);
        m_tableWidget->setColumnHidden(eColQuality, false);

        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColSize, QHeaderView::ResizeToContents);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColDate, QHeaderView::ResizeToContents);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColType, QHeaderView::ResizeToContents);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColQuality, QHeaderView::ResizeToContents);

        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColName, QHeaderView::Stretch);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColPath, QHeaderView::Stretch);

        m_tableWidget->horizontalHeader()->doItemsLayout();

        int eColNameWidth = m_tableWidget->columnWidth(eColName);
        int eColPathWidth = m_tableWidget->columnWidth(eColPath);

        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColName, QHeaderView::Interactive);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(eColPath, QHeaderView::Interactive);

        m_tableWidget->setColumnWidth(eColName, eColNameWidth);
        m_tableWidget->setColumnWidth(eColPath, eColPathWidth);
    } else {
        m_tableWidget->horizontalHeader()->setVisible(false);

        m_tableWidget->setColumnHidden(eColSize, true);
        m_tableWidget->setColumnHidden(eColDate, true);
        m_tableWidget->setColumnHidden(eColType, true);
        m_tableWidget->setColumnHidden(eColQuality, true);

        m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        m_tableWidget->horizontalHeader()->doItemsLayout();

        int eColNameWidth = m_tableWidget->columnWidth(eColName);
        int eColPathWidth = m_tableWidget->columnWidth(eColPath);

        m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

        m_tableWidget->setColumnWidth(eColName, eColNameWidth);
        m_tableWidget->setColumnWidth(eColPath, eColPathWidth);
    }
}

void MainWindow::handleTextChange(const QString &text) {
    if (m_bSearchActive) {
        m_bRestartPendingSearch = true;
        m_bAbortRequested = true;
        emit abortSearchWorkerRequested();

        if (m_workerThread && !m_workerThread->isRunning()) {
            qDebug() << "Worker not running but flag active. Resetting m_bSearchActive.";
            m_bSearchActive = false;
            startSearch();
        }

        //qDebug() << "handleTextChange() m_bSearchActive==true -> setting m_bAbortRequested and m_bRestartPendingSearch so workers will startSearch() when ready.";
    } else {
        //qDebug() << "handleTextChange() m_bSearchActive==false -> leaving for startSearch()";
        startSearch();
    }
}

void MainWindow::startSearch() {
    QString InputBox1Text = m_LineEdit1->text();
    QString InputBox2Text = m_LineEdit2->text();

    //qDebug() << "startSearch() entrypoint";

    if (InputBox1Text.trimmed().isEmpty() && InputBox2Text.trimmed().isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_tableWidget->hide();

        m_mainLayout->activate();
        this->adjustSize();
        this->resize(this->width(), this->layout()->minimumSize().height());

        m_bSearchActive = false;
        return;
    }

    //qDebug() << "startSearch() starting search";
    m_bSearchActive = true;
    m_bAbortRequested = false;
    m_bRestartPendingSearch = false;

    m_BenchmarkTimer.start();

    m_bWorkerFinished = false;
    m_isProcessingPending = false;
    m_WorkerSearchInterrupted = false;

    m_tableWidget->setUpdatesEnabled(false);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);  // Important!! Calculations of header don't get stopped by "m_tableWidget->setUpdatesEnabled(false)"
    m_tableWidget->setRowCount(0);
    m_tableWidget->setSortingEnabled(false);
    m_tableWidget->blockSignals(true);  // block "itemChanged" signals

    m_workerThread = new QThread;
    SearchWorker* worker = new SearchWorker(m_settings.searchFolders, InputBox1Text, m_settings.recentOpenList);
    worker->moveToThread(m_workerThread);

    // Verbindungen
    connect(m_workerThread, &QThread::started, worker, &SearchWorker::process);
    connect(worker, &SearchWorker::filesFoundBatch, this, &MainWindow::onWorkerSentBatch);
    connect(worker, &SearchWorker::searchStats, this, &MainWindow::onWorkerFinished);
    connect(worker, &SearchWorker::finished, m_workerThread, &QThread::quit);                   // Stop thread
    connect(worker, &SearchWorker::finished, worker, &SearchWorker::deleteLater);               // Clean up object
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);         // Clean up thread
    connect(m_workerThread, &QObject::destroyed, this, [this]() { m_workerThread = nullptr; });

    connect(this, &MainWindow::abortSearchWorkerRequested, worker, &SearchWorker::abort, Qt::DirectConnection);   // React to "Escape" key press
    // WICHTIG: Wenn der Thread gelöscht wird, setzen wir unseren Pointer auf nullptr

    m_workerThread->start();
}

void MainWindow::onWorkerSentBatch(const QList<SearchResult> &batch) {
    m_pendingBatches.enqueue(batch);

    if (m_isProcessingPending) return;

    m_isProcessingPending = true;
    processNextBatch();
}

void MainWindow::processNextBatch() {
    if (m_pendingBatches.isEmpty()) {
        m_isProcessingPending = false;

        if (m_bWorkerFinished) {
            if (m_bRestartPendingSearch) {
                //qDebug() << "processNextBatch() m_bWorkerFinished -> leaving for startSearch()";
                startSearch();
            } else {
                //qDebug() << "processNextBatch() m_bWorkerFinished -> leaving for finalizeUI()";
                finalizeUI();
            }
        }
        return;
    }

    if (m_bAbortRequested) {
        m_pendingBatches.clear();
        m_isProcessingPending = false;

        if (m_bWorkerFinished) {
            if (m_bRestartPendingSearch) {
                //qDebug() << "processNextBatch() m_bAbortRequested -> leaving for startSearch()";
                startSearch();
            } else {
                //qDebug() << "processNextBatch() m_bAbortRequested -> leaving for finalizeUI()";
                finalizeUI();
            }
        }
        return;
    }

    // Einen Batch verarbeiten
    QList<SearchResult> currentBatch = m_pendingBatches.dequeue();
    int currentRows = m_tableWidget->rowCount();
    m_tableWidget->setRowCount(currentRows + currentBatch.size());

    for (int i = 0; i < currentBatch.size(); ++i) {
        addFileToTable(currentBatch.at(i).fileInfo, currentRows + i, currentBatch.at(i).nameMatchQuality, currentBatch.at(i).displayName);
    }

    // Using an instant timer gives the app time to process messages in the interim
    QTimer::singleShot(0, this, &MainWindow::processNextBatch);
}

void MainWindow::onWorkerFinished(bool bSearchInterrupted) {
    m_WorkerSearchInterrupted = bSearchInterrupted;
    m_bWorkerFinished = true;

    //qDebug() << "onWorkerFinished() m_isProcessingPending=" << m_isProcessingPending << "m_bRestartPendingSearch=" << m_bRestartPendingSearch;
    // Nur wenn gerade KEIN Batch mehr verarbeitet wird,
    // müssen wir hier den Abschluss triggern.
    if (!m_isProcessingPending) {
        if (m_bRestartPendingSearch) {
            //qDebug() << "onWorkerFinished() leaving for startSearch()";
            startSearch();
        } else {
            //qDebug() << "onWorkerFinished() leaving for finalizeUI()";
            finalizeUI();
        }
    }
}

void MainWindow::finalizeUI() {
    qDebug() << "finalizeUI() entry point. m_BenchmarkTimer:" << m_BenchmarkTimer.elapsed() << " ms elapsed since start of search.  m_WorkerSearchInterrupted =" << m_WorkerSearchInterrupted << "  m_bAbortRequested = " << m_bAbortRequested;

    if (m_tableWidget->rowCount() == 0 || m_WorkerSearchInterrupted == true || m_bAbortRequested) {
        m_tableWidget->setRowCount(0);
        if (!m_tableWidget->isHidden()) {
            m_tableWidget->hide();
            m_mainLayout->activate();
            this->adjustSize();
            this->resize(this->width(), this->layout()->minimumSize().height());
        }
        m_bSearchActive = false;
        return;
    }

    if (m_tableWidget->isHidden()) {
        m_tableWidget->show();
        m_mainLayout->activate();
        this->adjustSize();
        this->resize(this->width(), this->layout()->minimumSize().height());
    }

    int tableLines = (m_tableWidget->viewport()->height() / m_tableLineHeight) - (m_bHeaderVisible == true ? 2 : 0) ;
    if (m_tableWidget->rowCount() > tableLines)
        m_tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    else
        m_tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_tableWidget->blockSignals(false);
    m_tableWidget->sortByColumn(eColQuality, Qt::AscendingOrder);
    m_tableWidget->setSortingEnabled(true);
    updateColumns();
    m_tableWidget->setUpdatesEnabled(true);

    if (m_tableWidget->rowCount() > 0)
        m_tableWidget->setCurrentCell(0, 0);

    m_bAbortRequested = false;
    m_bSearchActive = false;

    qDebug() << "finalizeUI() exit point. m_BenchmarkTimer:" << m_BenchmarkTimer.elapsed() << " ms elapsed since start of search. Items:" << m_tableWidget->rowCount();
    m_timerUpdateIcons->start(25);
}

void MainWindow::onItemChanged(QTableWidgetItem *item) {
    if (item->column() != eColName) return;

    QString oldPath = item->data(Qt::UserRole).toString();
    if (oldPath.isEmpty()) return;

    if (oldPath.endsWith(".desktop", Qt::CaseInsensitive)) {
        return;
    }

    QString originalInput = item->text();
    QString cleanedName = cleanFileName(originalInput);

    QFileInfo oldInfo(oldPath);
    QString newPath = oldInfo.absolutePath() + "/" + cleanedName;

    if (oldPath != newPath) {
        if (QFile::rename(oldPath, newPath)) {
            QSignalBlocker blocker(item->tableWidget());
            item->setText(cleanedName);
            item->setData(Qt::UserRole, newPath); // Pfad im UserRole Bereich des Items aktualisieren
        } else {
            QMessageBox::critical(this, "Fehler", "Umbenennen fehlgeschlagen.");
            QSignalBlocker blocker(item->tableWidget());
            item->setText(oldInfo.fileName()); // Text zurücksetzen
        }
    } else if (cleanedName != originalInput) {
        QSignalBlocker blocker(item->tableWidget());
        item->setText(cleanedName);
    }
}

void MainWindow::onShowContextMenu(const QPoint &pos) {
    QTableWidgetItem *item = m_tableWidget->itemAt(pos);
    if (!item) return;

    int row = item->row();
    QString filePath = m_tableWidget->item(row, eColName)->data(Qt::UserRole).toString();
    QFileInfo fileInfo(filePath);
    QString fileExt = fileInfo.suffix().toLower();

    QMenu mainMenu(this);

    mainMenu.addAction(m_actionListViewOpenFiles);
    mainMenu.setDefaultAction(m_actionListViewOpenFiles);

    if (m_settings.audioExts.contains(fileExt) || m_settings.imageExts.contains(fileExt) || m_settings.textExts.contains(fileExt) || m_settings.videoExts.contains(fileExt)) {
        mainMenu.addAction(m_actionListViewEditFiles);
    }


#ifdef Q_OS_WIN
    mainMenu.addSeparator(); //-----------------------------------------

    QDir sendToDir(getSendToPath());
    if (sendToDir.exists()) {
        QMenu *sendToMenu = mainMenu.addMenu(tr("Send to"));
        QFileInfoList shortcuts = sendToDir.entryInfoList({"*.lnk"}, QDir::Files);
        for (const QFileInfo &shortcutInfo : std::as_const(shortcuts)) {
            QString displayName = shortcutInfo.completeBaseName();

            QIcon cleanIcon;
            QString targetPath = shortcutInfo.symLinkTarget();
            if (!targetPath.isEmpty() && QFileInfo::exists(targetPath)) {
                cleanIcon = m_iconProvider.icon(QFileInfo(targetPath));
            } else {
                cleanIcon = m_iconProvider.icon(shortcutInfo);
            }

            QPixmap pix = cleanIcon.pixmap(16, 16);

            QAction *sendAction = sendToMenu->addAction(QIcon(pix), displayName);
            sendAction->setData(shortcutInfo.absoluteFilePath());	// store lnk path inside sendAction object

            connect(sendAction, &QAction::triggered, this, [this, shortcutInfo]() {
                QStringList pathList = getTablePathList();
                if (pathList.isEmpty()) return;

                QString targetPath = shortcutInfo.symLinkTarget();

                if (!targetPath.isEmpty() && QFileInfo::exists(targetPath)) {
                    QStringList args;
                    for (const QString &p : std::as_const(pathList)) {
                        args << QDir::toNativeSeparators(p);
                    }

                    QProcess::startDetached(targetPath, args);
                }
                else {
                    QString allParams;
                    for (const QString &p : std::as_const(pathList)) {
                        if (!allParams.isEmpty()) allParams += " ";
                        allParams += "\"" + QDir::toNativeSeparators(p) + "\"";
                    }

                    ShellExecuteW(nullptr, L"open",
                                  reinterpret_cast<const wchar_t*>(shortcutInfo.absoluteFilePath().utf16()),
                                  reinterpret_cast<const wchar_t*>(allParams.utf16()),
                                  nullptr, SW_SHOWNORMAL);
                }
            });
        }
    }
#else
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    QString mimeName = mime.name();

    QStringList appIds = m_mimeCache.value(mime.name());
    if (appIds.isEmpty()) {
        for (const QString &parent : mime.allAncestors()) {
            appIds = m_mimeCache.value(parent);
            if (!appIds.isEmpty()) break;
        }
    }

    if (!appIds.isEmpty()) {
        mainMenu.addSeparator(); //-----------------------------------------

        QMenu *openWithMenu = mainMenu.addMenu(tr("Open with"));
        if (m_settings.showIconsInMenu == true) {
            openWithMenu->setIcon(QIcon::fromTheme("system-run"));
        }
        for (const QString &id : std::as_const(appIds)) {
            DesktopEntry info = getDesktopEntryById(id);
            if (info.isValid) {
                QAction *action = openWithMenu->addAction(QIcon::fromTheme(info.icon), info.name);
                connect(action, &QAction::triggered, [info, filePath, this]() {
                    openFileListWithHandler(info.id, getTablePathList());
                });
            }
        }
    }
#endif

    mainMenu.addSeparator(); //-----------------------------------------
    mainMenu.addAction(m_actionListViewBrowseToFile);
    mainMenu.addAction(m_actionListViewCopyPaths);
    mainMenu.addAction(m_actionListViewCutFiles);
    mainMenu.addAction(m_actionListViewCopyFiles);
    mainMenu.addAction(m_actionListViewDeleteFiles);
    mainMenu.addAction(m_actionListViewRenameFiles);
    mainMenu.addSeparator(); //-----------------------------------------
    mainMenu.addAction(m_actionListViewFileProperties);

    mainMenu.exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------------------------
// Actions

QStringList MainWindow::getTablePathList() {
    QStringList pathList;

    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();
    if (selectedItems.isEmpty()) return pathList;

    // Zeilenindizes sammeln (verhindert Dopplungen bei Mehrfachauswahl in einer Zeile)
    QSet<int> rowSet;
    for (auto *item : std::as_const(selectedItems)) {
        rowSet.insert(item->row());
    }

    for (int row : rowSet) {
        // Wir nehmen an, der Pfad liegt in Spalte 0 in der UserRole
        QString path = m_tableWidget->item(row, eColName)->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            pathList << path;
        }
    }

    return pathList;
}

void MainWindow::onListViewItemDoubleClicked(QTableWidgetItem *item) {
    action_ListViewOpenFiles();
}

void MainWindow::action_ListViewOpenFiles() {
    //QStringList pathList = getTablePathList();
    QTableWidgetItem *item = m_tableWidget->currentItem();
    if (!item) return;

    QString path = m_tableWidget->item(item->row(), eColName)->data(Qt::UserRole).toString();
    QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        return;
    }

    RecentOpenList_Add(path);

    QString fileExt = fileInfo.suffix().toLower();

    if (fileExt == "desktop") {
        launchDesktopFile(getDesktopEntry(fileInfo));
#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    } else if (fileInfo.isExecutable() && !fileInfo.isDir() && !m_settings.audioExts.contains(fileExt) && !m_settings.imageExts.contains(fileExt) && !m_settings.videoExts.contains(fileExt)) {
        // Workaround on linux where executible files are not neccessarily executed when opened via QDesktopServices::openUrl().
        QProcess::startDetached(path, QStringList(), fileInfo.absolutePath());
#endif
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    guiHideConditional();
}

void MainWindow::action_ListViewEditFiles() {

    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    // Zeilenindizes sammeln (verhindert Dopplungen bei Mehrfachauswahl in einer Zeile)
    // (Die Funktion selectedItems() gibt stumpf jedes einzelne Item (jede Zelle) zurück, das gerade farblich hinterlegt ist.)
    QSet<int> rowSet;
    for (auto *item : std::as_const(selectedItems)) {
        rowSet.insert(item->row());
    }

    QStringList pathListAudio;
    QStringList pathListImage;
    QStringList pathListText;
    QStringList pathListVideo;

    for (int row : rowSet) {
        // Der Pfad liegt in Spalte 0 in der UserRole
        QString fullPath = m_tableWidget->item(row, eColName)->data(Qt::UserRole).toString();
        if (fullPath.isEmpty()) {
            continue;
        }

        QFileInfo fileInfo(fullPath);
        QString fileExt = fileInfo.suffix().toLower();
        if (fileExt.isEmpty()) {
            continue;
        }

        if (m_settings.audioExts.contains(fileExt)) {
            pathListAudio << fullPath;
        } else if (m_settings.imageExts.contains(fileExt)) {
            pathListImage << fullPath;
        } else if (m_settings.textExts.contains(fileExt)) {
            pathListText << fullPath;
        } else if (m_settings.videoExts.contains(fileExt)) {
            pathListVideo << fullPath;
        }
    }

    if (!pathListAudio.isEmpty()) {
        openFileListWithHandler(m_settings.audioEditor, pathListAudio);
    }

    if (!pathListImage.isEmpty()) {
        openFileListWithHandler(m_settings.imageEditor, pathListImage);
    }

    if (!pathListText.isEmpty()) {
        openFileListWithHandler(m_settings.textEditor, pathListText);
    }

    if (!pathListVideo.isEmpty()) {
        openFileListWithHandler(m_settings.videoEditor, pathListVideo);
    }

    guiHideConditional();
}



void MainWindow::action_ListViewCopyPaths() {
    QStringList pathList = getTablePathList();

    QStringList pathListNative;
    for (const QString &path : std::as_const(pathList)) {
        pathListNative << QDir::toNativeSeparators(path);
    }

#ifdef Q_OS_WIN
    QString sClipboardList = pathListNative.join("\r\n");
#else
    QString sClipboardList = pathListNative.join("\n");
#endif

    QGuiApplication::clipboard()->setText(sClipboardList);
}

void MainWindow::action_ListViewDeleteFiles(bool bRecycleOnly) {
    QList<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    //----------------------------------------------------------------------------------------------
    // 1. Collect unique rows
    QSet<int> rowSet;
    for (auto *item : std::as_const(selected)) {
        rowSet.insert(item->row());
    }

    QString sSingleFilePath = "";
    if (rowSet.size() == 1) {
        int firstElement = rowSet.values().at(0);
        sSingleFilePath = m_tableWidget->item(firstElement, eColName)->data(Qt::UserRole).toString();
        if (sSingleFilePath.isEmpty()) {
            return;
        }

        if (!QFile::exists(sSingleFilePath)) {
            return;
        }
    }

    //----------------------------------------------------------------------------------------------
    // 2. Prepare dialog

    QString sTitle;
    QString sText;
    QString sWarning;
    QMessageBox::Icon iIcon;

    if (bRecycleOnly) {
        iIcon = QMessageBox::Question;
        sWarning = "";
        if (rowSet.size() == 1) {
            sTitle = tr("Delete File");
            sText = tr("Do you really want to move this file into the recycle bin?");
        } else {
            sTitle = tr("Delete multiple elements");
            sText = QString(tr("Do you really want to move these %1 files into the recylce bin?")).arg(rowSet.size());
        }
    } else {
        iIcon = QMessageBox::Warning;
        sWarning = "<p style='color: red;'><i>" + tr("This process cannot be undone.") + "</i></p>";
        if (rowSet.size() == 1) {
            sTitle = tr("Delete File");
            sText = tr("Are you sure you want to delete this file permanently?");
        } else {
            sTitle = tr("Delete multiple elements");
            sText = QString(tr("Are you sure you want to delete these %1 files permanently?")).arg(rowSet.size());
        }
    }

    //----------------------------------------------------------------------------------------------
    // 3. Show dialog

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(sTitle);
    msgBox.setIcon(iIcon);

    if (rowSet.size() == 1) {
        QFileInfo fileInfo(sSingleFilePath);
        QString fileName = fileInfo.fileName();
        QString size = formatAdaptiveSize(fileInfo.size());
        QString lastModified = fileInfo.lastModified().toString("yyyy-MM-dd  HH:mm:ss");

        QFileIconProvider provider;
        QIcon icon = provider.icon(fileInfo);
        QPixmap pix = icon.pixmap(QSize(48, 48));

/*
//        qDebug() << "Icon loaded via pixmap() has size " << pix.width() << "x" << pix.height() << "available48: " << available48.width() << "x" << available48.height() << "available64: " << available64.width() << "x" << available64.height() << "available256: " << available256.width() << "x" << available256.height();

        QList<QSize> sizes = icon.availableSizes();
        qDebug() << "--- Icon Analyse für:" << fileInfo.fileName() << "---";
        if (sizes.isEmpty()) {
            qDebug() << "Keine festen Größen hinterlegt (dynamisches/skalierbares Icon).";
        } else {
            for (int i = 0; i < sizes.size(); ++i) {
                qDebug() << QString("Auflösung [%1]: %2 x %3")
                                .arg(i)
                                .arg(sizes[i].width())
                                .arg(sizes[i].height());
            }
        }
        qDebug() << "------------------------------------------";

        if (pix.width() != 48 || pix.height() != 48) {
            pix = pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
*/
        QByteArray ba;
        QBuffer bu(&ba);
        pix.save(&bu, "PNG");
        QString imgBase64 = ba.toBase64();

        msgBox.setText(QString("<h3>%1</h3>").arg(sText));
        msgBox.setInformativeText(QString(tr("<table width='100%' cellspacing='0' cellpadding='0'><tr><td rowspan=4 width='48' valign='top' style='padding-right: 10px;'><img src='data:image/png;base64,%1'></td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;' width='1%'>Name:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%2</td></tr><tr><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>Size:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%3</td></tr><tr><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>Date:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%4</td></tr><tr><td colspan=2 style='padding-top: 8px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%5</td></tr></table>")).arg(imgBase64, fileName, size, lastModified, sWarning));
    } else {
        //msgBox.setText(sText);
        msgBox.setText(QString("<h3>%1</h3>").arg(sText));
        msgBox.setInformativeText(sWarning);
    }

    QPushButton *deleteButton = msgBox.addButton(tr("Delete"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(deleteButton);
    deleteButton->setStyleSheet("QPushButton { font-weight: bold; min-width: 80px; }");

    msgBox.exec();

    if (msgBox.clickedButton() != deleteButton) {
        return;
    }

    //----------------------------------------------------------------------------------------------
    // 4. Delete in reverse order. (Important! Wenn deleting row 2, row 3 will become row 2!)

    QList<int> sortedRows = rowSet.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (int row : std::as_const(sortedRows)) {
        QTableWidgetItem *nameItem = m_tableWidget->item(row, eColName);
        if (!nameItem) continue;

        QString path = nameItem->data(Qt::UserRole).toString();

        if (bRecycleOnly) {
            if (QFile::moveToTrash(path)) {
                m_tableWidget->removeRow(row);
            } else {
                // Fehlerbehandlung
            }
        } else {
            if (QFile::remove(path)) {
                m_tableWidget->removeRow(row);
            } else {
                // Fehlerbehandlung
            }
        }
    }
}

void MainWindow::action_ListViewCutFiles() {
    removeCutMarkers();

    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();

    // Zeilenindizes sammeln (verhindert Dopplungen bei Mehrfachauswahl in einer Zeile)
    QSet<int> rowSet;
    for (const auto *item : std::as_const(selectedItems)) {
        rowSet.insert(item->row());
    }

    for (int row : std::as_const(rowSet)) {
        for (int col = 0; col < m_tableWidget->columnCount(); ++col) {
            QTableWidgetItem* item = m_tableWidget->item(row, col);
            if (item) {
                item->setData(Qt::UserRole + 5, true);
            }
        }
    }

    m_rowsWithCutMarkers = rowSet;

    setupClipboardForCut(rowSet);   // Todo: Better first try to change clipboard, and only on success ghost out cut items
}

void MainWindow::setupClipboardForCut(const QSet<int> &rowSet) {
    auto *mimeData = new QMimeData();
    QList<QUrl> urls;

    for (int row : rowSet) {
        // Wir nehmen an, der Pfad liegt in Spalte 0 in der UserRole
        QString path = m_tableWidget->item(row, eColName)->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            urls << QUrl::fromLocalFile(path);
        }
    }

    mimeData->setUrls(urls);

    // "Drop Effect" (Copy oder Move) for Windows
    QByteArray buffer;
    buffer.append(static_cast<char>(Qt::MoveAction));
    buffer.append('\0'); buffer.append('\0'); buffer.append('\0');
    mimeData->setData("Preferred DropEffect", buffer);

    // Particular to Linux (GNOME/Dolphin/etc.)
    // Format: "cut" oder "copy", dann Zeilenumbruch, dann alle URLs (ebenfalls per \n getrennt)
    QByteArray gnomeFormat = "cut";
    for (const QUrl &url : std::as_const(urls)) {
        gnomeFormat.append("\n");
        gnomeFormat.append(url.toEncoded());
    }
    mimeData->setData("x-special/gnome-copied-files", gnomeFormat);

    m_currentClipboardToken = QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    mimeData->setData("application/x-mklauncher-token", m_currentClipboardToken);

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void MainWindow::onClipboardChanged() {
    // We can not just always remove the markers, since we get notified of our own changes to the clipboard as well.
    // We probably should set some flag with a random value when we cut items and check if this flag is still there.
    const QMimeData* mimeData = QApplication::clipboard()->mimeData();

    if (!mimeData->hasFormat("application/x-mklauncher-token") || mimeData->data("application/x-mklauncher-token") != m_currentClipboardToken) {
        removeCutMarkers();
        m_currentClipboardToken.clear();
    }
}

void MainWindow::removeCutMarkers() {
    if (!m_rowsWithCutMarkers.isEmpty()) {
        m_tableWidget->setUpdatesEnabled(false);

        for (int r : std::as_const(m_rowsWithCutMarkers)) {
            for (int c = 0; c < m_tableWidget->columnCount(); ++c) {
                if (QTableWidgetItem *item = m_tableWidget->item(r, c)) {
                    item->setData(Qt::UserRole + 5, false);
                }
            }
        }

        m_rowsWithCutMarkers.clear();
        m_tableWidget->setUpdatesEnabled(true);
    }
}

void MainWindow::action_ListViewCopyFiles() {
    removeCutMarkers();

    QStringList pathList = getTablePathList();

    auto *mimeData = new QMimeData();
    QList<QUrl> urls;

    for (const QString &path : std::as_const(pathList)) {
        urls << QUrl::fromLocalFile(path);
    }
    mimeData->setUrls(urls);

    // 2. Den "Drop Effect" (Copy oder Move) setzen
    QByteArray buffer;
    buffer.append(static_cast<char>(Qt::CopyAction));
    buffer.append('\0'); buffer.append('\0'); buffer.append('\0');
    mimeData->setData("Preferred DropEffect", buffer);

    // 3. Speziell für Linux (GNOME/Dolphin/etc.)
    // Format: "cut" oder "copy", dann Zeilenumbruch, dann alle URLs (ebenfalls per \n getrennt)
    QByteArray gnomeFormat = "copy";
    for (const QUrl &url : std::as_const(urls)) {
        gnomeFormat.append("\n");
        gnomeFormat.append(url.toEncoded());
    }
    mimeData->setData("x-special/gnome-copied-files", gnomeFormat);

    m_currentClipboardToken = QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    mimeData->setData("application/x-mklauncher-token", m_currentClipboardToken);

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void MainWindow::action_ListViewBrowseToFile() {
    QTableWidgetItem *item = m_tableWidget->currentItem();
    if (!item) return;

    QString path = m_tableWidget->item(item->row(), eColName)->data(Qt::UserRole).toString();
    browseToFile(path);
    guiHideConditional();
}


void MainWindow::action_ListViewRenameFiles() {
    QTableWidgetItem *item = m_tableWidget->currentItem();
    if (!item) return;

    int row = item->row();
    m_tableWidget->editItem(m_tableWidget->item(row, eColName));
}

void MainWindow::action_ListViewFileProperties() {
    QStringList pathList = getTablePathList();

    if (pathList.isEmpty()) {
        return;
    }

    auto *dialog = new FilePropertiesDialog(pathList);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::action_EditSettingsFile() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_settings.getSettingsPath()));
}

void MainWindow::onTimedUpdateIcons() {
    if (m_bSearchActive.load() || (m_tableWidget->rowCount() == 0)) {
        return;
    }

    int firstVisible = m_tableWidget->rowAt(0);
    int lastVisible = m_tableWidget->rowAt(m_tableWidget->viewport()->height() - 1);    // Substract 1 pixel to make sure we're in the viewport
    if (lastVisible == -1) {
        lastVisible = m_tableWidget->rowCount() - 1;
    }
    if ((firstVisible == -1) || (lastVisible == -1)) {
        return;
    }

    for (int i = firstVisible; i <= lastVisible; ++i) {
        QTableWidgetItem *nameItem = m_tableWidget->item(i, eColName);
        if (nameItem) {
            QString fullPath = nameItem->data(Qt::UserRole).toString();
            if (!fullPath.isEmpty()) {
                if (nameItem->data(Qt::UserRole + 1).toBool() == false) {
                    QFileInfo fileInfo(fullPath);
#ifdef Q_OS_WIN
                    bool needsTrueIcon = fileInfo.isDir() ||
                                         fullPath.endsWith(".exe", Qt::CaseInsensitive) ||
                                         fullPath.endsWith(".ico", Qt::CaseInsensitive) ||
                                         fullPath.endsWith(".lnk", Qt::CaseInsensitive) ||
                                         fullPath.endsWith(".msi", Qt::CaseInsensitive) ||
                                         fullPath.endsWith(".cur", Qt::CaseInsensitive) ||
                                         fullPath.endsWith(".ani", Qt::CaseInsensitive);
#else
                    bool needsTrueIcon = fileInfo.isDir() ||
                                         fileInfo.isExecutable() ||
                                         fullPath.endsWith(".desktop", Qt::CaseInsensitive);
#endif
                    if (needsTrueIcon) {
                        QIcon trueIcon = m_iconProvider.icon(fileInfo);

                        m_pathIconCache.insert(fullPath, trueIcon);

                        nameItem->setIcon(trueIcon);
                    }

                    nameItem->setData(Qt::UserRole + 1, true);  // set true so we don't update this item's icon again.
                }
            }
        }
    }
}

//################################################################################################
// Misc

void MainWindow::addFileToTable(const QFileInfo &fileInfo, int iRow, int nameMatchQuality, const QString &displayName) {

    // Icon & Name
    QString visibleName;
    if (!displayName.isEmpty())
        visibleName = displayName;
    else
        visibleName = fileInfo.fileName();

    QTableWidgetItem *nameItem = new QTableWidgetItem(visibleName);

    QHash<QString, QIcon>::iterator it;
    it = m_pathIconCache.find(fileInfo.absoluteFilePath());
    if (it != m_pathIconCache.end()) {
        nameItem->setData(Qt::UserRole + 1, true); // Mark as done, so the icon update timer ignores this one
    } else {
        if (fileInfo.isDir()) {
            // The '/' is a forbidden character for file names both on linux and windows,
            // so we can use it as an otherwise impossible suffix marker

            it = m_iconCache.find("//dir//");
            if (it == m_iconCache.end()) {
                it = m_iconCache.insert("//dir//", m_iconProvider.icon(QFileIconProvider::Folder));
            }
        } else {
            QString suffix = fileInfo.suffix().toLower();

            it = m_iconCache.find(suffix);
            if (it == m_iconCache.end()) {
                QFileInfo dummyInfo("any_filename." + suffix);
                it = m_iconCache.insert(suffix, m_iconProvider.icon(dummyInfo));
            }
        }
    }

    nameItem->setIcon(it.value());
    nameItem->setData(Qt::UserRole, fileInfo.absoluteFilePath());
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    m_tableWidget->setItem(iRow, eColName, nameItem);

    // Subfolder
    QTableWidgetItem *pathItem = new QTableWidgetItem();
    QString pathOnly = fileInfo.absolutePath();
    pathItem->setData(Qt::DisplayRole, QDir::toNativeSeparators(pathOnly));
    pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_tableWidget->setItem(iRow, eColPath, pathItem);

    // Size (right aligned)
    quint64 sizeInBytes = fileInfo.size();
    SizeTableItem *sizeItem = new SizeTableItem(formatAdaptiveSize(sizeInBytes));     // Using sublass "sizeItem" in place of "QTableWidgetItem"
    sizeItem->setData(Qt::UserRole, sizeInBytes);
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_tableWidget->setItem(iRow, eColSize, sizeItem);

    // Date
    QTableWidgetItem *dateItem = new QTableWidgetItem(fileInfo.lastModified().toString("yyyy-MM-dd  HH:mm:ss"));
    dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_tableWidget->setItem(iRow, eColDate, dateItem);

    // Type or File Extension
    QTableWidgetItem *typeItem = new QTableWidgetItem(fileInfo.suffix());
    //typeItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    typeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_tableWidget->setItem(iRow, eColType, typeItem);

    // Match quality
    QTableWidgetItem *qualityItem = new QTableWidgetItem();
    if (nameMatchQuality != -1)
        qualityItem->setData(Qt::DisplayRole, nameMatchQuality);
    qualityItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    qualityItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_tableWidget->setItem(iRow, eColQuality, qualityItem);
}

void MainWindow::launchAction() {
    QString input1 = m_LineEdit1->text().trimmed();
    QString input2 = m_LineEdit2->text().trimmed();

    if (input1.isEmpty()) return;

    RecentInputList_Add(input1);

    // --- FALL 1: URL ERKENNUNG (http, ftp, www...) ---
    static const QRegularExpression urlRegex(R"(^((ht|f)tp(s?)\:\/\/).*|^www\.)", QRegularExpression::CaseInsensitiveOption);
    if (urlRegex.match(input1).hasMatch()) {
        if (!input1.contains("://")) {
            input1 = "http://" + input1;
        }

        if (QDesktopServices::openUrl(QUrl(input1))) {
            guiHideConditional();
            return;
        }
    }

    if (input2.isEmpty()) {
        // --- FALL 2: E-MAIL ERKENNUNG ---
        static const QRegularExpression emailRegex(R"(^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?)*$)");
        if (emailRegex.match(input1).hasMatch()) {
            if (QDesktopServices::openUrl(QUrl("mailto:" + input1))) {
                qDebug() << "(QDesktopServices::openUrl(QUrl(mailto:" + input1 + ")))  sollte ausgelöst haben";
                guiHideConditional();
                return;
            } else {
                qDebug() << "(QDesktopServices::openUrl(QUrl(mailto:" + input1 + ")))  failed";
            }
        }

#ifdef Q_OS_WIN

#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
        QString possiblePath = input1;

        if (possiblePath.startsWith("~/")) {
            possiblePath = QDir::homePath() + possiblePath.mid(1);
        } else if (possiblePath.startsWith("$HOME/")) {
            possiblePath = QDir::homePath() + possiblePath.mid(5);
        }

        possiblePath = QDir::cleanPath(possiblePath);

        QFileInfo checkFile(possiblePath);
        if (checkFile.exists()) {
            browseToFile(possiblePath);
            guiHideConditional();
            return;
        }
#endif
    } else {
        // --- Case 3: search modifiers (m_LineEdit2 not empty) ---

        // allow only letters
        QString cleanOp;
        for(auto c : input2) { if(c.isLetter()) cleanOp += c; }

        if (cleanOp.isEmpty()) return;
        if (cleanOp != input2) {
            m_LineEdit2->setText(cleanOp);
            input2 = cleanOp;
        }

        QString searchText = QUrl::toPercentEncoding(input1);
        QString url;

             if (input2 == "a")   url = "http://www.amazon.de/gp/search/?keywords=" + searchText;
        else if (input2 == "auk") url = "http://www.amazon.co.uk/gp/search/?keywords=" + searchText;
        else if (input2 == "aus") url = "http://www.amazon.com/gp/search/?keywords=" + searchText;
        else if (input2 == "d")   url = "https://duckduckgo.com/?q=" + searchText + "&kp=-2";
        else if (input2 == "g")   url = "http://www.google.com/search?q=" + searchText + "&udm=14&tbs=li:1";
        else if (input2 == "gd")  url = "http://www.google.de/search?q=" + searchText + "&udm=14&tbs=li:1";
        else if (input2 == "i")   url = "http://www.imdb.com/find?q=" + searchText;
        else if (input2 == "m")   url = "http://maps.google.de?q=" + searchText;
        else if (input2 == "u")   url = "http://www.urbandictionary.com/define.php?term=" + searchText;
        else if (input2 == "w")   url = "http://en.wikipedia.org/wiki/Special:Search?search=" + searchText;
        else if (input2 == "wd")  url = "http://de.wikipedia.org/wiki/Special:Search?search=" + searchText;
        else if (input2 == "y")   url = "http://www.youtube.com/results?search_query=" + searchText;

        if (!url.isEmpty()) {
            qDebug() << url;
            if (QDesktopServices::openUrl(QUrl(url))) {
                guiHideConditional();
                return;
            }
        }
        return;
    }

    // default open
    action_ListViewOpenFiles();
}


void MainWindow::RecentOpenList_Add(const QString &filePath) {
    // If already in list, delete and prepend again in next step
    m_settings.recentOpenList.removeAll(filePath);

    // Insert at front (index 0)
    m_settings.recentOpenList.prepend(filePath);

    while (m_settings.recentOpenList.size() > 64) {
        m_settings.recentOpenList.removeLast();
    }
}


void MainWindow::RecentInputList_Add(QString searchTerm) {
    // Pipe-Zeichen entfernen (wie im Original)
    searchTerm.remove('|');
    if (searchTerm.trimmed().isEmpty()) return;

    // If already in list, delete and prepend again in next step
    m_settings.recentInputList.removeAll(searchTerm);

    // Insert at front (index 0)
    m_settings.recentInputList.prepend(searchTerm);

    while (m_settings.recentInputList.size() > 32) {
        m_settings.recentInputList.removeLast();
    }
}

QString MainWindow::RecentInputList_GetPrevious(const QString &currentText) {
    if (m_settings.recentInputList.isEmpty()) {
        return currentText;
    }

    int idx = m_settings.recentInputList.indexOf(currentText);

    // Wenn der aktuelle Text nicht in der Liste ist -> das Neueste zurückgeben
    if (idx == -1) {
        QString sFirst = m_settings.recentInputList.first();
        RecentInputList_Add(currentText);
        return sFirst;
    }

    // Wenn wir noch nicht am Ende der Liste sind -> das Nächste (ältere) holen
    if (idx + 1 < m_settings.recentInputList.size()) {
        return m_settings.recentInputList.at(idx + 1);
    }

    return currentText; // Wir sind am Ende der Geschichte angekommen
}

QString MainWindow::RecentInputList_GetNext(const QString &currentText) {
    int idx = m_settings.recentInputList.indexOf(currentText);

    // Wenn wir nicht in der Liste sind oder schon beim neuesten Eintrag (Index 0)
    if (idx <= 0) {
        return currentText;
    }

    // Einen Schritt nach vorne (Richtung Index 0)
    return m_settings.recentInputList.at(idx - 1);
}

void MainWindow::guiHideConditional() {
    if (!(windowState() & Qt::WindowMinimized)) {
        m_LineEdit2->clear();
        m_LineEdit1->setFocus();
        //this->showMinimized();
        QTimer::singleShot(50, this, [this]() {
            this->setWindowState(Qt::WindowMinimized);
        });
    }
}

#ifdef Q_OS_WIN
QString MainWindow::getSendToPath() {
    PWSTR path = nullptr;
    // FOLDERID_SendTo ist die offizielle GUID für diesen Ordner
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &path);
    if (SUCCEEDED(hr)) {
        QString result = QString::fromWCharArray(path);
        CoTaskMemFree(path); // Wichtig: Speicher freigeben
        return result;
    }
    return QString();
}
#endif

void MainWindow::loadMimeCache() {
    m_mimeCache.clear();
    m_mimeCache.reserve(500);

    QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    // Note to self: appDirs contains the path in the order from user specific to system defaults
    // We want to reverse this so we read the system default folders *first* and then overwrite with data from the user specific locations
    std::reverse(appDirs.begin(), appDirs.end());
    for (const QString &dirPath : std::as_const(appDirs)) {
        QString cachePath = QDir(dirPath).filePath("mimeinfo.cache");
        parseMimeInfoCache(cachePath);
    }

    parseMimeAppsList(QDir::homePath() + "/.config/mimeapps.list");
}

void MainWindow::parseMimeInfoCache(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        auto equalsPos = line.indexOf('=');
        if (equalsPos < 1) continue;  // -1 (kein '=') und 0 (leerer mime) überspringen

        QString mime = line.first(equalsPos).trimmed();
        QStringList newApps = line.sliced(equalsPos + 1).split(';', Qt::SkipEmptyParts);

        QStringList &currentApps = m_mimeCache[mime];
        for (const QString &app : std::as_const(newApps)) {
            QString trimmed = app.trimmed();
            if (!trimmed.isEmpty() && !currentApps.contains(trimmed)) {
                currentApps.append(trimmed);
            }
        }
    }
}

void MainWindow::parseMimeAppsList(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString currentGroup;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            currentGroup = line.mid(1, line.length() - 2);
            continue;
        }

        auto equalsPos = line.indexOf('=');
        if (equalsPos < 1) continue; // -1 (kein '=') und 0 (leerer mime) überspringen

        QString mime = line.first(equalsPos).trimmed();
        QStringList apps = line.sliced(equalsPos + 1).trimmed().split(';', Qt::SkipEmptyParts);

        if (currentGroup == "Added Associations" || currentGroup == "Default Applications") {
            QStringList &currentApps = m_mimeCache[mime];

            for (int i = apps.size() - 1; i >= 0; --i) {
                QString app = apps.at(i).trimmed();
                if (app.isEmpty()) continue;

                currentApps.removeAll(app);
                currentApps.prepend(app);
            }
        }
        else if (currentGroup == "Removed Associations") {
            QStringList &currentApps = m_mimeCache[mime];
            for (const QString &app : std::as_const(apps)) {
                currentApps.removeAll(app.trimmed());
            }
        }
    }
}


//######################################################################################
// Public Slots

void MainWindow::wasRestored() {
    qDebug() << "wasRestored()";
    //this->raise();
    //this->activateWindow();
    m_LineEdit1->setFocus();
    m_LineEdit1->selectAll();
}

//######################################################################################
// Protected Overrides

void MainWindow::closeEvent(QCloseEvent *event) {
    //m_settings.saveSettings();    // This would overwrite any settings the user might have changed in the INI since the process started
    m_settings.saveHistory();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateColumns();
}

// This happens only once, one creating the window
void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    updateColumns();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {

    if (obj == m_tableWidget->viewport() && event->type() == QEvent::Resize) {
        m_timerUpdateIcons->start(100);
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (obj == m_LineEdit1) {
            if (keyEvent->modifiers() == Qt::ControlModifier) {
                if (keyEvent->key() == Qt::Key_Y) {
                    QString s = RecentInputList_GetNext(m_LineEdit1->text());
                    m_LineEdit1->setText(s);

                    return true;
                } else if (keyEvent->key() == Qt::Key_Z) {
                    QString s = RecentInputList_GetPrevious(m_LineEdit1->text());
                    m_LineEdit1->setText(s);

                    return true;
                }
            }
        }

        if (obj == m_LineEdit2) {
            if (keyEvent->key() == Qt::Key_Backspace) {
                if (m_LineEdit2->text().isEmpty()) {
                    m_LineEdit1->setFocus();
                    m_LineEdit1->setCursorPosition(m_LineEdit1->text().length());
                    return true;
                }
            }
        }

        if (obj == m_tableWidget) {
            if (keyEvent->key() == Qt::Key_Home) {
                if ((keyEvent->modifiers() & Qt::ShiftModifier) == 0) {
                    if (m_tableWidget->rowCount() > 0) {
                        m_tableWidget->setCurrentCell(0, 0);
                        m_tableWidget->scrollToTop();
                        return true;
                    }
                }
            } else if (keyEvent->key() == Qt::Key_End) {
                if ((keyEvent->modifiers() & Qt::ShiftModifier) == 0) {
                    int lastRow = m_tableWidget->rowCount() - 1; if (lastRow >= 0) {
                        m_tableWidget->setCurrentCell(lastRow, 0);
                        m_tableWidget->scrollToBottom();
                        return true;
                    }
                }
            } else if (keyEvent->key() == Qt::Key_Delete) {
                if (keyEvent->modifiers() & Qt::ShiftModifier) {
                    action_ListViewDeleteFiles(false);
                    return true;
                }
            }
        }

        if (keyEvent->key() == Qt::Key_Escape) {
            if (m_bSearchActive) {
                m_bAbortRequested = true;
                emit abortSearchWorkerRequested();
            }

            if (m_tableWidget->hasFocus()) {
                // Empty Clipboard. But only if it's our own!
                const QMimeData* mimeData = QApplication::clipboard()->mimeData();
                if (mimeData->hasFormat("application/x-mklauncher-token") && mimeData->data("application/x-mklauncher-token") == m_currentClipboardToken) {
                    QApplication::clipboard()->clear();
                }
            }

            // Check if focus is actually on a widget inside m_tableWidget's viewport (the editor)
            if (m_tableWidget->viewport()->focusWidget()) {
                return false;
            }

            this->showMinimized();
            return true;
        } else if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
            if (!m_tableWidget->hasFocus())
                m_tableWidget->setFocus();

            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                int currentRow = m_tableWidget->currentRow();
                int rowCount = m_tableWidget->rowCount();
                int nextRow = (keyEvent->key() == Qt::Key_Up)
                                  ? ((currentRow <= 0) ? rowCount - 1 : currentRow - 1)
                                  : ((currentRow >= rowCount - 1) ? 0 : currentRow + 1);

                if (nextRow >= 0 && nextRow < rowCount) {
                    m_tableWidget->setCurrentCell(nextRow, 0);
                    return true;
                }
            }
        } else if (keyEvent->key() == Qt::Key_Tab) {
            if (m_tableWidget->hasFocus())
                m_LineEdit1->setFocus();
            else if (m_LineEdit1->hasFocus())
                m_LineEdit2->setFocus();
            else
                m_LineEdit1->setFocus();
            return true;
        } else if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            if (m_tableWidget->hasFocus() || m_LineEdit1->hasFocus() || m_LineEdit2->hasFocus()) {
                launchAction();
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

#ifdef Q_OS_WIN
// Note: To use this, you can do something akin to "PostMessage(hWnd, 0x807B, 0, 0)" with the hWnd of the window with the name "mkLauncher" and class "Qt6110QWindowIcon"
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_MY_LAUNCHER_RESTORE) {
        this->wasRestored();
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

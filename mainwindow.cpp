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

#include <algorithm> // Für std::reverse
#include <utility> // Für std::as_const

#ifdef Q_OS_WIN
#include <qt_windows.h>
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
    m_tableWidget->setItemDelegate(new tableStyledItemDelegate(m_cutFilePaths, this));
    m_tableWidget->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

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

#ifdef Q_OS_LINUX
    loadMimeCache();

    if (QDBusConnection::sessionBus().registerService("org.mkLauncher")) {
        QDBusConnection::sessionBus().registerObject("/Main", this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals | QDBusConnection::ExportScriptableProperties);
    }
#endif

    // --------------------------------------------------------------------
    // Shortcuts: Whole Window

    QShortcut *WindowShortcutN = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Comma), this);
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
    if (m_bSearchActive.load()) {
        m_bRestartPendingSearch.store(true);
        m_bAbortRequested.store(true);
        emit abortSearchWorkerRequested();

        if (m_workerThread && !m_workerThread->isRunning()) {
            qDebug() << "Worker not running but flag active. Resetting m_bSearchActive";
            m_bSearchActive.store(false);
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

        m_bSearchActive.store(false);
        return;
    }

    //qDebug() << "startSearch() starting search";
    m_bSearchActive.store(true);
    m_bAbortRequested.store(false);
    m_bRestartPendingSearch.store(false);

    m_BenchmarkTimer.start();

    m_bWorkerFinished.store(false);
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
    connect(worker, &SearchWorker::searchStats, this, &MainWindow::onWorkerFinished);   // 1. Store result info
    connect(worker, &SearchWorker::finished, m_workerThread, &QThread::quit);           // 2. Stop m_workerThread
    connect(worker, &SearchWorker::finished, worker, &SearchWorker::deleteLater);       // 3. Clean up object
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater); // 4. Clean up m_workerThread
    connect(m_workerThread, &QObject::destroyed, this, [this]() { m_workerThread = nullptr; });

    connect(this, &MainWindow::abortSearchWorkerRequested, worker, &SearchWorker::abort, Qt::DirectConnection);   // React to "Escape" key press

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

        if (m_bWorkerFinished.load()) {
            if (m_bRestartPendingSearch.load()) {
                //qDebug() << "processNextBatch() m_bWorkerFinished -> leaving for startSearch()";
                startSearch();
            } else {
                //qDebug() << "processNextBatch() m_bWorkerFinished -> leaving for finalizeUI()";
                finalizeUI();
            }
        }
        return;
    }

    if (m_bAbortRequested.load()) {
        m_pendingBatches.clear();
        m_isProcessingPending = false;

        if (m_bWorkerFinished.load()) {
            if (m_bRestartPendingSearch.load()) {
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
    m_bWorkerFinished.store(true);

    //qDebug() << "onWorkerFinished() m_isProcessingPending=" << m_isProcessingPending << "m_bRestartPendingSearch=" << m_bRestartPendingSearch.load();
    // Nur wenn gerade KEIN Batch mehr verarbeitet wird,
    // müssen wir hier den Abschluss triggern.
    if (!m_isProcessingPending) {
        if (m_bRestartPendingSearch.load()) {
            //qDebug() << "onWorkerFinished() leaving for startSearch()";
            startSearch();
        } else {
            //qDebug() << "onWorkerFinished() leaving for finalizeUI()";
            finalizeUI();
        }
    }
}

void MainWindow::finalizeUI() {
    qDebug() << "finalizeUI() entry point. m_BenchmarkTimer:" << m_BenchmarkTimer.elapsed() << " ms elapsed since start of search.  m_WorkerSearchInterrupted =" << m_WorkerSearchInterrupted << "  m_bAbortRequested = " << m_bAbortRequested.load();

    if (m_tableWidget->rowCount() == 0 || m_WorkerSearchInterrupted == true || m_bAbortRequested.load()) {
        m_tableWidget->setRowCount(0);
        if (!m_tableWidget->isHidden()) {
            m_tableWidget->hide();
            m_mainLayout->activate();
            this->adjustSize();
            this->resize(this->width(), this->layout()->minimumSize().height());
        }
        m_bSearchActive.store(false);
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

    m_bAbortRequested.store(false);
    m_bSearchActive.store(false);

    qDebug() << "finalizeUI() exit point. m_BenchmarkTimer:" << m_BenchmarkTimer.elapsed() << " ms elapsed since start of search. Items:" << m_tableWidget->rowCount();
    m_timerUpdateIcons->start(25);
}

void MainWindow::onItemChanged(QTableWidgetItem *nameItem) {
    if (nameItem->column() != eColName) return;

    QFileInfo oldInfo = nameItem->data(Qt::UserRole).value<QFileInfo>();
    QString oldPath = oldInfo.absoluteFilePath();
    if (oldPath.endsWith(".desktop", Qt::CaseInsensitive)) {
        return;
    }

    QString originalInput = nameItem->text();
    QString cleanedName = cleanFileName(originalInput);

    bool isDir = oldInfo.isDir();

    QString finalFileName = cleanedName;
    if (!m_settings.showFileExtensions && !oldInfo.isDir()) {
        QString base = oldInfo.completeBaseName();
        if (!base.isEmpty()) {
            // Schneidet z.B. ".exe" oder ".tar.gz" aus dem echten Dateinamen aus
            QString extension = oldInfo.fileName().mid(base.length());
            finalFileName += extension;
        }
    }
    QString newPath = QDir(oldInfo.absolutePath()).filePath(finalFileName);

    if (oldPath != newPath) {
        if (QFile::rename(oldPath, newPath)) {
            QSignalBlocker blocker(nameItem->tableWidget());
            nameItem->setText(getDisplayName(newPath, isDir, m_settings.showFileExtensions));
            nameItem->setData(Qt::UserRole, QVariant::fromValue(QFileInfo(newPath)));
        } else {
            QMessageBox::critical(this, "Fehler", "Umbenennen fehlgeschlagen.");
            QSignalBlocker blocker(nameItem->tableWidget());
            nameItem->setText(getDisplayName(oldInfo, m_settings.showFileExtensions));
        }
    } else if (cleanedName != originalInput) {
        QSignalBlocker blocker(nameItem->tableWidget());
        nameItem->setText(getDisplayName(newPath, isDir, m_settings.showFileExtensions));
    }
}

void MainWindow::onShowContextMenu(const QPoint &pos) {
    QTableWidgetItem *item = m_tableWidget->itemAt(pos);
    if (!item) return;

    QFileInfo fileInfo = m_tableWidget->item(item->row(), eColName)->data(Qt::UserRole).value<QFileInfo>();
    QString filePath = fileInfo.absoluteFilePath();
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
                QStringList pathList = getActiveViewPathList();
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
                    openFileListWithHandler(info.id, getActiveViewPathList());
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

QString MainWindow::getActiveViewCurrentItemPath() {
    QString path;

    QTableWidgetItem *currentItem = m_tableWidget->currentItem();
    if (currentItem) {
        QFileInfo fileInfo = m_tableWidget->item(currentItem->row(), eColName)->data(Qt::UserRole).value<QFileInfo>();
        path = fileInfo.absoluteFilePath();
    }

    return path;
}

QStringList MainWindow::getActiveViewPathList() {
    QStringList pathList;

    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();
    if (selectedItems.isEmpty()) return pathList;

    QSet<int> rowSet;
    for (auto *item : std::as_const(selectedItems)) {
        rowSet.insert(item->row());
    }

    for (int row : rowSet) {
        QFileInfo fileInfo = m_tableWidget->item(row, eColName)->data(Qt::UserRole).value<QFileInfo>();
        QString path = fileInfo.absoluteFilePath();
        if (!path.isEmpty()) {
            pathList << path;
        }
    }

    return pathList;
}

QSet<int> MainWindow::getActiveViewRowSet() {
    QSet<int> rowSet;

    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();
    if (selectedItems.isEmpty()) return rowSet;

    for (auto *item : std::as_const(selectedItems)) {
        rowSet.insert(item->row()); // Das war hier schon völlig richtig!
    }

    return rowSet;
}

void MainWindow::onListViewItemDoubleClicked(QTableWidgetItem *item) {
    action_ListViewOpenFiles();
}

void MainWindow::action_ListViewOpenFiles() {
    QString path = getActiveViewCurrentItemPath();
    if (path.isEmpty()) return;

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return;
    }

    RecentOpenList_Add(path);

    QString fileExt = fileInfo.suffix().toLower();

    if (fileExt == "desktop") {
        launchDesktopFile(getDesktopEntry(fileInfo));
#ifdef Q_OS_LINUX
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
    QStringList pathList = getActiveViewPathList();
    if (pathList.isEmpty()) return;

    QStringList pathListAudio;
    QStringList pathListImage;
    QStringList pathListText;
    QStringList pathListVideo;

    for (const QString &fullPath : std::as_const(pathList)) {
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
    QStringList pathList = getActiveViewPathList();
    if (pathList.isEmpty()) return;

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
    QStringList pathList = getActiveViewPathList();
    if (pathList.isEmpty()) {
        return;
    }

    if (!showDeleteConfirmationDialog(pathList, bRecycleOnly)) {
        return;
    }

    // Delete on disk and store successful deletion paths

    //m_fileSystemWatcher->blockSignals(true);

    QSet<QString> successfullyDeletedPaths;

    for (const QString &path : std::as_const(pathList)) {
        bool success = false;
        if (bRecycleOnly) {
            success = QFile::moveToTrash(path);
        } else {
            QFileInfo info(path);

            if (info.isDir()) {
                QDir dir(path);
                success = dir.rmpath(".");
            } else {
                success = QFile::remove(path);
            }
        }

        if (success) {
            successfullyDeletedPaths.insert(path);
        } else {
            qDebug() << "Could not delete:" << path;
        }
    }

    if (successfullyDeletedPaths.isEmpty()) {
        return; // Nichts zu tun im UI
    }

    // Clean both views in reverse order
    m_tableWidget->setUpdatesEnabled(false);
    for (int row = m_tableWidget->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *item = m_tableWidget->item(row, eColName);
        if (item) {
            QFileInfo fileInfo = item->data(Qt::UserRole).value<QFileInfo>();
            QString path = fileInfo.absoluteFilePath();
            if (!path.isEmpty() && successfullyDeletedPaths.contains(path)) {
                m_tableWidget->removeRow(row);
            }
        }
    }
    m_tableWidget->setUpdatesEnabled(true);
}

void MainWindow::action_ListViewCutFiles() {
    QStringList pathList = getActiveViewPathList();
    if (pathList.isEmpty()) return;

    m_cutFilePaths = QSet<QString>(pathList.begin(), pathList.end());

    m_tableWidget->viewport()->update();

    setupClipboardForCut(m_cutFilePaths);
}

void MainWindow::setupClipboardForCut(const QSet<QString> &cutFilePaths) {
    auto *mimeData = new QMimeData();
    QList<QUrl> urls;

    for (const QString &path : std::as_const(cutFilePaths)) {
        if (!path.isEmpty()) {
            urls << QUrl::fromLocalFile(path);
        }
    }

    mimeData->setUrls(urls);

#ifdef Q_OS_WIN
    // For MS Windows: set "Preferred DropEffect" (Copy oder Move)
    QByteArray buffer;
    buffer.append(static_cast<char>(Qt::MoveAction));
    buffer.append('\0'); buffer.append('\0'); buffer.append('\0');
    mimeData->setData("Preferred DropEffect", buffer);
#elif defined(Q_OS_LINUX)
    // For Linux on GNOME-based Desktops (Nautilus, PCManFM, etc.)
    // Format: "cut" oder "copy", dann Zeilenumbruch, dann alle URLs (ebenfalls per \n getrennt)
    QByteArray gnomeData = "cut";
    for (const QUrl &url : std::as_const(urls)) {
        gnomeData.append("\n");
        gnomeData.append(url.toEncoded());
    }
    mimeData->setData("x-special/gnome-copied-files", gnomeData);

    // For Linux on KDE-based desktops (Dolphin)
    // Expects separate flag set to  "1" (true)
    mimeData->setData("application/x-kde-cutselection", QByteArray("1"));
#endif

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
    m_cutFilePaths.clear();
    m_tableWidget->viewport()->update();
}

void MainWindow::action_ListViewCopyFiles() {
    removeCutMarkers();

    QStringList pathList = getActiveViewPathList();

    auto *mimeData = new QMimeData();
    QList<QUrl> urls;

    for (const QString &path : std::as_const(pathList)) {
        urls << QUrl::fromLocalFile(path);
    }
    mimeData->setUrls(urls);

#ifdef Q_OS_WIN
    // For MS Windows: set "Preferred DropEffect" (Copy oder Move)
    QByteArray buffer;
    buffer.append(static_cast<char>(Qt::CopyAction));
    buffer.append('\0'); buffer.append('\0'); buffer.append('\0');
    mimeData->setData("Preferred DropEffect", buffer);
#elif defined(Q_OS_LINUX)
    // For Linux on GNOME-based Desktops (Nautilus, PCManFM, etc.)
    // Format: "cut" oder "copy", dann Zeilenumbruch, dann alle URLs (ebenfalls per \n getrennt)
    QByteArray gnomeFormat = "copy";
    for (const QUrl &url : std::as_const(urls)) {
        gnomeFormat.append("\n");
        gnomeFormat.append(url.toEncoded());
    }
    mimeData->setData("x-special/gnome-copied-files", gnomeFormat);

    // For Linux on KDE-based desktops (Dolphin)
    // Nothing special to do. Defaults to "copy"
#endif

    m_currentClipboardToken = QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    mimeData->setData("application/x-mklauncher-token", m_currentClipboardToken);

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void MainWindow::action_ListViewBrowseToFile() {
    QString path = getActiveViewCurrentItemPath();
    if (path.isEmpty()) return;

    browseToFile(path, m_settings.fileManager);
    guiHideConditional();
}

void MainWindow::action_ListViewRenameFiles() {
    QTableWidgetItem *item = m_tableWidget->currentItem();
    if (item) m_tableWidget->editItem(m_tableWidget->item(item->row(), eColName));
}

void MainWindow::action_ListViewFileProperties() {
    QStringList pathList = getActiveViewPathList();

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
            QFileInfo fileInfo = nameItem->data(Qt::UserRole).value<QFileInfo>();
            QString fullPath = fileInfo.absoluteFilePath();
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

void MainWindow::addFileToTable(const QFileInfo &fileInfo, int iRow, int nameMatchQuality, const QString &displayName) {

    // Icon & Name
    QString visibleName;
    if (!displayName.isEmpty()) {
        visibleName = displayName;
    } else {
        visibleName = getDisplayName(fileInfo, m_settings.showFileExtensions);
    }

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
    nameItem->setData(Qt::UserRole, QVariant::fromValue(fileInfo));
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

#elif defined(Q_OS_LINUX)
        QString possiblePath = input1;

        if (possiblePath.startsWith("~/")) {
            possiblePath = QDir::homePath() + possiblePath.mid(1);
        } else if (possiblePath.startsWith("$HOME/")) {
            possiblePath = QDir::homePath() + possiblePath.mid(5);
        }

        possiblePath = QDir::cleanPath(possiblePath);

        QFileInfo checkFile(possiblePath);
        if (checkFile.exists()) {
            browseToFile(possiblePath, m_settings.fileManager);
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

    QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);  // Order: User before System
    std::reverse(appDirs.begin(), appDirs.end());   // Reverse to System before User, so we can overwrite System with User keys while parsing
    for (const QString &dirPath : std::as_const(appDirs)) {
        QString cachePath = QDir(dirPath).filePath("mimeinfo.cache");
        parseMimeInfoCache(cachePath);
    }

    parseMimeAppsList(QDir(QDir::homePath()).filePath(".config/mimeapps.list"));
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

bool MainWindow::showDeleteConfirmationDialog(const QStringList &pathList, bool bRecycleOnly) {

    QString sTitle;
    QString sText;
    QString sWarning;
    QMessageBox::Icon iIcon;

    if (bRecycleOnly) {
        iIcon = QMessageBox::Question;
        sWarning = "";
        if (pathList.size() == 1) {
            sTitle = tr("Delete File");
            sText = tr("Do you really want to move this file into the recycle bin?");
        } else {
            sTitle = tr("Delete multiple elements");
            sText = QString(tr("Do you really want to move these %1 files into the recylce bin?")).arg(pathList.size());
        }
    } else {
        iIcon = QMessageBox::Warning;
        sWarning = "<p style='color: red;'><i>" + tr("This process cannot be undone.") + "</i></p>";
        if (pathList.size() == 1) {
            sTitle = tr("Delete File");
            sText = tr("Are you sure you want to delete this file permanently?");
        } else {
            sTitle = tr("Delete multiple elements");
            sText = QString(tr("Are you sure you want to delete these %1 files permanently?")).arg(pathList.size());
        }
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(sTitle);
    msgBox.setIcon(iIcon);

    if (pathList.size() == 1) {
        QFileInfo fileInfo(pathList.first());
        QString fileName = fileInfo.fileName();
        QString size = formatAdaptiveSize(fileInfo.size());
        QString lastModified = fileInfo.lastModified().toString("yyyy-MM-dd  HH:mm:ss");

        QFileIconProvider provider;
        QIcon icon = provider.icon(fileInfo);
        QPixmap pix = icon.pixmap(QSize(48, 48));

        QByteArray ba;
        QBuffer bu(&ba);
        pix.save(&bu, "PNG");
        QString imgBase64 = ba.toBase64();

        msgBox.setText(QString("<h3>%1</h3>").arg(sText));
        msgBox.setInformativeText(QString(tr("<table width='100%' cellspacing='0' cellpadding='0'><tr><td rowspan=4 width='48' valign='top' style='padding-right: 10px;'><img src='data:image/png;base64,%1'></td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;' width='1%'>Name:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%2</td></tr><tr><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>Size:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%3</td></tr><tr><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>Date:</td><td style='color: #555; padding-top: 2px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%4</td></tr><tr><td colspan=2 style='padding-top: 8px; padding-bottom: 2px; padding-left: 8px; padding-right: 8px;'>%5</td></tr></table>")).arg(imgBase64, fileName, size, lastModified, sWarning));
    } else {
        msgBox.setText(QString("<h3>%1</h3>").arg(sText));
        msgBox.setInformativeText(sWarning);
    }

    QPushButton *deleteButton = msgBox.addButton(tr("Delete"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(deleteButton);
    deleteButton->setStyleSheet("QPushButton { font-weight: bold; min-width: 80px; }");

    msgBox.exec();

    if (msgBox.clickedButton() != deleteButton) {
        return false;
    }

    return true;
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

// This happens only once, on creating the window
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
            if (m_bSearchActive.load()) {
                m_bAbortRequested.store(true);
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

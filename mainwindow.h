#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "searchworker.h"
#include "settingsmanager.h"

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFileIconProvider>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QSet>
#include <QPainter>             // Added for cut item opacity drawing
#include <QStyledItemDelegate>  // Added for cut item opacity drawing
#include <QBoxLayout>

/*
 * [QTableWidget Qt::UserRole usage]
 *
 *  eColName	Qt::UserRole		QString absoluteFilePath
 *  eColSize	Qt::UserRole + 1	bool trueIcon
 *  eColSize	Qt::UserRole		qint64 sizeInBytes
 *  any item	Qt::UserRole + 5	bool isCut
*/

// Subclass QTableWidget as Custom_QTableWidget where F2 key presses are diverted to the first row's item
class Custom_QTableWidget : public QTableWidget {
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_F2) {
            int row = currentRow();
            if (row != -1) {
                setCurrentCell(row, 0);     // Den Fokus explizit auf die erste Spalte setzen
                editItem(item(row, 0));     // Den Editor manuell starten
                return;
            }
        }

        QTableWidget::keyPressEvent(event); // Alle anderen Tasten normal verarbeiten
    }
};

// Subclass QTableWidgetItem for sorting the filesize column by the data hidden with UserRole
class SizeTableItem : public QTableWidgetItem {
public:
    SizeTableItem(const QString &text) : QTableWidgetItem(text) {}

    bool operator<(const QTableWidgetItem &other) const override {
        QVariant v1 = this->data(Qt::UserRole);
        QVariant v2 = other.data(Qt::UserRole);

        if (v1.isValid() && v2.isValid()) {
            return v1.toLongLong() < v2.toLongLong();
        }

        return QTableWidgetItem::operator<(other);  // Fallback auf Standard-Textvergleich
    }
};


class CutDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        // Wir nutzen eine benutzerdefinierte Daten-Rolle (z.B. Qt::UserRole + 5),
        // um zu prüfen, ob dieses Item "ausgeschnitten" ist.
        bool isCut = index.data(Qt::UserRole + 5).toBool();

        if (isCut) {
            painter->save();
            painter->setOpacity(0.50);
            QStyledItemDelegate::paint(painter, option, index);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void abortSearchWorkerRequested();

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void wasRestored();

private slots:
    void onItemChanged(QTableWidgetItem *item);
    void onClipboardChanged();
    void onListViewHeaderClicked();
    void onListViewItemDoubleClicked(QTableWidgetItem *item);
    void onShowContextMenu(const QPoint &pos);
    void onTimedUpdateIcons();
    void onToggleListViewHeader();
    void onVerticalBarScrollChange();
    void handleTextChange(const QString &text);

private:
    enum Column {
        eColName = 0,
        eColPath = 1,
        eColSize = 2,
        eColDate = 3,
        eColType = 4,
        eColQuality = 5,
    };

    QStringList getTablePathList();
    QString RecentInputList_GetPrevious(const QString &currentText);
    QString RecentInputList_GetNext(const QString &currentText);
    void action_EditSettingsFile();
    void action_ListViewBrowseToFile();
    void action_ListViewCopyFiles();
    void action_ListViewCopyPaths();
    void action_ListViewCutFiles();
    void action_ListViewDeleteFiles(bool bRecycleOnly);
    void action_ListViewEditFiles();
    void action_ListViewFileProperties();
    void action_ListViewOpenFiles();
    void action_ListViewRenameFiles();
    void addFileToTable(const QFileInfo &fileInfo, int iRow, int nameMatchQuality, const QString &displayName);
    void finalizeUI();
    void guiHideConditional();
    void launchAction();
    void loadMimeCache();
    void onWorkerFinished(bool bSearchInterrupted);
    void onWorkerSentBatch(const QList<SearchResult> &results);
    void parseMimeInfoCache(const QString &path);
    void parseMimeAppsList(const QString &path);
    void processNextBatch();
    void RecentInputList_Add(QString searchTerm);
    void RecentOpenList_Add(const QString &filePath);
    void removeCutMarkers();
    void setupClipboardForCut(const QSet<int> &rowSet);
    void startSearch();
    void updateColumns();
    void validateInputBoxRegex();

    QWidget *m_centralWidget = nullptr;
    QVBoxLayout *m_mainLayout = nullptr;
    QWidget *m_topControlsContainerWidget = nullptr;
    QHBoxLayout *m_topControlsHBoxLayout = nullptr;
    QLineEdit *m_LineEdit1 = nullptr;
    QLineEdit *m_LineEdit2 = nullptr;
    QTableWidget *m_tableWidget = nullptr;
    QAction *m_actionListViewOpenFiles = nullptr;
    QAction *m_actionListViewEditFiles = nullptr;
    QAction *m_actionListViewBrowseToFile = nullptr;
    QAction *m_actionListViewCopyPaths = nullptr;
    QAction *m_actionListViewCutFiles = nullptr;
    QAction *m_actionListViewCopyFiles = nullptr;
    QAction *m_actionListViewDeleteFiles = nullptr;
    QAction *m_actionListViewRenameFiles = nullptr;
    QAction *m_actionListViewFileProperties = nullptr;
    QThread *m_workerThread = nullptr;
    QTimer *m_timerUpdateIcons = nullptr;

    QElapsedTimer m_BenchmarkTimer;
    QFileIconProvider m_iconProvider;
    bool m_WorkerSearchInterrupted;
    bool m_bHeaderVisible;
    bool m_isProcessingPending = false;
    int m_tableLineHeight;
    std::atomic<bool> m_bSearchActive{false};
    std::atomic<bool> m_bAbortRequested{false};
    std::atomic<bool> m_bRestartPendingSearch{false};
    std::atomic<bool> m_bWorkerFinished{false};
    QHash<QString, QIcon> m_iconCache;
    QHash<QString, QIcon> m_pathIconCache;
    QHash<QString, QStringList> m_mimeCache;
    QQueue<QList<SearchResult>> m_pendingBatches;
    QByteArray m_currentClipboardToken;
    QSet<int> m_rowsWithCutMarkers;
    SettingsManager m_settings;

#ifdef Q_OS_WIN
    QString getSendToPath();
#endif

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};
#endif // MAINWINDOW_H

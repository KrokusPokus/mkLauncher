#ifndef FILEPROPERTIESDIALOG_H
#define FILEPROPERTIESDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QPushButton>
#include <QtConcurrent>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class FilePropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit FilePropertiesDialog(const QStringList &filePaths, QWidget *parent = nullptr);
    ~FilePropertiesDialog();

private:
    void setupUi();
    void loadFileInfo();
    void setupUiMultiMode();
    void loadFileInfoMultiMode();
    QString getFileType(const QFileInfo &info);
    quint64 calculateFolderSize(const QString &path, int &fileCount);
    void onOkPressed();

#ifdef Q_OS_WIN
    DWORD getWindowsFileAttributes(const QString &filePath);
#endif

    QStringList m_filePaths;
    bool m_isMultiMode;
    QLocale m_locale;

    QLabel *m_iconLabel = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_mimeLabel = nullptr;
    QLabel *m_pathEdit = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_containsLabel = nullptr;
    QLabel *m_createdLabel = nullptr;
    QLabel *m_lastReadLabel = nullptr;
    QLabel *m_modifiedLabel = nullptr;

#ifdef Q_OS_WIN
    QCheckBox *m_readOnlyCB = nullptr;
    QCheckBox *m_hiddenCB = nullptr;
    QCheckBox *m_systemCB = nullptr;
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    QLabel *m_ownerLabel = nullptr;
    QLabel *m_groupLabel = nullptr;
    QLabel *m_permLabel = nullptr;
    QCheckBox *m_permCBs[3][3]{};   // The '{}' initializes all 9 pointers to nullptr
#endif

    std::atomic<bool> m_abort{false};

    // For calculating size concurrently
    struct ProgressResult {
        quint64 size = 0;
        int files = 0;
        int folders = 0;
    };

    QFutureWatcher<ProgressResult> *m_watcher = nullptr;
    void updateMultiUi(ProgressResult result);
};

#endif // FILEPROPERTIESDIALOG_H

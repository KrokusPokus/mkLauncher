#include <QFileIconProvider>
#include <QDateTime>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QMimeDatabase>
#include <QMimeType>

#include "filepropertiesdialog.h"
#include "helpers.h"

FilePropertiesDialog::FilePropertiesDialog(const QStringList &filePaths, QWidget *parent)
    : QDialog(parent), m_filePaths(filePaths) {

    if (m_filePaths.isEmpty()) {
        this->close();
        return;
    }

    m_isMultiMode = (m_filePaths.size() > 1);

    if (m_isMultiMode) {
        setupUiMultiMode();
        loadFileInfoMultiMode();
        setWindowTitle(tr("Properties of %1 elements").arg(m_filePaths.size()));
    } else {
        setupUi();
        loadFileInfo();
        QFileInfo info(m_filePaths.first());
        setWindowTitle(tr("Properties of ") + info.fileName());
    }
    setWindowIcon(QIcon(":/icons/res/info.ico"));
}

FilePropertiesDialog::~FilePropertiesDialog() {
    if (m_watcher) {
        m_abort.store(true);
        m_watcher->waitForFinished(); // Warten, bis der Thread sicher beendet ist
    }
}

void FilePropertiesDialog::setupUiMultiMode() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Header ---
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(30, 10, 30, 10);
    headerLayout->setSpacing(20);
    m_iconLabel = new QLabel();

    m_iconLabel->setPixmap(QIcon(":/icons/res/multi.ico").pixmap(48, 48));

    auto *summaryLabel = new QLabel(tr("<strong>%1 elements selected</strong>").arg(m_filePaths.size()));
    summaryLabel->setStyleSheet("font-size: 14px;");

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(summaryLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    auto *line1 = new QFrame();
    line1->setContentsMargins(20, 10, 20, 10);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // --- Aggregierte Infos ---
    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(30, 10, 30, 30);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_containsLabel = new QLabel(tr("Calculating content..."));
    m_containsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sizeLabel = new QLabel(tr("Calculating size..."));
    m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_typeLabel = new QLabel(tr("Multiple types"));

    formLayout->addRow(tr("Type:"), m_typeLabel);
    formLayout->addRow(tr("Content:"), m_containsLabel);
    formLayout->addRow(tr("Total size:"), m_sizeLabel);

    mainLayout->addLayout(formLayout);

    mainLayout->addStretch(); // Schiebt alles nach oben

#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    auto *line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    mainLayout->addWidget(line2);
#endif

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(5, 5, 5, 5);
    buttonLayout->setSpacing(10);
    auto *okBtn = new QPushButton(tr("OK"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, this, &FilePropertiesDialog::onOkPressed);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void FilePropertiesDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(30, 10, 30, 10);
    headerLayout->setSpacing(20);
    m_iconLabel = new QLabel();
    m_nameEdit = new QLineEdit();
    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(m_nameEdit);
    mainLayout->addLayout(headerLayout);

    auto *line1 = new QFrame();
    line1->setContentsMargins(20, 10, 20, 10);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // fileinfo
    m_typeLabel = new QLabel();
    m_typeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_mimeLabel = new QLabel();
    m_mimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathEdit = new QLabel();
    m_pathEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sizeLabel = new QLabel();
    m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_createdLabel = new QLabel();
    m_createdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_modifiedLabel = new QLabel();
    m_modifiedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lastReadLabel = new QLabel();
    m_lastReadLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *fileinfoLayout = new QFormLayout();
    fileinfoLayout->setContentsMargins(30, 10, 30, 10);
    fileinfoLayout->setSpacing(10);
    fileinfoLayout->addRow(tr("Type:"), m_typeLabel);
    fileinfoLayout->addRow(tr("MimeType:"), m_mimeLabel);
    fileinfoLayout->addRow(tr("Path:"), m_pathEdit);
    fileinfoLayout->addRow(tr("Size:"), m_sizeLabel);
    QFileInfo fileInfo(m_filePaths.first());
    if (fileInfo.isDir()) {
        m_containsLabel = new QLabel();
        fileinfoLayout->addRow(tr("Content:"), m_containsLabel);
    }

    fileinfoLayout->addRow(tr("Created:"), m_createdLabel);
    fileinfoLayout->addRow(tr("Modified:"), m_modifiedLabel);
    fileinfoLayout->addRow(tr("Accessed:"), m_lastReadLabel);

    mainLayout->addLayout(fileinfoLayout);

    auto *line3 = new QFrame();
    line3->setContentsMargins(20, 10, 20, 10);
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line3);

#ifdef Q_OS_WIN
    // Attributes
    auto *attrLayout = new QHBoxLayout();
    attrLayout->setContentsMargins(30, 10, 30, 30);
    attrLayout->setSpacing(10);
    QLabel *attributesLabel = new QLabel(tr("Attributes:"));
    m_readOnlyCB = new QCheckBox(tr("Read-only"));
    m_hiddenCB = new QCheckBox(tr("Hidden"));
    m_systemCB = new QCheckBox(tr("System"));
    attrLayout->addWidget(attributesLabel);
    attrLayout->addWidget(m_readOnlyCB);
    attrLayout->addWidget(m_hiddenCB);
    attrLayout->addWidget(m_systemCB);
    mainLayout->addLayout(attrLayout);
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    auto *securityLayout = new QFormLayout();
    securityLayout->setContentsMargins(30, 10, 30, 10);
    securityLayout->setSpacing(10);

    m_ownerLabel = new QLabel();
    m_ownerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    securityLayout->addRow(tr("Owner:"), m_ownerLabel);

    m_groupLabel = new QLabel();
    m_groupLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    securityLayout->addRow(tr("Group:"), m_groupLabel);

    // Label für die Oktal-Anzeige (z.B. 755)
    m_permLabel = new QLabel();
    m_permLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    securityLayout->addRow(tr("Permissions:"), m_permLabel);

    mainLayout->addLayout(securityLayout);

    // Eine kleine Matrix für die Übersicht
    auto *permGrid = new QGridLayout();
    permGrid->setContentsMargins(60, 10, 30, 10);
    permGrid->setSpacing(5);

    // Header
    permGrid->addWidget(new QLabel(tr("Read")), 0, 1);
    permGrid->addWidget(new QLabel(tr("Write")), 0, 2);
    permGrid->addWidget(new QLabel(tr("eXecute")), 0, 3);

    // Zeilen-Helper
    QStringList rows = { tr("Owner"), tr("Group"), tr("Others") };
    for (int i = 0; i < 3; ++i) {
        permGrid->addWidget(new QLabel(rows[i]), i + 1, 0);
        for (int j = 0; j < 3; ++j) {
            QCheckBox *cb = new QCheckBox();
            cb->setEnabled(false);
            m_permCBs[i][j] = cb;
            permGrid->addWidget(cb, i + 1, j + 1);
        }
    }
    mainLayout->addLayout(permGrid);

    auto *line4 = new QFrame();
    line4->setFrameShape(QFrame::HLine);
    line4->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line4);
#endif

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(5, 5, 5, 5);
    buttonLayout->setSpacing(10);
    auto *okBtn = new QPushButton(tr("OK"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, this, &FilePropertiesDialog::onOkPressed);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void FilePropertiesDialog::loadFileInfo() {
    QFileInfo fileInfo(m_filePaths.first());

    // .mimeTypeForFile goes by file extension AND if neccessary by content (magic bytes)
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(fileInfo.absoluteFilePath());
    m_mimeLabel->setText(mime.name());

    QFileIconProvider provider;
    m_iconLabel->setPixmap(QIcon::fromTheme(mime.iconName(), provider.icon(fileInfo)).pixmap(32, 32));
    m_nameEdit->setText(fileInfo.completeBaseName());
    m_pathEdit->setText(QDir::toNativeSeparators(fileInfo.absolutePath()));
    m_typeLabel->setText(getFileType(fileInfo));

    if (fileInfo.isDir()) {
        int fileCount = 0;
        quint64 totalSize = calculateFolderSize(fileInfo.absoluteFilePath(), fileCount);
        m_sizeLabel->setText(formatAdaptiveSize(totalSize));
        m_containsLabel->setText(tr("%1 Files")
                                     .arg(m_locale.toString(fileCount)));
    } else {
        m_sizeLabel->setText(tr("%1 (%2 Bytes)")
                                 .arg(formatAdaptiveSize(fileInfo.size()))
                                 .arg(m_locale.toString(fileInfo.size())));
    }

    m_createdLabel->setText(fileInfo.birthTime().toString("yyyy-MM-dd  HH:mm:ss"));
    m_modifiedLabel->setText(fileInfo.lastModified().toString("yyyy-MM-dd  HH:mm:ss"));
    m_lastReadLabel->setText(fileInfo.lastRead().toString("yyyy-MM-dd  HH:mm:ss"));

#ifdef Q_OS_WIN
    DWORD fileAttr = getWindowsFileAttributes(fileInfo.filePath());
    m_readOnlyCB->setChecked(fileAttr & FILE_ATTRIBUTE_READONLY);
    m_hiddenCB->setChecked(fileAttr & FILE_ATTRIBUTE_HIDDEN);
    m_systemCB->setChecked(fileAttr & FILE_ATTRIBUTE_SYSTEM);
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    QString ownerName = fileInfo.owner();
    if (ownerName.isEmpty()) {
        m_ownerLabel->setText(QString::number(fileInfo.ownerId()));
    } else {
        m_ownerLabel->setText(tr("%1 (%2)").arg(ownerName).arg(fileInfo.ownerId()));
    }

    QString groupName = fileInfo.group();
    if (groupName.isEmpty()) {
        m_groupLabel->setText(QString::number(fileInfo.groupId()));
    } else {
        m_groupLabel->setText(tr("%1 (%2)").arg(groupName).arg(fileInfo.groupId()));
    }

    QFile::Permissions p = fileInfo.permissions();

    int owner = ((p & QFile::ReadOwner) ? 4 : 0) + ((p & QFile::WriteOwner) ? 2 : 0) + ((p & QFile::ExeOwner) ? 1 : 0);
    int group = ((p & QFile::ReadGroup) ? 4 : 0) + ((p & QFile::WriteGroup) ? 2 : 0) + ((p & QFile::ExeGroup) ? 1 : 0);
    int other = ((p & QFile::ReadOther) ? 4 : 0) + ((p & QFile::WriteOther) ? 2 : 0) + ((p & QFile::ExeOther) ? 1 : 0);

    m_permLabel->setText(QString("0%1%2%3").arg(owner).arg(group).arg(other));

    m_permCBs[0][0]->setChecked(p & QFile::ReadOwner);
    m_permCBs[0][1]->setChecked(p & QFile::WriteOwner);
    m_permCBs[0][2]->setChecked(p & QFile::ExeOwner);

    m_permCBs[1][0]->setChecked(p & QFile::ReadGroup);
    m_permCBs[1][1]->setChecked(p & QFile::WriteGroup);
    m_permCBs[1][2]->setChecked(p & QFile::ExeGroup);

    m_permCBs[2][0]->setChecked(p & QFile::ReadOther);
    m_permCBs[2][1]->setChecked(p & QFile::WriteOther);
    m_permCBs[2][2]->setChecked(p & QFile::ExeOther);
#endif
}

void FilePropertiesDialog::loadFileInfoMultiMode() {
    m_watcher = new QFutureWatcher<ProgressResult>(this);

    connect(m_watcher, &QFutureWatcher<ProgressResult>::finished, this, [this]() {
        updateMultiUi(m_watcher->result());
    });

    auto computeTask = [this]() -> ProgressResult {
        ProgressResult res;

        for (const QString &path : std::as_const(m_filePaths)) {
            if (m_abort.load())
                return {};
            QFileInfo info(path);
            if (info.isDir()) {
                res.folders++;
                int subFiles = 0;
                res.size += calculateFolderSize(path, subFiles);
                res.files += subFiles;
            } else {
                res.files++;
                res.size += info.size();
            }
        }
        return res;
    };

    m_watcher->setFuture(QtConcurrent::run(computeTask));
}

void FilePropertiesDialog::updateMultiUi(ProgressResult result) {

    m_sizeLabel->setText(tr("%1 (%2 Bytes)")
                             .arg(formatAdaptiveSize(result.size))
                             .arg(m_locale.toString(result.size)));

    m_containsLabel->setText(tr("%1 Files, %2 Folders")
                                 .arg(m_locale.toString(result.files))
                                 .arg(m_locale.toString(result.folders)));

    m_watcher->deleteLater();
    m_watcher = nullptr;
}

quint64 FilePropertiesDialog::calculateFolderSize(const QString &path, int &fileCount) {
    quint64 size = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
        fileCount++;
    }
    return size;
}

QString FilePropertiesDialog::getFileType(const QFileInfo &info) {
    if (info.isDir()) return tr("Folder");
#if defined(Q_OS_LINUX)
    if (info.isSymLink()) return tr("SymLink");
#endif
    return info.suffix().toUpper() + tr("-File");
}

void FilePropertiesDialog::onOkPressed() {
    // Todo: add logic to write changed file attributes
    accept();
}

#ifdef Q_OS_WIN
DWORD FilePropertiesDialog::getWindowsFileAttributes(const QString &filePath) {
    return GetFileAttributesW(reinterpret_cast<const WCHAR*>(filePath.utf16()));
}
#endif

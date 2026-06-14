#include "filepropertiesdialog.h"
#include "helpers.h"    // needed for formatAdaptiveSize()

#include <QFileIconProvider>
#include <QDateTime>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStack>

#ifdef Q_OS_LINUX
#include <unistd.h> // Wird für getuid() benötigt
#endif

FilePropertiesDialog::FilePropertiesDialog(const QStringList &filePaths, QWidget *parent)
    : QDialog(parent), m_filePaths(filePaths) {

    setAttribute(Qt::WA_DeleteOnClose);

    m_isMultiMode = (m_filePaths.size() > 1);

    if (m_isMultiMode) {
        setupUiMultiMode();
        updateUiAsyncStart();
        setWindowTitle(tr("Properties of %1 elements").arg(m_filePaths.size()));
    } else {
        QFileInfo fileInfo(m_filePaths.first());
        setupUi(fileInfo);
        setWindowTitle(tr("Properties of %1").arg(fileInfo.fileName()));
    }
    setWindowIcon(QIcon(":/icons/info.ico"));
}

FilePropertiesDialog::~FilePropertiesDialog() {
    if (m_watcher) {
        m_abort.store(true);
        m_watcher->waitForFinished();
    }
}

void FilePropertiesDialog::done(int r) {
    m_abort.store(true);    // signal thread that it should stop
    QDialog::done(r);       // call default behaviour (closes window)
}

void FilePropertiesDialog::setupUiMultiMode() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Header ---
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(30, 25, 30, 25);
    headerLayout->setSpacing(25);
    m_iconLabel = new QLabel();

    m_iconLabel->setPixmap(QIcon(":/icons/res/multi.ico").pixmap(48, 48));

    auto *summaryLabel = new QLabel(tr("<strong>%1 elements selected</strong>").arg(m_filePaths.size()));
    summaryLabel->setStyleSheet("font-size: 14px;");

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(summaryLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    QFrame *line1 = new QFrame();
    line1->setContentsMargins(20, 10, 20, 10);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    // --- Aggregierte Infos ---
    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(30, 10, 30, 30);
    formLayout->setSpacing(10);
    formLayout->setHorizontalSpacing(20);
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_containsLabel = new QLabel(tr("Calculating content..."));
    m_containsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sizeLabel = new QLabel(tr("Calculating size..."));
    m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_typeLabel = new QLabel(tr("Multiple types"));

    formLayout->addRow(tr("Type:"), m_typeLabel);
    formLayout->addRow(tr("Total size:"), m_sizeLabel);
    formLayout->addRow(tr("Content:"), m_containsLabel);

    mainLayout->addLayout(formLayout);

    mainLayout->addStretch(); // Schiebt alles nach oben

#ifdef Q_OS_LINUX
    QFrame *line2 = new QFrame();
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

void FilePropertiesDialog::setupUi(const QFileInfo &fileInfo) {
    bool isDrive = false;
    QString absoluteFilePath = fileInfo.absoluteFilePath();
#ifdef Q_OS_WIN
    isDrive = (absoluteFilePath.length() == 3 && absoluteFilePath.at(1) == ':' && (absoluteFilePath.at(2) == '/' || absoluteFilePath.at(2) == '\\'));
#endif

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(30, 25, 30, 25);
    headerLayout->setSpacing(25);

    m_iconLabel = new QLabel();
    QFileIconProvider provider;
    QIcon icon = provider.icon(fileInfo);
    QPixmap pix = icon.pixmap(QSize(48, 48));
    if (hasImageExt(fileInfo)) {
        QPixmap thumb = generateThumbnail(absoluteFilePath);
        if (!thumb.isNull()) {
            pix = thumb;
        }
    }
    m_iconLabel->setPixmap(pix);
    QString itemName;
    if (isDrive) {
        QStorageInfo storageInfo(fileInfo.absoluteFilePath());
        itemName = storageInfo.name();
    } else {
        itemName = fileInfo.fileName();
    }
    m_nameEdit = new QLineEdit(itemName);
    m_nameEdit->setStyleSheet("QLineEdit { padding-left: 7px; padding-right: 7px; padding-top: 5px; padding-bottom: 5px;}");
    int textWidth = m_nameEdit->fontMetrics().horizontalAdvance(m_nameEdit->text());
    m_nameEdit->setMinimumWidth(textWidth + 22);

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(m_nameEdit);
    mainLayout->addLayout(headerLayout);

    QFrame *line1 = new QFrame();
    line1->setContentsMargins(20, 10, 20, 10);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line1);

    //---------------------------------------------------------------------------------------------------------------------------------------------------------
    // fileinfo

    QGridLayout *fileinfoGridLayout = new QGridLayout();

    fileinfoGridLayout->setContentsMargins(20, 10, 20, 20);
    fileinfoGridLayout->setHorizontalSpacing(0);
    fileinfoGridLayout->setVerticalSpacing(10);

    const int colLeftMargin  = 0; // Virtueller Rand links
    const int colLabel       = 1; // Hier sitzen die Labels
    const int colSpacingH    = 2; // Horizontal spacing
    const int colField       = 3; // Hier sitzen die Werte/Felder
    const int colRightMargin = 4; // Virtueller Rand rechts

    // Width of spacer columns
    fileinfoGridLayout->setColumnMinimumWidth(colLeftMargin, 10);
    fileinfoGridLayout->setColumnMinimumWidth(colSpacingH, 20);
    fileinfoGridLayout->setColumnMinimumWidth(colRightMargin, 10);

    // Set colField to stretch dynamically
    fileinfoGridLayout->setColumnStretch(colField, 1);

    int row = 0;

    // ================= BLOCK 1 =================
    fileinfoGridLayout->addWidget(new QLabel(tr("Type:")), row, colLabel);
    m_typeLabel = new QLabel();
    m_typeLabel->setText(getFileType(fileInfo));
    m_typeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileinfoGridLayout->addWidget(m_typeLabel, row, colField);
    row++;

    if (!isDrive) {
        fileinfoGridLayout->addWidget(new QLabel(tr("MimeType:")), row, colLabel);
        m_mimeLabel = new QLabel();
        QMimeDatabase mimeDb;
        QMimeType mime = mimeDb.mimeTypeForFile(absoluteFilePath);  // .mimeTypeForFile goes by file extension AND if neccessary by content (magic bytes)
        m_mimeLabel->setText(mime.name());
        m_mimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fileinfoGridLayout->addWidget(m_mimeLabel, row, colField);
        row++;
    }

    fileinfoGridLayout->addWidget(new QLabel(tr("Path:")), row, colLabel);
    m_pathEdit = new QLabel();
    m_pathEdit->setText(QDir::toNativeSeparators(fileInfo.absolutePath()));
    m_pathEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileinfoGridLayout->addWidget(m_pathEdit, row, colField);
    row++;


    fileinfoGridLayout->addWidget(new QLabel(tr("Size:")), row, colLabel);
    m_sizeLabel = new QLabel();
    if (isDrive) {
        QStorageInfo storageInfo(fileInfo.absoluteFilePath());
        qint64 free = storageInfo.bytesFree();
        qint64 total = storageInfo.bytesTotal();
        m_sizeLabel->setText(tr("%1 (%2 free)").arg(formatAdaptiveSize(total)).arg(formatAdaptiveSize(free)));
    } else if (fileInfo.isDir()) {
       m_sizeLabel->setText(tr("Calculating size..."));    // will be changed again by updateUiAsyncStart() later on
    } else {
        if (fileInfo.size() <= 1024) {
            m_sizeLabel->setText(formatAdaptiveSize(fileInfo.size()));
        } else {
            m_sizeLabel->setText(tr("%1 (%2 Bytes)").arg(formatAdaptiveSize(fileInfo.size())).arg(m_locale.toString(fileInfo.size())));
        }
    }
    m_sizeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileinfoGridLayout->addWidget(m_sizeLabel, row, colField);
    row++;

    if (fileInfo.isDir() && !isDrive) {
        m_containsLabel = new QLabel(tr("Calculating content..."));
        m_containsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fileinfoGridLayout->addWidget(new QLabel(tr("Content:")), row, colLabel);
        fileinfoGridLayout->addWidget(m_containsLabel, row, colField);
        row++;
    }
#ifdef Q_OS_WIN
    else if (fileInfo.isShortcut()) {

        // ================= TRENNLINIE =================
        QFrame *line5 = new QFrame();
        line5->setFrameShape(QFrame::HLine);
        line5->setFrameShadow(QFrame::Sunken);
        // Die Linie startet bei Spalte 0 (ganz links) und spannt sich über alle 4 Spalten!
        // Parameter: addWidget(widget, row, column, rowSpan, columnSpan)
        fileinfoGridLayout->addWidget(line5, row, colLeftMargin, 1, 5);
        row++;


        fileinfoGridLayout->addWidget(new QLabel(tr("Target:")), row, colLabel);
        m_linkTargetEdit = new QLineEdit();
        fileinfoGridLayout->addWidget(m_linkTargetEdit, row, colField);
        row++;

        fileinfoGridLayout->addWidget(new QLabel(tr("Arguments:")), row, colLabel);
        m_linkArgumentsEdit = new QLineEdit();
        fileinfoGridLayout->addWidget(m_linkArgumentsEdit, row, colField);
        row++;

        fileinfoGridLayout->addWidget(new QLabel(tr("WorkDir:")), row, colLabel);
        m_linkWorkingDirectoryEdit = new QLineEdit();
        fileinfoGridLayout->addWidget(m_linkWorkingDirectoryEdit, row, colField);

        WinShortcutDetails shortcutDetails;
        if (getWindowsShortcutDetails(absoluteFilePath, shortcutDetails)) {
            m_linkTargetEdit->setText(shortcutDetails.targetPath);
            m_linkArgumentsEdit->setText(shortcutDetails.arguments);
            m_linkWorkingDirectoryEdit->setText(shortcutDetails.workingDirectory);
        } else {
            m_linkTargetEdit->setEnabled(false);
            m_linkArgumentsEdit->setEnabled(false);
            m_linkWorkingDirectoryEdit->setEnabled(false);
        }
        row++;
    }
    else if (fileInfo.isJunction()) {
        // ================= TRENNLINIE =================
        QFrame *line5 = new QFrame();
        line5->setFrameShape(QFrame::HLine);
        line5->setFrameShadow(QFrame::Sunken);
        // Die Linie startet bei Spalte 0 (ganz links) und spannt sich über alle 4 Spalten!
        // Parameter: addWidget(widget, row, column, rowSpan, columnSpan)
        fileinfoGridLayout->addWidget(line5, row, colLeftMargin, 1, 5);
        row++;


        fileinfoGridLayout->addWidget(new QLabel(tr("Target:")), row, colLabel);
        m_linkTargetEdit = new QLineEdit();
        m_linkTargetEdit->setText(fileInfo.junctionTarget());
        fileinfoGridLayout->addWidget(m_linkTargetEdit, row, colField);
        row++;
    }
#endif
    else if (fileInfo.isSymbolicLink()) {
        // ================= TRENNLINIE =================
        QFrame *line5 = new QFrame();
        line5->setFrameShape(QFrame::HLine);
        line5->setFrameShadow(QFrame::Sunken);
        // Die Linie startet bei Spalte 0 (ganz links) und spannt sich über alle 4 Spalten!
        // Parameter: addWidget(widget, row, column, rowSpan, columnSpan)
        fileinfoGridLayout->addWidget(line5, row, colLeftMargin, 1, 5);
        row++;


        fileinfoGridLayout->addWidget(new QLabel(tr("Target:")), row, colLabel);
        m_linkTargetEdit = new QLineEdit();
        m_linkTargetEdit->setText(fileInfo.symLinkTarget());
        fileinfoGridLayout->addWidget(m_linkTargetEdit, row, colField);
        row++;
    }


    // ================= TRENNLINIE =================
    QFrame *line6 = new QFrame();
    line6->setFrameShape(QFrame::HLine);
    line6->setFrameShadow(QFrame::Sunken);
    // Die Linie startet bei Spalte 0 (ganz links) und spannt sich über alle 4 Spalten!
    // Parameter: addWidget(widget, row, column, rowSpan, columnSpan)
    fileinfoGridLayout->addWidget(line6, row, colLeftMargin, 1, 5);
    row++;

    if (!isDrive) {
        // ================= BLOCK 2 =================
        fileinfoGridLayout->addWidget(new QLabel(tr("Created:")), row, colLabel);
        m_createdLabel = new QLabel();
        m_createdLabel->setText(fileInfo.birthTime().toString("yyyy-MM-dd  HH:mm:ss"));
        m_createdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fileinfoGridLayout->addWidget(m_createdLabel, row, colField);
        row++;

        fileinfoGridLayout->addWidget(new QLabel(tr("Modified:")), row, colLabel);
        m_modifiedLabel = new QLabel();
        m_modifiedLabel->setText(fileInfo.lastModified().toString("yyyy-MM-dd  HH:mm:ss"));
        m_modifiedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fileinfoGridLayout->addWidget(m_modifiedLabel, row, colField);
        row++;

        fileinfoGridLayout->addWidget(new QLabel(tr("Accessed:")), row, colLabel);
        m_lastReadLabel = new QLabel();
        m_lastReadLabel->setText(fileInfo.lastRead().toString("yyyy-MM-dd  HH:mm:ss"));
        m_lastReadLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fileinfoGridLayout->addWidget(m_lastReadLabel, row, colField);
        row++;

        // ================= TRENNLINIE =================
        QFrame *line3 = new QFrame();
        line3->setFrameShape(QFrame::HLine);
        line3->setFrameShadow(QFrame::Sunken);
        fileinfoGridLayout->addWidget(line3, row, colLeftMargin, 1, 5);
        row++;
    }

#ifdef Q_OS_WIN
    if (!isDrive) {
        // Windows Attributes
        DWORD fileAttr = getWindowsFileAttributes(fileInfo.filePath());
        QDir parentDir = fileInfo.dir();
        bool canModifyAttributes = QFileInfo(parentDir.absolutePath()).isWritable();

        fileinfoGridLayout->addWidget(new QLabel(tr("Attributes:")), row, colLabel);
        m_readOnlyCB = new QCheckBox(tr("Read-only"));
        m_readOnlyCB->setChecked(fileAttr & FILE_ATTRIBUTE_READONLY);
        m_readOnlyCB->setEnabled(canModifyAttributes);
        fileinfoGridLayout->addWidget(m_readOnlyCB, row, colField);
        row++;

        m_hiddenCB = new QCheckBox(tr("Hidden"));
        m_hiddenCB->setChecked(fileAttr & FILE_ATTRIBUTE_HIDDEN);
        m_hiddenCB->setEnabled(canModifyAttributes);
        fileinfoGridLayout->addWidget(m_hiddenCB, row, colField);
        row++;

        m_systemCB = new QCheckBox(tr("System"));
        m_systemCB->setChecked(fileAttr & FILE_ATTRIBUTE_SYSTEM);
        m_systemCB->setEnabled(canModifyAttributes);
        fileinfoGridLayout->addWidget(m_systemCB, row, colField);
        row++;
    }
#elif defined(Q_OS_LINUX)

    m_ownerLabel = new QLabel();
    m_ownerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_groupLabel = new QLabel();
    m_groupLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_permLabel = new QLabel(); // Label für die Oktal-Anzeige (z.B. 755)
    m_permLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    fileinfoGridLayout->addWidget(new QLabel(tr("Owner:")), row, colLabel);
    fileinfoGridLayout->addWidget(m_ownerLabel, row, colField);
    row++;

    fileinfoGridLayout->addWidget(new QLabel(tr("Group:")), row, colLabel);
    fileinfoGridLayout->addWidget(m_groupLabel, row, colField);
    row++;

    fileinfoGridLayout->addWidget(new QLabel(tr("Permissions:")), row, colLabel);
    fileinfoGridLayout->addWidget(m_permLabel, row, colField);
    row++;

    // Eine kleine Matrix für die Übersicht
    auto *permGrid = new QGridLayout();
    permGrid->setContentsMargins(0,0,0,0);
    permGrid->setSpacing(5);
    permGrid->setAlignment(Qt::AlignLeft);
    permGrid->setColumnStretch(4, 1);

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
            m_permCBs[i][j] = cb;
            permGrid->addWidget(cb, i + 1, j + 1);
        }
    }

    fileinfoGridLayout->addLayout(permGrid, row, colField);
    row++;

    // Fill
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
    bool canModify = canModifyPermissions(fileInfo);

    int owner = ((p & QFile::ReadOwner) ? 4 : 0) + ((p & QFile::WriteOwner) ? 2 : 0) + ((p & QFile::ExeOwner) ? 1 : 0);
    int group = ((p & QFile::ReadGroup) ? 4 : 0) + ((p & QFile::WriteGroup) ? 2 : 0) + ((p & QFile::ExeGroup) ? 1 : 0);
    int other = ((p & QFile::ReadOther) ? 4 : 0) + ((p & QFile::WriteOther) ? 2 : 0) + ((p & QFile::ExeOther) ? 1 : 0);

    m_permLabel->setText(QString("0%1%2%3").arg(owner).arg(group).arg(other));

    m_permCBs[0][0]->setChecked(p & QFile::ReadOwner);
    m_permCBs[0][1]->setChecked(p & QFile::WriteOwner);
    m_permCBs[0][2]->setChecked(p & QFile::ExeOwner);
    m_permCBs[0][0]->setEnabled(canModify);
    m_permCBs[0][1]->setEnabled(canModify);
    m_permCBs[0][2]->setEnabled(canModify);

    m_permCBs[1][0]->setChecked(p & QFile::ReadGroup);
    m_permCBs[1][1]->setChecked(p & QFile::WriteGroup);
    m_permCBs[1][2]->setChecked(p & QFile::ExeGroup);
    m_permCBs[1][0]->setEnabled(canModify);
    m_permCBs[1][1]->setEnabled(canModify);
    m_permCBs[1][2]->setEnabled(canModify);

    m_permCBs[2][0]->setChecked(p & QFile::ReadOther);
    m_permCBs[2][1]->setChecked(p & QFile::WriteOther);
    m_permCBs[2][2]->setChecked(p & QFile::ExeOther);
    m_permCBs[2][0]->setEnabled(canModify);
    m_permCBs[2][1]->setEnabled(canModify);
    m_permCBs[2][2]->setEnabled(canModify);

    // ================= TRENNLINIE =================
    QFrame *line4 = new QFrame();
    line4->setFrameShape(QFrame::HLine);
    line4->setFrameShadow(QFrame::Sunken);
    fileinfoGridLayout->addWidget(line4, row, colLeftMargin, 1, 5);
    row++;
#endif

    mainLayout->addLayout(fileinfoGridLayout);

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

    if (fileInfo.isDir() && !isDrive) {
        updateUiAsyncStart();
    }
}


void FilePropertiesDialog::updateUiAsyncStart() {
    m_watcher = new QFutureWatcher<ProgressResult>(this);

    connect(m_watcher, &QFutureWatcher<ProgressResult>::finished, this, [this]() {
        updateUiAsync(m_watcher->result());
    });

    auto computeTask = [this]() -> ProgressResult {
        ProgressResult res;
        QSet<QString> visitedDirs; // Das Set wird hier einmalig für den Task erstellt

        for (const QString &path : std::as_const(m_filePaths)) {
            if (m_abort.load())
                return {};

            QFileInfo info(path);
            if (info.isDir()) {
                res.folders++; // Der ausgewählte Hauptordner selbst

                int subFiles = 0;
                int subFolders = 0;

                // Funktion mit dem visitedDirs-Set aufrufen
                res.size += calculateFolderSize(path, subFiles, subFolders, visitedDirs, m_abort);

                res.files += subFiles;
                res.folders += subFolders;
            } else {
                res.files++;
                res.size += info.size();
            }
        }
        return res;
    };

    m_watcher->setFuture(QtConcurrent::run(computeTask));
}


void FilePropertiesDialog::updateUiAsync(ProgressResult result) {
    if (m_abort.load()) return;

    if (result.size <= 1024) {
        m_sizeLabel->setText(formatAdaptiveSize(result.size));
    } else {
        m_sizeLabel->setText(tr("%1 (%2 Bytes)")
                                 .arg(formatAdaptiveSize(result.size))
                                 .arg(m_locale.toString(result.size)));
    }

    if (m_containsLabel) { // Null pointer check since m_containsLabel is only created in single file mode if isDir()
        m_containsLabel->setText(tr("%1 Files, %2 Folders")
                                     .arg(m_locale.toString(result.files))
                                     .arg(m_locale.toString(result.folders)));
    }

    m_watcher->deleteLater();
    m_watcher = nullptr;
}

quint64 FilePropertiesDialog::calculateFolderSize(const QString &rootPath, int &fileCount, int &folderCount, QSet<QString> &visitedDirs, const std::atomic<bool> &abortFlag) {
    quint64 totalSize = 0;

    QStack<QString> dirStack;
    dirStack.push(rootPath);

    while (!dirStack.isEmpty()) {
        if (abortFlag.load())
            return totalSize;

        // Den nächsten Ordner vom Stack holen
        QString currentPath = dirStack.pop();
        QFileInfo dirInfo(currentPath);
        QString canonical = dirInfo.canonicalFilePath();

        if (canonical.isEmpty()) continue; // dangling symlink

        // SCHLEIFEN-SCHUTZ: Haben wir diesen echten Ort bereits gescannt?
        if (visitedDirs.contains(canonical)) {
            continue; // Überspringen, aber im Stack weitermachen
        }
        visitedDirs.insert(canonical);

        QDirIterator it(currentPath, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

        while (it.hasNext()) {
            if (abortFlag.load())
                return totalSize;

            it.next();
            QFileInfo info = it.fileInfo();

            if (info.isDir()) {
                folderCount++;

                // Statt die Funktion neu aufzurufen, legen wir den Pfad
                // einfach auf unseren Heap-Stack. Die Schleife arbeitet ihn ab.
                dirStack.push(info.absoluteFilePath());
            } else {
                totalSize += info.size();
                fileCount++;
            }
        }
    }

    return totalSize;
}

QString FilePropertiesDialog::getFileType(const QFileInfo &info) {
    bool isDrive = false;
#ifdef Q_OS_WIN
    QString absoluteFilePath = info.absoluteFilePath();
    isDrive = (absoluteFilePath.length() == 3 && absoluteFilePath.at(1) == ':' && (absoluteFilePath.at(2) == '/' || absoluteFilePath.at(2) == '\\'));
    if (isDrive) {
        QStorageInfo storage(info.absoluteFilePath());
        QString volumeType;

        if (storage.fileSystemType() == "CDFS" || storage.fileSystemType() == "UDF") {
            volumeType = tr("Optical Drive");
        } else {
            volumeType = tr("Local Drive");
        }

        volumeType = volumeType + " (" + storage.fileSystemType() + ")";
        return volumeType;
    }
#endif

    if (info.isSymbolicLink()) {
        return tr("SymLink");
    }
#ifdef Q_OS_WIN
    else if (info.isJunction()) {
        return tr("Junction");
    }
    else if (info.isShortcut()) {
        return tr("Shortcut");
    }
#endif
    else if (info.isDir()) {
        return tr("Folder");
    }

    return info.suffix().isEmpty() ? tr("File") : info.suffix().toUpper() + tr("-File");
}

void FilePropertiesDialog::onOkPressed() {
    if (!m_isMultiMode && !m_filePaths.isEmpty()) {
        QString filePath = m_filePaths.first();

#ifdef Q_OS_WIN
        QFileInfo fileInfo(filePath);
        QString absoluteFilePath = fileInfo.absoluteFilePath();
        bool isDrive = (absoluteFilePath.length() == 3 && absoluteFilePath.at(1) == ':' && (absoluteFilePath.at(2) == '/' || absoluteFilePath.at(2) == '\\'));

        if (!isDrive) {
            // Aktuelle Attribute holen
            DWORD attr = GetFileAttributesW(reinterpret_cast<const WCHAR*>(filePath.utf16()));

            if (attr != INVALID_FILE_ATTRIBUTES) {
                // Bits basierend auf Checkboxen setzen oder löschen
                if (m_readOnlyCB->isChecked()) attr |= FILE_ATTRIBUTE_READONLY;
                else                           attr &= ~FILE_ATTRIBUTE_READONLY;

                if (m_hiddenCB->isChecked())   attr |= FILE_ATTRIBUTE_HIDDEN;
                else                           attr &= ~FILE_ATTRIBUTE_HIDDEN;

                if (m_systemCB->isChecked())   attr |= FILE_ATTRIBUTE_SYSTEM;
                else                           attr &= ~FILE_ATTRIBUTE_SYSTEM;

                // Zurückschreiben
                if (!SetFileAttributesW(reinterpret_cast<const WCHAR*>(filePath.utf16()), attr)) {
                    QMessageBox::warning(this, tr("Error"),
                                         tr("Failed to apply file attributes.\nDo you have permission to modify this file?"));
                    return; // Dialog offen lassen
                }
            }
        }

        if (fileInfo.isShortcut()) {
            WinShortcutDetails shortcutDetails;
            if (getWindowsShortcutDetails(filePath, shortcutDetails)) {
                shortcutDetails.targetPath       = m_linkTargetEdit->text();
                shortcutDetails.arguments        = m_linkArgumentsEdit->text();
                shortcutDetails.workingDirectory = m_linkWorkingDirectoryEdit->text();

                if (!setWindowsShortcutDetails(filePath, shortcutDetails)) {
                    qCritical() << "Fehler beim Schreiben der Verknüpfung";
                    // Hier ggf. eine QMessageBox mit einer Fehlermeldung anzeigen
                }
            }
        }

#elif defined(Q_OS_LINUX)
        QFile::Permissions newPerms = QFile::Permissions();

        // 1. Besitzer-Rechte (Row 0)
        if (m_permCBs[0][0]->isChecked()) newPerms |= QFile::ReadOwner;
        if (m_permCBs[0][1]->isChecked()) newPerms |= QFile::WriteOwner;
        if (m_permCBs[0][2]->isChecked()) newPerms |= QFile::ExeOwner;

        // 2. Gruppen-Rechte (Row 1)
        if (m_permCBs[1][0]->isChecked()) newPerms |= QFile::ReadGroup;
        if (m_permCBs[1][1]->isChecked()) newPerms |= QFile::WriteGroup;
        if (m_permCBs[1][2]->isChecked()) newPerms |= QFile::ExeGroup;

        // 3. Andere-Rechte (Row 2)
        if (m_permCBs[2][0]->isChecked()) newPerms |= QFile::ReadOther;
        if (m_permCBs[2][1]->isChecked()) newPerms |= QFile::WriteOther;
        if (m_permCBs[2][2]->isChecked()) newPerms |= QFile::ExeOther;

        // Berechtigungen auf das Dateisystem anwenden
        if (!QFile::setPermissions(filePath, newPerms)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Failed to set permissions for:\n%1\n\nDo you have sufficient rights?").arg(filePath));
            return; // Dialog nicht schließen, damit der Nutzer es korrigieren kann
        }
#endif
    }
    accept();
}

#ifdef Q_OS_WIN
DWORD FilePropertiesDialog::getWindowsFileAttributes(const QString &filePath) {
    return GetFileAttributesW(reinterpret_cast<const WCHAR*>(filePath.utf16()));
}

bool FilePropertiesDialog::getWindowsShortcutDetails(const QString &lnkFilePath, WinShortcutDetails &details) {
    bool success = false;

    // COM-Bibliothek für diesen Thread initialisieren (falls noch nicht geschehen)
    // Qt initialisiert COM meistens schon intern, aber das hier ist die sichere Variante
    HRESULT hres = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IShellLinkW* psl = nullptr;
    // 1. Instanz der ShellLink-Klasse erstellen
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl))) {
        IPersistFile* ppf = nullptr;

        // 2. Interface zum Laden von Dateien abfragen
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {

            // 3. Die .lnk Datei laden (wichtig: .utf16() für wchar_t Kompatibilität)
            if (SUCCEEDED(ppf->Load(reinterpret_cast<const wchar_t*>(lnkFilePath.utf16()), STGM_READ))) {

                wchar_t szBuf[MAX_PATH];
                wchar_t szArgs[INFOTIPSIZE]; // Größerer Puffer für Argumente
                wchar_t szDir[MAX_PATH];
                wchar_t szDesc[INFOTIPSIZE];

                // 4. Alle gewünschten Eigenschaften auslesen
                // Ziel-Pfad
                if (SUCCEEDED(psl->GetPath(szBuf, MAX_PATH, nullptr, SLGP_RAWPATH))) {
                    details.targetPath = QString::fromWCharArray(szBuf);
                }

                // Argumente
                if (SUCCEEDED(psl->GetArguments(szArgs, INFOTIPSIZE))) {
                    details.arguments = QString::fromWCharArray(szArgs);
                }

                // Arbeitsverzeichnis (Working Directory / "Ausführen in")
                if (SUCCEEDED(psl->GetWorkingDirectory(szDir, MAX_PATH))) {
                    details.workingDirectory = QString::fromWCharArray(szDir);
                }

                // Beschreibung / Kommentar
                if (SUCCEEDED(psl->GetDescription(szDesc, INFOTIPSIZE))) {
                    details.description = QString::fromWCharArray(szDesc);
                }

                success = true;
            }
            ppf->Release();
        }
        psl->Release();
    }

    // Falls CoInitialize erfolgreich war, wieder freigeben
    if (SUCCEEDED(hres)) {
        CoUninitialize();
    }

    return success;
}

bool FilePropertiesDialog::setWindowsShortcutDetails(const QString &lnkFilePath, const WinShortcutDetails &details) {
    bool success = false;

    // COM-Bibliothek initialisieren
    HRESULT hres = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IShellLinkW* psl = nullptr;
    // 1. Instanz der ShellLink-Klasse anfordern
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl))) {
        IPersistFile* ppf = nullptr;

        // 2. Interface für Dateioperationen abfragen
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {

            // 3. WICHTIG: Die bestehende Datei im Lese-/Schreibmodus (STGM_READWRITE) laden
            if (SUCCEEDED(ppf->Load(reinterpret_cast<const wchar_t*>(lnkFilePath.utf16()), STGM_READWRITE))) {

                // 4. Werte über die Set-Methoden neu schreiben
                psl->SetPath(reinterpret_cast<const wchar_t*>(details.targetPath.utf16()));
                psl->SetArguments(reinterpret_cast<const wchar_t*>(details.arguments.utf16()));
                psl->SetWorkingDirectory(reinterpret_cast<const wchar_t*>(details.workingDirectory.utf16()));
                psl->SetDescription(reinterpret_cast<const wchar_t*>(details.description.utf16()));

                // 5. Änderungen dauerhaft speichern
                // Wenn der erste Parameter 'nullptr' ist, speichert Windows die Daten
                // automatisch wieder unter dem Pfad ab, aus dem sie geladen wurden.
                // 'TRUE' sagt Windows, dass die Datei danach als "sauber" (not dirty) gilt.
                if (SUCCEEDED(ppf->Save(nullptr, TRUE))) {
                    success = true;
                }
            }
            ppf->Release();
        }
        psl->Release();
    }

    if (SUCCEEDED(hres)) {
        CoUninitialize();
    }

    return success;
}
#endif

bool FilePropertiesDialog::canModifyPermissions(const QFileInfo &fileInfo) {
#if defined(Q_OS_LINUX)
    uid_t currentUid = getuid();

    // 1. Wenn wir Root (ID 0) sind, dürfen wir alles.
    // 2. Wenn uns die Datei gehört, dürfen wir die Rechte ändern.
    if (currentUid == 0 || fileInfo.ownerId() == currentUid) {
        return true;
    }
    return false;
#else
    // Unter Windows ist das Rechtesystem (ACLs) völlig anders.
    // Hier ist isWritable() oft ein guter, pragmatischer Indikator.
    return fileInfo.isWritable();
#endif
}

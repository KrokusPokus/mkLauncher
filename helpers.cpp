#include <QDir>
#include <QMimeType>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <zlib.h>
#include "helpers.h"

bool isTextFile(const QString &filePath) {
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    return mime.inherits("text/plain");
}

QString cleanFileName(const QString &fileName) {
    QString pattern;

#ifdef Q_OS_WIN
    // Windows verbotene Zeichen: < > : " / \ | ? *
    // Plus: Steuerzeichen (0-31)
    pattern = "[<>:\"/\\\\|?*\\x00-\\x1F]";
#else
    // Linux/Unix verbotene Zeichen: / und der Null-Terminator \0
    pattern = "[/\\x00]";
#endif

    static const QRegularExpression re(pattern);

    QString cleanedName = fileName; // local copy
    cleanedName.replace(re, "_");

#ifdef Q_OS_WIN
    cleanedName = cleanedName.trimmed();
    while (cleanedName.endsWith('.')) {
        cleanedName.chop(1);
    }
#endif

    return cleanedName;
}

QString formatAdaptiveSize(qint64 bytes) {
    static const QLocale locale = QLocale::system();

    if (bytes < 1024) {
        return locale.toString(bytes) + " Bytes";
    }

    double size = static_cast<double>(bytes);
    static const QStringList units = {"Bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    int precision = 0;
    if (size < 10.0) {
        precision = 2; // z.B. 1,23 MiB
    } else if (size < 100.0) {
        precision = 1; // z.B. 12,3 MiB
    } else {
        precision = 0; // z.B. 123 MiB
    }

    return locale.toString(size, 'f', precision) + " " + units[unitIndex];
}

quint32 calculateCRC32(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0; // Or handle error accordingly
    }

    // Initialize the CRC value
    uLong crc = crc32(0L, Z_NULL, 0);

    // Read in chunks to be memory efficient
    const int bufferSize = 1024 * 1024; // 1 MB buffer
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        // Update the CRC with the current chunk
        crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), bytesRead);
    }

    file.close();
    return static_cast<quint32>(crc);
}

bool atWordBoundary(const QString &fileName, const QString &word) {
    static const QString separators = " .(-_[";
    int pos = 0;
    while ((pos = fileName.indexOf(word, pos, Qt::CaseInsensitive)) != -1) {
        if (pos > 0 && separators.contains(fileName[pos - 1])) {
            return true;
        }
        pos += word.length();
    }
    return false;
};

uint getNameMatchQuality(const QFileInfo &fileInfo, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList) {
    QString sFilePath = fileInfo.filePath();
    QString sFileName = fileInfo.fileName();
    QString sBaseNameComplete = fileInfo.completeBaseName();    // for "/home/user/archive.tar.gz" this would return "archive.tar"
    QString sBaseName =  fileInfo.baseName();                   // for "/home/user/archive.tar.gz" this would return "archive"

    if ((QString::compare(sFileName, searchString, Qt::CaseInsensitive) == 0) || (QString::compare(sBaseNameComplete, searchString, Qt::CaseInsensitive) == 0) || (QString::compare(sBaseName, searchString, Qt::CaseInsensitive) == 0)) {
        return 99 - recentOpenList.indexOf(sFilePath);
    }

    if (sFileName.startsWith(searchString, Qt::CaseInsensitive)) {
        return 199 - recentOpenList.indexOf(sFilePath);
    }

    if (searchString.contains("/", Qt::CaseSensitive) || searchString.contains("\\", Qt::CaseSensitive)) {
        if (sFilePath.contains(searchString, Qt::CaseInsensitive)) {
            return 299 - recentOpenList.indexOf(sFilePath);
        }
    }

    // Match search terms separatately
    bool bMatchType1 = true;
    bool bMatchType2 = false;
    bool bMatchType3 = false;
    int iIndex = 0;
    int iFoundPos = 0;

    for (const QString &word : std::as_const(searchStringSplit)) {
        iFoundPos = sFileName.indexOf(word, 0, Qt::CaseInsensitive);

        if (iFoundPos == -1) { // not found
            bMatchType1 = false;
            break;
        }

        if (iFoundPos == 0) {   // Improved match, since one searchterm found at very beginning of filename
            bMatchType2 = true;
            if (iIndex == 0) {  // Further improved match, since *first* searchterm found at very beginning of filename
                bMatchType3 = true;
            }
        } else if (atWordBoundary(sFileName, word)) {
            // Improved match: one searchterm found at a word boundary in filename
            bMatchType2 = true;
        }

        iIndex++;
    }

    if (bMatchType1 == false) {
        return 0;
    }

    if (bMatchType3 == true) {
        return 399 - recentOpenList.indexOf(sFilePath);
    } else if (bMatchType2 == true) {
        return 499 - recentOpenList.indexOf(sFilePath);
    } else {
        return 599 - recentOpenList.indexOf(sFilePath);
    }
}

// Simplified version for name of app given in *.desktop files
uint getDesktopNameMatchQuality(const QFileInfo &fileInfo, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList, const QString &desktopName) {
//qDebug() << "searchstring =" << searchString << "vs desktopName=" << desktopName;
    QString sFilePath = fileInfo.filePath();

    if (QString::compare(desktopName, searchString, Qt::CaseInsensitive) == 0) {
        return 99 - recentOpenList.indexOf(sFilePath);
    }

    if (desktopName.startsWith(searchString, Qt::CaseInsensitive)) {
        return 199 - recentOpenList.indexOf(sFilePath);
    }

    // Match search terms separatately
    bool bMatchType1 = true;
    bool bMatchType2 = false;
    bool bMatchType3 = false;
    int iIndex = 0;
    int iFoundPos = 0;

    for (const QString &word : std::as_const(searchStringSplit)) {
        iFoundPos = desktopName.indexOf(word, 0, Qt::CaseInsensitive);

        if (iFoundPos == -1) { // not found
            bMatchType1 = false;
            break;
        }

        if (iFoundPos == 0) {   // Improved match, since one searchterm found at very beginning of filename
            bMatchType2 = true;
            if (iIndex == 0) {  // Further improved match, since *first* searchterm found at very beginning of filename
                bMatchType3 = true;
            }
        } else if (atWordBoundary(desktopName, word)) {
            // Improved match: one searchterm found at a word boundary in filename
            bMatchType2 = true;
        }

        iIndex++;
    }

    if (bMatchType1 == false) {
        return 0;
    }

    if (bMatchType3 == true) {
        return 399 - recentOpenList.indexOf(sFilePath);
    } else if (bMatchType2 == true) {
        return 499 - recentOpenList.indexOf(sFilePath);
    } else {
        return 599 - recentOpenList.indexOf(sFilePath);
    }
}

void launchDesktopFile(const QFileInfo &fileInfo) {
    QFile file(fileInfo.filePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString execCommand, execPath;
    bool inMainSection = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.startsWith('[') && line.endsWith(']')) {
            if (inMainSection && !execCommand.isEmpty()) break; // if this is a new section but we already got execCommand, break
            inMainSection = (line == "[Desktop Entry]");
            continue;
        }

        if (!inMainSection) continue;
        if (line.startsWith("Exec=")) execCommand = line.mid(5);
        else if (line.startsWith("Path=")) execPath = line.mid(5);
    }

    if (!execCommand.isEmpty()) {
        execCommand.remove(QRegularExpression("%[uUfFiIcK]"));
        QString program = execCommand.trimmed();
        QStringList arguments;

        QStringList tokens = QProcess::splitCommand(program);
        if (!tokens.isEmpty()) {
            program = tokens.takeFirst();
            arguments = tokens;
        }

        if (program.startsWith("$HOME")) {
            QString home = qgetenv("HOME"); // Holt /home/benutzer
            program.replace("$HOME", home);
        } else if (program.startsWith("~")) {
            program.replace(0, 1, QDir::homePath());
        }

        if (!QFileInfo(program).isAbsolute()) {
            QString absoluteProgram = QStandardPaths::findExecutable(program);
            if (absoluteProgram.isEmpty()) {
                qWarning() << "Couldn't find path for binary:" << program;
                return;
            }
            program = absoluteProgram;
        }

        QString workingDir = execPath.trimmed();
        if (workingDir.isEmpty()) {
            if (workingDir.startsWith("$HOME")) {
                QString home = qgetenv("HOME"); // Holt /home/benutzer
                workingDir.replace("$HOME", home);
            } else if (workingDir.startsWith("~")) {
                workingDir.replace(0, 1, QDir::homePath());
            }

            workingDir = QFileInfo(program).absolutePath();
        }

        QProcess::startDetached(program, arguments, workingDir);
    }
}

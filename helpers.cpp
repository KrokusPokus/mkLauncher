#include "helpers.h"

#include <QDir>
#include <QMimeType>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUrl>
#include <zlib.h>

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

QString formatAdaptiveSize(quint64 bytes) {
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

DesktopEntry getDesktopEntryById(const QString &id) {
    static const QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dirPath : appDirs) {
        QFileInfo fileInfo(QDir(dirPath).filePath(id));
        DesktopEntry entry = getDesktopEntry(fileInfo);
        if (entry.isValid) {
            return entry;
        }
    }
    return DesktopEntry();
}

DesktopEntry getDesktopEntry(const QFileInfo &fileInfo) {

    static const QString localeName = QLocale::system().name().left(2); // "de", "fr", ...
    static const QString localKey = QString("Name[%1]=").arg(localeName);

    DesktopEntry currentEntry;

    QFile file(fileInfo.filePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);

        QString iniName, iniNameLocalised;
        bool bInMainSection = false;

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            if (line.startsWith('[') && line.endsWith(']')) {
                if (bInMainSection) break;
                bInMainSection = (line == "[Desktop Entry]");
                continue;
            }

            if (!bInMainSection || line.startsWith('#')) continue;

            if (line.startsWith("Name=")) iniName = line.mid(5);
            else if (line.startsWith(localKey)) iniNameLocalised = line.mid(localKey.size());
            else if (line.startsWith("Icon=")) currentEntry.icon = line.mid(5);
            else if (line.startsWith("Exec=")) currentEntry.exec = line.mid(5);
            else if (line.startsWith("Path=")) currentEntry.workDir = line.mid(5);
            else if (line.startsWith("Hidden=true")) {
                return DesktopEntry();
            }
        }

        currentEntry.name = iniNameLocalised.isEmpty() ? iniName : iniNameLocalised;
        currentEntry.isValid = !currentEntry.name.isEmpty();
        currentEntry.id = fileInfo.fileName();
        currentEntry.path = fileInfo.filePath();

        if (currentEntry.isValid) {
            return currentEntry;
        }
    }

    return DesktopEntry();
}

void openFileListWithHandler(const QString &handlerApp, const QStringList &fileList) {
    if (handlerApp.endsWith(".desktop", Qt::CaseInsensitive)) {
        launchDesktopFile(getDesktopEntryById(handlerApp), fileList);
        return;
    }

    if (QFile::exists(handlerApp)) {
        QProcess::startDetached(QDir::toNativeSeparators(handlerApp), fileList);
        return;
    }

    QString absolutePath = QStandardPaths::findExecutable(handlerApp);
    if (!absolutePath.isEmpty()) {
        // bool QProcess::startDetached(const QString &program, const QStringList &arguments = {}, const QString &workingDirectory = QString(), qint64 *pid = nullptr)
        QProcess::startDetached(QDir::toNativeSeparators(absolutePath), fileList);
        return;
    }

    qDebug() << "openFileListWithHandler() No absolute path found for:" << handlerApp;
}

void launchDesktopFile(const DesktopEntry &info, const QStringList &fileList) {
    if (info.exec.isEmpty()) return;

    QStringList args = QProcess::splitCommand(info.exec);
    if (args.isEmpty()) return;
    QString program = args.takeFirst();

    if (!fileList.isEmpty()) {
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "%f" || args[i] == "%u") {
                args[i] = fileList.first();
            } else if (args[i] == "%F" || args[i] == "%U") {
                args.removeAt(i);
                for (int j = 0; j < fileList.size(); ++j)
                    args.insert(i + j, fileList.at(j));
                break;
            }
        }
        if (!info.exec.contains('%'))
            args.append(fileList);
    } else {
        static const QRegularExpression placeholders("%[uUfFiIcK]");
        args.removeIf([&](const QString &a) { return a.contains(placeholders); });
    }

    auto expandHome = [](QString path) -> QString {
        if (path.startsWith("$HOME") || path.startsWith('~')) {
            const QString home = QDir::homePath();
            if (path.startsWith("$HOME"))
                return path.replace("$HOME", home);
            if (path.startsWith('~'))
                return path.replace(0, 1, home);
        }
        return path;
    };

    program = expandHome(program);
    if (!QFileInfo(program).isAbsolute()) {
        program = QStandardPaths::findExecutable(program);
        if (program.isEmpty()) {
            return;
        }
    }

    QString workDir = expandHome(info.workDir.trimmed());
    if (workDir.startsWith('"') && workDir.endsWith('"'))
        workDir = workDir.mid(1, workDir.length() - 2);
    if (workDir.isEmpty() || !QDir(workDir).exists())
        workDir = QFileInfo(program).absolutePath();

    QProcess::startDetached(program, args, workDir);
}

void browseToFile(const QString &path) {
#ifdef Q_OS_WIN
    QStringList args;
    if (!m_settings.fileManager.isEmpty()) {
        QFileInfo fileInfo(path);
        QString sDir = fileInfo.dir().path();
        qDebug() << m_settings.fileManager;
        args << "-p" << QDir::toNativeSeparators(sDir) << "-f" << fileInfo.fileName();
        QProcess::startDetached(QDir::toNativeSeparators(m_settings.fileManager), args);
    } else {
        QStringList args;
        args << "/select," + QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", args);
    }
#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    QProcess::startDetached("dbus-send", {
                                             "--session",
                                             "--print-reply",
                                             "--dest=org.freedesktop.FileManager1",
                                             "/org/freedesktop/FileManager1",
                                             "org.freedesktop.FileManager1.ShowItems",
                                             "array:string:" + QUrl::fromLocalFile(path).toString(),
                                             "string:\"\""
                                         });
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

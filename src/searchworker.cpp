#include "searchworker.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>        // needed for std::reverse();

void SearchWorker::process() {
    QElapsedTimer BenchmarkTimer;
    BenchmarkTimer.start();

    std::reverse(m_recentOpenList.begin(), m_recentOpenList.end());

    int nameMatchQuality = -1;
    QStringList searchStringSplit = m_searchString.split(' ', Qt::SkipEmptyParts);
    QList<SearchResult> resultsBatch;
    resultsBatch.reserve(1000); // small optimization. no measurable gains though.

#if defined(Q_OS_LINUX)
    static const QString currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toUpper();
    static const QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    // Note to self: appDirs contains the path in the order from user specific to system defaults
    // We read user specific first and remember which ones we already got in "seenIds" to prevent overwriting with system defaults
    QSet<QString> seenIds;

    const QString localeName1 = QLocale::system().name();
    const QString localeName2 = localeName1.left(2);
    static const QString localKeyLong = QString("Name[%1]=").arg(localeName1); // e.g. Name[de_DE]
    static const QString localKeyShort = QString("Name[%1]=").arg(localeName2); // e.g. Name[de]

    for (const QString &dirPath : appDirs) {
        QDirIterator iter(dirPath, {"*.desktop"}, QDir::NoDotAndDotDot | QDir::Files);   // Parameter: Pfad, Namensfilter, Filter-Flags

        while (iter.hasNext()) {
            iter.next();

            if (m_abort.load()) {
                qDebug() << "SearchWorker::process() aborted at" << BenchmarkTimer.elapsed() << "ms";
                emit searchStats(true);
                emit finished();
                return;
            }

            QString fileName = iter.fileName(); // Die ID (z.B. "firefox.desktop")

            if (!seenIds.contains(fileName)) {
                seenIds.insert(fileName);

                QString filePath = iter.filePath();
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    in.setEncoding(QStringConverter::Utf8);

                    QString iniName, iniNameLocalised, iniGenericName, iniKeywords;
                    bool bShowApplication = true;
                    bool bForceHidden = false;
                    uint iSystemSettings = 0;
                    bool bInMainSection = false;

                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();

                        if (line.startsWith('[') && line.endsWith(']')) {
                            if (bInMainSection) break; // we're already inside the main "[Desktop Entry]" and hit another section -> break

                            bInMainSection = (line == "[Desktop Entry]");
                            continue;
                        }

                        if (!bInMainSection || line.startsWith('#')) continue;

                        if (line.startsWith("Name=")) iniName = line.mid(5);
                        else if (line.startsWith(localKeyLong)) iniNameLocalised = line.mid(localKeyLong.size());
                        else if (line.startsWith(localKeyShort) && iniNameLocalised.isEmpty()) iniNameLocalised = line.mid(localKeyShort.size());
                        //else if (line.startsWith("GenericName=")) iniGenericName = line.mid(12);
                        else if (line.startsWith("NoDisplay=true")) bShowApplication = false;
                        else if (line.startsWith("Exec=systemsettings")) {
                            iSystemSettings++;
                            if (line == "Exec=systemsettings") {
                                iSystemSettings++;
                            }
                        } else if (line.startsWith("OnlyShowIn=")) {
                            QStringList onlyShowIn = line.mid(11).split(';', Qt::SkipEmptyParts);
                            if (!onlyShowIn.isEmpty()) {
                                if (!onlyShowIn.contains(currentDesktop)) {
                                    bForceHidden = true;
                                }
                            }
                        } else if (line.startsWith("NotShowIn=")) {
                            QStringList notShowIn = line.mid(10).split(';', Qt::SkipEmptyParts);
                            qDebug() << fileName << "notShowIn:" << notShowIn << "currentDesktop:" << currentDesktop;
                            if (!notShowIn.isEmpty()) {
                                if (notShowIn.contains(currentDesktop)) {
                                    bForceHidden = true;
                                }
                            }
                        }
                    }

                    if ((bShowApplication || iSystemSettings) && !bForceHidden) {
                        QString alternativeName = iniNameLocalised.isEmpty() ? iniName
                                                                             : iniNameLocalised + " " + iniName;
                        nameMatchQuality = getDesktopNameMatchQuality(filePath, m_searchString, searchStringSplit, m_recentOpenList, alternativeName);
                        if (nameMatchQuality == 0) {
                            continue;
                        }

                        SearchResult res;
                        res.fileInfo = iter.fileInfo();
                        res.nameMatchQuality = nameMatchQuality;

                        QString displayName = iniNameLocalised.isEmpty() ? iniName : iniNameLocalised;
                        if (iSystemSettings == 1) {
                            res.displayName = tr("System Settings:") + ' ' + displayName;
                        } else {
                            // If this is *not* systemsettings or is *exactly* systemsettings, don't prepend the term!
                            res.displayName = displayName;
                        }

                        resultsBatch.append(res);

                        if (resultsBatch.size() >= 1000) {
                            emit filesFoundBatch(resultsBatch);
                            resultsBatch.clear();
                        }
                    }
                }
            }
        }
    }
#endif

    // Search through list of user-defined folders
    for (const QString &searchDir : std::as_const(m_searchFolders)) {
        QDirIterator iter(searchDir, QDir::NoDotAndDotDot | QDir::System | QDir::Files, QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            iter.next();

            if (m_abort.load()) {
                qDebug() << "SearchWorker::process() aborted at" << BenchmarkTimer.elapsed() << "ms";
                emit searchStats(true);
                emit finished();
                return;
            }

            nameMatchQuality = getNameMatchQuality(iter.fileInfo(), m_searchString, searchStringSplit, m_recentOpenList);
            if (nameMatchQuality == 0) {
                continue;
            }

            SearchResult res;
            res.fileInfo = iter.fileInfo();
            res.nameMatchQuality = nameMatchQuality;

            resultsBatch.append(res);

            if (resultsBatch.size() >= 1000) {
                emit filesFoundBatch(resultsBatch);
                resultsBatch.clear();
            }
        }
    }

    if (!resultsBatch.isEmpty() && !m_abort.load()) {
        emit filesFoundBatch(resultsBatch); // Send the remainder
    }

    qDebug() << "SearchWorker::process() finished in" << BenchmarkTimer.elapsed() << "ms";
    emit searchStats(false);
    emit finished();
}

void SearchWorker::abort() {
    m_abort.store(true);
}

uint SearchWorker::getNameMatchQuality(const QFileInfo &fileInfo, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList) {
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
uint SearchWorker::getDesktopNameMatchQuality(const QString &sFilePath, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList, const QString &alternativeNames) {
    //qDebug() << "searchstring =" << searchString << "vs alternativeName=" << alternativeName;

    if (QString::compare(alternativeNames, searchString, Qt::CaseInsensitive) == 0) {
        return 99 - recentOpenList.indexOf(sFilePath);
    }

    if (alternativeNames.startsWith(searchString, Qt::CaseInsensitive)) {
        return 199 - recentOpenList.indexOf(sFilePath);
    }

    // Match search terms separatately
    bool bMatchType1 = true;
    bool bMatchType2 = false;
    bool bMatchType3 = false;
    int iIndex = 0;
    int iFoundPos = 0;

    for (const QString &word : std::as_const(searchStringSplit)) {
        iFoundPos = alternativeNames.indexOf(word, 0, Qt::CaseInsensitive);

        if (iFoundPos == -1) { // not found
            bMatchType1 = false;
            break;
        }

        if (iFoundPos == 0) {   // Improved match, since one searchterm found at very beginning of filename
            bMatchType2 = true;
            if (iIndex == 0) {  // Further improved match, since *first* searchterm found at very beginning of filename
                bMatchType3 = true;
            }
        } else if (atWordBoundary(alternativeNames, word)) {
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

bool SearchWorker::atWordBoundary(const QString &fileName, const QString &word) {
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

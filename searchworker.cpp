#include <algorithm>        // needed for std::reverse();
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QStandardPaths>
#include "searchworker.h"
#include "helpers.h"

void SearchWorker::process() {
    QElapsedTimer BenchmarkTimer;
    BenchmarkTimer.start();

    std::reverse(m_recentOpenList.begin(), m_recentOpenList.end());

    int nameMatchQuality = -1;
    QStringList searchStringSplit = m_searchString.split(' ', Qt::SkipEmptyParts);
    QList<SearchResult> resultsBatch;
    resultsBatch.reserve(1000); // small optimization. no measurable gains though.

#if defined(Q_OS_LINUX)
    QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    QSet<QString> seenIds;

    for (const QString &dirPath : std::as_const(appDirs)) {
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
                    bool bVisible = true;
                    bool bSystemSettings = false;
                    bool bInMainSection = false;

                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();

                        if (line.startsWith('[') && line.endsWith(']')) {
                            if (bInMainSection) break; // we were already inside the main "[Desktop Entry]" and hit another section, so break

                            bInMainSection = (line == "[Desktop Entry]");
                            continue;
                        }

                        if (!bInMainSection) continue;

                        if (line.startsWith("Name=")) iniName = line.mid(5);
                        else if (line.startsWith("Name[de]=")) iniNameLocalised = line.mid(9);
                        //else if (line.startsWith("GenericName=")) iniGenericName = line.mid(12);
                        //else if (line.startsWith("Keywords=")) iniKeywords = line.mid(9);
                        else if (line.startsWith("NoDisplay=true")) bVisible = false;
                        else if (line.startsWith("Exec=systemsettings")) bSystemSettings = true;
                    }

                    if (bVisible || bSystemSettings) {
                        QString alternativeName = (iniNameLocalised + " " + iniName);

                        nameMatchQuality = getDesktopNameMatchQuality(filePath, m_searchString, searchStringSplit, m_recentOpenList, alternativeName.trimmed());
                        if (nameMatchQuality == 0) {
                            continue;
                        }

                        SearchResult res;
                        res.fileInfo = iter.fileInfo();
                        res.nameMatchQuality = nameMatchQuality;
                        res.iniName = iniNameLocalised.isEmpty() ? iniName : iniNameLocalised;

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

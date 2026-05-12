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
    QStringList searchStringFilenameSplit = m_searchString.split(' ', Qt::SkipEmptyParts);
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

                QFile file(iter.filePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    QString iniName, iniGenericName, iniKeywords;
                    bool iniNoDisplay = false;
                    bool inMainSection = false;

                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();

                        // Sektions-Wechsel erkennen
                        if (line.startsWith('[') && line.endsWith(']')) {
                            // Wir sind nur an der Sektion [Desktop Entry] interessiert
                            inMainSection = (line == "[Desktop Entry]");
                            continue;
                        }

                        // Nur verarbeiten, wenn wir in der richtigen Sektion sind
                        if (!inMainSection) continue;

                        if (line.startsWith("Name=")) iniName = line.mid(5);
                        else if (line.startsWith("GenericName=")) iniGenericName = line.mid(12);
                        else if (line.startsWith("Keywords=")) iniKeywords = line.mid(9);
                        else if (line.startsWith("NoDisplay=true")) iniNoDisplay = true;
                    }

                    if (!iniNoDisplay) {
                        nameMatchQuality = getDesktopNameMatchQuality(iter.fileInfo(), m_searchString, searchStringFilenameSplit, m_recentOpenList, iniName);
                        if (nameMatchQuality == 0) {
                            continue;
                        }

                        SearchResult res;
                        res.fileInfo = iter.fileInfo();
                        res.nameMatchQuality = nameMatchQuality;
                        res.iniName = iniName;

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

            nameMatchQuality = getNameMatchQuality(iter.fileInfo(), m_searchString, searchStringFilenameSplit, m_recentOpenList);
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

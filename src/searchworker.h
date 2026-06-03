#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include <QFileInfo>
#include <QObject>

struct SearchResult {
    QFileInfo fileInfo;
    int nameMatchQuality;
    QString displayName;
};

// WICHTIG: Damit Qt diese Struktur zwischen Threads verschicken kann,
// muss sie bekannt gemacht werden (falls sie nicht nur einfache Typen enthält).
Q_DECLARE_METATYPE(SearchResult)

class SearchWorker : public QObject {
    Q_OBJECT

public:
    explicit SearchWorker(const QStringList &searchFolders, const QString &searchString, const QStringList &recentOpenList, QObject *parent = nullptr)
        : QObject(parent), m_searchFolders(searchFolders), m_searchString(searchString), m_recentOpenList(recentOpenList) {}

signals:
    void filesFoundBatch(const QList<SearchResult> &results);
    void searchStats(bool bSearchInterrupted);
    void finished();

public slots:
    void process();
    void abort();

private:
    uint getNameMatchQuality(const QFileInfo &fileInfo, const QString &searchStringFilename, const QStringList &searchStringSplit, const QStringList &recentOpenList);
    uint getDesktopNameMatchQuality(const QString &filePath, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList, const QString &alternativeNames);
    bool atWordBoundary(const QString &fileName, const QString &word);

    std::atomic<bool> m_abort{false};
    QStringList m_searchFolders;
    QString m_searchString;
    QStringList m_recentOpenList;
};

#endif // SEARCHWORKER_H

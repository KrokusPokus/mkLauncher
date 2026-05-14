#ifndef HELPERS_H
#define HELPERS_H

#include <QString>
#include <QFileInfo>

struct DesktopEntry {
    QString id;
    QString name;
    QString icon;
    QString exec;
    QString path;
    QString workDir;
    bool isValid = false;
};

bool isTextFile(const QString &filePath);
QString cleanFileName(const QString &fileName);
QString formatAdaptiveSize(quint64 bytes);
quint32 calculateCRC32(const QString &filePath);

bool atWordBoundary(const QString &fileName, const QString &word);
uint getNameMatchQuality(const QFileInfo &fileInfo, const QString &searchStringFilename, const QStringList &searchStringSplit, const QStringList &recentOpenList);
uint getDesktopNameMatchQuality(const QString &filePath, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList, const QString &alternativeNames);

DesktopEntry getDesktopEntryById(const QString &id);
DesktopEntry getDesktopEntry(const QFileInfo &fileInfo);
void openFileListWithHandler(const QString &handler, const QStringList &fileList);
void launchDesktopFile(const DesktopEntry &info, const QStringList &fileList = {});

#endif // HELPERS_H

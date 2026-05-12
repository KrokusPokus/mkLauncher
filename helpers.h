#ifndef HELPERS_H
#define HELPERS_H

#include <QString>
#include <QFileInfo>

bool isTextFile(const QString &filePath);
QString cleanFileName(const QString &fileName);
QString formatAdaptiveSize(quint64 bytes);
quint32 calculateCRC32(const QString &filePath);

bool atWordBoundary(const QString &fileName, const QString &word);
uint getNameMatchQuality(const QFileInfo &fileInfo, const QString &searchStringFilename, const QStringList &searchStringSplit, const QStringList &recentOpenList);
uint getDesktopNameMatchQuality(const QFileInfo &fileInfo, const QString &searchString, const QStringList &searchStringSplit, const QStringList &recentOpenList, const QString &desktopName);
void launchDesktopFile(const QFileInfo &fileInfo);

#endif // HELPERS_H

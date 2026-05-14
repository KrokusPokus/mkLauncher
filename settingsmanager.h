#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>

#include <QString>
#include <QSet>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>

class SettingsManager {
public:
	SettingsManager();

	void load();
    void saveSettings();
    void saveHistory();
    QString getSettingsPath();

    QStringList searchFolders;

	QSet<QString> audioExts;
	QSet<QString> imageExts;
	QSet<QString> textExts;
	QSet<QString> videoExts;

	QString audioEditor;
	QString imageEditor;
	QString textEditor;
	QString videoEditor;
	QString fileManager;

	bool alternatingRowColors;
	bool showGrid;
    bool showIconsInMenu;
	bool showPlaceholderText;
    bool showShortcutsInMenu;
    int fontSizeOverride;
    QString fontNameOverride;

    QStringList recentOpenList;
    QStringList recentInputList;

private:

	QSet<QString> parseExtensions(const QString &input);
    QStringList importSearchFolders(const QString &input);
    QString exportSearchFolders(const QStringList &folders);
    QString getHistoryPath();

    void safeSetValue(QSettings &settings, const QString &key, const QVariant &value);
    QString formatStringSet(const QSet<QString> &extensionSet);

    // Defaults
    const QString DEFAULT_AUDIO = "aac,flac,m4a,mid,mp3,ogg,wav";
    const QString DEFAULT_IMAGE = "avif,bmp,gif,heic,heif,jpg,jpeg,jxl,png,qoi,tga,tif,tiff,webp,xcf";
    const QString DEFAULT_TEXT  = "ahk,ass,au3,bat,c,cfg,conf,cpp,cs,css,cue,cxx,desktop,dic,dsf,dsk,duf,h,hpp,htm,html,inf,ini,ion,js,json,log,lst,lua,md,nfo,py,rc,reg,scp,sfv,sh,slang,slangp,sql,srt,ssa,ts,txt,url,vbs,vcxproj,xhtml,xml,xul,yml";
    const QString DEFAULT_VIDEO = "3gp,asf,avi,mkv,mov,mp4,mpg,ogm,ts,wm,wmv,webm";
};

#endif // SETTINGSMANAGER_H

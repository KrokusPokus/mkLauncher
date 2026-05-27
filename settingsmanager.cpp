#include "settingsmanager.h"

#include <QCoreApplication>
#include <QFile>

SettingsManager::SettingsManager() {
	load(); // Lädt beim Erstellen automatisch
}

void SettingsManager::load() {
    QSettings s(getSettingsPath(), QSettings::IniFormat);

	// Extensions
	audioExts = parseExtensions(s.value("Extensions/FileExtAudio", DEFAULT_AUDIO).toString());
	imageExts = parseExtensions(s.value("Extensions/FileExtImage", DEFAULT_IMAGE).toString());
	textExts  = parseExtensions(s.value("Extensions/FileExtText",  DEFAULT_TEXT).toString());
	videoExts = parseExtensions(s.value("Extensions/FileExtVideo", DEFAULT_VIDEO).toString());

	// Handlers
    audioEditor      = QDir::toNativeSeparators(s.value("Handlers/AudioEditor", "org.kde.kwave.desktop").toString());
    imageEditor      = QDir::toNativeSeparators(s.value("Handlers/ImageEditor", "gimp.desktop").toString());
    textEditor       = QDir::toNativeSeparators(s.value("Handlers/TextEditor", "org.kde.kate.desktop").toString());
    videoEditor      = QDir::toNativeSeparators(s.value("Handlers/VideoEditor", "org.kde.kdenlive.desktop").toString());
    fileManager      = QDir::toNativeSeparators(s.value("Handlers/FileManager", "org.kde.dolphin.desktop").toString());
    searchTool       = QDir::toNativeSeparators(s.value("Handlers/SearchTool", "org.kde.kfind.desktop").toString());
    renameTool       = QDir::toNativeSeparators(s.value("Handlers/RenameTool", "").toString());

	// Interface
	alternatingRowColors = s.value("Interface/AlternatingRowColors", true).toBool();
	fontNameOverride     = s.value("Interface/FontNameOverride", "").toString();
	fontSizeOverride     = s.value("Interface/FontSizeOverride", 0).toInt();
	showGrid             = s.value("Interface/ShowGrid", false).toBool();
	showPlaceholderText  = s.value("Interface/ShowPlaceholderText", true).toBool();
    showIconsInMenu      = s.value("Interface/ShowIconsInMenu", true).toBool();
    showShortcutsInMenu  = s.value("Interface/ShowShortcutsInMenu", true).toBool();
    showFileExtensions   = s.value("Interface/ShowFileExtensions", true).toBool();

    // mkLauncher specific
    searchFolders = importSearchFolders(s.value("mkLauncher/UserFolders", "$HOME").toString());

    QSettings h(getHistoryPath(), QSettings::IniFormat);
    recentInputList = h.value("History/InputList").toStringList();
    recentOpenList = h.value("History/OpenList").toStringList();

    saveSettings();
}

void SettingsManager::saveSettings() {
    QSettings s(getSettingsPath(), QSettings::IniFormat);

    // Extensions
    safeSetValue(s, "Extensions/FileExtAudio", formatStringSet(audioExts));
    safeSetValue(s, "Extensions/FileExtImage", formatStringSet(imageExts));
    safeSetValue(s, "Extensions/FileExtText", formatStringSet(textExts));
    safeSetValue(s, "Extensions/FileExtVideo", formatStringSet(videoExts));

    // Handlers
    safeSetValue(s, "Handlers/AudioEditor", audioEditor);
    safeSetValue(s, "Handlers/ImageEditor", imageEditor);
    safeSetValue(s, "Handlers/TextEditor", textEditor);
    safeSetValue(s, "Handlers/VideoEditor", videoEditor);
    safeSetValue(s, "Handlers/FileManager", fileManager);
    safeSetValue(s, "Handlers/SearchTool", searchTool);
    safeSetValue(s, "Handlers/RenameTool", renameTool);

    // Interface
    safeSetValue(s, "Interface/AlternatingRowColors", alternatingRowColors);
    safeSetValue(s, "Interface/FontNameOverride", fontNameOverride);
    safeSetValue(s, "Interface/FontSizeOverride", fontSizeOverride);
    safeSetValue(s, "Interface/ShowGrid", showGrid);
    safeSetValue(s, "Interface/ShowPlaceholderText", showPlaceholderText);
    safeSetValue(s, "Interface/ShowIconsInMenu", showIconsInMenu);
    safeSetValue(s, "Interface/ShowShortcutsInMenu", showShortcutsInMenu);
    safeSetValue(s, "Interface/ShowFileExtensions", showFileExtensions);

    // mkLauncher specific
    safeSetValue(s, "mkLauncher/UserFolders", exportSearchFolders(searchFolders));

    // Because we used safeSetValue(), the file will only get written if there were changes in values.
	s.sync();
}

void SettingsManager::saveHistory() {
    QSettings h(getHistoryPath(), QSettings::IniFormat);
    safeSetValue(h, "History/InputList", recentInputList);
    safeSetValue(h, "History/OpenList", recentOpenList);
    h.sync();
}

void SettingsManager::safeSetValue(QSettings &settings, const QString &key, const QVariant &value) {
    if (settings.value(key) != value) {
        settings.setValue(key, value);
    }
}

QString SettingsManager::formatStringSet(const QSet<QString> &extensionSet) {
    QStringList stringList = extensionSet.values();
    stringList.sort(Qt::CaseInsensitive);
    return stringList.join(",");
    }

QSet<QString> SettingsManager::parseExtensions(const QString &input) {
    const QStringList list = input.split(u',', Qt::SkipEmptyParts);

	QSet<QString> set;
    set.reserve(list.size());

	for (const QString &s : list) {
		set.insert(s.trimmed().toLower());
	}

	return set;
}

QStringList SettingsManager::importSearchFolders(const QString &input) {
    QString rawInput = input;

    QString homePath = QDir::homePath();
    rawInput.replace("$HOME", homePath);

    QStringList searchFolders = rawInput.split(u'|', Qt::SkipEmptyParts);

    for (QString &path : searchFolders) {
        path = path.trimmed();

        if (path.startsWith("~")) {
            path.replace(0, 1, homePath);
        }

        QDir dir(path);
        path = QDir::cleanPath(dir.absolutePath());
    }

    searchFolders.removeDuplicates();

    return searchFolders;
}

QString SettingsManager::exportSearchFolders(const QStringList &folders) {
    QStringList workingCopy = folders;

    workingCopy.removeDuplicates();

    QString homePath = QDir::homePath();

    for (QString &path : workingCopy) {
        if (path.startsWith(homePath)) {
            path.replace(0, homePath.length(), "$HOME");
        }
    }

    return workingCopy.join(u'|');
}

QString SettingsManager::getSettingsPath() {
    QString iniFilePath = QDir(QCoreApplication::applicationDirPath()).filePath("/settings.ini");
    if (!QFile::exists(iniFilePath)) {
        QString toolsDirPath = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("mkTools");
        if (!QDir(toolsDirPath).exists()) {
            QDir().mkpath(toolsDirPath);
        }
        QDir toolsDir(toolsDirPath);
        iniFilePath = toolsDir.filePath("settings.ini");
    }
    return iniFilePath;
}

QString SettingsManager::getHistoryPath() {
    QString iniFilePath = QDir(QCoreApplication::applicationDirPath()).filePath("/history.ini");
    if (!QFile::exists(iniFilePath)) {
        QString toolsDirPath = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("mkTools");
        if (!QDir(toolsDirPath).exists()) {
            QDir().mkpath(toolsDirPath);
        }
        QDir toolsDir(toolsDirPath);
        iniFilePath = toolsDir.filePath("history.ini");
    }
    return iniFilePath;
}

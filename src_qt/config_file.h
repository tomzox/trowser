/* ----------------------------------------------------------------------------
 * Copyright (C) 2007-2010,2020 Th. Zoerner
 * ----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */
#ifndef _CONFIG_FILE_H
#define _CONFIG_FILE_H

#include <QObject>
#include <QJsonObject>
#include <QFileDevice>

class QTimer;
class QFile;

class MainWin;
class MainText;
class MainSearch;
class Highlighter;

enum class LoadMode { Head, Tail };

// ----------------------------------------------------------------------------

class ConfigFile : public QObject
{
    Q_OBJECT

public:
    static void connectWidgets(MainWin * mainWin, MainSearch * search,
                               MainText * mainText, Highlighter * higl);
    static void loadRcFile();
    static void updateRcFile();
    static void updateRcAfterIdle();
    static bool isValid();
    static void getFileLoadParams(LoadMode& mode, size_t& size);
    static void updateFileLoadParams(LoadMode mode, size_t size);

private:
    // constructor can only be invoked via the static interface
    ConfigFile();
    ~ConfigFile();
    void loadConfig();
    void writeConfig();
    void writeConfigDelayed();
    QJsonObject getRcValues();
    void setRcValues(const QJsonObject& obj);
    bool checkRcVersion(const QJsonObject& obj);
    QFileDevice::Permissions backupConfig(QFile& fh);
    QString getRcPath();

private:
    static MainWin    * s_mainWin;
    static MainText   * s_mainText;
    static MainSearch * s_search;
    static Highlighter* s_higl;

    static ConfigFile * s_instance;

    QTimer            * m_timUpdateRc = nullptr;
    qint64              m_tsUpdateRc = 0;
    bool                m_rcFileWriteError = false;
    bool                m_rcFileBackedup = false;
    bool                m_rcLoadComplete = false;
    QJsonObject         m_prevRcContent;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // command line parameters
    //
    friend void ParseArgv(int argc, const char * const argv[]);

    static LoadMode load_file_mode;
    static size_t load_buf_size;
    static bool load_buf_size_opt;

    static const LoadMode defaultLoadFileMode = LoadMode::Head;
    static const size_t defaultLoadBufSize = 20*1024*1024;
    static constexpr const char * defaultRcFileName = ".trowserc.qt";

    static const char * myrcfile;
    static const uint32_t rcfile_compat = 0x03000001;
    static const uint32_t rcfile_version = 0x03000001;
};

void ParseArgv(int argc, const char * const argv[]);

#endif // _CONFIG_FILE_H

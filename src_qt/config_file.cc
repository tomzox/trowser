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
 *
 * Module description:
 *
 * This module reads and writes configuration data from/to a file. The
 * configuration is read once during start-up, immediately after parsing
 * command line parameters (which may specify a non-standard config file).
 * Afterward the config file is written after any change of parameters that
 * affect the contents (the file indeed is written only when the new content
 * would be different.) Usually writing changes is delayed slightly via timer,
 * so that multiple consecutive changes cause only one update.
 *
 * The configuration is writtein in JSON format. This module only contains the
 * top-level data structure plus file I/O control. The actual parameters are
 * read and written by handlers in the various other classes. Each such class
 * has one sub-structure at top-level that encapsulates all its parameters.
 *
 * ----------------------------------------------------------------------------
 */

#include <QApplication>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTimer>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "bookmarks.h"
#include "status_line.h"
#include "search_list.h"
#include "load_pipe.h"
#include "dlg_higl.h"
#include "dlg_history.h"
#include "dlg_bookmarks.h"
#include "dlg_markup_sa.h"

// ----------------------------------------------------------------------------

/**
 * Constructor: create timer for controlling delayed write.
 */
ConfigFile::ConfigFile()
{
    m_timUpdateRc = new QTimer(this);
    m_timUpdateRc->setSingleShot(true);
    m_timUpdateRc->setInterval(3000);
    connect(m_timUpdateRc, &QTimer::timeout, this, &ConfigFile::writeConfig);

    m_tsUpdateRc = QDateTime::currentMSecsSinceEpoch();  // use MSecs for compatibility with Qt 5.7
}

/**
 * Destructor: Freeing resources.
 */
ConfigFile::~ConfigFile()
{
    delete m_timUpdateRc;
}

/**
 * This function collects configuration data from all modules with persistent state
 * and returns it in a JSON object.
 */
QJsonObject ConfigFile::getRcValues()
{
    QJsonObject obj;

    // dump search history
    obj.insert("main_search", s_search->getRcValues());

    // dump highlighting patterns
    obj.insert("highlight", s_higl->getRcValues());

    // dialog window geometry and other options
    obj.insert("dlg_highlight", DlgHigl::getRcValues());
    obj.insert("dlg_history", DlgHistory::getRcValues());
    obj.insert("dlg_bookmarks", DlgBookmarks::getRcValues());

    // search list options & custom column parser configuration
    obj.insert("search_list", SearchList::getRcValues());

    obj.insert("main_win_geom", QJsonValue(QString(s_mainWin->saveGeometry().toHex())));
    obj.insert("main_win_state", QJsonValue(QString(s_mainWin->saveState().toHex())));

    // font and color settings
    obj.insert("main_text_font", QJsonValue(s_mainText->getFontContent().toString()));

    // misc (note the head/tail mode is omitted intentionally)
    obj.insert("load_buf_size_lsb", QJsonValue(int(load_buf_size & 0xFFFFFFFFU)));
    obj.insert("load_buf_size_msb", QJsonValue(int(load_buf_size >> 32)));

    // dump software version (use prefix "xx" to keep this entry at the end when sorted)
    obj.insert("xx_trowser_version", QJsonValue(int(rcfile_version)));

    return obj;
}

/**
 * This function distributes configuration data loaded from JSON to the various
 * modules during start-up.
 */
void ConfigFile::setRcValues(const QJsonObject& obj)
{
    int load_buf_size_lsb = 0;
    int load_buf_size_msb = 0;

    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        if (var == "main_search")               s_search->setRcValues(val.toObject());
        else if (var == "highlight")            s_higl->setRcValues(val);
        else if (var == "main_text_font")       s_mainText->setFontInitial(val.toString());
        else if (var == "load_buf_size_lsb")    load_buf_size_lsb = val.toInt();
        else if (var == "load_buf_size_msb")    load_buf_size_msb = val.toInt();
        else if (var == "main_win_state")       s_mainWin->restoreState(QByteArray::fromHex(val.toString().toLatin1()));
        else if (var == "main_win_geom")        s_mainWin->restoreGeometry(QByteArray::fromHex(val.toString().toLatin1()));
        else if (var == "search_list")          SearchList::setRcValues(val);
        else if (var == "dlg_highlight")        DlgHigl::setRcValues(val);
        else if (var == "dlg_history")          DlgHistory::setRcValues(val);
        else if (var == "dlg_bookmarks")        DlgBookmarks::setRcValues(val);
        else if (var == "xx_trowser_version")   /*nop*/ ;
        else
            fprintf(stderr, "trowser: ignoring unknown keyword at top-level in rcfile: %s\n", var.toLatin1().data());
    }

    // buffer size provided via command line has precedence over the one from RC file
    if (!load_buf_size_opt && (load_buf_size_lsb || load_buf_size_msb))
        load_buf_size = size_t(load_buf_size_lsb) | (size_t(load_buf_size_msb) << 32);
}

/**
 * This function checks if the version of the JSON file is compatible with the
 * version of this software. The JSON file contains the RC format version
 * number of the SW which created it. Versions that are newer than the own SW
 * version, or older than a known compatibility limit will not be loaded.
 *
 * Note the RC format version should be changed only when the actual RC format
 * is changed, not for each SW version change. Therefore we do not need to
 * include a backwards compatibility indicator in the file for supporting
 * slightly outdated SW versions.
 */
bool ConfigFile::checkRcVersion(const QJsonObject& obj)
{
    bool result = false;
    auto rcVersion = obj.find("xx_trowser_version");
    if (rcVersion != obj.end())
    {
        if (   (uint32_t(rcVersion->toInt()) >= rcfile_compat)
            && (uint32_t(rcVersion->toInt()) <= rcfile_version))
        {
            result = true;
        }
        else
            fprintf(stderr, "Config file has an incompatible version (%X) and cannot be "
                            "loaded. Starting with default config.\n", rcVersion->toInt());
    }
    else
        fprintf(stderr, "Config file appears truncated: starting with default configuration\n");

    return result;
}

/**
 * This function reads configuration variables from the rc file.
 * The function is called once during start-up.
 */
void ConfigFile::loadConfig()
{
    QString rcFileName = getRcPath();
    QFile fh(rcFileName);
    if (fh.open(QFile::ReadOnly | QFile::Text))
    {
        QJsonParseError err;
        QTextStream readFile(&fh);
        auto txt = readFile.readAll();

        // detect comments at the start of the file, which will be skipped
        int startOff = 0;
        while (startOff < txt.length())
        {
            static const QRegularExpression re1("\\s*(?:#.*)?(?:\n|$)");
            auto mat1 = re1.match(txt.midRef(startOff), 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
            if (!mat1.hasMatch())
                break;
            startOff += mat1.captured(0).length();
        }

        auto doc = QJsonDocument::fromJson(txt.midRef(startOff).toUtf8(), &err);
        if (!doc.isNull() && doc.isObject())
        {
            QJsonObject obj = doc.object();

            if (checkRcVersion(obj))
            {
                // distribute the contents to the various modules
                setRcValues(obj);

                // keep a copy of the content for comparison and avoiding unnecessary write-back
                m_prevRcContent = obj;
            }
        }
        else
        {
            if (doc.isNull())
                fprintf(stderr, "Error parsing config file: %s\n", err.errorString().toLatin1().data());
            else
                fprintf(stderr, "Config file in unexpected format\n");
        }
        fh.close();
    }
    else
    {
        // Application GUI is not initialized yet, so print to console
        fprintf(stderr, "trowser: warning: failed to load config file '%s': %s\n", rcFileName.toLatin1().data(), strerror(errno));
    }

    // finally unblock writing of RC file
    m_rcLoadComplete = true;
}

/**
 * This function returns path / file name of the config file.
 */
QString ConfigFile::getRcPath()
{
    if (myrcfile == nullptr)
    {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        bool dirOk = false;
        if (!dir.path().isEmpty())
        {
            if (!dir.exists())
            {
                if (dir.mkpath(".") == false)
                {
                    fprintf(stderr, "Failed to create config directory: %s\n", dir.path().toLatin1().data());
                }
                else
                    dirOk = true;
            }
            else
                dirOk = true;
        }
        else
            fprintf(stderr, "Warning: Failed to determine config directory; will use current directory\n");

        if (dirOk)
            return dir.filePath(defaultRcFileName);
        else
            return defaultRcFileName;
    }
    else
        return myrcfile;
}

/**
 * This functions keeps a backup copy of the previous configuration file before
 * overwriting it for the first time. The backup is done by renaming the file;
 * a possible pre-existing backup file is removed. The function returns the
 * file permissions of the renamed file, so that the same persmissions can be
 * set on the newly written file.
 */
QFileDevice::Permissions ConfigFile::backupConfig(QFile& fh)
{
    QFileDevice::Permissions bakPerm;

    if (m_rcFileBackedup == false)
    {
        if (fh.exists())
        {
            bakPerm = fh.permissions();
            QString bakName = QString(fh.fileName()) + ".bak";

            QFile::remove(bakName);
            if (QFile::rename(fh.fileName(), bakName))
            {
                m_rcFileBackedup = true;
            }
            else
                fprintf(stderr, "Warning: Failed to rename %s to %s.bak: %s\n",
                        fh.fileName().toLatin1().data(), bakName.toLatin1().data(), fh.errorString().toLatin1().data());
        }
        else  // nothing to back-up
            m_rcFileBackedup = true;
    }
    return bakPerm;
}

/**
 * This functions writes configuration variables into the configuration file.
 */
void ConfigFile::writeConfig()
{
    m_timUpdateRc->stop();
    m_tsUpdateRc = QDateTime::currentMSecsSinceEpoch();

    // collect content from all modules with persistent state
    QJsonObject obj = getRcValues();

    // skip write if content is unchanged since loading or previous write respectively
    if (obj != m_prevRcContent)
    {
        QJsonDocument doc;
        doc.setObject(obj);

        QString rcFileName = getRcPath();
        QFile fh(rcFileName);
        QFileDevice::Permissions bakPerm = backupConfig(fh);
        bool ioError = false;

        if (fh.open(QFile::WriteOnly | QFile::Text))
        {
            QTextStream out(&fh);

            out << "#\n"
                   "# trowser configuration file\n"
                   "#\n"
                   "# This file is automatically generated - do not edit\n"
                   "# Written at: " << QDateTime::currentDateTime().toString() << "\n"
                   "#\n";
            out << doc.toJson();

            out.flush();
            if (out.status() == QTextStream::Ok)
            {
                // set permissions if creating a new file after moving original RC file
                if (bakPerm != 0)
                    fh.setPermissions(bakPerm);

                // keep a copy of the content for comparison and avoiding duplicate writes
                m_prevRcContent = obj;
                m_rcFileWriteError = false;

                fh.close();  // does not report errors, which is why explicit flush is done above
            }
            else
            {
                fh.remove();  // includes close()
                ioError = true;
            }
        }
        else
            ioError = true;

        // warn the user about error only once, not for every attempt
        if (ioError && (m_rcFileWriteError == false))
        {
            QMessageBox::critical(s_mainWin, "trowser",
                                  QString("Error creating config file ") + rcFileName + ": " + fh.errorString(),
                                  QMessageBox::Ok);
            m_rcFileWriteError = true;
        }
    }
}

/**
 * This function is used to trigger writing the RC file after changes. The
 * write is delayed by a few seconds to avoid writing the file multiple times
 * when multiple values are changed. This timer is restarted when another
 * change occurs during the delay, however only up to a limit.
 */
void ConfigFile::writeConfigDelayed()
{
    if (   !m_timUpdateRc->isActive()
        || (QDateTime::currentMSecsSinceEpoch() - m_tsUpdateRc) < 60000)
    {
        m_timUpdateRc->start();
    }
}


// ----------------------------------------------------------------------------
// Command line parser

/**
 * This function is called when the program is started with -help to list all
 * possible command line options.
 */
static void PrintUsage(const char * const argv[], int argvn=-1, const char * reason=nullptr)
{
    if (reason != nullptr)
        fprintf(stderr, "%s: %s: %s\n", argv[0], reason, argv[argvn]);

    fprintf(stderr, "Usage: %s [options] {file|-}\n", argv[0]);

    if (argvn != -1)
    {
        fprintf(stderr, "Use -h or --help for a list of options\n");
    }
    else
    {
        fprintf(stderr, "The following options are available:\n"
                        "  --head=size\t\tLoad <size> bytes from the start of the file\n"
                        "  --tail=size\t\tLoad <size> bytes from the end of the file\n"
                        "  --rcfile=<path>\tUse alternate config file (default: ~/.trowserc)\n");
    }
    exit(1);
}


/**
 * This helper function checks if a command line flag which requires an
 * argument is followed by at least another word on the command line.
 */
static void ParseArgvLenCheck(int argc, const char * const argv[], int arg_idx)
{
    if (arg_idx + 1 >= argc)
        PrintUsage(argv, arg_idx, "this option requires an argument");
}

/**
 * This helper function reads an integer value from a command line parameter
 */
static int ParseArgInt(const char * const argv[], int arg_idx, const char * opt)
{
    int ival = 0;
    try
    {
        std::size_t pos;
        ival = std::stoi(opt, &pos);
        if (opt[pos] != 0)
            PrintUsage(argv, arg_idx, "numerical value is followed by garbage");
    }
    catch (const std::exception& ex)
    {
        PrintUsage(argv, arg_idx, "is not a numerical value");
    }
    return ival;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * This function parses and evaluates the command line arguments.
 */
void ParseArgv(int argc, const char * const argv[])
{
    bool file_seen = false;
    int arg_idx = 1;

    while (arg_idx < argc)
    {
        const char * arg = argv[arg_idx];

        if ((arg[0] == '-') && (arg[1] != 0))
        {
            if (strcmp(arg, "-t") == 0)
            {
                ParseArgvLenCheck(argc, argv, arg_idx);
                arg_idx += 1;
                ConfigFile::load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                ConfigFile::load_file_mode = LoadMode::Tail;
                ConfigFile::load_buf_size_opt = true;
            }
            else if (strncmp(arg, "--tail", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    ConfigFile::load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    ConfigFile::load_file_mode = LoadMode::Tail;
                    ConfigFile::load_buf_size_opt = true;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --tail=10000000)");
            }
            else if (strcmp(arg, "-h") == 0)
            {
                ParseArgvLenCheck(argc, argv, arg_idx);
                arg_idx += 1;
                ConfigFile::load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                ConfigFile::load_file_mode = LoadMode::Head;
                ConfigFile::load_buf_size_opt = true;
            }
            else if (strncmp(arg, "--head", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    ConfigFile::load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    ConfigFile::load_file_mode = LoadMode::Head;
                    ConfigFile::load_buf_size_opt = true;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --head=10000000)");
            }
            else if (strcmp(arg, "-r") == 0)
            {
                if (arg_idx + 1 < argc)
                {
                    arg_idx += 1;
                    ConfigFile::myrcfile = argv[arg_idx];
                }
                else
                    PrintUsage(argv, arg_idx, "this option requires an argument");
            }
            else if (strncmp(arg, "--rcfile", 8) == 0)
            {
                if ((arg[8] == '=') && (arg[9] != 0))
                {
                    ConfigFile::myrcfile = arg + 8+1;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a path argument (e.g. --rcfile=foo/bar)");
            }
            else if (strcmp(arg, "-?") == 0 || strcmp(arg, "--help") == 0)
            {
                PrintUsage(argv);
            }
            else
                PrintUsage(argv, arg_idx, "unknown option");
        }
        else
        {
            if (arg_idx + 1 >= argc)
            {
                file_seen = true;
            }
            else
            {
                arg_idx += 1;
                PrintUsage(argv, arg_idx, "only one file name expected");
            }
        }
        arg_idx += 1;
    }

    if (!file_seen)
    {
        fprintf(stderr, "File name missing (use \"-\" for stdin)\n");
        PrintUsage(argv);
    }
}

// ----------------------------------------------------------------------------
// Static state & interface

ConfigFile  * ConfigFile::s_instance;

MainWin     * ConfigFile::s_mainWin;
MainText    * ConfigFile::s_mainText;
MainSearch  * ConfigFile::s_search;
Highlighter * ConfigFile::s_higl;

// Command line parameters
LoadMode      ConfigFile::load_file_mode = ConfigFile::defaultLoadFileMode;
size_t        ConfigFile::load_buf_size = ConfigFile::defaultLoadBufSize;
bool          ConfigFile::load_buf_size_opt = false;
const char *  ConfigFile::myrcfile = nullptr;

/**
 * This external interface function is called once during start-up after all
 * classes are instantiated to establish the required connections.
 */
void ConfigFile::connectWidgets(MainWin * mainWin, MainSearch * search, MainText * mainText,
                                Highlighter * higl)  /*static*/
{
    s_mainWin = mainWin;
    s_mainText = mainText;
    s_search = search;
    s_higl = higl;
}

/**
 * This functions writes configuration variables into the configuration file.
 */
void ConfigFile::updateRcFile()  /*static*/
{
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(s_instance->m_rcLoadComplete);  // recursive store while still loading

    s_instance->writeConfig();
}

void ConfigFile::updateRcAfterIdle()  /*static*/
{
    Q_ASSERT(s_instance != nullptr);

    // ignore recursive store while still loading
    if (s_instance->m_rcLoadComplete)
        s_instance->writeConfigDelayed();
}

void ConfigFile::getFileLoadParams(LoadMode& mode, size_t& size)  /*static*/
{
    mode = load_file_mode;
    size = load_buf_size;
}

void ConfigFile::updateFileLoadParams(LoadMode mode, size_t size)  /*static*/
{
    load_file_mode = mode;
    load_buf_size = size;
    updateRcAfterIdle();
}

bool ConfigFile::isValid()  /*static*/
{
    return s_instance && !s_instance->m_prevRcContent.empty();
}

/**
 * This function reads configuration variables from the configuration file.
 * The function is called once during start-up.
 */
void ConfigFile::loadRcFile()  /*static*/
{
    if (s_instance == nullptr)
    {
        s_instance = new ConfigFile();
    }
    s_instance->loadConfig();
}

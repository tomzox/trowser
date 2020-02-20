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

#include <QWidget>
#include <QTextDocument>
#include <QTextBlock>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "status_line.h"
#include "highlighter.h"
#include "search_list.h"
#include "dlg_bookmarks.h"
#include "bookmarks.h"

// ----------------------------------------------------------------------------

/**
 * This function adds or removes a bookmark at the given line of text.
 * The function is used to toggle bookmarks via key bindings.
 */
bool Bookmarks::toggleBookmark(int line)
{
    bool result;

    auto it = m_bookmarks.find(line);
    if (it != m_bookmarks.end())
    {
        m_bookmarks.erase(it);
        result = false;
    }
    else
    {
        QTextBlock block = m_mainText->document()->findBlockByNumber(line);
        m_bookmarks[line] = block.text();
        result = true;
    }
    m_isModified = true;

    m_higl->bookmarkHighlight(line, result);
    SearchList::signalBookmarkLine(line);
    DlgBookmarks::signalBookmarkListChanged();

    return result;
}

/**
 * This function removes bookmarks at the given line numbers. This is used by
 * the bookmark dialog.
 */
void Bookmarks::removeLines(const std::vector<int>& lineList)
{
    for (int line : lineList)
    {
        auto it = m_bookmarks.find(line);
        if (it != m_bookmarks.end())
        {
            m_bookmarks.erase(it);
            SearchList::signalBookmarkLine(line);
        }
    }
    DlgBookmarks::signalBookmarkListChanged();
    m_isModified = true;
}

/**
 * This function deletes all bookmarks. This function is used when loading a
 * new document, or on request of the user to clear the bookmark list (e.g.
 * when a large number of bookmarks was imported previously from a file.)
 */
void Bookmarks::removeAll()
{
    m_bookmarks.clear();

    SearchList::signalBookmarkLine();
    DlgBookmarks::signalBookmarkListChanged();
    m_isModified = false;
    m_loadedFileName.clear();
}

/**
 * This function returns the bookmark description text associated with the
 * bookmark at the given line. The caller shall ensure there is a bookmark at
 * that line before calling the function.
 */
const QString& Bookmarks::getText(int line) const
{
    auto it = m_bookmarks.find(line);
    if (it != m_bookmarks.end())
    {
        return it->second;
    }
    Q_ASSERT(false);
    static QString nil;
    return nil;
}

/**
 * This function replaces the description text associated with the
 * bookmark at the given line.
 */
void Bookmarks::setText(int line, const QString& text)
{
    auto it = m_bookmarks.find(line);
    if (it != m_bookmarks.end())
    {
        m_bookmarks[line] = text;
        m_isModified = true;
    }
    else
        Q_ASSERT(false);
}

/**
 * The function fills the given vector with a list of line numbers of all
 * currently defined bookmarks. Note the caller is responsible to clear the
 * list before the call, if necessary.
 */
void Bookmarks::getLineList(std::vector<int>& lineList)
{
    lineList.reserve(lineList.size() + m_bookmarks.size());

    for (auto it = m_bookmarks.cbegin(); it != m_bookmarks.cend(); ++it)
    {
        lineList.push_back(it->first);
    }
}

/**
 * This function returns the line number of the next bookmark in the given
 * direction starting at the given line. (Note the given line may or may not
 * have a bookmark; the function never returns the given line as result.) The
 * function returns -1 if there if no such bookmark is found.
 */
int Bookmarks::getNextLine(int line, bool is_fwd) const
{
    if (is_fwd)
    {
        for (auto it = m_bookmarks.cbegin(); it != m_bookmarks.cend(); ++it)
            if (it->first > line)
                return it->first;
    }
    else
    {
        for (auto it = m_bookmarks.crbegin(); it != m_bookmarks.crend(); ++it)
            if (it->first < line)
                return it->first;
    }
    return -1;
}

// ----------------------------------------------------------------------------

/**
 * This function reads a list of line numbers and tags from a file and
 * adds them to the bookmark list. (Note already existing bookmarks are
 * not discarded, hence there's no warning when bookmarks already exist.)
 */
void Bookmarks::readFile(QWidget * parent, const QString& fileName)
{
    bool result = false;
    auto fh = new QFile(QString(fileName));
    if (fh->open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(fh);
        int max_line = m_mainText->document()->blockCount();
        int skipped = 0;
        int synerr = 0;
        QString line_str;
        std::vector<std::pair<int,QString>> line_list;

        while (in.readLineInto(&line_str))
        {
            static const QRegularExpression re1("^(\\d+)(?:[ \\t\\:\\.\\,\\;\\=\\'\\/](.*))?$");
            static const QRegularExpression re2("^\\s*(?:#.*)?$");
            auto mat1 = re1.match(line_str);
            if (mat1.hasMatch())
            {
                bool ok;
                int line = mat1.captured(1).toInt(&ok, 10);
                QString txt = mat1.captured(2).trimmed();  // string may be empty

                if (ok && (line > 0) && (line < max_line))
                {
                    // OK - add number & text to the list
                    line_list.push_back(std::make_pair(line - 1, txt));
                }
                else if (ok)
                    skipped += 1;
                else
                    synerr += 1;
            }
            else
            {
                auto mat2 = re2.match(line_str);
                if (mat2.hasMatch() == false)
                    synerr += 1;
            }
        }

        // error handling
        if (in.status() != QTextStream::Ok)
        {
            QString msg = QString("Error while reading bookmark file ") + fileName + ": " + fh->errorString();
            QMessageBox::warning(parent, "trowser", msg, QMessageBox::Ok);
        }
        else if ((skipped > 0) || (synerr > 0))
        {
            QString msg("Found ");
            QTextStream msgBld(&msg);
            if (skipped > 0)
            {
                msgBld << skipped << " lines with a value outside of the allowed range 1 .. " << max_line;
            }
            if (synerr > 0)
            {
                if (skipped)
                    msgBld << " and ";
                msgBld << synerr << " non-empty lines without a numeric value";
            }
            if (line_list.size() == 0)
            {
                msgBld << ". No valid numbers found in the file.";
                QMessageBox::critical(parent, "trowser", msg, QMessageBox::Ok);
            }
            else
            {
                auto answer = QMessageBox::warning(parent, "trowser", msg, QMessageBox::Ignore | QMessageBox::Cancel);
                if (answer == QMessageBox::Ignore)
                    result = true;
            }
        }
        else if (line_list.size() == 0)
        {
            QString msg("No bookmark line numbers found in file " + fileName);
            QMessageBox::critical(parent, "trowser", msg, QMessageBox::Ok);
        }
        else
            result = true;

        if (result)
        {
            if (m_bookmarks.size() != 0)
                m_isModified = true;

            for (auto& it : line_list)
            {
                int line = it.first;
                QString txt(it.second);

                if (txt.isEmpty())
                {
                    QTextBlock block = m_mainText->document()->findBlockByNumber(line);
                    txt = block.text();
                    if (txt.isEmpty())
                        txt = QString("Bookmark in empty line ") + QString::number(line);
                }

                m_bookmarks[line] = txt;
                SearchList::signalBookmarkLine(line);
            }

            // update bookmark list dialog window, if opened
            DlgBookmarks::signalBookmarkListChanged();
        }
    }
    else
    {
        QString msg = QString("Error opening bookmark file ") + fileName + ": " + fh->errorString();
        QMessageBox::critical(parent, "trowser", msg, QMessageBox::Ok);
    }
}

/**
 * This function writes the bookmark list into a file.
 */
bool Bookmarks::saveFile(QWidget * parent, const QString& fileName)
{
    bool result = false;
    auto fh = new QFile(QString(fileName));
    if (fh->open(QFile::WriteOnly | QFile::Text))
    {
        QTextStream out(fh);

        for (const auto& it : m_bookmarks)
            out << (it.first + 1) << " " << it.second << "\n";

        out << flush;
        if (out.status() != QTextStream::Ok)
        {
            QString msg = QString("Error writing bookmark file ") + fileName + ": " + fh->errorString();
            QMessageBox::warning(parent, "trowser", msg, QMessageBox::Ok);
        }
        else
            result = true;
    }
    else
    {
        QString msg = QString("Error creating bookmark file ") + fileName + ": " + fh->errorString();
        QMessageBox::critical(parent, "trowser", msg, QMessageBox::Ok);
    }
    return result;
}

/**
 * This function is called by menu entry "Read bookmarks from file"
 * The user is asked to select a file; if he does so it's content is read.
 */
void Bookmarks::readFileFrom(QWidget * parent)
{
    QString defaultName = getDefaultFileName(m_mainWin->getFilename());
    if (defaultName.isEmpty())
        defaultName = ".";

    QString fileName = QFileDialog::getOpenFileName(parent, "Select bookmark file",
                                       defaultName, "Trace Files (out.*);;Any (*)");
    if (fileName.isEmpty() == false)
    {
        readFile(parent, fileName);
    }
}


/**
 * This function automatically reads a previously stored bookmark list
 * for a newly loaded file, if the bookmark file is named by the default
 * naming convention, i.e. with ".bok" extension.
 */
void Bookmarks::readFileAuto(QWidget * parent)
{
    bool isOlder = false;
    QString defaultName = getDefaultFileName(m_mainWin->getFilename(), &isOlder);
    if (defaultName.isEmpty() == false)
    {
        if (isOlder == false)
        {
            readFile(parent, defaultName);
        }
        else
            m_mainWin->mainStatusLine()->showWarning("bookmarks", "Bookmark file " + defaultName +
                                   " is older than content - not loaded");
    }
}


/**
 * This helper function determines the default file name for reading bookmarks.
 * Default is the trace file name or base file name plus ".bok". The name is
 * only returned if a file with this name actually exists and is not older
 * than the trace file.
 */
QString Bookmarks::getDefaultFileName(const QString& trace_name, bool *isOlder)
{
    QString bok_name;
    if (trace_name.isEmpty() == false)
    {
        QFileInfo traceFinfo(trace_name);
        if (traceFinfo.isFile())
        {
            QString tmpName = trace_name + ".bok";
            QFileInfo bokFinfo(tmpName);

            if (bokFinfo.isFile() == false)
            {
                QString name2(trace_name);
                name2.replace("\\.[^\\.]+$", ".bok");
                if (name2 != trace_name)
                {
                    QFileInfo name2Finfo(name2);
                    if (name2Finfo.isFile())
                    {
                        tmpName = name2;
                        bokFinfo = name2Finfo;
                    }
                }
            }
            if (bokFinfo.isFile())
            {
                if (isOlder != nullptr)
                {
                    QDateTime cur_mtime = traceFinfo.lastModified();
                    QDateTime bok_mtime = bokFinfo.lastModified();

                    *isOlder = (bok_mtime < cur_mtime);
                }
                bok_name = tmpName;
            }
        }
    }
    return bok_name;
}


/**
 * This function is called by menu entry "Save bookmarks to file". The user is
 * asked to select a file; when done, the bookmarks are written into it.
 */
void Bookmarks::saveFileAs(QWidget * parent, bool usePrevious)
{
    if (m_bookmarks.size() > 0)
    {
        QString fileName(m_loadedFileName);

        if (!usePrevious || m_loadedFileName.isEmpty())
        {
            const QString& mainFileName = m_mainWin->getFilename();
            QString defaultName(".");

            if (mainFileName.isEmpty() == false)
                defaultName = mainFileName + ".bok";

            fileName = QFileDialog::getSaveFileName(parent,
                                        "Save bookmarks list to file",
                                        defaultName,
                                        "Bookmarks (*.bok);;Any (*)");
        }
        if (fileName.isEmpty() == false)
        {
            if (saveFile(parent, fileName))
            {
                m_loadedFileName = fileName;
                m_isModified = false;
            }
        }
    }
    else
        QMessageBox::critical(parent, "trowser", "Your bookmark list is empty.", QMessageBox::Ok);
}


/**
 * This function offers to store the bookmark list into a file if the list was
 * modified.  The function is called when the application is closed or a new
 * file is loaded.
 */
bool Bookmarks::offerSave(QWidget * parent)
{
    bool result = true;
    if (m_isModified && (m_bookmarks.size() != 0))
    {
        auto answer = QMessageBox::question(parent, "trowser", "Store changes in the bookmark list before quitting?",
                                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (answer == QMessageBox::Yes)
        {
            saveFileAs(parent, true);

            if (m_isModified == false)
                m_mainWin->mainStatusLine()->showPlain("bookmarks", "Bookmarks have been saved");
            else
                result = false;
        }
        else if (answer == QMessageBox::Cancel)
        {
            result = false;
        }
    }
    return result;
}

/**
 * This function must be called when portions of the text in the main window
 * have been deleted to update references to text lines. Parameter meaning:
 *
 * @param top_l     First line which is NOT deleted, or 0 to delete nothing at top
 * @param bottom_l  This line and all below are removed, or -1 if none
 */
void Bookmarks::adjustLineNums(int top_l, int bottom_l)
{
    std::map<int,QString> newMap;

    for (auto it = m_bookmarks.begin(); it != m_bookmarks.end(); ++it)
    {
        if ( (it->first >= top_l) && ((it->first < bottom_l) || (bottom_l < 0)) )
            newMap[it->first - top_l] = it->second;
    }
    m_bookmarks = std::move(newMap);

    DlgBookmarks::signalBookmarkListChanged();
}

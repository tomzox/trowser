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
#ifndef _BOOKMARKS_H
#define _BOOKMARKS_H

#include <map>
#include <vector>

class MainWin;
class MainText;
class Highlighter;

class Bookmarks
{
public:
    Bookmarks(MainWin * mainWin)
        : m_mainWin(mainWin)
    {
    }
    void connectWidgets(MainText * textWid, Highlighter * higl)
    {
        m_mainText = textWid;
        m_higl = higl;
    }

    bool isEmpty() const { return m_bookmarks.empty(); }
    int getCount() const { return m_bookmarks.size(); }
    bool isBookmarked(int line) const
    {
        auto it = m_bookmarks.find(line);
        return (it != m_bookmarks.end());
    }
    const QString& getText(int line) const;
    int getNextLine(int line, bool isFwd) const;
    void getLineList(std::vector<int>& lineList);
    bool validFileName() const { return !m_loadedFileName.isEmpty(); }

    void setText(int line, const QString& text);
    bool toggleBookmark(int line);
    void removeLines(const std::vector<int>& lineList);
    void removeAll();

    void readFileFrom(QWidget * parent);
    void saveFileAs(QWidget * parent, bool usePrevious);
    bool offerSave(QWidget * parent);
    void readFileAuto(QWidget * parent);
    void adjustLineNums(int top_l, int bottom_l);

private:
    void readFile(QWidget * parent, const QString& fileName);
    bool saveFile(QWidget * parent, const QString& fileName);
    QString getDefaultFileName(const QString& docName, bool *isOlder = nullptr);

private:
    MainWin * const m_mainWin;
    MainText * m_mainText = nullptr;
    Highlighter * m_higl = nullptr;

    std::map<int,QString> m_bookmarks;

    QString m_loadedFileName;
    bool m_isModified = false;
};

#endif /* _BOOKMARKS_H */

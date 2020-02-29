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
#ifndef _HIGHLIGHTER_H
#define _HIGHLIGHTER_H

#include <QMainWindow>
#include <QTextCursor>
#include <QLineEdit>
#include <QTimer>

#include <main_search.h>  // for SearchPar

#include <vector>
#include <map>

class QProgressBar;
class QJsonObject;

class MainText;
class BgTask;

// ----------------------------------------------------------------------------

class HiglFmtSpec
{
public:
    HiglFmtSpec() = default;
    void merge(const HiglFmtSpec& other);
    bool operator==(const HiglFmtSpec& other) const;
    static constexpr QRgb INVALID_COLOR{0x00000000};

    QRgb                m_bgCol = INVALID_COLOR;
    QRgb                m_fgCol = INVALID_COLOR;
    QRgb                m_olCol = INVALID_COLOR;
    Qt::BrushStyle      m_bgStyle = Qt::NoBrush;
    Qt::BrushStyle      m_fgStyle = Qt::NoBrush;
    bool                m_bold = false;
    bool                m_italic = false;
    bool                m_underline = false;
    bool                m_overline = false;
    bool                m_strikeout = false;
    int                 m_sizeOff = 0;
    QString             m_font;
};

using HiglId = unsigned;

class HiglPatExport
{
public:
    SearchPar       m_srch;
    HiglFmtSpec     m_fmtSpec;
    HiglId          m_id;
};

class HiglPat
{
public:
    SearchPar       m_srch;
    HiglFmtSpec     m_fmtSpec;
    QTextCharFormat m_fmt;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class Highlighter : public QWidget
{
    Q_OBJECT

public:
    Highlighter(MainText * textWid, MainWin * mainWin);
    ~Highlighter();
    QJsonArray getRcValues();
    void setRcValues(const QJsonValue& val);
    void removeInc(QTextDocument * doc);
    void getPatList(std::vector<HiglPatExport>&) const;
    void setPatList(const std::vector<HiglPatExport>& patList);
    void setFmtSpec(HiglId id, const HiglFmtSpec& fmtSpec);
    const HiglFmtSpec * getFmtSpecForLine(int line, bool filterBookmark, bool filterHall);
    const HiglFmtSpec * getFmtSpecForId(HiglId id);
    void configFmt(QTextCharFormat& fmt, const HiglFmtSpec& fmtSpec);
    void adjustLineNums(int top_l, int bottom_l);

    void highlightInit();
    void searchHighlightMatch(QTextCursor& match);
    void searchHighlightClear();
    void searchHighlightUpdate(const SearchPar& par, bool onlyVisible);
    void searchHighlightAll(HiglId id, int startPos);
    void bookmarkHighlight(int line, bool enabled);

    static constexpr int INVALID_HIGL_ID = -1;
    static constexpr int HIGL_ID_SEARCH = 0;
    static constexpr int HIGL_ID_SEARCH_INC = 1;
    static constexpr int HIGL_ID_BOOKMARK = 2;
    static constexpr int HIGL_ID_FIRST_USER_DEF = 3;

private:
    void addSearchInc(QTextCursor& sel);
    void addSearchHall(const QTextBlock& blk, const QTextCharFormat& fmt, HiglId id);
    void removeHall(QTextDocument * doc, HiglId id);
    void redrawHall(QTextDocument * doc, HiglId id);
    void redraw(QTextDocument * doc, int line);
    void clearAll(QTextDocument * doc);

    void addPattern(const SearchPar& srch, const HiglFmtSpec& fmtSpec);
    void highlightInitBg(HiglId id, int startPos);
    int  highlightLines(HiglId id, int startPos);
    void highlightVisible(HiglId id);
    void highlightYviewRedirect();
    void highlightYviewCallback(int value);
    void highlightYviewHook();
    void highlightAll(const HiglPat& pat, int startPos);

private:
    static constexpr QRgb s_colFind   {0xfffaee0a};
    static constexpr QRgb s_colFindInc{0xffc8ff00};

    MainText * const m_mainText;
    MainWin * const m_mainWin;
    QProgressBar* m_hipro = nullptr;

    std::multimap<int,HiglId> m_tags;
    int           m_findIncPos = -1;

    std::vector<HiglPat> m_patList;
    bool          m_hallPatComplete = false;
    int           m_yViewValue = -1;
    bool          m_yViewRedirected = false;

    BgTask      * tid_high_init = nullptr;
    BgTask      * tid_search_hall = nullptr;
};

#endif /* _HIGHLIGHTER_H */

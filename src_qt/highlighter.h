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
    bool                m_strikeout = false;
    QString             m_font;
    // relief: "", raised, sunken, ridge, groove
    // relief borderwidth: 1,2,...,9
    // spacing: 0,1,2,...
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
    HiglId          m_id;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class Highlighter : public QWidget
{
    Q_OBJECT

public:
    Highlighter(MainText * textWid);
    QJsonArray getRcValues();
    void setRcValues(const QJsonValue& val);
    void removeInc(QTextDocument * doc);
    void getPatList(std::vector<HiglPatExport>&) const;
    void setList(const std::vector<HiglPatExport>& patList);
    const HiglFmtSpec * getFmtSpecForLine(int line);
    void configFmt(QTextCharFormat& fmt, const HiglFmtSpec& fmtSpec);
    void adjustLineNums(int top_l, int bottom_l);
    HiglId allocateNewId();

    void highlightInit();
    void highlightAll(const HiglPat& pat, int line);
    void highlightVisible(const HiglPat& pat);
    void searchHighlightMatch(QTextCursor& match);
    void searchHighlightClear();
    void searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case, bool onlyVisible);
    void searchHighlightAll(const HiglPat& pat, int line);

    static const int INVALID_HIGL_ID = 0;

private:
    void addSearchInc(QTextCursor& sel);
    void addSearchHall(QTextCursor& sel, const QTextCharFormat& fmt, HiglId id);
    void removeHall(QTextDocument * doc, HiglId id);
    void redraw(QTextDocument * doc, int blkNum);
    void clearAll(QTextDocument * doc);
    const HiglPat* getPatById(HiglId id);

    void addPattern(const SearchPar& srch, const HiglFmtSpec& fmtSpec, HiglId id = INVALID_HIGL_ID);
    void highlightInitBg(int pat_idx, int line);
    int  highlightLines(const HiglPat& pat, int line);
    void highlightYviewRedirect();
    void highlightYviewCallback(int value);

private:
    MainText * const m_mainText;
    QProgressBar* m_hipro = nullptr;

    std::multimap<int,HiglId> m_tags;
    int           m_findIncPos = -1;

    std::vector<HiglPat> m_patList;
    HiglPat       m_hallPat;
    HiglId        m_lastId = INVALID_HIGL_ID;
    bool          m_hallPatComplete = false;
    int           m_hallYview = 0;
    bool          m_yViewRedirected = false;  // TODO obsolete

    static const bool block_bg_tasks = false;  //TODO
    BgTask      * tid_high_init = nullptr;
    BgTask      * tid_search_hall = nullptr;
};

#endif /* _HIGHLIGHTER_H */

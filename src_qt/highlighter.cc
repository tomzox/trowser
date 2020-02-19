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

#include <QTextBlock>
#include <QTextDocument>
#include <QProgressBar>
#include <QScrollBar>
#include <QAbstractSlider>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "bg_task.h"
#include "highlighter.h"
#include "search_list.h"


static constexpr QRgb s_colFind   (0xfffaee0a);
static constexpr QRgb s_colFindInc(0xffc8ff00);
//static constexpr s_colSelection QRgb(0xffc3c3c3);

// ----------------------------------------------------------------------------

Highlighter::Highlighter(MainText * textWid)
    : m_mainText(textWid)
{
    // default format for search highlighting
    m_hallPat.m_id = 0;
    m_hallPat.m_fmtSpec.m_bgCol = s_colFind;
    configFmt(m_hallPat.m_fmt, m_hallPat.m_fmtSpec);

    m_hipro = new QProgressBar(m_mainText);
        m_hipro->setOrientation(Qt::Horizontal);
        m_hipro->setTextVisible(true);
        m_hipro->setMinimum(0);
        m_hipro->setMaximum(100);
        m_hipro->setVisible(false);

    tid_high_init = new BgTask(m_mainText, BG_PRIO_HIGHLIGHT_INIT);
    tid_search_hall = new BgTask(m_mainText, BG_PRIO_HIGHLIGHT_SEARCH);
}

// mark the matching portion of a line of text
// only one line can be marked this way
void Highlighter::addSearchInc(QTextCursor& sel)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor(s_colFindInc));
    sel.mergeCharFormat(fmt);

    m_findIncPos = sel.blockNumber();
}

// mark the complete line containing the match
// merge with pre-existing format (new has precedence though), if any
void Highlighter::addSearchHall(QTextCursor& sel, const QTextCharFormat& fmt, HiglId id)
{
    QTextCursor c(sel);
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    auto pair = m_tags.equal_range(c.blockNumber());
    bool found = false;
    int others = 0;
    for (auto it = pair.first; it != pair.second; ++it)
    {
        if (it->second == id)
        {
            found = true;
            break;
        }
        else
            ++others;
    }
    if (found == false)
    {
        if (others == 0)
            c.setCharFormat(fmt);
        else
            c.mergeCharFormat(fmt);

        m_tags.emplace(std::make_pair(c.blockNumber(), id));
    }
}

// remove "incremental" search highlight from the respective line
void Highlighter::removeInc(QTextDocument * doc)
{
    if (m_findIncPos != -1)
    {
        redraw(doc, m_findIncPos);
        m_findIncPos = -1;
    }
}

// remove highlight indicated by ID from the entire document
void Highlighter::removeHall(QTextDocument * doc, HiglId id)
{
    for (auto it = m_tags.begin(); it != m_tags.end(); /*nop*/)
    {
        if (it->second == id)
        {
            int blkNum = it->first;
            it = m_tags.erase(it);
            redraw(doc, blkNum);
        }
        else
            ++it;
    }
}

// re-calculate line format for the given line after removal of a specific highlight
void Highlighter::redraw(QTextDocument * doc, int blkNum)
{
    QTextBlock blk = doc->findBlockByNumber(blkNum);

    QTextCursor c(blk);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    // TODO for better performance pass iterator from removeHall
    auto pair = m_tags.equal_range(c.blockNumber());
    if (pair.first != pair.second)
    {
        auto firstId = pair.first->second;
        auto it = pair.first;

        if (++it != pair.second)
        {
            QTextCharFormat fmt(getPatById(firstId)->m_fmt);

            for (/*nop*/; it != pair.second; ++it)
            {
                fmt.merge(getPatById(it->second)->m_fmt);
            }
            c.setCharFormat(fmt);
        }
        else  // one highlight remaining in this line
        {
            c.setCharFormat(getPatById(firstId)->m_fmt);
        }
    }
    else  // no highlight remaining in this line
    {
        QTextCharFormat fmt;  // default format w/o mark-up
        c.setCharFormat(fmt);
    }
}

const HiglPat* Highlighter::getPatById(HiglId id)
{
    if (id == 0)
        return &m_hallPat;

    for (auto& pat : m_patList)
        if (pat.m_id == id)
            return &pat;

    return &m_hallPat;  // should never happen
}

// UNUSED
void Highlighter::clearAll(QTextDocument * doc)
{
    QTextCharFormat fmt;  // default format w/o mark-up
    QTextBlock lastBlk = doc->lastBlock();
    QTextCursor c(doc);
    c.setPosition(lastBlk.position() + lastBlk.length(), QTextCursor::KeepAnchor);
    c.setCharFormat(fmt);

    m_tags.clear();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// non-reentrant due to internal static buffer
const HiglFmtSpec * Highlighter::getFmtSpecForLine(int line)
{
    const HiglFmtSpec * ptr = nullptr;

    auto pair = m_tags.equal_range(line);
    if (pair.first != pair.second)
    {
        auto firstId = pair.first->second;
        auto it = pair.first;

        if (++it != pair.second)
        {
            static HiglFmtSpec fmtSpecBuf;

            fmtSpecBuf = getPatById(firstId)->m_fmtSpec;

            for (/*nop*/; it != pair.second; ++it)
            {
                fmtSpecBuf.merge(getPatById(it->second)->m_fmtSpec);
            }
            ptr = &fmtSpecBuf;
        }
        else  // one highlight remaining in this line
        {
            ptr = &getPatById(firstId)->m_fmtSpec;
        }
    }
    return ptr;
}

void HiglFmtSpec::merge(const HiglFmtSpec& other)
{
    if (   (other.m_bgCol != HiglFmtSpec::INVALID_COLOR)
        || (other.m_bgStyle != Qt::NoBrush))
    {
        m_bgCol = other.m_bgCol;
        m_bgStyle = other.m_bgStyle;
    }
    if (   (other.m_fgCol != HiglFmtSpec::INVALID_COLOR)
        || (other.m_fgStyle != Qt::NoBrush))
    {
        m_fgCol = other.m_fgCol;
        m_fgStyle = other.m_fgStyle;
    }
    if (other.m_olCol != HiglFmtSpec::INVALID_COLOR)
    {
        m_olCol = other.m_olCol;
    }

    m_bold |= other.m_bold;
    m_italic |= other.m_italic;
    m_underline |= other.m_underline;
    m_strikeout |= other.m_strikeout;

    if (!other.m_font.isEmpty())
        m_font = other.m_font;
}

bool HiglFmtSpec::operator==(const HiglFmtSpec& other) const
{
    return  (m_bgCol == other.m_bgCol) &&
            (m_fgCol == other.m_fgCol) &&
            (m_olCol == other.m_olCol) &&
            (m_bgStyle == other.m_bgStyle) &&
            (m_fgStyle == other.m_fgStyle) &&
            (m_bold == other.m_bold) &&
            (m_italic == other.m_italic) &&
            (m_underline == other.m_underline) &&
            (m_strikeout == other.m_strikeout) &&
            (m_font == other.m_font);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * This function initializes a text char format instance with the options of a
 * syntax highlight format specification. The given format instance must have
 * default content (i.e. a newly created instance).
 */
void Highlighter::configFmt(QTextCharFormat& fmt, const HiglFmtSpec& fmtSpec)
{
    if (   (fmtSpec.m_bgCol != HiglFmtSpec::INVALID_COLOR)
        && (fmtSpec.m_bgStyle != Qt::NoBrush))
        fmt.setBackground(QBrush(QColor(fmtSpec.m_bgCol), fmtSpec.m_bgStyle));
    else if (fmtSpec.m_bgCol != HiglFmtSpec::INVALID_COLOR)
        fmt.setBackground(QColor(fmtSpec.m_bgCol));
    else if (fmtSpec.m_bgStyle != Qt::NoBrush)
        fmt.setBackground(fmtSpec.m_bgStyle);

    if (   (fmtSpec.m_fgCol != HiglFmtSpec::INVALID_COLOR)
        && (fmtSpec.m_fgStyle != Qt::NoBrush))
        fmt.setForeground(QBrush(QColor(fmtSpec.m_fgCol), fmtSpec.m_fgStyle));
    else if (fmtSpec.m_fgCol != HiglFmtSpec::INVALID_COLOR)
        fmt.setForeground(QColor(fmtSpec.m_fgCol));
    else if (fmtSpec.m_fgStyle != Qt::NoBrush)
        fmt.setForeground(fmtSpec.m_fgStyle);

    if (fmtSpec.m_olCol != HiglFmtSpec::INVALID_COLOR)
        fmt.setTextOutline(QColor(fmtSpec.m_olCol));

    if (!fmtSpec.m_font.isEmpty())
    {
        QFont tmp;
        if (tmp.fromString(fmtSpec.m_font))
            fmt.setFont(tmp);
    }

    if (fmtSpec.m_underline)
        fmt.setFontUnderline(true);
    if (fmtSpec.m_bold)
        fmt.setFontWeight(QFont::Bold);
    if (fmtSpec.m_italic)
        fmt.setFontItalic(true);
    if (fmtSpec.m_strikeout)
        fmt.setFontStrikeOut(true);
}

void Highlighter::addPattern(const SearchPar& srch, const HiglFmtSpec& fmtSpec, HiglId id)
{
    HiglPat fmt;

    configFmt(fmt.m_fmt, fmtSpec);
    fmt.m_srch = srch;
    fmt.m_fmtSpec = fmtSpec;
    fmt.m_id = ((id != INVALID_HIGL_ID) ? id : ++m_lastId);

    m_patList.push_back(fmt);
}

// TODO save search & inc highlight formats
QJsonArray Highlighter::getRcValues()
{
    QJsonArray arr;

    for (const HiglPat& pat : m_patList)
    {
        QJsonObject obj;

        obj.insert("search_pattern", QJsonValue(pat.m_srch.m_pat));
        obj.insert("search_reg_exp", QJsonValue(pat.m_srch.m_opt_regexp));
        obj.insert("search_match_case", QJsonValue(pat.m_srch.m_opt_case));

        if (pat.m_fmtSpec.m_bgCol != HiglFmtSpec::INVALID_COLOR)
            obj.insert("bg_col", QJsonValue(int(pat.m_fmtSpec.m_bgCol)));
        if (pat.m_fmtSpec.m_fgCol != HiglFmtSpec::INVALID_COLOR)
            obj.insert("fg_col", QJsonValue(int(pat.m_fmtSpec.m_fgCol)));
        if (pat.m_fmtSpec.m_bgStyle != Qt::NoBrush)
            obj.insert("bg_style", QJsonValue(pat.m_fmtSpec.m_bgStyle));
        if (pat.m_fmtSpec.m_fgStyle != Qt::NoBrush)
            obj.insert("fg_style", QJsonValue(pat.m_fmtSpec.m_fgStyle));
        if (pat.m_fmtSpec.m_olCol != HiglFmtSpec::INVALID_COLOR)
            obj.insert("outline_col", QJsonValue(int(pat.m_fmtSpec.m_olCol)));

        if (pat.m_fmtSpec.m_underline)
            obj.insert("font_underline", QJsonValue(true));
        if (pat.m_fmtSpec.m_bold)
            obj.insert("font_bold", QJsonValue(true));
        if (pat.m_fmtSpec.m_italic)
            obj.insert("font_italic", QJsonValue(true));
        if (pat.m_fmtSpec.m_strikeout)
            obj.insert("font_strikeout", QJsonValue(true));

        if (!pat.m_fmtSpec.m_font.isEmpty())
            obj.insert("font", QJsonValue(pat.m_fmtSpec.m_font));

        arr.push_back(obj);
    }

    return arr;
}

void Highlighter::setRcValues(const QJsonValue& val)
{
    const QJsonArray arr = val.toArray();
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        SearchPar srch;
        HiglFmtSpec fmtSpec;

        const QJsonObject obj = it->toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
        {
            const QString& var = it.key();
            const QJsonValue& val = it.value();

            if (var == "search_pattern")
                srch.m_pat = val.toString();
            else if (var == "search_reg_exp")
                srch.m_opt_regexp = val.toBool();
            else if (var == "search_match_case")
                srch.m_opt_case = val.toBool();

            else if (var == "bg_col")
                fmtSpec.m_bgCol = QRgb(val.toInt());
            else if (var == "fg_col")
                fmtSpec.m_fgCol = QRgb(val.toInt());
            else if (var == "bg_style")
                fmtSpec.m_bgStyle = Qt::BrushStyle(val.toInt());
            else if (var == "fg_style")
                fmtSpec.m_fgStyle = Qt::BrushStyle(val.toInt());
            else if (var == "outline_col")
                fmtSpec.m_olCol = QRgb(val.toInt());

            else if (var == "font_underline")
                fmtSpec.m_underline = true;
            else if (var == "font_bold")
                fmtSpec.m_bold = true;
            else if (var == "font_italic")
                fmtSpec.m_italic = true;
            else if (var == "font_strikeout")
                fmtSpec.m_strikeout = true;

            else if (var == "font")
                fmtSpec.m_font = val.toString();
        }
        addPattern(srch, fmtSpec);
    }
}

void Highlighter::getPatList(std::vector<HiglPatExport>& exp) const
{
    for (auto& w : m_patList)
    {
        exp.emplace_back(HiglPatExport{w.m_srch, w.m_fmtSpec, w.m_id});
    }
}

void Highlighter::setList(const std::vector<HiglPatExport>& patList)
{
    for (auto& w : m_patList)
    {
        removeHall(m_mainText->document(), w.m_id);
    }
    m_patList.clear();

    for (auto& w : patList)
    {
        addPattern(w.m_srch, w.m_fmtSpec, w.m_id);
    }

    tid_high_init->stop();
    highlightInit();
    // FIXME changing only a single pattern (note: redo foreach where m_id > thresh)
    //HighlightAll(w[0], w[4], opt)


    SearchList::signalHighlightReconfigured();
#if 0 //TODO

    // remove the highlight in other dialogs, if currently open
    //TODO MarkList_DeleteTag(tagname)
    //TODO MarkList_CreateHighlightTags()

    //TODO UpdateRcAfterIdle()

    searchHighlightClear()
#endif
}


HiglId Highlighter::allocateNewId()
{
    return ++m_lastId;
}

/**
 * This function must be called when portions of the text in the main window
 * have been deleted to update references to text lines. Parameter meaning:
 *
 * @param top_l     First line which is NOT deleted, or 0 to delete nothing at top
 * @param bottom_l  This line and all below are removed, or -1 if none
 */
void Highlighter::adjustLineNums(int top_l, int bottom_l)
{
    std::multimap<int,HiglId> newMap;

    for (auto it = m_tags.begin(); it != m_tags.end(); ++it)
    {
        if ( (it->first >= top_l) && ((it->first < bottom_l) || (bottom_l < 0)) )
        {
            newMap.emplace(std::make_pair(it->first - top_l, it->second));
        }
    }
    m_tags = std::move(newMap);

    // re-start initial highlighting, if not complete yet
    if (tid_high_init->isActive())
    {
        highlightInit();
    }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * This function is called after loading a new text to apply the color
 * highlighting to the complete text. This means all matches on all
 * highlight patterns has to be searched for. Since this can take some
 * time, the operation done in the background to avoid blocking the user.
 * The CPU is given up voluntarily after each pattern and after max. 100ms
 */
void Highlighter::highlightInit()
{
    if (m_patList.size() > 0)
    {
        // place a progress bar as overlay to the main window
        m_hipro->setValue(0);
        m_hipro->setVisible(true);

        //wt.f1_t.tag_add("margin", "1.0", "end")

        m_mainText->viewport()->setCursor(Qt::BusyCursor);

        // trigger highlighting for the 1st pattern in the background
        tid_high_init->start([=](){ highlightInitBg(0, 0); });

        // apply highlighting on the text in the visible area (this is quick)
        // use the yview callback to redo highlighting in case the user scrolls
        highlightYviewRedirect();
    }
    else
    {
        m_hipro->setVisible(false);
    }
}


/**
 * This function is a slave-function of HighlightInit. The function
 * loops across all members in the global pattern list to apply color
 * the respective highlighting. The loop is broken up by installing each
 * new iteration as an idle event (and limiting each step to 100ms)
 */
void Highlighter::highlightInitBg(int pat_idx, int line)
{
    if ((size_t)pat_idx < m_patList.size())
    {
        // here we do the actual work:
        // apply the tag to all matching lines of text
        line = highlightLines(m_patList[pat_idx], line);

        if (line >= 0)
        {
            // not done yet - reschedule
            tid_high_init->start([=](){ highlightInitBg(pat_idx, line); });
        }
        else
        {
            // trigger next tag
            pat_idx += 1;
            tid_high_init->start([=](){ highlightInitBg(pat_idx, 0); });

            // update the progress bar
            m_hipro->setValue(100 * pat_idx / m_patList.size());
        }
    }
    else
    {
        m_hipro->setVisible(false);
        m_mainText->viewport()->setCursor(Qt::ArrowCursor);
    }
}


/**
 * This function searches for all lines in the main text widget which match the
 * given pattern and adds the given tag to them.  If the loop doesn't complete
 * within 100ms, the search is paused and the function returns the number of the
 * last searched line.  In this case the caller must invoke the funtion again
 * (as an idle event, to allow user-interaction in-between.)
 */
int Highlighter::highlightLines(const HiglPat& pat, int line)
{
    qint64 start_t = QDateTime::currentMSecsSinceEpoch();

    while (true)
    {
        QTextBlock block;
        int matchPos, matchLen;
        if (m_mainText->findInBlocks(pat.m_srch, line, true, matchPos, matchLen, &block))
        {
            // match found, highlight this line
            //wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
            QTextCursor match(block);
            match.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, matchPos - block.position());
            match.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);
            addSearchHall(match, pat.m_fmt, pat.m_id);

            // trigger the search result list dialog in case the line is included there too
            SearchList::signalHighlightLine(block.blockNumber());

            line = matchPos + matchLen;
        }
        else if (matchPos >= 0)
        {
            line = matchPos;
        }
        else
            break;

        // limit the runtime of the loop - return start line number for the next invocation
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - start_t > 100)
            return line;
    }
    // all done for this pattern
    return -1;
}


/**
 * This helper function schedules the line highlight function until highlighting
 * is complete for the given pattern.  This function is used to add highlighting
 * for single tags (e.g. modified highlight patterns or colors; currently not used
 * for search highlighting because a separate "cancel ID" is required.)
 */
void Highlighter::highlightAll(const HiglPat& pat, int line)
{
    line = highlightLines(pat, line);
    if (line >= 0)
    {
        tid_high_init->start([=](){ highlightAll(pat, line); });
    }
    else
    {
        m_mainText->viewport()->setCursor(Qt::ArrowCursor);
    }
}


/**
 * This function searches the currently visible text content for all lines
 * which contain the given sub-string and marks these lines with the given tag.
 */
void Highlighter::highlightVisible(const HiglPat& pat)
{
    auto view = m_mainText->viewport();
    auto c1 = m_mainText->cursorForPosition(QPoint(0, 0));
    auto c2 = m_mainText->cursorForPosition(QPoint(0, view->height()));

    int line = c1.block().position();
    int line_end = c2.block().position() + c2.block().length();

    while (line < line_end)
    {
        QTextBlock block;
        int matchPos, matchLen;
        if (m_mainText->findInBlocks(pat.m_srch, line, true, matchPos, matchLen, &block))
        {
            //wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
            QTextCursor match(block);
            match.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, matchPos - block.position());
            match.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);
            addSearchHall(match, pat.m_fmt, pat.m_id);
            line = matchPos + matchLen;
        }
        else if (matchPos >= 0)
        {
            line = matchPos;
        }
        else
            break;
    }
}


/**
 * This callback is installed to the main text widget's yview. It is used
 * to detect changes in the view to update highlighting if the highlighting
 * task is not complete yet. The event is forwarded to the vertical scrollbar.
 */
void Highlighter::highlightYviewCallback(int)
{
    if (tid_high_init->isActive())
    {
        for (const HiglPat& pat : m_patList)
        {
            highlightVisible(pat);
        }
    }

    //if (tid_search_hall->isActive())
    //{
    //    highlightVisible(m_hallPat);
    //}

    // automatically remove the redirect if no longer needed
    if (!tid_high_init->isActive() && !tid_search_hall->isActive())
    {
        disconnect(m_mainText->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &Highlighter::highlightYviewCallback);
        m_yViewRedirected = false;
    }
}


/**
 * This function redirect the yview callback from the scrollbar into the above
 * function. This is used to install a redirection for the duration of the
 * initial or search highlighting task.
 */
void Highlighter::highlightYviewRedirect()
{
    if (m_yViewRedirected == false)
    {
        // TODO/FIXME does not work when scrolling via keyboard control
        //  possible solution: override MainText::paintEvent()
        connect(m_mainText->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &Highlighter::highlightYviewCallback);
        m_yViewRedirected = true;
    }
}


/**
 * This function clears the current search color highlighting without
 * resetting the search string. It's bound to the "&" key, but also used
 * during regular search reset.
 */
void Highlighter::searchHighlightClear()
{
    tid_search_hall->stop();

    removeHall(m_mainText->document(), m_hallPat.m_id);
    removeInc(m_mainText->document());

    m_hallPat.m_srch.reset();
    m_hallPatComplete = false;

    m_mainText->viewport()->setCursor(Qt::ArrowCursor);

    SearchList::signalHighlightReconfigured();
}


/**
 * This function triggers color highlighting of all lines of text which match
 * the current search string.  The function is called when global highlighting
 * is en-/disabled, when the search string is modified or when search options
 * are changed.
 */
void Highlighter::searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case, bool onlyVisible)
{
    if (   (m_hallPat.m_srch.m_pat != pat)
        || (m_hallPat.m_srch.m_opt_regexp != opt_regexp)
        || (m_hallPat.m_srch.m_opt_case != opt_case)
        || (m_hallPatComplete == false))
    {
        // remember options for comparison
        m_hallPat.m_srch.m_pat = pat;
        m_hallPat.m_srch.m_opt_regexp = opt_regexp;
        m_hallPat.m_srch.m_opt_case = opt_case;
        m_hallPatComplete = false;

        if (!onlyVisible)
        {
            // display "busy" cursor until highlighting is finished
            m_mainText->viewport()->setCursor(Qt::BusyCursor);

            // implicitly kill background highlight process for obsolete pattern
            // start highlighting in the background
            tid_search_hall->start([=](){ searchHighlightAll(m_hallPat, 0); });
        }

        // apply highlighting on the text in the visible area (this is quick)
        // (note this is required in addition to the redirect below)
        m_hallYview = 0;
        highlightVisible(m_hallPat);

        // use the yview callback to redo highlighting in case the user scrolls
        //highlightYviewRedirect();
    }
}

/**
 * This helper function calls the global search highlight function until
 * highlighting is complete.
 */
void Highlighter::searchHighlightAll(const HiglPat& pat, int line)
{
    // FIXME this is a work-around because highlightYviewRedirect does not work
    if (m_hallYview != m_mainText->verticalScrollBar()->value())
    {
        m_hallYview = m_mainText->verticalScrollBar()->value();
        highlightVisible(pat);
    }

    line = highlightLines(pat, line);
    if (line >= 0)
    {
        tid_search_hall->start([=](){ searchHighlightAll(pat, line); });
    }
    else
    {
        m_mainText->viewport()->setCursor(Qt::ArrowCursor);
        m_hallPatComplete = true;
    }
}


/**
 * This function is called after a search match to mark the matching text and
 * the complete line containint the matching text (even when "highlight all" is
 * not enabled.)
 */
void Highlighter::searchHighlightMatch(QTextCursor& match)
{
    addSearchHall(match, m_hallPat.m_fmt, m_hallPat.m_id);
    addSearchInc(match);
}



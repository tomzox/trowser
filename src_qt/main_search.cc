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

#include <QApplication>
#include <QWidget>
#include <QKeyEvent>
#include <QShortcut>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QCheckBox>
#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "status_line.h"
#include "search_list.h"
#include "bg_task.h"
#include "dlg_hist.h"

// ----------------------------------------------------------------------------

MainFindEnt::MainFindEnt(MainSearch * search, QWidget * parent)
    : QLineEdit(parent)
    , m_search(search)
{
    connect(this, &QLineEdit::textChanged, m_search, &MainSearch::searchVarTrace);
}

void MainFindEnt::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    m_search->searchInit();
}

void MainFindEnt::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    m_search->searchLeave();
}

void MainFindEnt::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Qt::Key_Escape:
            m_search->searchAbort();
            break;

        case Qt::Key_Return:
            m_search->searchReturn();
            break;

        case Qt::Key_N:
            if (e->modifiers() == Qt::ControlModifier)
                m_search->searchIncrement(true, false);
            else if (e->modifiers() == (Qt::ControlModifier + Qt::ShiftModifier))
                m_search->searchIncrement(false, false);
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_E:
            if (e->modifiers() == Qt::AltModifier)
                m_search->searchOptToggleRegExp();
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_M:
            if (e->modifiers() == Qt::AltModifier)
                m_search->searchOptToggleCase();
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_C:
            if (e->modifiers() == Qt::ControlModifier)
                m_search->searchAbort();
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_X:
            if (e->modifiers() == Qt::ControlModifier)
                m_search->searchRemoveFromHistory();
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_D:
            if (e->modifiers() == Qt::ControlModifier)
                m_search->searchComplete();
            else if (e->modifiers() == (Qt::ControlModifier + Qt::ShiftModifier))
                m_search->searchCompleteLeft();
            else
                QLineEdit::keyPressEvent(e);
            break;

        case Qt::Key_Up:
            m_search->searchBrowseHistory(true);
            break;
        case Qt::Key_Down:
            m_search->searchBrowseHistory(false);
            break;

        default:
            QLineEdit::keyPressEvent(e);
            break;
    }
}

// ----------------------------------------------------------------------------

MainSearch::MainSearch(MainWin * mainWin)
    : QWidget(mainWin)
    , m_mainWin(mainWin)
{
    m_timSearchInc = new BgTask(this, BG_PRIO_SEARCH_INC);

    tlb_history.reserve(TLB_HIST_MAXLEN + 1);
}

MainSearch::~MainSearch()
{
    delete m_timSearchInc;
}

void MainSearch::connectWidgets(MainText    * textWid,
                                Highlighter * higl,
                                MainFindEnt * f2_e,
                                QCheckBox   * f2_hall,
                                QCheckBox   * f2_mcase,
                                QCheckBox   * f2_regexp)
{
    m_mainText = textWid;
    m_higl = higl;
    m_f2_e = f2_e;
    m_f2_hall = f2_hall;
    m_f2_mcase = f2_mcase;
    m_f2_regexp = f2_regexp;
}

// Note: QJsonObject does not support move-assignment operator (crashes)
QJsonObject MainSearch::getRcValues()
{
    QJsonObject obj;

    QJsonArray arr;
    for (auto& hl : tlb_history)
    {
        QJsonArray el{ QJsonValue(hl.m_pat),
                       QJsonValue(hl.m_opt_regexp),
                       QJsonValue(hl.m_opt_case) };
        arr.push_back(el);
    }
    obj.insert("tlb_history", arr);

    // dump search settings
    searchGetParams(); // update tlb_find et.al. from widget state
    obj.insert("tlb_case", QJsonValue(tlb_case));
    obj.insert("tlb_regexp", QJsonValue(tlb_regexp));
    obj.insert("tlb_hall", QJsonValue(tlb_hall));
    obj.insert("tlb_hist_maxlen", QJsonValue((int)TLB_HIST_MAXLEN));

    return obj;
}

void MainSearch::setRcValues(const QJsonObject& obj)
{
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        if (var == "tlb_history")
        {
            QJsonArray arr = val.toArray();
            for (auto it = arr.begin(); it != arr.end(); ++it)
            {
                const QJsonArray hl = it->toArray();
                tlb_history.emplace_back(SearchPar(hl.at(0).toString(),
                                                   hl.at(1).toBool(),
                                                   hl.at(2).toBool()));
            }
        }
        else if (var == "tlb_case")
        {
            m_f2_mcase->setChecked(val.toBool());
        }
        else if (var == "tlb_regexp")
        {
            m_f2_regexp->setChecked(val.toBool());
        }
        else if (var == "tlb_hall")
        {
            m_f2_hall->setChecked(val.toBool());
        }
        else if (var == "tlb_hist_maxlen")
        {
        }
        else
            fprintf(stderr, "trowser: unknown keyword %s in search RC config\n", var.toLatin1().data());
    }
}

//
// This function is bound to the "Highlight all" checkbutton to en- or disable
// global highlighting.
//
void MainSearch::searchOptToggleHall()
{
    m_f2_hall->setChecked( !m_f2_hall->isChecked() );
    searchHighlightSettingChange();
}

void MainSearch::searchOptToggleRegExp()
{
    m_f2_regexp->setChecked( !m_f2_regexp->isChecked() );
    searchHighlightSettingChange();
}

void MainSearch::searchOptToggleCase()
{
    m_f2_mcase->setChecked( !m_f2_mcase->isChecked() );
    searchHighlightSettingChange();
}


//
// This function is invoked after a change in search settings (i.e. case
// match, reg.exp. or global highlighting.)  The changed settings are
// stored in the RC file and a possible search highlighting is removed
// or updated (the latter only if global highlighting is enabled)
//
void MainSearch::searchHighlightSettingChange()
{
    m_mainWin->updateRcAfterIdle();

    m_higl->searchHighlightClear();
    searchHighlightUpdateCurrent();
}


//
// This is a wrapper for the above function which works on the current
// pattern in the search entry field.
//
void MainSearch::searchHighlightUpdateCurrent()
{
    searchGetParams(); // update tlb_find et.al. from widget state
    if (tlb_hall)
    {
        if (tlb_find != "")
        {
            if (searchExprCheck(tlb_find, tlb_regexp, true))
            {
                searchHighlightUpdate(tlb_find, tlb_regexp, tlb_case);
            }
        }
    }
}

void MainSearch::searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case)
{
    if (!pat.isEmpty())
    {
        if (tlb_hall)
        {
            m_higl->searchHighlightUpdate(pat, opt_regexp, opt_case, m_f2_e->hasFocus());
        }
        else
            m_higl->searchHighlightClear();
    }
}

void MainSearch::searchHighlightClear()
{
    m_higl->searchHighlightClear();
}

// -------------------------------------------

//
// This function is invoked when the user enters text in the "find" entry field.
// In contrary to the "atomic" search, this function only searches a small chunk
// of text, then re-schedules itself as an "idle" task.  The search can be aborted
// at any time by canceling the task.
//
void MainSearch::searchBackground(const SearchPar& par, bool is_fwd, int startPos, bool is_changed,
                                  const std::function<void(QTextCursor&)>& callback)
{
    //if {$block_bg_tasks} {
    //  // background tasks are suspended - re-schedule with timer
    //  set tid_search_inc [after 100 [list Search_Background $pat $is_fwd $opt $startPos $is_changed $callback]]
    //  return
    //}

    bool isDone;
    if (is_fwd) {
        QTextBlock b = m_mainText->document()->end();
        if (b != m_mainText->document()->begin())
            b = b.previous();
        isDone = (startPos >= b.position() + b.length());
    } else {
        isDone = (startPos <= 0);
    }
    if (!isDone)
    {
        // invoke the actual search in the selected portion of the document
        int matchPos, matchLen;
        bool found = m_mainText->findInBlocks(par, startPos, is_fwd, matchPos, matchLen);
        //printf("XXX %d -> %d,%d found?:%d\n", startPos, matchPos, matchLen, found);

        if (found)
        {
            // match found -> report; done
            QTextCursor c2 = m_mainText->textCursor();
            c2.setPosition(matchPos);
            c2.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);

            searchHandleMatch(c2, par.m_pat, par.m_opt_regexp, par.m_opt_case, is_changed);
            callback(c2);
        }
        else if (matchPos >= 0)
        {
            // no match found in this portion -> reschedule next iteration
            m_timSearchInc->start([=](){ searchBackground(par, is_fwd, matchPos, is_changed, callback); });
        }
        else
            isDone = true;
    }
    if (isDone)
    {
        QTextCursor c2;
        searchHandleMatch(c2, par.m_pat, par.m_opt_regexp, par.m_opt_case, is_changed);
        callback(c2);
    }
}


/**
 * This function searches the main text content for the expression in the
 * search entry field, starting at the current cursor position. When a match
 * is found, the cursor is moved there and the line is highlighed.
 */
bool MainSearch::searchAtomic(const QString& pat, bool opt_regexp, bool opt_case, bool is_fwd, bool is_changed)
{
    bool found = false;

    if (!pat.isEmpty() && searchExprCheck(pat, opt_regexp, true))
    {
        m_mainText->cursorJumpPushPos();
        tlb_last_dir = is_fwd;

        int start_pos = searchGetBase(is_fwd, false);

        auto c2 = m_mainText->findInDoc(pat, opt_regexp, opt_case, is_fwd, start_pos);

        // update cursor position and highlight
        searchHandleMatch(c2, pat, opt_regexp, opt_case, is_changed);
        found = !c2.isNull();
    }
    else
    {
        // empty or invalid expression: just remove old highlights
        searchReset();
    }
    return found;
}

/**
 * This function handles the result of a text search in the main window.
 * If a match was found, the cursor is moved to the start of the match and
 * the word, line are highlighted. Optionally, a background process to
 * highlight all matches is started.  If no match is found, any previously
 * applies highlights are removed.
 */
void MainSearch::searchHandleMatch(QTextCursor& match, const QString& pat,
                                   bool opt_regexp, bool opt_case, bool is_changed)
{
    if (!match.isNull() || is_changed)
    {
        m_higl->removeInc(m_mainText->document());

        if (!tlb_hall)  // else done below
            searchHighlightClear();
    }
    if (!match.isNull())
    {
        // mark the matching text & complete line containing the match
        m_higl->searchHighlightMatch(match);

        // move the cursor to the beginning of the matching text
        match.setPosition(std::min(match.position(), match.anchor()));
        m_mainText->setTextCursor(match);
        //m_mainText->centerCursor();

        int line = match.block().blockNumber();
        SearchList::signalHighlightLine(line);
        SearchList::matchView(line);
    }

    if (tlb_hall)
    {
        searchHighlightUpdate(pat, opt_regexp, opt_case);
    }
}


/**
 * This function displays a message if no match was found for a search
 * pattern. This is split off from the search function so that some
 * callers can override the message.
 */
void MainSearch::searchHandleNoMatch(const QString& pat, bool is_fwd)
{
    QString msg = QString("No match until ")
                    + (is_fwd ? "end" : "start")
                    + " of file"
                    + (pat.isEmpty() ? "" : ": ")
                    + pat;
    m_mainWin->mainStatusLine()->showWarning("search", msg);
}


/**
 * This function is bound to all changes of the search text in the "find" entry
 * field. It's called when the user enters new text and triggers an incremental
 * search.
 */
void MainSearch::searchVarTrace(const QString &)
{
    if (m_f2_e->hasFocus())
    {
        //TODO m_timSearchInc->setInterval(SEARCH_INC_DELAY);
        m_timSearchInc->start([=, is_fwd=tlb_last_dir](){ searchIncrement(is_fwd, true); });
    }
}


/**
 * This function performs a so-called "incremental" search after the user
 * has modified the search text. This means searches are started already
 * while the user is typing.
 */
void MainSearch::searchIncrement(bool is_fwd, bool is_changed)
{
    m_timSearchInc->stop();

    searchGetParams(); // update tlb_find et.al. from widget state

    if ((tlb_find != "") && searchExprCheck(tlb_find, tlb_regexp, false))
    {
        if (tlb_inc_base < 0)
        {
            tlb_inc_base = searchGetBase(is_fwd, true);
            tlb_inc_xview = m_mainText->verticalScrollBar()->value();
            tlb_inc_yview = m_mainText->horizontalScrollBar()->value();

            m_mainText->cursorJumpPushPos();
        }
        int start_pos;
        if (is_changed)
        {
            m_higl->searchHighlightClear();
            start_pos = tlb_inc_base;
        }
        else
            start_pos = searchGetBase(is_fwd, false);

        searchBackground(SearchPar(tlb_find, tlb_regexp, tlb_case),
                         is_fwd, start_pos, is_changed,
                         [=](QTextCursor& c){ searchIncMatch(c, tlb_find, is_fwd, is_changed); });
    }
    else
    {
        searchReset();

        if (tlb_find != "")
            m_mainWin->mainStatusLine()->showError("search", "Incomplete or invalid reg.exp.");
        else
            m_mainWin->mainStatusLine()->clearMessage("search");
    }
}

/**
 * This function is invoked as callback after a background search for the
 * incremental search in the entry field is completed.  (Before this call,
 * cursor position and search highlights are already updated.)
 */
void MainSearch::searchIncMatch(QTextCursor& match, const QString& pat, bool is_fwd, bool is_changed)
{
    if (match.isNull() && (tlb_inc_base >= 0))
    {
        if (is_changed)
        {
            m_mainText->verticalScrollBar()->setValue(tlb_inc_xview);
            m_mainText->horizontalScrollBar()->setValue(tlb_inc_yview);

            QTextCursor c = m_mainText->textCursor();
            c.setPosition(tlb_inc_base);
            m_mainText->setTextCursor(c);
            //m_mainText->centerCursor();
        }

        if (is_fwd)
            m_mainWin->mainStatusLine()->showWarning("search", "No match until end of file");
        else
            m_mainWin->mainStatusLine()->showWarning("search", "No match until start of file");
    }
    else
        m_mainWin->mainStatusLine()->clearMessage("search");

    if (tlb_hist_pos > 0)
    {
        auto it = tlb_history.begin() + tlb_hist_pos;
        if ((it != tlb_history.end()) && (pat != it->m_pat))
        {
            tlb_hist_pos = -1;
            tlb_hist_prefix.clear();
        }
    }
}

/**
 * This function checks if the search pattern syntax is valid
 */
bool MainSearch::searchExprCheck(const QString& pat, bool is_re, bool display)
{
    if (is_re)
    {
        QRegularExpression re(pat);
        if (re.isValid() == false)
        {
            if (display)
            {
                QString msg = QString("Syntax error in search expression: ")
                                + re.errorString();
                m_mainWin->mainStatusLine()->showError("search", msg);
            }
            return false;
        }
    }
    return true;
}


/**
 * This function returns the start address for a search.  The first search
 * starts at the insertion cursor. If the cursor is not visible, the search
 * starts at the top or bottom of the visible text. When a search is repeated,
 * the search must behind the previous match (for a forward search) to prevent
 * finding the same word again, or finding an overlapping match. (For backwards
 * searches overlaps cannot be handled via search arguments; such results are
 * filtered out when a match is found.)
 */
int MainSearch::searchGetBase(bool is_fwd, bool is_init)
{
    int view_start = m_mainText->cursorForPosition(QPoint(0, 0)).position();
    QPoint bottom_right(m_mainText->viewport()->width() - 1, m_mainText->viewport()->height() - 1);
    int view_end = m_mainText->cursorForPosition(bottom_right).position();
    //cursor.setPosition(view_end, QTextCursor::KeepAnchor);

    QTextCursor c = m_mainText->textCursor();
    int cur_pos = c.position();
    int start_pos;

    if ((cur_pos >= view_start) && (cur_pos <= view_end))
    {
        if (is_init)
            start_pos = cur_pos;
        else if (is_fwd == false)
            start_pos = ((cur_pos > 0) ? (cur_pos - 1) : 0);
        else
            start_pos = cur_pos + 1;
    }
    else
    {
        start_pos = is_fwd ? view_start : view_end;
        c.setPosition(cur_pos);
        m_mainText->setTextCursor(c);
        m_mainText->centerCursor();
    }

#if 0 //TODO?
    // move start position for forward search after the end of the previous match
    if (is_fwd) {
        // search for tag which marks the previous match (would have been cleared if the pattern changed)
        set pos12 [.f1.t tag nextrange findinc [concat $start_pos linestart] [concat $start_pos lineend]]
        if {$pos12 ne ""} {
            // check if the start position (i.e. the cursor) is still inside of the area of the match
            if {[scan $start_pos "%d.%d" line1 char1] == 2} {
                if {[scan $pos12 "%d.%d %*d.%d" line2 char2 char3] >= 3} {
                    if {($line1 == $line2) && ($char1 >= $char2) && ($char1 < $char3)} {
                        set start_pos [lindex $pos12 1]
                    }
                }
            }
        }
    }
#endif
    return start_pos;
}

SearchPar MainSearch::getCurSearchParams()
{
    searchGetParams(); // update tlb_find et.al. from widget state

    if (!tlb_find.isEmpty())
    {
        return SearchPar(tlb_find, tlb_regexp, tlb_case);
    }
    else if (tlb_history.size() > 0)
    {
        return tlb_history.front();
    }
    else
        return SearchPar();
}

void MainSearch::searchGetParams()
{
    tlb_find   = m_f2_e->text();
    tlb_regexp = m_f2_regexp->isChecked();
    tlb_case   = m_f2_mcase->isChecked();
    tlb_hall   = m_f2_hall->isChecked();
}

/**
 * This function is used by the highlight pattern dialog for searching one or
 * more of the defined patterns. The cursor is set onto the first line matching
 * one of the patterns in the given direction, if any.
 */
void MainSearch::searchFirst(bool is_fwd, const std::vector<SearchPar>& patList)
{
    searchGetParams(); // update tlb_hall (for match handling)
    m_mainText->cursorJumpPushPos();
    searchHighlightClear();

    QTextCursor match;
    const SearchPar * matchPar = nullptr;
    int start_pos = searchGetBase(is_fwd, false);

    for (auto& pat : patList)
    {
        if (pat.m_pat.isEmpty() || !searchExprCheck(pat.m_pat, pat.m_opt_regexp, true))
            continue;

        searchAddHistory(pat.m_pat, pat.m_opt_regexp, pat.m_opt_case);

        auto c2 = m_mainText->findInDoc(pat.m_pat, pat.m_opt_regexp, pat.m_opt_case, is_fwd, start_pos);

        if (   !c2.isNull()
            && (match.isNull() || (is_fwd ? (c2 < match) : (c2 > match))) )
        {
            match = c2;
            matchPar = &pat;
        }
    }

    if (matchPar != nullptr)
    {
        m_mainWin->mainStatusLine()->clearMessage("search");
        searchHandleMatch(match, matchPar->m_pat, matchPar->m_opt_regexp, matchPar->m_opt_case, true);
    }
    else
    {
        searchHandleNoMatch("", is_fwd);
    }
}

/**
 * This function is used by the highlight editor to copy a set of search
 * parameters into the respective entry fields.
 */
void MainSearch::searchEnterOpt(const SearchPar& pat)
{
    // force focus into find entry field & suppress "Enter" event
    searchInit();
    tlb_find_focus = true;
    m_f2_e->setFocus(Qt::ShortcutFocusReason);
    m_f2_e->activateWindow();
    searchHighlightClear();

    m_f2_e->setText(pat.m_pat);
    m_f2_regexp->setChecked(pat.m_opt_regexp);
    m_f2_mcase->setChecked(pat.m_opt_case);

    searchNext(tlb_last_dir);
}

/**
 * This function is used by the various key bindings which repeat a
 * previous search.
 */
bool MainSearch::searchNext(bool is_fwd)
{
    bool found = false;

    m_mainWin->mainStatusLine()->clearMessage("search");
    searchGetParams(); // update tlb_find et.al. from widget state

    if (!tlb_find.isEmpty())
    {
        found = searchAtomic(tlb_find, tlb_regexp, tlb_case, is_fwd, false);
        if (!found)
            searchHandleNoMatch(tlb_find, is_fwd);
    }
    else if (tlb_history.size() > 0)
    {
        // empty expression: repeat last search
        const SearchPar &par = tlb_history.front();
        found = searchAtomic(par.m_pat, par.m_opt_regexp, par.m_opt_case, is_fwd, false);

        if (!found)
            searchHandleNoMatch(par.m_pat, is_fwd);
    }
    else
    {
        m_mainWin->mainStatusLine()->showError("search", "No pattern defined for search repeat");
    }
    return found;
}

/**
 * This function is used by the "All" or "List all" buttons and assorted
 * keyboard shortcuts to list all text lines matching the current search
 * expression in a separate dialog window.
 */
void MainSearch::searchAll(bool raiseWin, int direction)
{
    searchGetParams(); // update tlb_find et.al. from widget state
    if (searchExprCheck(tlb_find, tlb_regexp, true))
    {
        searchAddHistory(tlb_find, tlb_regexp, tlb_case);

        // make focus return and cursor jump back to original position
        if (tlb_find_focus)
        {
            searchHighlightClear();
            searchReset();

            if (tlb_last_wid != nullptr)
            {
                // raise the caller's window above the main window
                tlb_last_wid->setFocus(Qt::ShortcutFocusReason);
                tlb_last_wid->activateWindow();
                tlb_last_wid->raise();
            }
            else
            {
              // note more clean-up is triggered via the focus-out event
              m_mainText->setFocus(Qt::ShortcutFocusReason);
            }
        }

        SearchPar par(tlb_find, tlb_regexp, tlb_case);
        SearchList::getInstance(raiseWin)->searchMatches(true, direction, par);
    }
}

/**
 * This function resets the state of the search engine.  It is called when
 * the search string is empty or a search is aborted with the Escape key.
 */
void MainSearch::searchReset()
{
    m_higl->searchHighlightClear();

    if (tlb_inc_base >= 0)
    {
        m_mainText->verticalScrollBar()->setValue(tlb_inc_xview);
        m_mainText->horizontalScrollBar()->setValue(tlb_inc_yview);

        QTextCursor c = m_mainText->textCursor();
        c.setPosition(tlb_inc_base);
        m_mainText->setTextCursor(c);
        //m_mainText->centerCursor();
        tlb_inc_base = -1;
    }
    m_mainWin->mainStatusLine()->clearMessage("search");
}


/**
 * This function is called when the "find" entry field receives keyboard focus
 * to intialize the search state machine for a new search.
 */
void MainSearch::searchInit()
{
    if (tlb_find_focus == false)
    {
        tlb_find_focus = true;
        tlb_hist_pos = -1;
        tlb_hist_prefix.clear();

        m_mainWin->mainStatusLine()->clearMessage("search");
    }
}


/**
 * This function is called to move keyboard focus into the search entry field.
 * The focus change will trigger the "init" function.  The caller can pass a
 * widget to which focus is passed when leaving the search via the Return or
 * Escape keys.
 */
void MainSearch::searchEnter(bool is_fwd, QWidget * parent)
{
    tlb_last_dir = is_fwd;
    m_f2_e->setText("");
    m_f2_e->setFocus(Qt::ShortcutFocusReason);

    // clear "highlight all" since search pattern is reset above
    searchHighlightClear();

    tlb_last_wid = parent;
    if (tlb_last_wid != nullptr)
    {
        tlb_last_wid->activateWindow();
        tlb_last_wid->raise();
    }
}


/**
 * This function is bound to the FocusOut event in the search entry field.
 * It resets the incremental search state.
 */
void MainSearch::searchLeave()
{
    m_timSearchInc->stop();

    // ignore if the keyboard focus is leaving towards another application
    if (m_mainWin->focusWidget() != nullptr)
    {
        if (searchExprCheck(tlb_find, tlb_regexp, false))
        {
            if (tlb_hall)
            {
                searchHighlightUpdateCurrent();
            }
            searchAddHistory(tlb_find, tlb_regexp, tlb_case);
        }

        tlb_hist_pos = -1;
        tlb_hist_prefix.clear();
        tlb_inc_base = -1;
        tlb_last_wid = nullptr;
        tlb_find_focus = false;
    }
}


/**
 * This function is called when the search window is left via "Escape" key.
 * The search highlighting is removed and the search text is deleted.
 */
void MainSearch::searchAbort()
{
    if (searchExprCheck(tlb_find, tlb_regexp, false))
    {
        searchAddHistory(tlb_find, tlb_regexp, tlb_case);
    }

    m_f2_e->setText("");
    searchReset();
    if (tlb_last_wid != nullptr)
    {
        tlb_last_wid->setFocus(Qt::ShortcutFocusReason);
        tlb_last_wid->activateWindow();
        tlb_last_wid->raise();
    }
    else
    {
        m_mainText->setFocus(Qt::ShortcutFocusReason);
    }
    // note more clean-up is triggered via the focus-out event
}


/**
 * This function is bound to the Return key in the search entry field.
 * If the search pattern is invalid (reg.exp. syntax) an error message is
 * displayed and the focus stays in the entry field. Else, the keyboard
 * focus is switched to the main window.
 */
void MainSearch::searchReturn()
{
    bool restart = false;

    if (m_timSearchInc->isActive())
    {
        m_timSearchInc->stop();
        restart = 1;
    }

    searchGetParams(); // update tlb_find et.al. from widget state
    if (tlb_find.isEmpty())
    {
        // empty expression: repeat last search
        if (tlb_history.size() > 0)
        {
            m_f2_e->setText(tlb_history.front().m_pat);
            restart = true;
        }
        else
        {
            m_mainWin->mainStatusLine()->showError("search", "No pattern defined for search repeat");
        }
    }

    if (searchExprCheck(tlb_find, tlb_regexp, true))
    {
        if (restart)
        {
            // incremental search not completed -> start regular search
            if (searchNext(tlb_last_dir) == false)
            {
                if (tlb_inc_base >= 0)
                {
                    m_mainText->verticalScrollBar()->setValue(tlb_inc_xview);
                    m_mainText->horizontalScrollBar()->setValue(tlb_inc_yview);

                    QTextCursor c = m_mainText->textCursor();
                    c.setPosition(tlb_inc_base);
                    m_mainText->setTextCursor(c);
                    //m_mainText->centerCursor();
                }
            }
        }

        // note this implicitly triggers the leave event
        if (tlb_last_wid != nullptr)
        {
            tlb_last_wid->setFocus(Qt::ShortcutFocusReason);
            tlb_last_wid->activateWindow();
            tlb_last_wid->raise();
        }
        else
        {
            m_mainText->setFocus(Qt::ShortcutFocusReason);
        }
    }
}

// FIXME temporary interface to search history dialog
// -> move history into sub-class, make this private and befriend the dialog
void MainSearch::removeFromHistory(const std::set<int>& excluded)
{
    if (excluded.size() != 0)
    {
        std::vector<SearchPar> tmp;
        tmp.reserve(TLB_HIST_MAXLEN + 1);

        for (size_t idx = 0; idx < tlb_history.size(); ++idx)
        {
            if (excluded.find(idx) == excluded.end())
                tmp.push_back(tlb_history[idx]);
        }
        tlb_history = tmp;

        DlgHistory::signalHistoryChanged();
        m_mainWin->updateRcAfterIdle();
    }
}

/**
 * This function add the given search string to the search history stack.
 * If the string is already on the stack, it's moved to the top. Note: top
 * of the stack is the front of the list.
 */
void MainSearch::searchAddHistory(const QString& txt, bool is_re, bool use_case)
{
    if (!txt.isEmpty())
    {
        // search for the expression in the history (options not compared)
        // remove the element if already in the list
        for (auto it = tlb_history.begin(); it != tlb_history.end(); ++it)
        {
            if (it->m_pat == txt)
            {
                tlb_history.erase(it);
                break;
            }
        }

        // insert the element at the top of the stack
        tlb_history.emplace(tlb_history.begin(), SearchPar(txt, is_re, use_case));

        // maintain max. stack depth
        while (tlb_history.size() > TLB_HIST_MAXLEN)
        {
            tlb_history.erase(tlb_history.end() - 1);
        }

        m_mainWin->updateRcAfterIdle();

        DlgHistory::signalHistoryChanged();
    }
}

/**
 * This function is bound to the up/down cursor keys in the search entry
 * field. The function is used to iterate through the search history stack.
 * The "up" key starts with the most recently used pattern, the "down" key
 * with the oldest. When the end of the history is reached, the original
 * search string is displayed again.
 */
void MainSearch::searchBrowseHistory(bool is_up)
{
    if (tlb_history.size() > 0) {
        if (tlb_hist_pos < 0) {
            tlb_hist_prefix = tlb_find;
            tlb_hist_pos = is_up ? 0 : (tlb_history.size() - 1);
        }
        else if (is_up) {
            if (tlb_hist_pos + 1 < (long)tlb_history.size()) {
                tlb_hist_pos += 1;
            }
            else {
                tlb_hist_pos = -1;
            }
        }
        else {
            tlb_hist_pos -= 1;
        }

        if (tlb_hist_prefix.length() > 0) {
            tlb_hist_pos = searchHistoryComplete(is_up ? 1 : -1);
        }

        if (tlb_hist_pos >= 0) {
            tlb_find = tlb_history.at(tlb_hist_pos).m_pat;
            m_f2_e->setText(tlb_find);
            m_f2_e->setCursorPosition(tlb_find.length());
        }
        else {
            // end of history reached -> reset
            tlb_find = tlb_hist_prefix;
            tlb_hist_pos = -1;
            tlb_hist_prefix = -1;
            m_f2_e->setText(tlb_find);
            m_f2_e->setCursorPosition(tlb_find.length());
        }
    }
}

/**
 * This helper function searches the search history stack for a search
 * string with a given prefix.
 */
int MainSearch::searchHistoryComplete(int step)
{
    for (int idx = tlb_hist_pos; (idx >= 0) && (idx < (long)tlb_history.size()); idx += step)
    {
        if (tlb_history.at(idx).m_pat.startsWith(tlb_hist_prefix))
            return idx;
    }
    return -1;
}

/**
 * This function is bound to "CTRL-x" in the "Find" entry field and
 * removes the current entry from the search history.
 */
void MainSearch::searchRemoveFromHistory()
{
    if ((tlb_hist_pos >= 0) && (tlb_hist_pos < (long)tlb_history.size()))
    {
        tlb_history.erase(tlb_history.begin() + tlb_hist_pos);

        m_mainWin->updateRcAfterIdle();

        DlgHistory::signalHistoryChanged();

        if (tlb_history.size() == 0)
        {
            tlb_hist_pos = -1;
            tlb_hist_prefix.clear();
        }
        else if (tlb_hist_pos >= (long)tlb_history.size())
        {
            tlb_hist_pos = tlb_history.size() - 1;
        }
    }
}

/**
 * This function is bound to "CTRL-d" in the "Find" entry field and
 * performs auto-completion of a search text by adding any following
 * characters in the word matched by the current expression.
 */
void MainSearch::searchComplete()
{
    searchGetParams(); // update tlb_find et.al. from widget state

    auto line_str = m_mainText->textCursor().block().text();
    int pos = m_mainText->textCursor().positionInBlock();
    int off;

    if (tlb_regexp && !tlb_find.isEmpty())
    {
        QRegularExpression::PatternOptions reflags =
                tlb_case ? QRegularExpression::NoPatternOption
                           : QRegularExpression::CaseInsensitiveOption;
        if (!searchExprCheck(tlb_find, true, true))
            return;
        QRegularExpression re2(tlb_find, reflags);
        auto mat2 = re2.match(line_str.midRef(pos), 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
        if (mat2.hasMatch())
            off = mat2.captured(0).length();
        else
            off = 0;
    }
    else
    {
        off = tlb_find.length();
    }

    static const QRegularExpression re1("^(?:\\W+|\\w+)"); // not thread-safe
    auto mat1 = re1.match(line_str.midRef(pos + off));
    if (mat1.hasMatch())
    {
        QString word(mat1.captured(0));
        searchEscapeSpecialChars(word, tlb_regexp);

        tlb_find += word;
        m_f2_e->setText(tlb_find);
        m_f2_e->setCursorPosition(tlb_find.length());
        m_f2_e->setSelection(tlb_find.length() - word.length(), tlb_find.length());
    }
}


/**
 * This function is bound to "CTRL-SHIFT-D" in the "Find" entry field and
 * performs auto-completion to the left by adding any preceding characters
 * before the current cursor position.
 */
void MainSearch::searchCompleteLeft()
{
    searchGetParams(); // update tlb_find et.al. from widget state

    auto line_str = m_mainText->textCursor().block().text();
    int off = m_mainText->textCursor().positionInBlock();

    static const QRegularExpression re1("(?:\\W+|\\w+)$"); // not thread-safe
    auto mat1 = re1.match(line_str.leftRef(off));
    if (mat1.hasMatch())
    {
        QString word(mat1.captured(0));
        searchEscapeSpecialChars(word, tlb_regexp);

        tlb_find = word + tlb_find;
        m_f2_e->setText(tlb_find);
        m_f2_e->setCursorPosition(word.length());
        m_f2_e->setSelection(0, word.length());
    }
}


/**
 * This function if bound to "*" and "#" in the main text window (as in VIM)
 * These keys allow to search for the word under the cursor in forward and
 * backwards direction respectively.
 */
void MainSearch::searchWord(bool is_fwd)
{
    searchGetParams(); // update tlb_find et.al. from widget state

    auto line_str = m_mainText->textCursor().block().text();
    int off = m_mainText->textCursor().positionInBlock();

    // extract word to the right starting at the cursor position
    static const QRegularExpression re1("^[\\w\\-]+"); // not thread-safe
    auto mat1 = re1.match(line_str.midRef(off));
    if (mat1.hasMatch())
    {
        QString word;

        // complete word to the left
        static const QRegularExpression re2("[\\w\\-]+$");
        auto mat2 = re2.match(line_str.leftRef(off));
        if (mat2.hasMatch())
            word = mat2.captured(0) + mat1.captured(0);
        else
            word = mat1.captured(0);

        searchEscapeSpecialChars(word, tlb_regexp);

        // add regexp to match on word boundaries
        if (tlb_regexp)
            word = QString("\\b") + word + "\\b";

        tlb_find = word;
        m_f2_e->setText(tlb_find);
        m_f2_e->setCursorPosition(0);

        searchAddHistory(tlb_find, tlb_regexp, tlb_case);
        m_mainWin->mainStatusLine()->clearMessage("search");

        bool found = searchAtomic(tlb_find, tlb_regexp, tlb_case, is_fwd, true);
        if (!found)
            searchHandleNoMatch(tlb_find, is_fwd);
    }
}

/**
 * This helper function escapes characters with special semantics in
 * regular expressions in a given word. The function is used for adding
 * arbitrary text to the search string.
 */
void MainSearch::searchEscapeSpecialChars(QString& word, bool is_re)
{
    if (is_re)
    {
        static const QRegularExpression re("([\\\\\\^\\.\\$\\|\\(\\)\\[\\]\\*\\+\\?\\{\\}\\-])");  // not thread-safe
        word.replace(re, "\\\\\\1");
    }
}

// legacy name was Mark_Line()
void MainSearch::highlightFixedLine(int line)
{
    m_mainText->cursorJumpPushPos();

    // move the cursor into the specified line
    QTextBlock block = m_mainText->document()->findBlockByNumber(line);
    QTextCursor c(block);
    m_mainText->setTextCursor(c);

    // remove a possible older highlight
    searchHighlightClear();

    // highlight the specified line
    m_higl->searchHighlightMatch(c);
}
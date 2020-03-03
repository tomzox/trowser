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
 * This module implements the class hat performs searches in the text widget of
 * the main window. The class has many interfaces that are connected to the
 * main menu, key bindings in the main text window, and most importantly to the
 * entry field and checkboxes of the main window's "find" toolbar.
 *
 * The class becomes active as soon as keyboard focus changes into the entry
 * fields. For each modification of the entry text, the class immediately
 * schedules a search in the background, and when found highlights the next
 * match (via the Highligher module) and makes that text line visible in the
 * main text widget. When global search highlighting is enabled, the class has
 * the Highligher class mark all matches wihin visible part of the text. When
 * the user modifies the search pattern before this process is completed, it
 * is aborted and restarted for the new pattern. When no match is found for the
 * new pattern, the cursor and view are reset to the start of the search. The
 * process described here is called "incremental search".
 *
 * In addition to the above, the class offers interfaces such as for repeating
 * the previous search in either direction. Such searches are done
 * "atomically", which means they are blocking. Notably the "search all" button
 * of the "find" toolbar is not implemented here, but rather in the search list
 * class.
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
#include "text_block_find.h"
#include "dlg_history.h"
#include "dlg_bookmarks.h"

// ----------------------------------------------------------------------------

/**
 * This wrapper class implements the search string entry field in the main
 * window. The class is derived from QLineEdit for overriding the key and focus
 * in/out event handlers. The events are forwarded directly to the MainSearch
 * object.
 */
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
    : QObject(mainWin)
    , m_mainWin(mainWin)
{
    m_timSearchInc = new BgTask(this, BG_PRIO_SEARCH_INC);

    tlb_history.reserve(TLB_HIST_MAXLEN + 1);
}

/**
 * Destructor: Freeing resources not automatically deleted via widget tree
 */
MainSearch::~MainSearch()
{
    delete m_timSearchInc;
}

/**
 * This external interface function is called once during start-up after all
 * classes are instantiated to establish the required connections, which are
 * the main text widget (within which this class performs searches), the
 * Highlighter class (which is used to highlight search matches) and the
 * widgets of the "find" toolbar (which provide user input to this class).
 */
void MainSearch::connectWidgets(MainText    * mainText,
                                Highlighter * higl,
                                MainFindEnt * f2_e,
                                QCheckBox   * f2_hall,
                                QCheckBox   * f2_mcase,
                                QCheckBox   * f2_regexp)
{
    m_mainText = mainText;
    m_higl = higl;
    m_f2_e = f2_e;
    m_f2_hall = f2_hall;
    m_f2_mcase = f2_mcase;
    m_f2_regexp = f2_regexp;
}

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this class. Currently this includes the current search string
 * and the search history stack.
 */
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
    obj.insert("tlb_case", QJsonValue(tlb_find.m_opt_case));
    obj.insert("tlb_regexp", QJsonValue(tlb_find.m_opt_regexp));
    obj.insert("tlb_hall", QJsonValue(tlb_hall));
    obj.insert("tlb_hist_maxlen", QJsonValue((int)TLB_HIST_MAXLEN));

    return obj;
}

/**
 * This function is called during start-up to apply configuration variables.
 * The function is the inverse of getRcValues()
 */
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
            tlb_find.m_opt_case = val.toBool();
            m_f2_mcase->setChecked(tlb_find.m_opt_case);
        }
        else if (var == "tlb_regexp")
        {
            tlb_find.m_opt_regexp = val.toBool();
            m_f2_regexp->setChecked(tlb_find.m_opt_regexp);
        }
        else if (var == "tlb_hall")
        {
            tlb_hall = val.toBool();
            m_f2_hall->setChecked(tlb_hall);
        }
        else if (var == "tlb_hist_maxlen")
        {
            //TODO
        }
        else
            fprintf(stderr, "trowser: unknown keyword %s in search RC config\n", var.toLatin1().data());
    }
}

/**
 * This function is bound to the "Highlight all" checkbutton and keyboard
 * shortcut to enable or disable global highlighting of search matches.
 */
void MainSearch::searchOptToggleHall(int v)
{
    tlb_hall = (v != 0);
    searchHighlightSettingChange();
}

/**
 * This function is bound to the "Reg.Exp." checkbutton and keyboard shortcut
 * to enable or disable use of regular expression in search matches.
 */
void MainSearch::searchOptToggleRegExp(int v)
{
    tlb_find.m_opt_regexp = (v != 0);
    searchHighlightSettingChange();
}

/**
 * This function is bound to the "Match case" checkbutton and keyboard shortcut
 * to enable or disable use of regular expression in search matches.
 */
void MainSearch::searchOptToggleCase(int v)
{
    tlb_find.m_opt_case = (v != 0);
    searchHighlightSettingChange();
}


/**
 * This function is invoked after a change in search settings (i.e. case
 * match, reg.exp. or global highlighting.)  The changed settings are
 * stored in the RC file and a possible search highlighting is removed
 * or updated (the latter only if global highlighting is enabled)
 */
void MainSearch::searchHighlightSettingChange()
{
    if (m_f2_e->hasFocus())
    {
        searchIncrement(tlb_last_dir, true);
    }
    else
    {
        searchHighlightClear();
        searchHighlightUpdateCurrent();
    }
    m_mainWin->updateRcAfterIdle();
}


/**
 * This is a wrapper for the following function which works on the current
 * pattern in the search entry field.
 */
void MainSearch::searchHighlightUpdateCurrent()
{
    if (tlb_hall)
    {
        if (tlb_find.m_pat.isEmpty() == false)
        {
            if (searchExprCheck(tlb_find, true))
            {
                searchHighlightUpdate(tlb_find);
            }
        }
    }
}

/**
 * This function initiates global highlighting (using "search highlight"
 * mark-up) of all lines matching the given pattern and options.
 */
void MainSearch::searchHighlightUpdate(const SearchPar& par)
{
    Q_ASSERT(!par.m_pat.isEmpty());

    m_higl->searchHighlightUpdate(par, m_f2_e->hasFocus());
}

/**
 * This function clears highlighting of search results. This applies to all
 * forms of search highlighting, namely (1) the specific line containing the
 * last match, (2) incremental search highlight, and optional global search
 * highlighting.
 */
void MainSearch::searchHighlightClear()
{
    m_higl->searchHighlightClear();
}

// ----------------------------------------------------------------------------

/**
 * This function is invoked when the user enters text in the "find" entry field.
 * In contrary to the "atomic" search, this function only searches a small chunk
 * of text, then re-schedules itself as an "idle" task.  The search can be aborted
 * at any time by canceling the task.
 */
void MainSearch::searchBackground(const SearchPar& par, bool is_fwd, int startPos, bool is_changed,
                                  const std::function<void(QTextCursor&)>& callback)
{
    bool isDone;
    if (is_fwd)
    {
        QTextBlock b = m_mainText->document()->end();
        if (b != m_mainText->document()->begin())
            b = b.previous();
        isDone = (startPos >= b.position() + b.length());
    }
    else
    {
        isDone = (startPos <= 0);
    }
    if (!isDone)
    {
        auto finder = MainTextFind::create(m_mainText->document(), par, is_fwd, startPos);

        // invoke the actual search in the selected portion of the document
        int matchPos, matchLen;
        bool found = finder->findNext(matchPos, matchLen);
        //printf("XXX %d -> %d,%d found?:%d\n", startPos, matchPos, matchLen, found);

        if (found)
        {
            // match found -> report; done
            QTextCursor c2 = m_mainText->textCursor();
            c2.setPosition(matchPos);
            c2.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);

            searchHandleMatch(c2, par, is_changed);
            callback(c2);
        }
        else if (!finder->isDone())
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
        searchHandleMatch(c2, par, is_changed);
        callback(c2);
    }
}


/**
 * This function searches the main text content for the given pattern in the
 * given direction, starting at the current cursor position. When a match is
 * found, the cursor is moved there and the line is highlighed. If no match is
 * found, a warning is displayed and the cursor and previous search
 * highlight(!) remains unchanged.
 *
 * The search is repeated the given number of times; if not enough matches are
 * found before reaching the end of document, a warning is displayed, but the
 * cursor is still moved to the last match and the function return value still
 * indicates success.
 */
bool MainSearch::searchAtomic(const SearchPar& par, bool is_fwd, bool is_changed, int repCnt)
{
    bool found = false;

    if (!par.m_pat.isEmpty() && searchExprCheck(par, true))
    {
        m_mainText->cursorJumpPushPos();
        tlb_last_dir = is_fwd;

        QTextCursor lastMatch;
        int repIdx;
        for (repIdx = 0; repIdx < repCnt; ++repIdx)
        {
            int start_pos = searchGetBase(is_fwd, false);

            auto match = m_mainText->findInDoc(par, is_fwd, start_pos);
            if (match.isNull())
                break;

            lastMatch = match;

            // determine new start position
            match.setPosition(std::min(match.position(), match.anchor()));
            m_mainText->setTextCursor(match);
        }

        if (!lastMatch.isNull())
        {
            if (repIdx < repCnt)
            {
                QString msg = QString("Only ") + QString::number(repIdx) + " of "
                                    + QString::number(repCnt) + " matches until "
                                    + (is_fwd ? "end" : "start") + " of file";
                m_mainWin->mainStatusLine()->showWarning("search", msg);
            }
            // update cursor position and highlight
            searchHandleMatch(lastMatch, par, is_changed);
            found = true;
        }
        else  // no match found at all
        {
            QString msg = QString("No match until ") + (is_fwd ? "end" : "start")
                            + " of file" + (par.m_pat.isEmpty() ? "" : ": ") + par.m_pat;
            m_mainWin->mainStatusLine()->showWarning("search", msg);
        }
    }
    else
    {
        // empty or invalid expression: just remove old highlights
        searchHighlightClear();
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
void MainSearch::searchHandleMatch(QTextCursor& match, const SearchPar& par, bool is_changed)
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

        int line = match.block().blockNumber();
        SearchList::signalHighlightLine(line);
        SearchList::matchView(line);
        DlgBookmarks::matchView(line);
    }

    if (tlb_hall)
    {
        searchHighlightUpdate(par);
    }
}


/**
 * This function is bound to all changes of the search text in the "find" entry
 * field. It's called when the user enters new text and triggers an incremental
 * search.
 */
void MainSearch::searchVarTrace(const QString & val)
{
    tlb_find.m_pat = val;

    if (m_f2_e->hasFocus())
    {
        // delay start of search, to avoid jumps while user still typing fast
        m_timSearchInc->start(SEARCH_INC_DELAY, [=, is_fwd=tlb_last_dir](){ searchIncrement(is_fwd, true); });
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

    if ((tlb_find.m_pat != "") && searchExprCheck(tlb_find, false))
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
            searchHighlightClear();
            start_pos = tlb_inc_base;
        }
        else
            start_pos = searchGetBase(is_fwd, false);

        searchBackground(tlb_find, is_fwd, start_pos, is_changed,
                         [=](QTextCursor& c){ searchIncMatch(c, tlb_find.m_pat, is_fwd, is_changed); });
    }
    else
    {
        searchReset();

        if (tlb_find.m_pat != "")
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
bool MainSearch::searchExprCheck(const SearchPar& par, bool display)
{
    if (par.m_opt_regexp)
    {
        QRegularExpression re(par.m_pat);
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
        {
            auto lastBlk = m_mainText->document()->lastBlock();
            int docLen = lastBlk.position() + lastBlk.length();
            start_pos = ((cur_pos + 1 < docLen) ? (cur_pos + 1) : cur_pos);
        }
    }
    else
    {
        start_pos = is_fwd ? view_start : view_end;
        c.setPosition(cur_pos);
        m_mainText->setTextCursor(c);
        m_mainText->centerCursor();
    }

    return start_pos;
}


/**
 * This function returns the current search pattern and options as configured
 * via the widgets in the main window. If the search field is empty (maybe
 * because a search was aborted via ESCAPE), the parameters of the last search
 * are returned, equivalently as for search repetition via Next/Prev buttons.
 */
SearchPar MainSearch::getCurSearchParams()
{
    if (!tlb_find.m_pat.isEmpty())
    {
        return tlb_find;
    }
    else if (tlb_history.size() > 0)
    {
        return tlb_history.front();
    }
    else
        return SearchPar();
}

/**
 * This function is used by the search history and highlight pattern dialogs
 * for searching one or more of the user-defined patterns within the main
 * window. The cursor is set onto the first line matching one of the patterns
 * in the given direction, if any.
 */
bool MainSearch::searchFirst(bool is_fwd, const std::vector<SearchPar>& patList)
{
    m_mainText->cursorJumpPushPos();
    searchHighlightClear();

    // add to history in reverse order to avoid toggling ordering in history list
    for (auto it = patList.rbegin(); it != patList.rend(); ++it)
        searchAddHistory(*it);

    QTextCursor match;
    const SearchPar * matchPar = nullptr;
    int start_pos = searchGetBase(is_fwd, false);

    for (auto& pat : patList)
    {
        if (pat.m_pat.isEmpty() || !searchExprCheck(pat, false))
            continue;

        auto c2 = m_mainText->findInDoc(pat, is_fwd, start_pos);

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
        searchHandleMatch(match, *matchPar, true);
    }
    return (matchPar != nullptr);
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

    // copy parameters
    tlb_find = pat;

    // update widgets accordingly
    m_f2_e->setText(pat.m_pat);
    m_f2_regexp->setChecked(pat.m_opt_regexp);
    m_f2_mcase->setChecked(pat.m_opt_case);

    searchNext(tlb_last_dir);
}

/**
 * This function is used by the various key bindings which repeat a previous
 * search in the given direction. NOTE unlike vim, the direction parameter
 * (e.g. derived from "n" vs "N") does not invert the direction of the previous
 * search, but instead specifies the direction directly.
 *
 * If a match is found, the cursor is moved there and the line is marked using
 * search highlighting. If no match is found, a warning is 
 */
bool MainSearch::searchNext(bool is_fwd, int repCnt)
{
    bool found = false;

    m_mainWin->mainStatusLine()->clearMessage("search");

    if (!tlb_find.m_pat.isEmpty())
    {
        found = searchAtomic(tlb_find, is_fwd, false, repCnt);
    }
    else if (tlb_history.size() > 0)
    {
        // empty expression: repeat last search
        const SearchPar &par = tlb_history.front();
        found = searchAtomic(par, is_fwd, false, repCnt);
    }
    else
    {
        m_mainWin->mainStatusLine()->showError("search", "No pattern defined for search");
    }
    return found;
}

/**
 * This function is used by the "All" or "List all" buttons and assorted
 * keyboard shortcuts to list all text lines matching the current search
 * expression in a separate dialog window. In case the window is already open,
 * the first parameter indicates if it should be raised. The second parameter
 * indicates search range and direction: 0 to list all; -1 to list all above
 * and including the cursor; +1 to list all below and including the cursor.
 */
void MainSearch::searchAll(bool raiseWin, int direction)
{
    if (searchExprCheck(tlb_find, true))
    {
        searchAddHistory(tlb_find);

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

        SearchList::getInstance(raiseWin)->searchMatches(true, direction, tlb_find);
    }
}

/**
 * This function resets the state of the search engine.  It is called when
 * the search string is empty or a search is aborted with the Escape key.
 */
void MainSearch::searchReset()
{
    searchHighlightClear();

    if (tlb_inc_base >= 0)
    {
        m_mainText->verticalScrollBar()->setValue(tlb_inc_xview);
        m_mainText->horizontalScrollBar()->setValue(tlb_inc_yview);

        QTextCursor c = m_mainText->textCursor();
        c.setPosition(tlb_inc_base);
        m_mainText->setTextCursor(c);
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

    tlb_find.m_pat.clear();
    m_f2_e->setText(tlb_find.m_pat);
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
        if (searchExprCheck(tlb_find, false))
        {
            searchHighlightUpdateCurrent();
            searchAddHistory(tlb_find);
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
    if (searchExprCheck(tlb_find, false))
    {
        searchAddHistory(tlb_find);
    }

    tlb_find.m_pat.clear();
    m_f2_e->setText(tlb_find.m_pat);
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

    if (tlb_find.m_pat.isEmpty())
    {
        // empty expression: repeat last search
        if (tlb_history.size() > 0)
        {
            tlb_find.m_pat = tlb_history.front().m_pat;
            m_f2_e->setText(tlb_find.m_pat);
            restart = true;
        }
        else
        {
            m_mainWin->mainStatusLine()->showError("search", "No pattern defined for search repetition");
        }
    }

    if (searchExprCheck(tlb_find, true))
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
void MainSearch::searchAddHistory(const SearchPar& par)
{
    if (!par.m_pat.isEmpty())
    {
        // search for the expression in the history (options not compared)
        // remove the element if already in the list
        for (auto it = tlb_history.begin(); it != tlb_history.end(); ++it)
        {
            if (it->m_pat == par.m_pat)
            {
                tlb_history.erase(it);
                break;
            }
        }

        // insert the element at the top of the stack
        tlb_history.insert(tlb_history.begin(), par);

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
            tlb_hist_prefix = tlb_find.m_pat;
            tlb_hist_pos = is_up ? 0 : (tlb_history.size() - 1);
        }
        else if (is_up) {
            if (size_t(tlb_hist_pos) + 1 < tlb_history.size()) {
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
            tlb_find.m_pat = tlb_history.at(tlb_hist_pos).m_pat;
            m_f2_e->setText(tlb_find.m_pat);
            m_f2_e->setCursorPosition(tlb_find.m_pat.length());
        }
        else {
            // end of history reached -> reset
            tlb_hist_pos = -1;
            tlb_hist_prefix = -1;
            tlb_find.m_pat = tlb_hist_prefix;
            m_f2_e->setText(tlb_find.m_pat);
            m_f2_e->setCursorPosition(tlb_find.m_pat.length());
        }
    }
}

/**
 * This helper function searches the search history stack for a search
 * string with a given prefix.
 */
int MainSearch::searchHistoryComplete(int step)
{
    for (int idx = tlb_hist_pos; (idx >= 0) && (size_t(idx) < tlb_history.size()); idx += step)
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
    if ((tlb_hist_pos >= 0) && (size_t(tlb_hist_pos) < tlb_history.size()))
    {
        tlb_history.erase(tlb_history.begin() + tlb_hist_pos);

        m_mainWin->updateRcAfterIdle();

        DlgHistory::signalHistoryChanged();

        if (tlb_history.size() == 0)
        {
            tlb_hist_pos = -1;
            tlb_hist_prefix.clear();
        }
        else if (size_t(tlb_hist_pos) >= tlb_history.size())
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
    auto line_str = m_mainText->textCursor().block().text();
    int pos = m_mainText->textCursor().positionInBlock();
    int off;

    if (tlb_find.m_opt_regexp && !tlb_find.m_pat.isEmpty())
    {
        QRegularExpression::PatternOptions reflags =
                tlb_find.m_opt_case ? QRegularExpression::NoPatternOption
                           : QRegularExpression::CaseInsensitiveOption;
        if (!searchExprCheck(tlb_find, true))
            return;
        QRegularExpression re2(tlb_find.m_pat, reflags);
        auto mat2 = re2.match(line_str.midRef(pos), 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
        if (mat2.hasMatch())
            off = mat2.captured(0).length();
        else
            off = 0;
    }
    else
    {
        off = tlb_find.m_pat.length();
    }

    static const QRegularExpression re1("^(?:\\W+|\\w+)"); // not thread-safe
    auto mat1 = re1.match(line_str.midRef(pos + off));
    if (mat1.hasMatch())
    {
        QString word(mat1.captured(0));
        searchEscapeSpecialChars(word, tlb_find.m_opt_regexp);

        tlb_find.m_pat += word;
        m_f2_e->setText(tlb_find.m_pat);
        m_f2_e->setCursorPosition(tlb_find.m_pat.length());
        m_f2_e->setSelection(tlb_find.m_pat.length() - word.length(), tlb_find.m_pat.length());
    }
}


/**
 * This function is bound to "CTRL-SHIFT-D" in the "Find" entry field and
 * performs auto-completion to the left by adding any preceding characters
 * before the current cursor position.
 */
void MainSearch::searchCompleteLeft()
{
    auto line_str = m_mainText->textCursor().block().text();
    int off = m_mainText->textCursor().positionInBlock();

    static const QRegularExpression re1("(?:\\W+|\\w+)$"); // not thread-safe
    auto mat1 = re1.match(line_str.leftRef(off));
    if (mat1.hasMatch())
    {
        QString word(mat1.captured(0));
        searchEscapeSpecialChars(word, tlb_find.m_opt_regexp);

        tlb_find.m_pat = word + tlb_find.m_pat;
        m_f2_e->setText(tlb_find.m_pat);
        m_f2_e->setCursorPosition(word.length());
        m_f2_e->setSelection(0, word.length());
    }
}


/**
 * This function if bound to "*" and "#" in the main text window (as in VIM)
 * These keys allow to search for the word under the cursor in forward and
 * backwards direction respectively.
 */
void MainSearch::searchWord(bool is_fwd, int repCnt)
{
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

        searchEscapeSpecialChars(word, tlb_find.m_opt_regexp);

        // add regexp to match on word boundaries
        if (tlb_find.m_opt_regexp)
            word = QString("\\b") + word + "\\b";

        tlb_find.m_pat = word;
        m_f2_e->setText(tlb_find.m_pat);
        m_f2_e->setCursorPosition(0);

        searchAddHistory(tlb_find);
        m_mainWin->mainStatusLine()->clearMessage("search");

        searchAtomic(tlb_find, is_fwd, true, repCnt);
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

/**
 * This external interface function is called by the search list and bookmark
 * dialogs after change of selection to show & mark the same line in the main
 * window.
 */
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

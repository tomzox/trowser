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
 * DESCRIPTION:  Browser for line-oriented text files, e.g. debug traces.
 * This is a translation from the original implementation in Tcl/Tk.
 *
 * ----------------------------------------------------------------------------
 */

#include <QApplication>
#include <QWidget>
#include <QKeyEvent>
#include <QShortcut>
#include <QMenu>
#include <QMenuBar>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QToolBar>
#include <QStatusBar>
#include <QFontDialog>
#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTimer>
#include <QScrollBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QProgressBar>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdio>
#include <string>

#include "main_win.h"

static int load_file_mode = 0;
static int load_buf_size = 0x100000;
static const char * myrcfile = ".trowserc.qt";
static const char * const DEFAULT_FONT_FAM = "DejaVu Sans Mono";
static const int DEFAULT_FONT_SZ = 9;

static constexpr QRgb s_colFind   (0xfffaee0a);
static constexpr QRgb s_colFindInc(0xffc8ff00);
//static constexpr s_colSelection QRgb(0xffc3c3c3);

static constexpr QRgb s_colStError(0xffff6b6b);
static constexpr QRgb s_colStWarning(0xffffcc5d);

// ----------------------------------------------------------------------------

ATimer::ATimer(QWidget * parent)
    : QTimer(parent)
{
    this->setSingleShot(true);
}

void ATimer::after(int delay, const std::function<void()>& callback)
{
    this->stop();
    this->disconnect();

    connect(this, &QTimer::timeout, callback);
    this->setInterval(delay);
    this->start();
}

void ATimer::reschedule(int delay)
{
    this->stop();
    this->setInterval(delay);
    this->start();
}

// ----------------------------------------------------------------------------

StatusMsg::StatusMsg(QWidget * parent)
{
    m_parent = parent;

    m_lab = new QLabel(parent);
        m_lab->setFrameShape(QFrame::StyledPanel);
        m_lab->setLineWidth(2);
        m_lab->setAutoFillBackground(true);
        m_lab->setMargin(5);
        m_lab->setFocusPolicy(Qt::NoFocus);
        m_lab->setVisible(false);

    m_timStLine = new QTimer(this);
        m_timStLine->setSingleShot(true);
        m_timStLine->setInterval(DISPLAY_DURATION);
        connect(m_timStLine, &QTimer::timeout, this, &StatusMsg::expireStatusMsg);
};

void StatusMsg::showStatusMsg(const QString& msg, QRgb col)
{
    // withdraw to force recalculation of geometry
    m_lab->setVisible(false);

    QPalette stline_pal(m_lab->palette());
    stline_pal.setColor(QPalette::Window, col);

    m_lab->setPalette(stline_pal);
    m_lab->setText(msg);
    m_lab->move(10, m_parent->height() - m_lab->height() - 20);
    m_lab->setVisible(true);

    m_timStLine->start();
}

void StatusMsg::showPlain(QWidget * widget, const QString& msg)
{
    auto pal = QApplication::palette(m_lab) ;

    showStatusMsg(msg, pal.color(QPalette::Window).rgba());
    m_owner = widget;
}

void StatusMsg::showWarning(QWidget * widget, const QString& msg)
{
    showStatusMsg(msg, s_colStWarning);
    m_owner = widget;
}

void StatusMsg::showError(QWidget * widget, const QString& msg)
{
    showStatusMsg(msg, s_colStError);
    m_owner = widget;
}

void StatusMsg::clearMessage(QWidget * widget)
{
    if (widget == m_owner)
    {
        m_lab->setVisible(false);
        m_lab->setText("");
    }
}

void StatusMsg::expireStatusMsg()
{
    m_lab->setVisible(false);
    m_lab->setText("");
}

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

void Highlighter::connectWidgets(MainText * textWid)
{
    m_mainText = textWid;

    m_hipro = new QProgressBar(m_mainText);
        m_hipro->setOrientation(Qt::Horizontal);
        m_hipro->setTextVisible(true);
        m_hipro->setMinimum(0);
        m_hipro->setMaximum(100);
        m_hipro->setVisible(false);

    tid_high_init = new ATimer(m_mainText);
    tid_search_hall = new ATimer(m_mainText);
}

// mark the matching text
void Highlighter::addSearchInc(QTextCursor& sel, QRgb col)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor(col));
    sel.setCharFormat(fmt);

    m_findIncPos = sel.blockNumber();
    //printf("XXX add-inc blk:%d\n", m_findIncPos);
}

// mark the complete line containing the match
void Highlighter::addSearchHall(QTextCursor& sel, QRgb col)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor(col));
    //fmt.setFontUnderline(true);
    fmt.setFontWeight(2);

    QTextCursor c(sel);
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    c.setCharFormat(fmt);

    auto pair = m_tags.equal_range(c.blockNumber());
    bool found = false;
    for (auto it = pair.first; it != pair.second; ++it)
    {
        if (it->second == col)
        {
            found = true;
            break;
        }
    }
    if (found == false)
    {
        //printf("XXX add blk:%d\n", c.blockNumber());
        m_tags.emplace(std::make_pair(c.blockNumber(), col));
    }
}

void Highlighter::removeInc(QTextDocument * doc)
{
    if (m_findIncPos != -1)
    {
        //printf("XXX rm-inc blk:%d\n", m_findIncPos);
        redraw(doc, m_findIncPos);
        m_findIncPos = -1;
    }
}

void Highlighter::removeHall(QTextDocument * doc, QRgb col)
{
    for (auto it = m_tags.begin(); it != m_tags.end(); /*nop*/)
    {
        if (it->second == col)
        {
            //printf("XXX rm-inc blk:%d\n", it->first);
            redraw(doc, it->first);
            it = m_tags.erase(it);
        }
        else
            ++it;
    }
}

void Highlighter::clear(QTextDocument * doc)
{
    for (auto it = m_tags.begin(); it != m_tags.end(); /*nop*/)
    {
        redraw(doc, it->first);
    }
    m_tags.clear();
}

void Highlighter::redraw(QTextDocument * doc, int blkNum)
{
    QTextBlock blk = doc->findBlockByNumber(blkNum);
    QTextCursor c(blk);

    QTextCharFormat fmt;
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    c.setCharFormat(fmt);
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
    if (patlist.size() > 0)
    {
        // place a progress bar as overlay to the main window
        m_hipro->setValue(0);
        m_hipro->setVisible(true);

        //wt.f1_t.tag_add("margin", "1.0", "end")

        m_mainText->setCursor(Qt::BusyCursor);

        // trigger highlighting for the 1st pattern in the background
        tid_high_init->after(50, [=](){ highlightInitBg(0, 0, 0); });

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
void Highlighter::highlightInitBg(int pat_idx, int line, int loop_cnt)
{
    if (   block_bg_tasks
        //TODO || tid_search_inc.isActive()
        || tid_search_hall->isActive())
        //TODO || tid_search_list.isActive())
    {
        // background tasks are suspended - re-schedule with timer
        tid_high_init->after(100, [=](){ highlightInitBg(pat_idx, line, 0); });
    }
    else if (loop_cnt > 10)
    {
        // insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
        tid_high_init->after(10, [=](){ highlightInitBg(pat_idx, line, 0); });
    }
    else if ((size_t)pat_idx < patlist.size())
    {
        const HiglDef& w = patlist[pat_idx];
        int tagnam = 0; //TODO w[4]
        loop_cnt += 1;

        // here we do the actual work:
        // apply the tag to all matching lines of text
        line = highlightLines(w.m_pat, tagnam, w.m_opt_regexp, w.m_opt_case, line);

        if (line >= 0)
        {
            // not done yet - reschedule
            tid_high_init->after(0, [=](){ highlightInitBg(pat_idx, line, loop_cnt); });
        }
        else
        {
            // trigger next tag
            pat_idx += 1;
            tid_high_init->after(0, [=](){ highlightInitBg(pat_idx, 0, loop_cnt); });

            // update the progress bar
            m_hipro->setValue(100 * pat_idx / patlist.size());
        }
    }
    else
    {
        m_hipro->setVisible(false);
        m_mainText->setCursor(Qt::ArrowCursor);
        tid_high_init->stop();
    }
}


/**
 * This function searches for all lines in the main text widget which match the
 * given pattern and adds the given tag to them.  If the loop doesn't complete
 * within 100ms, the search is paused and the function returns the number of the
 * last searched line.  In this case the caller must invoke the funtion again
 * (as an idle event, to allow user-interaction in-between.)
 */
int Highlighter::highlightLines(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line)
{
    qint64 start_t = QDateTime::currentMSecsSinceEpoch();

    while (true)
    {
        //TODO search range? (Tcl used "end")
        int matchPos, matchLen;
        if (m_mainText->findInBlocks(pat, line, true, opt_regexp, opt_case, matchPos, matchLen))
        {
            // match found, highlight this line
            //wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
            QTextCursor match = m_mainText->textCursor();
            match.setPosition(matchPos);
            match.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);
            addSearchHall(match, s_colFind);  //TODO tagnam

            // trigger the search result list dialog in case the line is included there too
            //TODO SearchList_HighlightLine(tagnam, line)

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
void Highlighter::highlightAll(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line, int loop_cnt)
{
    if (block_bg_tasks)
    {
        // background tasks are suspended - re-schedule with timer
        tid_high_init->after(100, [=](){ highlightAll(pat, tagnam, opt_regexp, opt_case, line, 0); });
    }
    else if (loop_cnt > 10)
    {
        // insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
        tid_high_init->after(10, [=](){ highlightAll(pat, tagnam, opt_regexp, opt_case, line, 0); });
    }
    else
    {
        line = highlightLines(pat, tagnam, opt_regexp, opt_case, line);
        if (line >= 0)
        {
            loop_cnt += 1;
            tid_high_init->after(0, [=](){ highlightAll(pat, tagnam, opt_regexp, opt_case, line, loop_cnt); });
        }
        else
        {
            m_mainText->setCursor(Qt::ArrowCursor);
            tid_high_init->stop();
        }
    }
}


/**
 * This function searches the currently visible text content for all lines
 * which contain the given sub-string and marks these lines with the given tag.
 */
void Highlighter::highlightVisible(const QString& pat, int tagnam, bool opt_regexp, bool opt_case)
{
    auto view = m_mainText->viewport();
    auto c1 = m_mainText->cursorForPosition(QPoint(0, 0));
    auto c2 = m_mainText->cursorForPosition(QPoint(0, view->height()));

    int line = c1.block().position();
    int line_end = c2.block().position() + c2.block().length();

    while (line < line_end)
    {
        int matchPos, matchLen;
        if (m_mainText->findInBlocks(pat, line, true, opt_regexp, opt_case, matchPos, matchLen))
        {
            //wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
            QTextCursor match = m_mainText->textCursor();
            match.setPosition(matchPos);
            match.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);
            addSearchHall(match, s_colFind);  //TODO tagnam
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
void Highlighter::highlightYviewCallback(double frac1, double frac2)
{
#if 0
    if (tid_high_init->isActive())
    {
        for (w : patlist)
        {
            opt = Search_GetOptions(w[0], w[1], w[2])
            highlightVisible(w[0], w[4], opt)
        }
    }

    if (tid_search_hall->isActive())
        highlightVisible(tlb_cur_hall_str, "find", tlb_cur_hall_regexp, tlb_cur_hall_case);

    // automatically remove the redirect if no longer needed
    if (!tid_high_init->isActive() && !tid_search_hall->isActive())
        wt.f1_t.configure(yscrollcommand=wt.f1_sb.set);

    wt.f1_sb.set(frac1, frac2);
#endif
}


/**
 * This function redirect the yview callback from the scrollbar into the above
 * function. This is used to install a redirection for the duration of the
 * initial or search highlighting task.
 */
void Highlighter::highlightYviewRedirect()
{
    //TODO wt.f1_t.configure(yscrollcommand=highlightYviewCallback)
}


/**
 * This function creates or updates a text widget tag with the options of
 * a color highlight entry.  The function is called during start-up for all
 * highlight patterns, and by the highlight edit dialog (also used for the
 * sample text widget.)
 */
#if 0
void Highlighter::highlightConfigure(int tagname, w)
{
    cfg = {}
    if w[8]:
        cfg["font"] = DeriveFont(font_content, 0, "bold")
    else:
        cfg["font"] = ""

    if w[9]:
        cfg["underline"] = w[9]
    else:
        cfg["underline"] = ""

    if w[10]:
        cfg["overstrike"] = w[10]
    else:
        cfg["overstrike"] = ""

    if w[13] != "":
        cfg["relief"] = w[13]
        cfg["borderwidth"] = w[14]
    else:
        cfg["relief"] = ""
        cfg["borderwidth"] = ""

    if w[15] > 0:
        cfg["spacing1"] = w[15]
        cfg["spacing3"] = w[15]
    else:
        cfg["spacing1"] = ""
        cfg["spacing3"] = ""

    cfg["background"] = w[6]
    cfg["foreground"] = w[7]

    cfg["bgstipple"] = w[11]
    cfg["fgstipple"] = w[12]

    wid.tag_config(tagname, **cfg)
}
#endif


/**
 * This function clears the current search color highlighting without
 * resetting the search string. It's bound to the "&" key, but also used
 * during regular search reset.
 */
void Highlighter::searchHighlightClear()
{
    tid_search_hall->stop();
    searchHighlightReset();

    m_mainText->setCursor(Qt::ArrowCursor);
    //TODO SearchList_HighlightClear();
}

void Highlighter::searchHighlightReset()
{
    removeHall(m_mainText->document(), s_colFind);
    removeInc(m_mainText->document());

    tlb_cur_hall_str.clear();
    tlb_cur_hall_regexp = false;
    tlb_cur_hall_case = false;
}


/**
 * This function triggers color highlighting of all lines of text which match
 * the current search string.  The function is called when global highlighting
 * is en-/disabled, when the search string is modified or when search options
 * are changed.
 */
void Highlighter::searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case)
{
    if (   (tlb_cur_hall_str != pat)
        || (tlb_cur_hall_regexp != opt_regexp)
        || (tlb_cur_hall_case != opt_case))
    {
        // remember options for comparison
        tlb_cur_hall_str = pat;
        tlb_cur_hall_regexp = opt_regexp;
        tlb_cur_hall_case = opt_case;

        // display "busy" cursor until highlighting is finished
        m_mainText->setCursor(Qt::BusyCursor);

        // implicitly kill background highlight process for obsolete pattern
        // start highlighting in the background
        tid_search_hall->after(100, [=](){ searchHighlightAll(pat, TAG_NAME_FIND, opt_regexp, opt_case); });

        // apply highlighting on the text in the visible area (this is quick)
        // (note this is required in addition to the redirect below)
        highlightVisible(pat, TAG_NAME_FIND, opt_regexp, opt_case);

        // use the yview callback to redo highlighting in case the user scrolls
        highlightYviewRedirect();
    }
}


/**
 * This helper function calls the global search highlight function until
 * highlighting is complete.
 */
void Highlighter::searchHighlightAll(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line, int loop_cnt)
{
    if (block_bg_tasks)
    {
        // background tasks are suspended - re-schedule with timer
        tid_search_hall->after(100, [=](){ searchHighlightAll(pat, tagnam, opt_regexp, opt_case, line, 0); });
    }
    else if (loop_cnt > 10)
    {
        // insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
        tid_search_hall->after(10, [=](){ searchHighlightAll(pat, tagnam, opt_regexp, opt_case, line, 0); });
    }
    else
    {
        line = highlightLines(pat, tagnam, opt_regexp, opt_case, line);
        if (line >= 0)
        {
            loop_cnt += 1;
            tid_search_hall->after(0, [=](){ searchHighlightAll(pat, tagnam, opt_regexp, opt_case, line, loop_cnt); });
        }
        else
        {
            m_mainText->setCursor(Qt::ArrowCursor);
        }
    }
}


// ----------------------------------------------------------------------------

MainText::MainText(MainWin * mainWin, MainSearch * search, QWidget * parent)
    : QPlainTextEdit(parent)
    , m_mainWin(mainWin)
    , m_search(search)
{
    // commands for scrolling vertically
    m_keyCmdCtrl.emplace(Qt::Key_Up, [=](){ YviewScrollLine(-1); });
    m_keyCmdCtrl.emplace(Qt::Key_Down, [=](){ YviewScrollLine(1); });
    m_keyCmdCtrl.emplace(Qt::Key_F, [=](){ QKeyEvent ne(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier); QPlainTextEdit::keyPressEvent(&ne); });
    m_keyCmdCtrl.emplace(Qt::Key_B, [=](){ QKeyEvent ne(QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier); QPlainTextEdit::keyPressEvent(&ne); });
    m_keyCmdCtrl.emplace(Qt::Key_E, [=](){ YviewScrollLine(1); });
    m_keyCmdCtrl.emplace(Qt::Key_Y, [=](){ YviewScrollLine(-1); });
    m_keyCmdCtrl.emplace(Qt::Key_D, [=](){ YviewScrollHalf(1); });
    m_keyCmdCtrl.emplace(Qt::Key_U, [=](){ YviewScrollHalf(-1); });

    m_keyCmdText.emplace(KeySet('z','-'), [=](){ YviewSet('B', 0); });
    m_keyCmdText.emplace(KeySet('z','b'), [=](){ YviewSet('B', 1); });
    m_keyCmdText.emplace(KeySet('z','.'), [=](){ YviewSet('C', 0); });
    m_keyCmdText.emplace(KeySet('z','z'), [=](){ YviewSet('C', 1); });
    m_keyCmdText.emplace(KeySet('z','\r'), [=](){ YviewSet('T', 0); });
    m_keyCmdText.emplace(KeySet('z','t'), [=](){ YviewSet('T', 1); });

    m_keyCmdText.emplace('+', [=](){ cursorMoveLine(1, true); });
    m_keyCmdText.emplace('-', [=](){ cursorMoveLine(-1, true); });
    m_keyCmdText.emplace('k', [=](){ cursorMoveLine(-1, false); });
    m_keyCmdText.emplace('j', [=](){ cursorMoveLine(1, false); });
    m_keyCmdText.emplace('H', [=](){ cursorSetLineTop(0); });
    m_keyCmdText.emplace('M', [=](){ cursorSetLineCenter(); });
    m_keyCmdText.emplace('L', [=](){ cursorSetLineBottom(0); });

    m_keyCmdText.emplace('G', [=](){ cursorGotoBottom(0); });
    m_keyCmdText.emplace(KeySet('g','g'), [=](){ cursorGotoTop(0); });

    // commands for scrolling horizontally
    m_keyCmdCtrl.emplace(Qt::Key_Left, [=](){ XviewScroll(1, -1); });
    m_keyCmdCtrl.emplace(Qt::Key_Right, [=](){ XviewScroll(1, 1); });

    m_keyCmdText.emplace(KeySet('z','l'), [=](){ XviewScroll(1, 1); });
    m_keyCmdText.emplace(KeySet('z','h'), [=](){ XviewScroll(1, -1); });
    m_keyCmdText.emplace(KeySet('z','L'), [=](){ XviewScrollHalf(1); });
    m_keyCmdText.emplace(KeySet('z','H'), [=](){ XviewScrollHalf(-1); });
    m_keyCmdText.emplace(KeySet('z','s'), [=](){ XviewSet(XVIEW_SET_LEFT); });
    m_keyCmdText.emplace(KeySet('z','e'), [=](){ XviewSet(XVIEW_SET_RIGHT); });

    m_keyCmdText.emplace('0', [=](){ cursorSetLineStart(); });
    m_keyCmdText.emplace('^', [=](){ cursorSetLineStart(); cursorMoveLine(0, true); });
    m_keyCmdText.emplace('$', [=](){ cursorSetLineEnd(); });

    // commands to move the cursor
    //bind .f1.t <Key-Home> {if {%s == 0} {cursorSetLineStart(); KeyClr; break}}
    //bind .f1.t <Key-End> {if {%s == 0} {cursorSetLineEnd(); KeyClr; break}}
    //bind .f1.t <Key-space> {CursorMoveLeftRight .f1.t 1; break}
    //bind .f1.t <Key-BackSpace> {CursorMoveLeftRight .f1.t -1; break}
    m_keyCmdText.emplace('h', [=](){ QKeyEvent ne(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier); QPlainTextEdit::keyPressEvent(&ne); });
    m_keyCmdText.emplace('l', [=](){ QKeyEvent ne(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier); QPlainTextEdit::keyPressEvent(&ne); });
    m_keyCmdText.emplace('\r', [=](){ cursorMoveLine(1, true); });
    m_keyCmdText.emplace('w', [=](){ cursorMoveWord(true, false, false); });
    m_keyCmdText.emplace('e', [=](){ cursorMoveWord(true, false, true); });
    m_keyCmdText.emplace('b', [=](){ cursorMoveWord(false, false, false); });
    m_keyCmdText.emplace('W', [=](){ cursorMoveWord(true, true, false); });
    m_keyCmdText.emplace('E', [=](){ cursorMoveWord(true, true, true); });
    m_keyCmdText.emplace('B', [=](){ cursorMoveWord(false, true, false); });
    m_keyCmdText.emplace(KeySet('g','e'), [=](){ cursorMoveWord(false, false, true); });
    m_keyCmdText.emplace(KeySet('g','E'), [=](){ cursorMoveWord(false, true, true); });
    m_keyCmdText.emplace(';', [=](){ searchCharInLine(0, 1); });
    m_keyCmdText.emplace(',', [=](){ searchCharInLine(0, -1); });

    // cursor history
    m_keyCmdCtrl.emplace(Qt::Key_O, [=](){ cursorJumpHistory(-1); });
    m_keyCmdCtrl.emplace(Qt::Key_I, [=](){ cursorJumpHistory(1); });

    // commands for searching & repeating
    m_keyCmdText.emplace('/', [=](){ m_search->searchEnter(true); });
    m_keyCmdText.emplace('?', [=](){ m_search->searchEnter(false); });
    m_keyCmdText.emplace('n', [=](){ m_search->searchNext(true); });
    m_keyCmdText.emplace('N', [=](){ m_search->searchNext(false); });
    m_keyCmdText.emplace('*', [=](){ m_search->searchWord(true); });
    m_keyCmdText.emplace('#', [=](){ m_search->searchWord(false); });
    m_keyCmdText.emplace('&', [=](){ m_search->searchHighlightClear(); });

    // misc
    //TODO m_keyCmdText.emplace('i' [=](){ SearchList_Open(0); SearchList_CopyCurrentLine(); });
    //TODO m_keyCmdText.emplace('u' [=](){ SearchList_Undo(); });
    //TODO m_keyCmdCtrl.emplace(Qt::Key_R, [=](){ SearchList_Redo(); });
    m_keyCmdCtrl.emplace(Qt::Key_G, [=](){ m_mainWin->menuCmdDisplayLineNo(); });
    //bind .f1.t <Double-Button-1> {if {%s == 0} {Mark_ToggleAtInsert; KeyClr; break}};
    //TODO m_keyCmdText.emplace('m', [=](){ Mark_ToggleAtInsert(); });
    m_keyCmdCtrl.emplace(Qt::Key_Plus, [=](){ m_mainWin->keyCmdZoomFontSize(true); });
    m_keyCmdCtrl.emplace(Qt::Key_Minus, [=](){ m_mainWin->keyCmdZoomFontSize(false); });
    //bind .f1.t <Control-Alt-Delete> DebugDumpAllState
}

void MainText::keyPressEvent(QKeyEvent *e)
{
    if (e->modifiers() == Qt::ControlModifier)
    {
        auto it = m_keyCmdCtrl.find(e->key());
        if (it != m_keyCmdCtrl.end())
        {
            (it->second)();
            keyCmdClear();
            return;
        }
    }

    if (   ((e->modifiers() & ~Qt::ShiftModifier) == 0)
        && (   (e->key() == Qt::Key_Return)
            || (e->text().length() == 1)) )
    {
        wchar_t chr = (e->key() == Qt::Key_Return) ? '\r' : e->text()[0].unicode();
        if (keyCmdText(chr))
            return;
    }

    switch (e->key())
    {
        case Qt::Key_Space:
        {   QKeyEvent ne(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
            QPlainTextEdit::keyPressEvent(&ne);
            break;
        }
        case Qt::Key_Backspace:
        {   QKeyEvent ne(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
            QPlainTextEdit::keyPressEvent(&ne);
            break;
        }

        // permissible standard key-bindings, filtered for read-only
        case Qt::Key_Up:
            if (e->modifiers() == Qt::NoModifier)
                cursorMoveLine(-1, false);
            else if (e->modifiers() == Qt::ControlModifier)
                QPlainTextEdit::keyPressEvent(e);
            break;
        case Qt::Key_Down:
            if (e->modifiers() == Qt::NoModifier)
                cursorMoveLine(1, false);
            else if (e->modifiers() == Qt::ControlModifier)
                QPlainTextEdit::keyPressEvent(e);
            break;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Tab:
            QPlainTextEdit::keyPressEvent(e);
            break;

        case Qt::Key_A:  // select-all
        case Qt::Key_C:  // copy selection to clipboard
        case Qt::Key_Insert:  // copy selection to clipboard
            if (e->modifiers() == Qt::ControlModifier)
                QPlainTextEdit::keyPressEvent(e);
            break;

        default:
            break;
    }
}

/**
 * This function is bound to key presses in the main window. It's called
 * when none of the single-key bindings match. It's intended to handle
 * complex key sequences, but also has to handle single key bindings for
 * keys which can be part of sequences (e.g. "b" due to "zb")
 */
bool MainText::keyCmdText(wchar_t chr)
{
    bool result = false;

    if (last_key_char == '\'') {
        // single quote char: jump to marker or bookmark
        m_mainWin->clearMessage(this);
        if (chr == '\'') {
            cursorJumpToggle();
        }
        else if (chr == '^') {
            // '^ and '$ are from less
            cursorGotoTop(0);
        }
        else if (chr == '$') {
            cursorGotoBottom(0);
        }
        else if (chr == '+') {
            //TODO Mark_JumpNext 1
        }
        else if (chr == '-') {
            //TODO Mark_JumpNext 0
        }
        else {
            QString msg = QString("Undefined key sequence: ") + last_key_char + chr;
            m_mainWin->showError(this, msg);
        }
        last_key_char = 0;
        result = true;
    }
    else if ((last_key_char == 'z') || (last_key_char == 'g')) {
        m_mainWin->clearMessage(this);

        auto it = m_keyCmdText.find(KeySet(last_key_char, chr));
        if (it != m_keyCmdText.end()) {
            (it->second)();
        }
        else {
            QString msg = QString("Undefined key sequence: ") + last_key_char + chr;
            m_mainWin->showError(this, msg);
        }
        last_key_char = 0;
        result = true;
    }
    else if (last_key_char == 'f') {
        searchCharInLine(chr, 1);
        last_key_char = 0;
        result = true;
    }
    else if (last_key_char == 'F') {
        searchCharInLine(chr, -1);
        last_key_char = 0;
        result = true;
    }
    else {
        last_key_char = 0;

        auto it = m_keyCmdText.find(chr);
        if (it != m_keyCmdText.end()) {
            (it->second)();
            result = true;
        }
        else if ((chr >= '0') && (chr <= '9')) {
            //TODO KeyCmd_OpenDialog any chr
            last_key_char = 0;
            result = true;
        }
        else if ((chr == 'z') || (chr == '\'') ||
                 (chr == 'f') || (chr == 'F') || (chr == 'g')) {
            last_key_char = chr;
            result = true;
        }
    }
    return result;
}

/**
 * This function is called for all explicit key bindings to forget about
 * any previously buffered partial multi-keypress commands.
 */
void MainText::keyCmdClear()
{
    last_key_char = 0;
}

/**
 * This function adjusts the view so that the line holding the cursor is
 * placed at the top, center or bottom of the viewable area, if possible.
 */
void MainText::YviewSet(char where, int col)
{
    this->ensureCursorVisible();

    auto c = this->textCursor();
    auto rect = this->blockBoundingGeometry(c.block());
    qreal delta = 0.0;

    if (where == 'T') { // top
        delta = rect.y();
    }
    else if (where == 'C') {
        // center
        auto view = this->viewport();
        delta = (rect.y() + rect.height() / 2) - qreal(view->height()) / 2;
    }
    else if (where == 'B') {
        // bottom
        auto view = this->viewport();
        delta = rect.y() + rect.height() - view->height();
    }
    int lineH = this->cursorRect().height();
    auto sb = this->verticalScrollBar();
    sb->setValue( sb->value() + delta / lineH * sb->singleStep() );

    if (col == 0) {
        // place cursor on first non-blank character in the selected row
        c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        this->setTextCursor(c);
        cursorMoveLine(0, true);
    }
    else {
        this->ensureCursorVisible();
    }

    // synchronize the search result list (if open) with the main text
    //TODO scan [.f1.t index insert] "%d" line
    //TODO SearchList_MatchView $line
}

/**
 * This function scrolls the view vertically by the given number of lines.
 * When the line holding the cursor is scrolled out of the window, the cursor
 * is placed in the last visible line in scrolling direction.
 */
void MainText::YviewScrollLine(int delta)
{
    auto sb = this->verticalScrollBar();
    sb->setValue( sb->value() + delta * sb->singleStep() );

    YviewScrollVisibleCursor(delta);
}


/**
 * This function scrolls the view vertically by half the screen height
 * in the given direction.  When the line holding the cursor is scrolled out of
 * the window, the cursor is placed in the last visible line in scrolling
 * direction.
 */
void MainText::YviewScrollHalf(int dir)
{
    auto sb = this->verticalScrollBar();
    sb->setValue( sb->value() + dir * sb->pageStep() / 2 );

    YviewScrollVisibleCursor(dir);
}

/**
 * This helper function places the cursor into the first or last visible line
 * if scrolled out of view. This is used after vertical scrolling by line or
 * page.
 */
void MainText::YviewScrollVisibleCursor(int dir)
{
    auto rect = this->cursorRect();
    auto view = this->viewport();
    auto coff = this->contentOffset();

    if ((dir > 0) && (rect.y() < 0))
    {
        auto c = this->cursorForPosition(QPoint(rect.x(), 0));
        this->setTextCursor(c);
    }
    else if (   (dir < 0) && (view->height() >= rect.height() + coff.y())
             && (rect.y() + rect.height() + coff.y() > view->height()))
    {
        auto c = this->cursorForPosition(QPoint(rect.x(), view->height() - rect.height() - coff.y()));
        this->setTextCursor(c);
    }
}

/**
 * This function moves the cursor to the start of the line at the top of the
 * current view.
 */
void MainText::cursorSetLineTop(int /*TODO off*/)
{
    cursorJumpPushPos();

    auto c = this->cursorForPosition(QPoint(0, 0));
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);

    // move cursor up if last line is only partially visible (to avoid automatic scrolling)
    auto rect = this->cursorRect(c);
    if (rect.y() < 0)
    {
        c = this->cursorForPosition(QPoint(rect.x(), 0));
    }
    this->setTextCursor(c);

#if 0 // TODO: off
    set index [$wid index [list {@1,1} + $off lines]]
    if {($off > 0) && ![IsRowFullyVisible $wid $index]} {
        // offset out of range - set to bottom instead
        CursorSetLine $wid bottom 0
        return
    } else {
        $wid mark set insert $index
    }
#endif
    // place cursor on first non-blank character in the selected row
    cursorMoveLine(0, true);
}

/**
 * This function moves the cursor to the start of the line in the center of the
 * current view. Note in contrary to top/bottom placement, this variant does
 * not support the "offset" parameter.
 */
void MainText::cursorSetLineCenter()
{
    cursorJumpPushPos();

    auto view = this->viewport();
    auto c = this->cursorForPosition(QPoint(0, view->height() / 2));
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    this->setTextCursor(c);

    // place cursor on first non-blank character in the selected row
    cursorMoveLine(0, true);
}

/**
 * This function moves the cursor to the start of the line at the bottom of the
 * current view.
 */
void MainText::cursorSetLineBottom(int /*TODO off*/)
{
    cursorJumpPushPos();

    auto view = this->viewport();
    auto c = this->cursorForPosition(QPoint(0, view->height() - 1));
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);

    // move cursor up if last line is only partially visible (to avoid automatic scrolling)
    auto rect = this->cursorRect(c);
    auto coff = this->contentOffset();
    if (   (view->height() >= rect.height() + coff.y())
        && (rect.y() + rect.height() + coff.y() > view->height()))
    {
        c = this->cursorForPosition(QPoint(rect.x(), view->height() - rect.height() - coff.y()));
    }
    this->setTextCursor(c);

#if 0 // TODO: off
    set index [$wid index [list "@1,[winfo height $wid]" linestart - $off lines]]
    if {![IsRowFullyVisible $wid $index]} {
        if {$off == 0} {
            // move cursor to the last fully visible line to avoid scrolling
            set index [$wid index [list $index - 1 lines]]
        } else {
            // offset out of range - set to top instead
            CursorSetLine $wid top 0
            return
        }
    }
    $wid mark set insert $index
#endif

    // place cursor on first non-blank character in the selected row
    cursorMoveLine(0, true);
}


/**
 * This function moves the cursor by the given number of lines and places
 * the cursor on the first non-blank character in that line. The delta may
 * be zero (e.g. to just place the cursor onto the first non-blank)
 */
void MainText::cursorMoveLine(int delta, bool toStart)
{
    auto c = this->textCursor();

    if (delta > 0) {
        auto newBlk = c.block().next();
        c.setPosition(newBlk.position());
        this->setTextCursor(c);
    }
    else if (delta < 0) {
        auto newBlk = c.block().previous();
        c.setPosition(newBlk.position());
        this->setTextCursor(c);
    }
    this->horizontalScrollBar()->setValue(0);

    if (toStart)
    {
        // forward to the first non-blank character
        auto line_str = c.block().text();
        static const QRegularExpression re1("^\\s*"); // not thread-safe
        auto mat1 = re1.match(line_str);
        if (mat1.hasMatch())
        {
            c.setPosition(c.position() + mat1.captured(0).length());
            this->setTextCursor(c);
        }
    }
    this->ensureCursorVisible();
}

/**
 * This function moves the cursor to the start or end of the main text.
 * Additionally the cursor position prior to the jump is remembered in
 * the jump stack.
 */
void MainText::cursorGotoTop(int off)
{
    cursorJumpPushPos();

    QTextCursor c = this->textCursor();
    c.setPosition(0);
    if (off > 0)
        c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, off);
    this->setTextCursor(c);
    cursorMoveLine(0, true);
}

void MainText::cursorGotoBottom(int off)
{
    cursorJumpPushPos();

    QTextCursor c = this->textCursor();
    c.setPosition(this->document()->lastBlock().position());
    if (off > 0)
        c.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor, off);
    this->setTextCursor(c);
    cursorMoveLine(0, true);
}

/**
 * This function scrolls the view horizontally by the given number of characters.
 * When the cursor is scrolled out of the window, it's placed in the last visible
 * column in scrolling direction.
 */
void MainText::XviewScroll(int /*TODO delta*/, int /*TODO dir*/)
{
#if 0
  set pos_old [$wid bbox insert]

  if (delta != 0) {
    $wid xview scroll [expr {$dir * $delta}] units
  }

  if {$pos_old ne ""} {
    set pos_new [$wid bbox insert]

    // check if cursor is fully visible
    if {([llength $pos_new] != 4) || ([lindex $pos_new 2] == 0)} {
      set ycoo [expr {[lindex $pos_old 1] + int([lindex $pos_old 3] / 2)}]
      if {$dir < 0} {
        $wid mark set insert "@[winfo width $wid],$ycoo"
      } else {
        $wid mark set insert [list "@1,$ycoo" + 1 chars]
      }
    }
  }
#endif
}


/**
 * This function scrolls the view horizontally by half the screen width
 * in the given direction.
 */
void MainText::XviewScrollHalf(int dir)
{
#if 0
  set xpos [$wid xview]
  set w [winfo width $wid]
  if {$w != 0} {
    set fract_visible [expr {[lindex $xpos 1] - [lindex $xpos 0]}]
    set off [expr {[lindex $xpos 0] + $dir * (0.5 * $fract_visible)}]
    if {$off > 1} {set off 1}
    if {$off < 0} {set off 0}
  }
  $wid xview moveto $off
#endif
    XviewScroll(0, dir);
}


/**
 * This function adjusts the view so that the column holding the cursor is
 * placed at the left or right of the viewable area, if possible.
 */
void MainText::XviewSet(xviewSetWhere /*TODO where*/)
{
#if 0
  set xpos [$wid xview]
  set coo [$wid bbox insert]
  set w [winfo width $wid]
  if {($coo ne "") && ($w != 0)} {
    set fract_visible [expr {[lindex $xpos 1] - [lindex $xpos 0]}]
    set fract_insert [expr {double(2 + [lindex $coo 0] + [lindex $coo 2]) / $w}]

    if {$where eq "left"} {
      set off [expr {[lindex $xpos 0] + ($fract_insert * $fract_visible)}]
      if {$off > 1.0} {set off 1.0}
    } else {
      set off [expr {[lindex $xpos 0] - ((1 - $fract_insert) * $fract_visible)}]
      if {$off < 0.0} {set off 0.0}
    }
    $wid xview moveto $off
    $wid see insert
  }
#endif
}


/**
 * This function moves the cursor to the start of the current line.
 */
void MainText::cursorSetLineStart()
{
    QTextCursor c = this->textCursor();
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    this->setTextCursor(c);
    this->horizontalScrollBar()->setValue(0);
}

void MainText::cursorSetLineEnd()
{
    QTextCursor c = this->textCursor();
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    this->setTextCursor(c);
    this->ensureCursorVisible();
}


/**
 * This function moves the cursor onto the next or previous word.
 * (Same as "w", "b" et.al. in vim)
 */
void MainText::cursorMoveWord(bool is_fwd, bool spc_only, bool to_end)
{
    auto c = this->textCursor();
    auto line_str = c.block().text();
    int pos = c.positionInBlock();

    if (is_fwd)
    {
        static const QRegularExpression re_spc_end("^\\s*\\S*");
        static const QRegularExpression re_spc_beg("^\\S*\\s*");
        static const QRegularExpression re_any_end("^\\W*\\w*");
        static const QRegularExpression re_any_beg("^\\w*\\W*");
        const QRegularExpression * re;

        if (spc_only)
            re = to_end ? &re_spc_end : &re_spc_beg;
        else
            re = to_end ? &re_any_end : &re_any_beg;

        auto mat = re->match(line_str.midRef(pos));
        if (mat.hasMatch() &&
            ((pos + mat.captured(0).length() < line_str.length()) || to_end))
        {
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, mat.captured(0).length());
        }
        else
        {
            c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
            c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        }
        this->setTextCursor(c);
    }
    else /* !is_fwd */
    {
        static const QRegularExpression re_spc_end("\\s(\\s+)$");
        static const QRegularExpression re_spc_beg("(\\S+\\s*)$");
        static const QRegularExpression re_any_end("\\w(\\W+\\w*)$");
        static const QRegularExpression re_any_beg("(\\w+|\\w+\\W+)$");
        const QRegularExpression * re;

        if (spc_only)
            re = to_end ? &re_spc_end : &re_spc_beg;
        else
            re = to_end ? &re_any_end : &re_any_beg;

        auto mat = re->match(line_str.leftRef(pos));
        if (mat.hasMatch())
        {
            c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, mat.captured(1).length());
        }
        else
        {
            c.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        }
        this->setTextCursor(c);
    }
    this->ensureCursorVisible();
}

/**
 * This function moves the cursor onto the next occurence of the given
 * character in the current line.
 */
void MainText::searchCharInLine(wchar_t chr, int dir)
{
    m_mainWin->clearMessage(this);

    if (chr != 0) {
        last_inline_char = chr;
        last_inline_dir = dir;
    }
    else {
        if (last_inline_char != 0) {
            chr = last_inline_char;
            dir *= last_inline_dir;
        }
        else {
            m_mainWin->showError(this, "No previous in-line character search");
            return;
        }
    }
    auto c = this->textCursor();
    auto blk = c.block();
    int off = c.positionInBlock();

    if (dir > 0) {
        int idx = blk.text().indexOf(chr, off + 1);
        if (idx > off) {
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, idx - off);
            this->setTextCursor(c);
            this->ensureCursorVisible();
        }
        else {
            QString msg = QString("Character \"") + QString(chr) + "\" not found until line end";
            m_mainWin->showWarning(this, msg);
        }
    }
    else if (off > 0) {
        int idx = blk.text().lastIndexOf(chr, off - 1);
        if ((idx != -1) && (idx < off)) {
            c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, off - idx);
            this->setTextCursor(c);
            this->ensureCursorVisible();
        }
        else {
            QString msg = QString("Character ") + chr + " not found until line start";
            m_mainWin->showWarning(this, msg);
        }
    }
}


/**
 * This function is called by all key bindings which make a large jump to
 * push the current cusor position onto the jump stack. Both row and column
 * are stored.  If the position is already on the stack, this entry is
 * deleted (note for this comparison only the line number is considered.)
 */
void MainText::cursorJumpPushPos()
{
    auto c = this->textCursor();

    // remove positions within the same line from the stack
    for (auto it = cur_jump_stack.begin(); it != cur_jump_stack.end(); ++it)
    {
        if (it->line == c.blockNumber())
        {
            cur_jump_stack.erase(it);
            break;
        }
    }
    // append to the stack
    cur_jump_stack.push_back(JumpPos{c.position(), c.blockNumber()});
    cur_jump_idx = -1;

    // limit size of the stack
    if (cur_jump_stack.size() > JUMP_STACK_MAXLEN)
        cur_jump_stack.resize(JUMP_STACK_MAXLEN);
}


/**
 * This function is bound to command "''" (i.e. two apostrophes) in the main
 * window. The command makes the cursor jump back to the origin of the last
 * jump (NOT to the target of the last jump, which may be confusing.) The
 * current position is pushed to the jump stack, if not already on the stack.
 */
void MainText::cursorJumpToggle()
{
    if (cur_jump_stack.size() > 0)
    {
        m_mainWin->clearMessage(this);

        // push current position to the stack
        cursorJumpPushPos();

        if (cur_jump_stack.size() > 1)
        {
            cur_jump_idx = cur_jump_stack.size() - 2;

            QTextCursor c = this->textCursor();
            c.setPosition(cur_jump_stack[cur_jump_idx].pos);
            this->setTextCursor(c);
            this->ensureCursorVisible();

            //TODO SearchList_MatchView $line
        }
        else
            m_mainWin->showWarning(this, "Already on the mark."); // warn
    }
    else
        m_mainWin->showError(this, "Jump stack is empty."); // error
}


/**
 * This function is bound to the CTRL-O and CTRL-I commands in the main
 * window. The function traverses backwards or forwards respectively
 * through the jump stack. During the first call the current cursor
 * position is pushed to the stack.
 */
void MainText::cursorJumpHistory(int rel)
{
    m_mainWin->clearMessage(this);

    if (cur_jump_stack.size() > 0)
    {
        if (cur_jump_idx < 0)
        {
            // push current position to the stack
            cursorJumpPushPos();

            if ((rel < 0) && (cur_jump_stack.size() >= 2))
                cur_jump_idx = cur_jump_stack.size() - 2;
            else
                cur_jump_idx = 0;
        }
        else
        {
            cur_jump_idx += rel;

            if (cur_jump_idx < 0) {
                m_mainWin->showWarning(this, "Jump stack wrapped from oldest to newest."); // warn
                cur_jump_idx = cur_jump_stack.size() - 1;
            }
            else if (cur_jump_idx >= (long)cur_jump_stack.size()) {
                m_mainWin->showWarning(this, "Jump stack wrapped from newest to oldest."); // warn
                cur_jump_idx = 0;
            }
        }
        QTextCursor c = this->textCursor();
        c.setPosition(cur_jump_stack[cur_jump_idx].pos);
        this->setTextCursor(c);
        this->ensureCursorVisible();

        //SearchList_MatchView $line
    }
    else
        m_mainWin->showError(this, "Jump stack is empty."); //error
}

void MainText::cursorJumpStackReset()
{
    cur_jump_stack.clear();
    cur_jump_idx = -1;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool MainText::findInBlocks(const QString &patStr, int from, bool is_fwd, bool opt_regexp,
                            bool opt_case, int& matchPos, int& matchLen)
{
    if (patStr.isEmpty())
        return false;

    int pos = from;
    // for backward search exclude match on string starting at given start pos
    if (is_fwd == false) {
        if (pos <= 0)
            return false;
        pos -= 1;
    }
    QTextBlock block = this->document()->findBlock(pos);
    int blockOffset = pos - block.position();
    int cnt = 50000;

    if (opt_regexp)
    {
        QRegularExpression::PatternOptions reflags =
                opt_case ? QRegularExpression::NoPatternOption
                         : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression expr(patStr, reflags);

        if (is_fwd)
        {
            while (block.isValid() && --cnt)
            {
                QRegularExpressionMatch mat;
                QString text = block.text();
                int idx = text.indexOf(expr, blockOffset, &mat);
                if (condUnlikely(idx >= 0))
                {
                    matchPos = block.position() + idx;
                    matchLen = mat.captured(0).length();
                    if (matchLen == 0) // may occur for "^" et.al.
                        matchLen = 1;
                    return true;
                }
                block = block.next();
                blockOffset = 0;
            }
        }
        else  /* !is_fwd */
        {
            while (block.isValid())
            {
                QRegularExpressionMatch mat;
                QString text = block.text();
                int idx = text.lastIndexOf(expr, blockOffset, &mat);
                if (condUnlikely(idx >= 0))
                {
                    matchPos = block.position() + idx;
                    matchLen = mat.captured(0).length();
                    if (matchLen == 0)
                        matchLen = 1;
                    return true;
                }
                if (condUnlikely(--cnt <= 0))
                    break;
                block = block.previous();
                blockOffset = -1;
            }
        }
    }
    else  /* sub-string search */
    {
        Qt::CaseSensitivity flags = opt_case ? Qt::CaseSensitive : Qt::CaseInsensitive;

        if (is_fwd)
        {
            while (condLikely(block.isValid() && --cnt))
            {
                QString text = block.text();
                int idx = text.indexOf(patStr, blockOffset, flags);
                if (condUnlikely(idx >= 0))
                {
                    matchPos = block.position() + idx;
                    matchLen = patStr.length();
                    return true;
                }
                block = block.next();
                blockOffset = 0;
            }
        }
        else  /* !is_fwd */
        {
            while (condLikely(block.isValid()))
            {
                QString text = block.text();
                int idx = text.lastIndexOf(patStr, blockOffset, flags);
                if (condUnlikely(idx >= 0))
                {
                    matchPos = block.position() + idx;
                    matchLen = patStr.length();
                    return true;
                }
                if (condUnlikely(--cnt <= 0))
                    break;
                block = block.previous();
                blockOffset = -1;
            }
        }
    }
    matchPos = block.isValid() ? block.position() : -1;
    return false;
}


// ----------------------------------------------------------------------------

MainSearch::MainSearch(MainWin * mainWin)
    : QWidget(mainWin)
    , m_mainWin(mainWin)
{
    m_timSearchInc = new QTimer(this);
    m_timSearchInc->setSingleShot(true);

    tlb_history.reserve(TLB_HIST_MAXLEN + 1);
}

void MainSearch::connectWidgets(MainText    * textWid,
                                MainFindEnt * f2_e,
                                QCheckBox   * f2_hall,
                                QCheckBox   * f2_mcase,
                                QCheckBox   * f2_regexp)
{
    m_mainText = textWid;
    m_f2_e = f2_e;
    m_f2_hall = f2_hall;
    m_f2_mcase = f2_mcase;
    m_f2_regexp = f2_regexp;

    m_higl.connectWidgets(m_mainText);
}

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

    return std::move(obj);
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

    m_higl.searchHighlightClear();
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
            if (!m_f2_e->hasFocus())
            {
                m_higl.searchHighlightUpdate(pat, opt_regexp, opt_case);
            }
            else
                m_higl.highlightVisible(pat, Highlighter::TAG_NAME_FIND, opt_regexp, opt_case);
        }
        else
            m_higl.searchHighlightClear();
    }
}

void MainSearch::searchHighlightClear()
{
    m_higl.searchHighlightClear();
}



// -------------------------------------------

//
// This function is invoked when the user enters text in the "find" entry field.
// In contrary to the "atomic" search, this function only searches a small chunk
// of text, then re-schedules itself as an "idle" task.  The search can be aborted
// at any time by canceling the task.
//
void MainSearch::searchBackground(const QString& pat, bool is_fwd, bool opt_regexp, bool opt_case,
                                  int startPos, bool is_changed,
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
        bool found = m_mainText->findInBlocks(pat, startPos, is_fwd, opt_regexp, opt_case, matchPos, matchLen);
        //printf("XXX %d -> %d,%d found?:%d\n", startPos, matchPos, matchLen, found);

        if (found)
        {
            // match found -> report; done
            QTextCursor c2 = m_mainText->textCursor();
            c2.setPosition(matchPos);
            c2.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, matchLen);

            searchHandleMatch(c2, pat, opt_regexp, opt_case, is_changed);
            callback(c2);
        }
        else if (matchPos >= 0)
        {
            // no match found in this portion -> reschedule next iteration
            m_timSearchInc->disconnect();
            connect(m_timSearchInc, &QTimer::timeout, [=](){ MainSearch::searchBackground(pat, is_fwd, opt_regexp, opt_case, matchPos, is_changed, callback); });
            m_timSearchInc->setInterval(0);
            m_timSearchInc->start();
        }
        else
            isDone = true;
    }
    if (isDone)
    {
        QTextCursor c2;
        searchHandleMatch(c2, pat, opt_regexp, opt_case, is_changed);
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
        tlb_last_dir = is_fwd;
        //set search_opt [Search_GetOptions $pat $is_re $use_case $tlb_last_dir]
        int start_pos = searchGetBase(is_fwd, false);
        QTextCursor c1 = m_mainText->textCursor();
        c1.setPosition(start_pos);

        m_mainText->cursorJumpPushPos();
        QTextCursor c2;

        while (true)
        {
            // invoke the actual search in the selected portion of the document
            QTextDocument::FindFlags flags = QTextDocument::FindFlags(is_fwd ? 0 : QTextDocument::FindBackward);
            if (opt_regexp)
            {
                QRegularExpression::PatternOptions reflags =
                        opt_case ? QRegularExpression::NoPatternOption
                                 : QRegularExpression::CaseInsensitiveOption;
                QRegularExpression re(pat, reflags);
                c2 = m_mainText->document()->find(re, c1, flags);
            }
            else
            {
                if (opt_case)
                    flags = QTextDocument::FindFlags(flags | QTextDocument::FindCaseSensitively);
                c2 = m_mainText->document()->find(pat, c1, flags);
            }

            // work-around for backwards search:
            // make sure the matching text is entirely to the left side of the cursor
            if (!is_fwd && !c2.isNull() &&
                (std::max(c2.position(), c2.anchor()) >= start_pos))
            {
                // match overlaps: search further backwards
                c1.setPosition(std::min(c2.position(), c2.anchor()));
                continue;
            }
            found = !c2.isNull();
            break;
        }

        // update cursor position and highlight
        searchHandleMatch(c2, pat, opt_regexp, opt_case, is_changed);
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
        m_higl.removeInc(m_mainText->document());

        if (!tlb_hall)  // else done below
            searchHighlightClear();
    }
    if (!match.isNull())
    {
        // mark the complete line containing the match
        m_higl.addSearchHall(match, s_colFind);

        // mark the matching text
        m_higl.addSearchInc(match, s_colFindInc);

        // move the cursor to the beginning of the matching text
        match.setPosition(std::min(match.position(), match.anchor()));
        m_mainText->setTextCursor(match);
        m_mainText->centerCursor();

        //TODO SearchList_HighlightLine find $tlb_find_line
        //TODO SearchList_MatchView $tlb_find_line
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
    m_mainWin->showWarning(this, msg);
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
        m_timSearchInc->stop();

        m_timSearchInc->disconnect();
        connect(m_timSearchInc, &QTimer::timeout, [=, is_fwd=tlb_last_dir](){ MainSearch::searchIncrement(is_fwd, true); });
        m_timSearchInc->setInterval(SEARCH_INC_DELAY);
        m_timSearchInc->start();
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
            m_higl.removeInc(m_mainText->document());
            m_higl.removeHall(m_mainText->document(), s_colFind);
            start_pos = tlb_inc_base;
        }
        else
            start_pos = searchGetBase(is_fwd, false);

        searchBackground(tlb_find, is_fwd, tlb_regexp, tlb_case, start_pos, is_changed,
                         [=](QTextCursor& c){ searchIncMatch(c, tlb_find, is_fwd, is_changed); });
    }
    else
    {
        searchReset();

        if (tlb_find != "")
            m_mainWin->showError(this, "Incomplete or invalid reg.exp.");
        else
            m_mainWin->clearMessage(this);
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
            m_mainWin->showWarning(this, "No match until end of file");
        else
            m_mainWin->showWarning(this, "No match until start of file");
    }
    else
        m_mainWin->clearMessage(this);

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
                m_mainWin->showError(this, msg);
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

#if 0 //TODO
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

void MainSearch::searchGetParams()
{
    tlb_find   = m_f2_e->text();
    tlb_regexp = m_f2_regexp->isChecked();
    tlb_case   = m_f2_mcase->isChecked();
    tlb_hall   = m_f2_hall->isChecked();
}

/**
 * This function is used by the various key bindings which repeat a
 * previous search.
 */
bool MainSearch::searchNext(bool is_fwd)
{
    bool found = false;

    m_mainWin->clearMessage(this);
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
        m_mainWin->showError(this, "No pattern defined for search repeat");
    }
    return found;
}

/**
 * This function is used by the "All" or "List all" buttons and assorted
 * keyboard shortcuts to list all text lines matching the current search
 * expression in a separate dialog window.
 */
void MainSearch::searchAll(bool /*raiseWin*/, int /*direction*/)
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

        //TODO SearchList_Open(raiseWin)
        //TODO SearchList_SearchMatches(True, tlb_find, tlb_regexp, tlb_case, direction)
    }
}

/**
 * This function resets the state of the search engine.  It is called when
 * the search string is empty or a search is aborted with the Escape key.
 */
void MainSearch::searchReset()
{
    m_higl.searchHighlightReset();

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
    m_mainWin->clearMessage(this);
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

        m_mainWin->clearMessage(this);
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
            m_mainWin->showError(this, "No pattern defined for search repeat");
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

/**
 * This function add the given search string to the search history stack.
 * If the string is already on the stack, it's moved to the top. Note: top
 * of the stack is the front of the list.
 */
void MainSearch::searchAddHistory(const QString& txt, bool is_re, bool use_case)
{
    if (!txt.isEmpty())
    {
        //TODO set old_sel [SearchHistory_StoreSel]

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

        //TODO SearchHistory_Fill();
        //TODO SearchHistory_RestoreSel(old_sel);
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
        //TODO old_sel = SearchHistory_StoreSel();

        tlb_history.erase(tlb_history.begin() + tlb_hist_pos);

        m_mainWin->updateRcAfterIdle();

        //TODO SearchHistory_Fill
        //TODO SearchHistory_RestoreSel $old_sel

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
            word = QString("\\m") + word + "\\M";

        tlb_find = word;
        m_f2_e->setText(tlb_find);
        m_f2_e->setCursorPosition(0);

        searchAddHistory(tlb_find, tlb_regexp, tlb_case);
        m_mainWin->clearMessage(this);

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


// ----------------------------------------------------------------------------

MainWin::MainWin(QApplication * app)
    : m_mainApp(app)
    , m_fontContent(DEFAULT_FONT_FAM, DEFAULT_FONT_SZ, false, false)
{
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    m_search = new MainSearch(this);

    m_f1_t = new MainText(this, m_search, central_wid);
        m_f1_t->setFont(m_fontContent);
        m_f1_t->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_f1_t->setCursorWidth(2);
        m_f1_t->setTabChangesFocus(true);   // enable Key_Tab moving keyboard focus
        layout_top->addWidget(m_f1_t);
    //m_higl = new Highlighter(m_f1_t->document());

    auto f2 = new QToolBar("Find", this);
        f2->setObjectName("Toolbar::Find"); // for saveState
        addToolBar(Qt::BottomToolBarArea, f2);
    auto f2_l = new QLabel("Find:", f2);
        f2->addWidget(f2_l);
    auto f2_e = new MainFindEnt(m_search, f2);
        f2->addWidget(f2_e);
    auto f2_bn = new QPushButton("&Next", f2);
        connect(f2_bn, &QPushButton::clicked, [=](){ m_search->searchNext(true); });
        f2->addWidget(f2_bn);
    auto f2_bp = new QPushButton("&Prev.", f2);
        connect(f2_bp, &QPushButton::clicked, [=](){ m_search->searchNext(false); });
        f2->addWidget(f2_bp);
    auto f2_bl = new QPushButton("&All", f2);
        // FIXME first param should be "false" when used via shortcut
        connect(f2_bp, &QPushButton::clicked, [=](){ m_search->searchAll(true, 0); });
        f2->addWidget(f2_bl);
    f2->addSeparator();
    auto f2_hall = new QCheckBox("Highlight all", f2);
        // do not add shortcut "ALT-H" here as this would move focus when triggered
        connect(f2_hall, &QPushButton::clicked, [=](){ m_search->searchHighlightSettingChange(); });
        f2->addWidget(f2_hall);
    auto f2_mcase = new QCheckBox("Match case", f2);
        connect(f2_mcase, &QPushButton::clicked, [=](){ m_search->searchHighlightSettingChange(); });
        f2->addWidget(f2_mcase);
    auto f2_regexp = new QCheckBox("Reg.Exp.", f2);
        connect(f2_regexp, &QPushButton::clicked, [=](){ m_search->searchHighlightSettingChange(); });
        f2->addWidget(f2_regexp);

    m_search->connectWidgets(m_f1_t, f2_e, f2_hall, f2_mcase, f2_regexp);

    layout_top->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(central_wid);

    m_stline = new StatusMsg(m_f1_t);

    populateMenus();

    QAction * act;
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_F));
        connect(act, &QAction::triggered, [=](){ m_search->searchEnter(true); });
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_N));
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(false, 1); });
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_P));
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(false, -1); });
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_H));
        connect(act, &QAction::triggered, [=](){ m_search->searchOptToggleHall(); });
        central_wid->addAction(act);

    m_f1_t->setFocus(Qt::ShortcutFocusReason);

    m_timUpdateRc = new QTimer(this);
    m_timUpdateRc->setSingleShot(true);
    m_timUpdateRc->setInterval(3000);
    connect(m_timUpdateRc, &QTimer::timeout, this, &MainWin::updateRcFile);
    m_tsUpdateRc = QDateTime::currentSecsSinceEpoch();
}

MainWin::~MainWin()
{
    delete m_search;
}

void MainWin::populateMenus()
{
    QAction * act;

    m_menubar_ctrl = menuBar()->addMenu("&Control");
    act = m_menubar_ctrl->addAction("Open file...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileOpen);
    act = m_menubar_ctrl->addAction("Reload current file");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdReload);
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Discard above cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(false); });
    act = m_menubar_ctrl->addAction("Discard below cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(true); });
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Toggle line wrap");
        act->setCheckable(true);
        connect(act, &QAction::toggled, this, &MainWin::toggleLineWrap);
    act = m_menubar_ctrl->addAction("Font selection...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdSelectFont);
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Quit");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileQuit);

    m_menubar_srch = menuBar()->addMenu("&Search");
    act = m_menubar_srch->addAction("Search history...");
    act = m_menubar_srch->addAction("Edit highlight patterns...");
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("List all search matches...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 0); });
    act = m_menubar_srch->addAction("List all matches above...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, -1); });
    act = m_menubar_srch->addAction("List all matches below...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 1); });
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("Clear search highlight");
        connect(act, &QAction::triggered, m_search, &MainSearch::searchHighlightClear);
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("Goto line number...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdGotoLine);

    m_menubar_mark = menuBar()->addMenu("&Bookmarks");
    act = m_menubar_mark->addAction("Toggle bookmark");
    act = m_menubar_mark->addAction("List bookmarks");
    act = m_menubar_mark->addAction("Delete all bookmarks");
    m_menubar_mark->addSeparator();
    act = m_menubar_mark->addAction("Jump to prev. bookmark");
    act = m_menubar_mark->addAction("Jump to next bookmark");
    m_menubar_mark->addSeparator();
    act = m_menubar_mark->addAction("Read bookmarks from file...");
    act = m_menubar_mark->addAction("Save bookmarks to file...");

    m_menubar_help = menuBar()->addMenu("Help");
    act = m_menubar_help->addAction("About trowser...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdAbout);
    act = m_menubar_help->addAction("About Qt...");
        connect(act, &QAction::triggered, m_mainApp, &QApplication::aboutQt);
}

QWidget * MainWin::focusWidget()
{
    return m_mainApp->focusWidget();
}


// ----------------------------------------------------------------------------

void MainWin::menuCmdAbout(bool)
{
    QString msg("TROWSER - A debug trace browser\n"
                "Copyright (C) 2007-2010,2020 Th. Zoerner\n"
                "\n"
                "trowser is a browser for large line-oriented text files with color "
                "highlighting, originally written 2007 in Tcl/Tk but in this version "
                "converted to C++ using Qt. trowser is designed especially for analysis "
                "of huge text files as generated by an application's debug/trace output. "
                "Compared to traditional tools such as 'less', trowser adds color "
                "highlighting, a persistent search history, "
                "graphical bookmarking, separate search result (i.e. filter) windows and "
                "flexible skipping of input from pipes to STDIN.  Trowser has a graphical "
                "interface, but is designed to allow browsing via the keyboard at least to "
                "the same extent as less. Key bindings and the cursor positioning concept "
                "are derived mainly from vim.\n"
                "\n"
                "This program is free software: you can redistribute it and/or modify it "
                "under the terms of the GNU General Public License as published by the "
                "Free Software Foundation, either version 3 of the License, or (at your "
                "option) any later version.\n"
                "\n"
                "This program is distributed in the hope that it will be useful, but "
                "WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY "
                "or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License "
                "for more details.\n"
                "\n"
                "You should have received a copy of the GNU General Public License along "
                "with this program.  If not, see <http://www.gnu.org/licenses/>.");

    QMessageBox::about(this, "About Trace Browser", msg);
}

/**
 * This function is bound to CTRL-G in the main window. It displays the
 * current line number and fraction of lines above the cursor in percent
 * (i.e. same as VIM)
 */
void MainWin::menuCmdDisplayLineNo()
{
    auto c = m_f1_t->textCursor();
    int line = c.block().blockNumber();
    int max = m_f1_t->document()->blockCount();

    QString msg = m_curFileName + ": line " + QString::number(line + 1) + " of "
                    + QString::number(max) + " lines";
    if (max > 1)
        msg += " (" + QString::number(int(100.0 * line / max + 0.5)) + "%)";

    m_stline->showPlain(this, msg);
}

/**
 * This function is bound to the "Goto line" menu command.  The function opens
 * a small modal dialog for entering a number in range of the number of lines
 * in the text buffer (starting at 1). When completed by the user the cursor is
 * set to the start of the specified line.
 */
void MainWin::menuCmdGotoLine(bool)
{
    auto doc = m_f1_t->document();
    int max = doc->blockCount();
    bool ok = false;

    int line = QInputDialog::getInt(this, "Goto line number...",
                                    QString("Enter line number: (max. ")
                                        + QString::number(max) + ")",
                                    1, 1, doc->blockCount(), 1, &ok);
    if (ok && (line > 0) && (line <= max))
    {
        auto blk = doc->findBlockByNumber(line - 1);

        auto c = m_f1_t->textCursor();
        c.setPosition(blk.position());
        m_f1_t->setTextCursor(c);
    }
}

void MainWin::menuCmdSelectFont(bool)
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_fontContent, this);
    if (ok)
    {
        m_f1_t->setFont(font);
        m_fontContent = font;
    }
}

void MainWin::keyCmdZoomFontSize(bool zoomIn)
{
    if (zoomIn)
        m_f1_t->zoomIn();
    else
        m_f1_t->zoomOut();

    m_fontContent = m_f1_t->font();
}

void MainWin::toggleLineWrap(bool checked)
{
    m_f1_t->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

/**
 * This procedure discards all text content and aborts all ongoing
 * activity and timers. The function is called before new data is
 * loaded.
 */
void MainWin::discardContent()
{
  // discard the current trace content
  m_f1_t->clear();

  m_search->searchReset();

  //TODO array unset mark_list
  //TODO set mark_list_modified 0
  //TODO MarkList_Fill();

  //TODO SearchList_Clear();
  //TODO SearchList_Init();
}


/*
 * This function is bound to the "Discard above/below" menu commands. The
 * parameter specifies if content above or below the cursor is discarded.
 */
void MainWin::menuCmdDiscard(bool is_fwd)
{
    //PreemptBgTasks()

    auto c = m_f1_t->textCursor();
    int curLine = c.block().blockNumber();
    int maxLine = m_f1_t->document()->blockCount();
    int count = 0;

    if (is_fwd)
    {
        // delete everything below the line holding the cursor
        count = maxLine - curLine - 2;
        if (count <= 0)
        {
            QMessageBox::information(this, "trowser", "Already at the bottom");
            return;
        }
        c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    }
    else
    {
        // delete everything above the line holding the cursor
        count = curLine;
        if (count <= 0)
        {
            QMessageBox::information(this, "trowser", "Already at the top");
            return;
        }
        c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        c.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
    }
    int perc = ((maxLine != 0) ? int(100.0 * count / maxLine) : 0);
    //ResumeBgTasks()

    // ask for confirmation, as this cannot be undone
    QString msg;
    QTextStream(&msg) << "Please confirm removing " << count
                      << ((count == 1) ? " line" : " lines")
                      << " (" << perc << "%), as this cannot be undone.";
    if (!m_curFileName.isEmpty())
        msg += " (The file will not be modified.)";

    auto answer = QMessageBox::question(this, "trowser", msg,
                                        QMessageBox::Ok | QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;

    //TODO if (SearchList_SearchAbort())
    {
        m_search->searchHighlightClear();

        // perform the removal
        c.removeSelectedText();

        // re-start initial highlighting, if not complete yet
        //TODO if tid_high_init is not None:
        //    tk.after_cancel(tid_high_init)
        //    tid_high_init = None
        //    HighlightInit()

        m_search->searchReset();
        m_f1_t->cursorJumpStackReset();

        //TODO MarkList_AdjustLineNums(1 if is_fwd else last_l, first_l if is_fwd else 0)
        //TODO SearchList_AdjustLineNums(1 if is_fwd else last_l, first_l if is_fwd else 0)
    }
}


/**
 * This function is bound to the "Reload current file" menu command.
 */
void MainWin::menuCmdReload(bool)
{
#if 0 //TODO
    if (load_pipe)
    {
        discardContent();
        load_pipe.LoadPipe_Start();
    }
    else
#endif
    {
        discardContent();
        LoadFile(m_curFileName);
    }
}


/**
 * This function is bound to the "Load file" menu command.  The function
 * allows to specify a file from which a new trace is read. The current browser
 * contents are discarded and all bookmarks are cleared.
 */
void MainWin::menuCmdFileOpen(bool)
{
    // offer to save old bookmarks before discarding them below
    //TODO Mark_OfferSave();

    QString fileName = QFileDialog::getOpenFileName(this,
                         "Open File", ".", "Trace Files (out.*);;Any (*)");
    if (fileName != "")
    {
        discardContent();
        LoadFile(fileName);
    }
}

/**
 * This function is installed as callback for destroy requests on the
 * main window to store the search history and bookmarks.
 */
void MainWin::menuCmdFileQuit(bool)
{
    updateRcFile();
    //TODO Mark_OfferSave();

    // FIXME connect(quitButton, clicked(), m_mainApp, &QApplication:quit, Qt::QueuedConnection);
    m_mainApp->quit();
}


// user closed main window via "X" button
void MainWin::closeEvent(QCloseEvent *)
{
    menuCmdFileQuit(true);
}


/**
 * This function loads a text file (or parts of it) into the text widget.
 */
void MainWin::LoadFile(const QString& fileName)
{
    auto fh = new QFile(QString(fileName));
    if (fh->open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream readFile(fh);
        m_f1_t->setPlainText(readFile.readAll());
        //m_f1_t->setReadOnly(true);
        delete fh;

        setWindowTitle(fileName + " - trowser");
        m_curFileName = fileName;
    }
    else
    {
        QMessageBox::critical(this, "trowser",
                              QString("Error opening file ") + fileName,
                              QMessageBox::Ok);
    }
}

// ----------------------------------------------------------------------------

/**
 * This function reads configuration variables from the rc file.
 * The function is called once during start-up.
 */
void MainWin::loadRcFile()
{
    QFile fh(myrcfile);
    if (fh.open(QFile::ReadOnly | QFile::Text))
    {
        QJsonParseError err;
        QTextStream readFile(&fh);
        auto txt = readFile.readAll();

        // skip comments at the start of the file
        int off = 0;
        while (off < txt.length())
        {
            static const QRegularExpression re1("\\s*(?:#.*)?(?:\n|$)");
            auto mat1 = re1.match(txt.midRef(off), 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
            if (!mat1.hasMatch())
                break;
            off += mat1.captured(0).length();
        }

        auto doc = QJsonDocument::fromJson(txt.midRef(off).toUtf8(), &err);
        if (!doc.isNull())
        {
            if (doc.isObject())
            {
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it)
                {
                    const QString& var = it.key();
                    const QJsonValue& val = it.value();

                    if (var == "main_search")               m_search->setRcValues(val.toObject());
                    //else if (var == "patlist")            patlist = val;
                    //else if (var == "col_palette")        col_palette = val;
                    //else if (var == "tick_pat_sep")       tick_pat_sep = val;
                    //else if (var == "tick_pat_num")       tick_pat_num = val;
                    //else if (var == "tick_str_prefix")    tick_str_prefix = val;
                    else if (var == "font_content")         m_fontContent.fromString(val.toString());
                    //else if (var == "fmt_selection")      fmt_selection = val;
                    //else if (var == "col_bg_content")     col_bg_content = val;
                    //else if (var == "col_fg_content")     col_fg_content = val;
                    //else if (var == "fmt_find")           fmt_find = val;
                    //else if (var == "fmt_findinc")        fmt_findinc = val;
                    else if (var == "load_buf_size")        load_buf_size = val.toInt();
                    //else if (var == "rcfile_version")     rcfile_version = val;
                    //else if (var == "rc_compat_version")  rc_compat_version = val;
                    //else if (var == "rc_timestamp")       { /* nop */ }
                    //else if (var.startswith("win_geom:"))  win_geom[var[9:]] = val;
                    else if (var == "main_win_state")       this->restoreState(QByteArray::fromHex(val.toString().toLatin1()));
                    else if (var == "main_win_geom")        this->restoreGeometry(QByteArray::fromHex(val.toString().toLatin1()));
                    else
                        fprintf(stderr, "trowser: ignoring unknown keyword in rcfile: %s\n", var.toLatin1().data());
                }
                m_f1_t->setFont(m_fontContent);
            }
        }
        else
        {
            fprintf(stderr, "Error parsing config file: %s\n", err.errorString().toLatin1().data());
        }
        fh.close();
    }
    else
    {
        // Application GUI is not initialized yet, so print to console
        fprintf(stderr, "trowser: warning: failed to load config file '%s': %s\n", myrcfile, strerror(errno));
    }
}

/**
 * This functions writes configuration variables into the rc file
 */
void MainWin::updateRcFile()
{
    static bool rc_file_error = false;

    m_timUpdateRc->stop();
    m_tsUpdateRc = QDateTime::currentSecsSinceEpoch();

    //expr {srand([clock clicks -milliseconds])}
    //append tmpfile $myrcfile "." [expr {int(rand() * 1000000)}] ".tmp"

    QJsonObject obj;

    // dump software version
    //puts $rcfile [list set rcfile_version $rcfile_version]
    //puts $rcfile [list set rc_compat_version $rcfile_compat]
    //puts $rcfile [list set rc_timestamp [clock seconds]]

    // dump highlighting patterns
    //puts $rcfile [list set patlist {}]
    //foreach val $patlist {
    //  puts $rcfile [list lappend patlist $val]
    //}

    // dump color palette
    //puts $rcfile [list set col_palette {}]
    //foreach val $col_palette {
    //  puts $rcfile [list lappend col_palette $val]
    //}

    // frame number parser patterns
    //puts $rcfile [list set tick_pat_sep $tick_pat_sep]
    //puts $rcfile [list set tick_pat_num $tick_pat_num]
    //puts $rcfile [list set tick_str_prefix $tick_str_prefix]

    // dump search history
    obj.insert("main_search", m_search->getRcValues());

    // dialog sizes
    //puts $rcfile [list set dlg_mark_geom $dlg_mark_geom]
    //puts $rcfile [list set dlg_hist_geom $dlg_hist_geom]
    //puts $rcfile [list set dlg_srch_geom $dlg_srch_geom]
    //puts $rcfile [list set dlg_tags_geom $dlg_tags_geom]
    obj.insert("main_win_geom", QJsonValue(QString(this->saveGeometry().toHex())));
    obj.insert("main_win_state", QJsonValue(QString(this->saveState().toHex())));

    // font and color settings
    obj.insert("font_content", QJsonValue(m_fontContent.toString()));
    //puts $rcfile [list set col_bg_content $col_bg_content]
    //puts $rcfile [list set col_fg_content $col_fg_content]
    //puts $rcfile [list set fmt_find $fmt_find]
    //puts $rcfile [list set fmt_findinc $fmt_findinc]
    //puts $rcfile [list set fmt_selection $fmt_selection]

    // misc (note the head/tail mode is omitted intentionally)
    obj.insert("load_buf_size", QJsonValue(load_buf_size));

    QJsonDocument doc;
    doc.setObject(obj);

    QFile fh(myrcfile);
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

        fh.close();
#if 0
        // copy attributes on the new file
        if {[catch {set att_perm [file attributes $myrcfile -permissions]}] == 0} {
            catch {file attributes $tmpfile -permissions $att_perm}
        }
        if {[catch {set att_grp [file attributes $myrcfile -group]}] == 0} {
            catch {file attributes $tmpfile -group $att_grp}
        }
        // move the new file over the old one
        if {[catch {file rename -force $tmpfile $myrcfile} errstr] != 0} {
            if {![info exists rc_file_error]} {
                tk_messageBox -type ok -default ok -icon error \
                              -message "Could not replace rc file $myrcfile: $errstr"
                set rc_file_error 1
            }
        } else {
            unset -nocomplain rc_file_error
        }

        //TODO catch write error
        {
            // write error - remove the file fragment, report to user
            catch {file delete $tmpfile}
            if {![info exists rc_file_error]} {
                tk_messageBox -type ok -default ok -icon error \
                              -message "Write error in file $myrcfile: $errstr"
                set rc_file_error 1
            }
        }
#endif
        rc_file_error = false;
    }
    else /* open error */
    {
        if (!rc_file_error)
        {
            QMessageBox::critical(this, "trowser",
                                  QString("Error writing config file ") + myrcfile,
                                  QMessageBox::Ok);
            rc_file_error = true;
        }
    }
}

/**
 * This function is used to trigger writing the RC file after changes.
 * The write is delayed by a few seconds to avoid writing the file multiple
 * times when multiple values are changed. This timer is restarted when
 * another change occurs during the delay, however only up to a limit.
 */
void MainWin::updateRcAfterIdle()
{
    if (   !m_timUpdateRc->isActive()
        || (QDateTime::currentSecsSinceEpoch() - m_tsUpdateRc) < 60)
    {
        m_timUpdateRc->start();
    }
}


// ----------------------------------------------------------------------------

/**
 * This function is called when the program is started with -help to list all
 * possible command line options.
 */
void PrintUsage(const char * const argv[], int argvn=-1, const char * reason=nullptr)
{
    if (reason != nullptr)
        fprintf(stderr, "%s: %s: %s", argv[0], reason, argv[argvn]);

    fprintf(stderr, "Usage: %s [options] {file|-}", argv[0]);

    if (argvn != -1)
    {
        fprintf(stderr, "Use -h or --help for a list of options");
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
void ParseArgvLenCheck(int argc, const char * const argv[], int arg_idx)
{
    if (arg_idx + 1 >= argc)
        PrintUsage(argv, arg_idx, "this option requires an argument");
}

/**
 * This helper function reads an integer value from a command line parameter
 */
int ParseArgInt(const char * const argv[], int arg_idx, const char * opt)
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
                load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                load_file_mode = 1;
            }
            else if (strncmp(arg, "--tail", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    load_file_mode = 1;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --tail=10000000)");
            }
            else if (strcmp(arg, "-h") == 0)
            {
                ParseArgvLenCheck(argc, argv, arg_idx);
                arg_idx += 1;
                load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                load_file_mode = 0;
            }
            else if (strncmp(arg, "--head", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    load_file_mode = 0;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --head=10000000)");
            }
            else if (strcmp(arg, "-r") == 0)
            {
                if (arg_idx + 1 < argc)
                {
                    arg_idx += 1;
                    myrcfile = argv[arg_idx];
                }
                else
                    PrintUsage(argv, arg_idx, "this option requires an argument");
            }
            else if (strncmp(arg, "--rcfile", 8) == 0)
            {
                if ((arg[8] == '=') && (arg[9] != 0))
                    myrcfile = arg + 8+1;
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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("trowser");
    ParseArgv(argc, argv);

    MainWin main(&app);
    main.loadRcFile();
    main.LoadFile(argv[argc - 1]);

    main.show();
    return app.exec();
}

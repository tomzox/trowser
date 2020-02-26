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
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "status_line.h"
#include "bookmarks.h"
#include "search_list.h"

// ----------------------------------------------------------------------------

MainText::MainText(MainWin * mainWin, MainSearch * search, Bookmarks * bookmarks, QWidget * parent)
    : QPlainTextEdit(parent)
    , m_mainWin(mainWin)
    , m_search(search)
    , m_bookmarks(bookmarks)
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
    m_keyCmdText.emplace('\r', [=](){ cursorMoveLine(1, true); });  // Key_Return
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
    m_keyCmdText.emplace('i', [=](){ SearchList::getInstance(false)->copyCurrentLine(true); });
    m_keyCmdText.emplace('u', [=](){ SearchList::extUndo(); });
    m_keyCmdCtrl.emplace(Qt::Key_R, [=](){ SearchList::extRedo(); });
    m_keyCmdCtrl.emplace(Qt::Key_G, [=](){ m_mainWin->menuCmdDisplayLineNo(); });
    //bind .f1.t <Double-Button-1> {if {%s == 0} {Mark_ToggleAtInsert; KeyClr; break}};
    m_keyCmdText.emplace('m', [=](){ toggleBookmark(); });
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
        case Qt::Key_Delete:  // reverse of 'i'
            if ((e->modifiers() == Qt::NoModifier) && SearchList::isDialogOpen())
                SearchList::getInstance(false)->copyCurrentLine(false);
            break;

        // permissible standard key-bindings, filtered for read-only
        case Qt::Key_Up:
            if (e->modifiers() == Qt::NoModifier)
                cursorMoveLine(-1, false);
            else
                QPlainTextEdit::keyPressEvent(e);
            break;
        case Qt::Key_Down:
            if (e->modifiers() == Qt::NoModifier)
                cursorMoveLine(1, false);
            else
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
        m_mainWin->mainStatusLine()->clearMessage("keycmd");
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
            jumpToNextBookmark(true);
        }
        else if (chr == '-') {
            jumpToNextBookmark(false);
        }
        else {
            QString msg = QString("Undefined key sequence: ") + last_key_char + chr;
            m_mainWin->mainStatusLine()->showError("keycmd", msg);
        }
        last_key_char = 0;
        result = true;
    }
    else if ((last_key_char == 'z') || (last_key_char == 'g')) {
        m_mainWin->mainStatusLine()->clearMessage("keycmd");

        auto it = m_keyCmdText.find(KeySet(last_key_char, chr));
        if (it != m_keyCmdText.end()) {
            (it->second)();
        }
        else {
            QString msg = QString("Undefined key sequence: ") + last_key_char + chr;
            m_mainWin->mainStatusLine()->showError("keycmd", msg);
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
    SearchList::matchView(this->textCursor().block().blockNumber());
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
void MainText::cursorSetLineTop(int /*off*/)
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
void MainText::cursorSetLineBottom(int /*off*/)
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

    if (toStart)
    {
        if (delta > 0) {
            c.movePosition(QTextCursor::NextBlock);
            this->setTextCursor(c);
        }
        else if (delta < 0) {
            c.movePosition(QTextCursor::PreviousBlock);
            this->setTextCursor(c);
        }

        this->horizontalScrollBar()->setValue(0);

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
    else  // up/down within same column
    {
        auto blk = c.block();
        int linePos = c.position() - blk.position();
        if (delta > 0)
            blk = blk.next();
        else if (delta < 0)
            blk = blk.previous();
        if (blk.isValid())
        {
            if (linePos >= blk.length())  // TODO remember target column, to be used in next up/down move
                linePos = (blk.length() != 0) ? (blk.length() - 1) : 0;
            c.setPosition(blk.position() + linePos);
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
void MainText::XviewScroll(int /*delta*/, int /*dir*/)
{
#if 0  //TODO
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
#if 0  //TODO
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
void MainText::XviewSet(xviewSetWhere /*where*/)
{
#if 0  //TODO
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
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
    this->setTextCursor(c);
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
    m_mainWin->mainStatusLine()->clearMessage("search_inline");

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
            m_mainWin->mainStatusLine()->showError("search_inline", "No previous in-line character search");
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
            m_mainWin->mainStatusLine()->showWarning("search_inline", msg);
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
            m_mainWin->mainStatusLine()->showWarning("search_inline", msg);
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
        m_mainWin->mainStatusLine()->clearMessage("keycmd");

        // push current position to the stack
        cursorJumpPushPos();

        if (cur_jump_stack.size() > 1)
        {
            cur_jump_idx = cur_jump_stack.size() - 2;

            QTextCursor c = this->textCursor();
            c.setPosition(cur_jump_stack[cur_jump_idx].pos);
            this->setTextCursor(c);
            this->ensureCursorVisible();

            SearchList::matchView(c.block().blockNumber());
        }
        else
            m_mainWin->mainStatusLine()->showWarning("keycmd", "Already on the mark.");
    }
    else
        m_mainWin->mainStatusLine()->showError("keycmd", "Jump stack is empty.");
}


/**
 * This function is bound to the CTRL-O and CTRL-I commands in the main
 * window. The function traverses backwards or forwards respectively
 * through the jump stack. During the first call the current cursor
 * position is pushed to the stack.
 */
void MainText::cursorJumpHistory(int rel)
{
    m_mainWin->mainStatusLine()->clearMessage("keycmd");

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
                m_mainWin->mainStatusLine()->showWarning("keycmd", "Jump stack wrapped from oldest to newest.");
                cur_jump_idx = cur_jump_stack.size() - 1;
            }
            else if (cur_jump_idx >= (long)cur_jump_stack.size()) {
                m_mainWin->mainStatusLine()->showWarning("keycmd", "Jump stack wrapped from newest to oldest.");
                cur_jump_idx = 0;
            }
        }
        QTextCursor c = this->textCursor();
        c.setPosition(cur_jump_stack[cur_jump_idx].pos);
        this->setTextCursor(c);
        this->ensureCursorVisible();

        SearchList::matchView(c.block().blockNumber());
    }
    else
        m_mainWin->mainStatusLine()->showError("keycmd", "Jump stack is empty.");
}

void MainText::cursorJumpStackReset()
{
    cur_jump_stack.clear();
    cur_jump_idx = -1;
}

void MainText::toggleBookmark()
{
    auto c = this->textCursor();
    int line = c.block().blockNumber();
    m_bookmarks->toggleBookmark(line);
}

/**
 * This function moves the cursor onto the next bookmark in the given
 * direction.
 */
void MainText::jumpToNextBookmark(bool is_fwd)
{
    auto c = this->textCursor();
    int line = c.block().blockNumber();
    line = m_bookmarks->getNextLine(line, is_fwd);
    if (line != -1)
    {
        jumpToLine(line);
    }
    else
    {
        if (m_bookmarks->getCount() == 0)
            m_mainWin->mainStatusLine()->showError("keycmd", "No bookmarks have been defined yet");
        else if (is_fwd)
            m_mainWin->mainStatusLine()->showWarning("keycmd", "No more bookmarks until end of file");
        else
            m_mainWin->mainStatusLine()->showWarning("keycmd", "No more bookmarks until start of file");
    }
}

void MainText::jumpToLine(int line)
{
    QTextBlock blk = this->document()->findBlockByNumber(line);
    if (blk.isValid())
    {
        cursorJumpPushPos();

        auto c = this->textCursor();
        c.setPosition(blk.position());
        this->setTextCursor(c);

        SearchList::matchView(c.block().blockNumber());
    }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * Helper function for calling the search function on the text document with
 * the given parameters.
 */
QTextCursor MainText::findInDoc(const SearchPar& par, bool is_fwd, int start_pos)
{
    QTextCursor c1 = this->textCursor();
    c1.setPosition(start_pos);

    QTextCursor c2;
    while (true)
    {
        // invoke the actual search in the selected portion of the document
        auto flags = QTextDocument::FindFlags(is_fwd ? 0 : QTextDocument::FindBackward);
        if (par.m_opt_case)
            flags = QTextDocument::FindFlags(flags | QTextDocument::FindCaseSensitively);

        if (par.m_opt_regexp)
        {
            QRegularExpression::PatternOptions reflags =
                    par.m_opt_case ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(par.m_pat, reflags);
            c2 = this->document()->find(re, c1, flags);
        }
        else
        {
            c2 = this->document()->find(par.m_pat, c1, flags);
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
        break;
    }
    return c2;
}

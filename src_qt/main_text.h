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
#ifndef _MAIN_TEXT_H
#define _MAIN_TEXT_H

#include <QMainWindow>
#include <QTextCursor>
#include <QPlainTextEdit>

#include <vector>
#include <unordered_map>

class MainWin;
class MainSearch;
class SearchPar;
class Bookmarks;

// ----------------------------------------------------------------------------

class KeySet
{
public:
    constexpr KeySet(wchar_t k1) : key(k1) {}
    constexpr KeySet(wchar_t k1, wchar_t k2) : key(k1 | (k2 << 16)) {}
    bool operator==(const KeySet& v) const { return key == v.key; }
    const uint32_t key;
};
template<class T> class KeySetHash;
template<> class KeySetHash<KeySet>
{
public:
    constexpr size_t operator()(const KeySet& v) const { return v.key; }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class MainText : public QPlainTextEdit
{
    Q_OBJECT

public:
    MainText(MainWin * mainWin, MainSearch * search, Bookmarks * bookmarks, QWidget * parent);
    void cursorJumpPushPos();
    void cursorJumpStackReset();
    void keyCmdClear();
    void toggleBookmark();
    void jumpToNextBookmark(bool);
    void jumpToLine(int line);

    QTextCursor findInDoc(const SearchPar& par, bool is_fwd, int start_pos);

private:
    using xviewSetWhere = enum { XVIEW_SET_LEFT, XVIEW_SET_RIGHT };
    using JumpPos = struct { int pos, line; };
    static const uint JUMP_STACK_MAXLEN = 100;

    virtual void keyPressEvent(QKeyEvent *e) override;
    bool keyCmdText(wchar_t chr);

    void YviewSet(char where, int col);
    void YviewScrollLine(int delta);
    void YviewScrollHalf(int dir);
    void YviewScrollVisibleCursor(int dir);
    void cursorSetLineTop(int off);
    void cursorSetLineCenter();
    void cursorSetLineBottom(int off);
    void cursorMoveLine(int delta, bool toStart);
    void cursorGotoTop(int off);
    void cursorGotoBottom(int off);
    void XviewScroll(int delta, int dir);
    void XviewScrollHalf(int dir);
    void XviewSet(xviewSetWhere where);
    void cursorMoveWord(bool is_fwd, bool spc_only, bool to_end);
    void cursorSetLineEnd();
    void cursorSetLineStart();
    void searchCharInLine(wchar_t chr, int dir);
    void cursorJumpToggle();
    void cursorJumpHistory(int rel);

private:
    MainWin     * const m_mainWin;
    MainSearch  * const m_search;
    Bookmarks   * const m_bookmarks;
    wchar_t       last_inline_char = 0;
    int           last_inline_dir = 0;
    wchar_t       last_key_char = 0;
    std::unordered_map<KeySet,const std::function<void()>,KeySetHash<KeySet>> m_keyCmdText;
    std::unordered_map<int,const std::function<void()>> m_keyCmdCtrl;
    std::vector<JumpPos> cur_jump_stack;
    int           cur_jump_idx = -1;
};

#endif /* _MAIN_TEXT_H */

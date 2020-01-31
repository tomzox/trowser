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
#ifndef _MAIN_WIN_H
#define _MAIN_WIN_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QLineEdit>
#include <memory>
#include <vector>
#include <unordered_map>

#include <QTextCursor>
#include <QTimer>

class QApplication;
class QMenu;
class QPlainTextEdit;
class QCheckBox;
class QTextDocument;
class QLabel;
class QProgressBar;
class MainWin;
class MainText;
class MainSearch;
class QJsonObject;

#ifdef __GNUC__
#define condLikely(x)       __builtin_expect(!!(x), 1)
#define condUnlikely(x)     __builtin_expect(!!(x), 0)
#else
#define condLikely(x)       (x)
#define condUnlikely(x)     (x)
#endif

// ----------------------------------------------------------------------------

class ATimer : public QTimer
{
public:
    ATimer(QWidget * parent = nullptr);
    void after(int delay, const std::function<void()>& callback);
    void reschedule(int delay);
};

// ----------------------------------------------------------------------------

class StatusMsg : public QWidget
{
    Q_OBJECT

public:
    StatusMsg(QWidget * parent);
    void showWarning(QWidget * widget, const QString& msg);
    void showError(QWidget * widget, const QString& msg);
    void showPlain(QWidget * widget, const QString& msg);
    void clearMessage(QWidget * widget);
private:
    void showStatusMsg(const QString& msg, QRgb col);
    void expireStatusMsg();

private:
    static const int DISPLAY_DURATION = 4000;
    QWidget     * m_parent;
    QLabel      * m_lab;
    QTimer      * m_timStLine;
    QWidget     * m_owner = nullptr;
};

// ----------------------------------------------------------------------------

class MainFindEnt : public QLineEdit
{
    Q_OBJECT

public:
    MainFindEnt(MainSearch * search, QWidget * parent);

private slots:
private:
    virtual void keyPressEvent(QKeyEvent *e) override;
    virtual void focusInEvent(QFocusEvent *e) override;
    virtual void focusOutEvent(QFocusEvent *e) override;

private:
    MainSearch  * const m_search;
};

// ----------------------------------------------------------------------------

class HiglDef
{
public:
    QString m_pat;
    bool  m_opt_regexp;
    bool  m_opt_case;

    QTextCharFormat m_fmt;

    QRgb        m_bgCol;  // 0 means invalid
    QRgb        m_fgCol;  // 0 means invalid
    bool        m_bold;
    bool        m_underline;
    bool        m_overstrike;
    // relief: "", raised, sunken, ridge, groove
    // relief borderwidth: 1,2,...,9
    // spacing: 0,1,2,...
};

class Highlighter
{
public:
    Highlighter() {}
    void connectWidgets(MainText * textWid);
    void addSearchInc(QTextCursor& c, QRgb col);
    void addSearchHall(QTextCursor& c, QRgb col);
    void removeInc(QTextDocument * doc);
    void removeHall(QTextDocument * doc, QRgb col);
    void clear(QTextDocument * doc);

    void highlightInit();
    void highlightInitBg(int pat_idx, int line, int loop_cnt);
    int  highlightLines(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line);
    void highlightAll(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line=0, int loop_cnt=0);
    void highlightVisible(const QString& pat, int tagnam, bool opt_regexp, bool opt_case);
    void highlightYviewCallback(double frac1, double frac2);
    void highlightYviewRedirect();
    //void highlightConfigure(int tagname, w);
    void searchHighlightClear();
    void searchHighlightReset();
    void searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case);
    void searchHighlightAll(const QString& pat, int tagnam, bool opt_regexp, bool opt_case, int line=0, int loop_cnt=0);

    static const int TAG_NAME_FIND = 1;

private:
    void redraw(QTextDocument * doc, int blkNum);

private:
    MainText    * m_mainText = nullptr;
    QProgressBar* m_hipro = nullptr;

    std::multimap<int,QRgb> m_tags;
    int           m_findIncPos = -1;

    std::vector<HiglDef> patlist;

    static const bool block_bg_tasks = false;  //TODO
    ATimer      * tid_high_init = nullptr;
    ATimer      * tid_search_hall = nullptr;
    QString       tlb_cur_hall_str;
    bool          tlb_cur_hall_regexp = false;
    bool          tlb_cur_hall_case = false;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

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
    MainText(MainWin * mainWin, MainSearch * search, QWidget * parent);
    void cursorJumpPushPos();
    void cursorJumpStackReset();
    void keyCmdClear();

    bool findInBlocks(const QString &patStr, int from, bool is_fwd, bool opt_regex,
                      bool opt_case, int& matchPos, int& matchLen);

private slots:

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
    wchar_t       last_inline_char = 0;
    int           last_inline_dir = 0;
    wchar_t       last_key_char = 0;
    std::unordered_map<KeySet,const std::function<void()>,KeySetHash<KeySet>> m_keyCmdText;
    std::unordered_map<int,const std::function<void()>> m_keyCmdCtrl;
    std::vector<JumpPos> cur_jump_stack;
    int           cur_jump_idx = -1;
};

// ----------------------------------------------------------------------------

class SearchPar
{
public:
    SearchPar(const QString& pat, bool opt_regexp, bool opt_case)
        : m_pat(pat)
        , m_opt_regexp(opt_regexp)
        , m_opt_case(opt_case)
    {
    }
    QString m_pat;
    bool  m_opt_regexp;
    bool  m_opt_case;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class MainSearch : public QWidget
{
    Q_OBJECT

public:
    MainSearch(MainWin * mainWin);
    void connectWidgets(MainText    * textWid,
                        MainFindEnt * f2_e,
                        QCheckBox   * f2_hall,
                        QCheckBox   * f2_mcase,
                        QCheckBox   * f2_regexp);
    QJsonObject getRcValues();
    void setRcValues(const QJsonObject& obj);
    void searchAll(bool raiseWin, int direction);
    bool searchNext(bool isFwd);
    void searchEnter(bool is_fwd, QWidget * parent = nullptr);
    void searchOptToggleHall();
    void searchOptToggleRegExp();
    void searchOptToggleCase();
    void searchHighlightSettingChange();
    void searchHighlightClear();
    void searchReset();
    void searchInit();
    void searchLeave();
    void searchAbort();
    void searchReturn();
    void searchVarTrace(const QString &text);
    void searchIncrement(bool is_fwd, bool is_changed);
    void searchAddHistory(const QString& txt, bool is_re, bool use_case);
    void searchBrowseHistory(bool is_up);
    int  searchHistoryComplete(int step);
    void searchRemoveFromHistory();
    void searchComplete();
    void searchCompleteLeft();
    void searchWord(bool is_fwd);

private slots:
private:
    void searchBackground(const QString& pat, bool is_fwd, bool opt_regexp, bool opt_case,
                          int start, bool is_changed,
                          const std::function<void(QTextCursor&)>& callback);
    bool searchAtomic(const QString& pat, bool opt_regexp, bool opt_case, bool is_fwd, bool is_changed);
    void searchHighlightUpdateCurrent();
    void searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case);
    void searchHandleMatch(QTextCursor& match, const QString& pat,
                           bool opt_regexp, bool opt_case, bool is_changed);
    void searchHandleNoMatch(const QString& pat, bool is_fwd);
    void searchIncMatch(QTextCursor& match, const QString& pat, bool is_fwd, bool is_changed);
    void searchEscapeSpecialChars(QString& word, bool is_re);
    bool searchExprCheck(const QString& pat, bool is_re, bool display);
    int  searchGetBase(bool is_fwd, bool is_init);
    void searchGetParams();

private:
    MainWin     * const m_mainWin;
    MainText    * m_mainText;
    MainFindEnt * m_f2_e;
    QCheckBox   * m_f2_hall;
    QCheckBox   * m_f2_mcase;
    QCheckBox   * m_f2_regexp;

    QTimer      * m_timSearchInc;
    QWidget     * tlb_last_wid = nullptr;
    bool          tlb_last_dir = true;
    QString       tlb_find;
    bool          tlb_regexp;
    bool          tlb_case;
    bool          tlb_hall;
    bool          tlb_find_focus = false;
    int           tlb_inc_base = -1;
    int           tlb_inc_xview = 0;
    int           tlb_inc_yview = 0;
    int           tlb_hist_pos = -1;
    QString       tlb_hist_prefix;
    std::vector<SearchPar> tlb_history;  // TODO store flags case&regexp
    static const uint TLB_HIST_MAXLEN = 50;
    static const int SEARCH_INC_DELAY = 100; // in ms
    Highlighter   m_higl;
};

// ----------------------------------------------------------------------------

class MainWin : public QMainWindow
{
    Q_OBJECT

public:
    MainWin(QApplication * app);
    ~MainWin();

    void keyCmdZoomFontSize(bool zoomIn);
    void LoadFile(const QString& fileName);

    QWidget * focusWidget();
    void showWarning(QWidget * widget, const QString& msg) { m_stline->showWarning(widget, msg); }
    void showError(QWidget * widget, const QString& msg) { m_stline->showWarning(widget, msg); }
    void clearMessage(QWidget * widget) { m_stline->clearMessage(widget); }
    void loadRcFile();
    void updateRcFile();
    void updateRcAfterIdle();
    void menuCmdDisplayLineNo();

private slots:
    void menuCmdReload(bool checked);
    void menuCmdFileOpen(bool checked);
    void menuCmdFileQuit(bool checked);
    void menuCmdSelectFont(bool checked);
    void menuCmdGotoLine(bool checked);
    void menuCmdAbout(bool checked);
    void toggleLineWrap(bool checked);

private:
    void closeEvent(QCloseEvent *event);
    void menuCmdDiscard(bool is_fwd);
    void discardContent();
    void populateMenus();

private:
    QApplication* const m_mainApp;
    QMenu       * m_menubar_ctrl;
    QMenu       * m_menubar_srch;
    QMenu       * m_menubar_mark;
    QMenu       * m_menubar_help;
    QFont         m_fontContent;
    MainText    * m_f1_t;
    StatusMsg   * m_stline;
    //Highlighter * m_higl;
    MainSearch  * m_search;
    QTimer      * m_timUpdateRc;
    qint64        m_tsUpdateRc;
    QString       m_curFileName;
};

#endif // _MAIN_WIN_H

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
class QStyle;
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

class SearchPar
{
public:
    SearchPar() : m_opt_regexp(false), m_opt_case(false) {}
    SearchPar(const QString& pat, bool opt_regexp, bool opt_case)
        : m_pat(pat)
        , m_opt_regexp(opt_regexp)
        , m_opt_case(opt_case)
    {
    }
    void reset() { m_pat.clear(); m_opt_regexp = false; m_opt_case = false; }

    QString m_pat;
    bool  m_opt_regexp;
    bool  m_opt_case;
};

class HiglFmtSpec
{
public:
    static constexpr unsigned INVALID_COLOR = QRgb(0x00000000);

    QRgb                m_bgCol = INVALID_COLOR;
    QRgb                m_fgCol = INVALID_COLOR;
    Qt::BrushStyle      m_bgStyle = Qt::NoBrush;
    Qt::BrushStyle      m_fgStyle = Qt::NoBrush;
    bool                m_bold = false;
    bool                m_italic = false;
    bool                m_underline = false;
    bool                m_overstrike = false;
    bool                m_outline = false;
    QString             m_font;
    // relief: "", raised, sunken, ridge, groove
    // relief borderwidth: 1,2,...,9
    // spacing: 0,1,2,...
};

using HiglId = unsigned;

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
    Highlighter();
    void connectWidgets(MainText * textWid);
    QJsonArray getRcValues();
    void setRcValues(const QJsonValue& val);
    void removeInc(QTextDocument * doc);
    const std::vector<HiglPat>& getPatList() const { return m_patList; }

    void highlightInit();
    void highlightAll(const HiglPat& pat, int line = 0, int loop_cnt = 0);
    void highlightVisible(const HiglPat& pat);
    void searchHighlightMatch(QTextCursor& match);
    void searchHighlightClear();
    void searchHighlightUpdate(const QString& pat, bool opt_regexp, bool opt_case, bool onlyVisible);
    void searchHighlightAll(const HiglPat& pat, int line = 0, int loop_cnt = 0);

    static const int TAG_NAME_FIND = 1;

private:
    void addSearchInc(QTextCursor& sel);
    void addSearchHall(QTextCursor& sel, const QTextCharFormat& fmt, HiglId id);
    void removeHall(QTextDocument * doc, HiglId id);
    void redraw(QTextDocument * doc, int blkNum);
    void clearAll(QTextDocument * doc);
    const QTextCharFormat* getFmtById(HiglId id);

    void configFmt(QTextCharFormat& fmt, const HiglFmtSpec& fmtSpec);
    void addPattern(const SearchPar& srch, const HiglFmtSpec& fmtSpec);
    void highlightInitBg(int pat_idx, int line = 0, int loop_cnt = 0);
    int  highlightLines(const HiglPat& pat, int line);
    void highlightYviewRedirect();
    void highlightYviewCallback(int value);

private:
    MainText    * m_mainText = nullptr;
    QProgressBar* m_hipro = nullptr;

    std::multimap<int,HiglId> m_tags;
    int           m_findIncPos = -1;

    std::vector<HiglPat> m_patList;
    HiglPat       m_hallPat;
    HiglId        m_lastId = 0;
    bool          m_hallPatComplete = false;
    int           m_hallYview = 0;
    bool          m_yViewRedirected = false;  // TODO obsolete

    static const bool block_bg_tasks = false;  //TODO
    ATimer      * tid_high_init = nullptr;
    ATimer      * tid_search_hall = nullptr;
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

    QTextCursor findInDoc(const QString& pat, bool opt_regexp, bool opt_case, bool is_fwd, int start_pos);
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

class MainSearch : public QWidget
{
    Q_OBJECT

public:
    MainSearch(MainWin * mainWin);
    void connectWidgets(MainText    * textWid,
                        Highlighter * higl,
                        MainFindEnt * f2_e,
                        QCheckBox   * f2_hall,
                        QCheckBox   * f2_mcase,
                        QCheckBox   * f2_regexp);
    QJsonObject getRcValues();
    void setRcValues(const QJsonObject& obj);
    void searchAll(bool raiseWin, int direction);
    bool searchNext(bool isFwd);
    void searchFirst(bool is_fwd, const std::vector<SearchPar>& patList);
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
    QTextCursor findInDoc(const QString& pat, bool opt_regexp, bool opt_case, bool is_fwd, int start_pos);
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
    Highlighter * m_higl;
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

    QWidget * focusWidget() const;
    QStyle * getAppStyle() const;
    const QFont& getFontContent() const { return m_fontContent; }
    const QColor& getFgColDefault() const;
    const QColor& getBgColDefault() const;
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
    void menuCmdSearchEdit(bool checked);
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
    MainText    * m_f1_t;
    StatusMsg   * m_stline;
    MainSearch  * m_search;
    Highlighter   m_higl;
    QTimer      * m_timUpdateRc;
    qint64        m_tsUpdateRc;
    QFont         m_fontContent;
    QString       m_curFileName;
};

#endif // _MAIN_WIN_H

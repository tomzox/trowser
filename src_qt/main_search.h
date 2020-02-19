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
#ifndef _MAIN_SEARCH_H
#define _MAIN_SEARCH_H

#include <QMainWindow>
#include <QTextCursor>
#include <QLineEdit>
#include <QTimer>

#include <vector>
#include <set>

class QCheckBox;
class QProgressBar;
class QJsonObject;

class MainWin;
class MainText;
class MainSearch;
class Highlighter;
class BgTask;

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

// ----------------------------------------------------------------------------

class MainFindEnt : public QLineEdit
{
    Q_OBJECT

public:
    MainFindEnt(MainSearch * search, QWidget * parent);

private:
    virtual void keyPressEvent(QKeyEvent *e) override;
    virtual void focusInEvent(QFocusEvent *e) override;
    virtual void focusOutEvent(QFocusEvent *e) override;

private:
    MainSearch  * const m_search;
};

// ----------------------------------------------------------------------------

class MainSearch : public QWidget
{
    Q_OBJECT

public:
    MainSearch(MainWin * mainWin);
    ~MainSearch();
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
    void searchEnterOpt(const SearchPar& pat);
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
    bool searchExprCheck(const QString& pat, bool is_re, bool display);
    void highlightFixedLine(int line);
    SearchPar getCurSearchParams();
    const std::vector<SearchPar>& getHistory() const { return tlb_history; }
    void removeFromHistory(const std::set<int>& excluded);

private:
    void searchBackground(const SearchPar& par, bool is_fwd, int start, bool is_changed,
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

    BgTask      * m_timSearchInc;
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
    std::vector<SearchPar> tlb_history;
    static const uint TLB_HIST_MAXLEN = 50;
    static const int SEARCH_INC_DELAY = 100; // in ms
};

#endif /* _MAIN_SEARCH_H */
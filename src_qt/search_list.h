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
#ifndef _SEARCH_LIST_H
#define _SEARCH_LIST_H

#include <QWidget>
#include <QMainWindow>
#include <QItemSelection>

#include <utility>        // pair
#include <set>

#include "main_search.h"  // for SearchPar

class QTableView;
class QTextCursor;
class QProgressBar;
class QJsonObject;

class MainWin;
class MainText;
class MainSearch;
class Highlighter;
class Bookmarks;
class SearchListView;
class SearchListModel;
class HighlightViewDelegate;
class SearchListDrawBok;
class SearchListUndo;
class ATimer;

class SearchList : public QMainWindow
{
    Q_OBJECT

public:
    static void connectWidgets(MainWin * mainWin, MainSearch * search, MainText * mainText,
                               Highlighter * higl, Bookmarks * bookmarks);
    static QJsonObject getRcValues();
    static void setRcValues(const QJsonValue& val);
    static SearchList* getInstance(bool raiseWin = true);
    static bool isDialogOpen();
    static void openDialog(bool raiseWin);
    static void matchView(int line);
    static void extUndo();
    static void extRedo();
    static void signalBookmarkLine(int line = -1);
    static void signalHighlightLine(int line);
    static void signalHighlightReconfigured();
    static void adjustLineNums(int top_l, int bottom_l);

    // external interfaces
    void copyCurrentLine(bool doAdd);
    void searchMatches(bool do_add, int direction, const SearchPar& par);
    void searchMatches(bool do_add, int direction, const std::vector<SearchPar>& pat_list);

private:
    // constructor can only be invoked via the static interface
    SearchList();
    ~SearchList();

    void populateMenus();
    virtual void closeEvent(QCloseEvent *) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void showContextMenu(const QPoint& pos);

    void cmdClose(bool);
    void editMenuAboutToShow();
    void configureColumnVisibility();
    void cmdToggleShowLineNumber(bool checked);
    void cmdToggleShowLineDelta(bool checked);
    void cmdSetLineIdxRoot(bool checked);
    void cmdSetDeltaColRoot(bool);
    void cmdClearAll();
    void cmdRemoveSelection();
    void cmdToggleBookmark(bool);
    void cmdUndo();
    void cmdRedo();
    void bgUndoRedoLoop(bool isRedo, int origCount);
    void addMatches(int direction);
    void removeMatches(int direction);
    void startSearchAll(const std::vector<SearchPar>& pat_list, bool do_add, int direction);
    void bgSearchLoop(const std::vector<SearchPar> pat_list, bool do_add, int direction, int line, int pat_idx, int loop_cnt);
    void searchProgress(int percent);
    bool searchAbort(bool doWarn __attribute__((unused)) = true) { return true; } //dummy,TODO
    using ListViewAnchor = std::pair<bool,int>;
    ListViewAnchor&& getViewAnchor();
    void seeViewAnchor(ListViewAnchor& anchor);
    void matchViewInt(int line, int idx = -1);
    void adjustLineNumsInt(int top_l, int bottom_l);
    void saveFile(const QString& fileName, bool lnum_only);
    void cmdSaveFileAs(bool lnum_only);
    void cmdLoadFrom(bool);
    bool loadLineList(const QString& fileName, std::set<int>& line_list);
    void cmdDisplayStats(bool);

private:
    static Highlighter * s_higl;
    static MainSearch  * s_search;
    static MainWin     * s_mainWin;
    static MainText    * s_mainText;
    static Bookmarks   * s_bookmarks;

    static SearchList * s_instance;
    static QByteArray   s_winGeometry;
    static QByteArray   s_winState;
    static QString      s_prevFileName;

    SearchListView    * m_table = nullptr;
    SearchListModel   * m_model = nullptr;
    HighlightViewDelegate * m_draw = nullptr;
    SearchListDrawBok * m_drawBok = nullptr;
    SearchListUndo    * m_undo = nullptr;
    QProgressBar      * m_hipro = nullptr;
    QAction           * m_menActUndo = nullptr;
    QAction           * m_menActRedo = nullptr;
    QAction           * m_actShowLineDelta = nullptr;
    ATimer            * tid_search_list = nullptr;
    int                 m_ignoreSelCb = -1;

    bool                m_showLineIdx = false;
    bool                m_showLineDelta = false;
    //bool                m_showFrameIdx;
    //bool                m_tickFrameDelta;
};

#endif /* _SEARCH_LIST_H */

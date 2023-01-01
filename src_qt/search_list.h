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
#include <QMessageBox>

#include <utility>        // pair
#include <set>

#include "main_search.h"  // for SearchPar
#include "parse_frame.h"

class QTableView;
class QTextCursor;
class QProgressBar;
class QJsonObject;

class MainWin;
class MainText;
class MainSearch;
class Highlighter;
class Bookmarks;
class StatusLine;
class SearchListView;
class SearchListModel;
class HighlightViewDelegate;
class SearchListDrawBok;
class SearchListUndo;
class BgTask;
class DlgParser;

// ----------------------------------------------------------------------------

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

    // static notification interfaces (ignored when dialog window is not open)
    static void matchView(int line);
    static void extUndo();
    static void extRedo();
    static void signalBookmarkLine(int line = -1);
    static void signalHighlightLine(int line);
    static void signalHighlightReconfigured();
    static void adjustLineNums(int top_l, int bottom_l);

    // external interfaces (invoked via getInstance() result)
    void copyCurrentLine(bool doAdd);
    void searchMatches(bool do_add, int direction, const SearchPar& par);
    void searchMatches(bool do_add, int direction, const std::vector<SearchPar>& patList);

private:
    // constructor can only be invoked via the static interface
    SearchList();
    virtual ~SearchList();

    void populateMenus();
    void configureCustomMenuActions();
    virtual void closeEvent(QCloseEvent *) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void showContextMenu(const QPoint& pos);

    void cmdClose(bool);
    void mainFontChanged();
    void mainDocNameChanged();
    void editMenuAboutToShow();
    void configureColumnVisibility();
    void cmdToggleShowLineNumber(bool checked);
    void cmdToggleShowLineDelta(bool checked);
    void cmdToggleShowCustom(ParseColumnFlags col, bool checked);
    void cmdSetLineIdxRoot(bool checked);
    void cmdSetCustomColRoot(ParseColumnFlags col);
    void cmdSetDeltaColRoot(bool);
    void cmdToggleSearchHighlight(bool checked);
    void cmdToggleBookmarkMarkup(bool checked);
    void cmdClearAll();
    void cmdRemoveSelection();
    void cmdToggleBookmark();
    void cmdUndo();
    void cmdRedo();
    int  searchAtomicInList(const SearchPar& pat, int textPos, bool is_fwd);
    void cmdSearchNext(bool is_fwd);
    void cmdNewSearch(bool is_fwd);
    void bgUndoRedoLoop(bool isRedo, int origCount);
    void addMatches(int direction);
    void removeMatches(int direction);
    void startSearchAll(const std::vector<SearchPar>& patList, bool do_add, int direction);
    void bgSearchLoop(const std::vector<SearchPar> patList, bool do_add, int direction, int line, int pat_idx);
    void searchProgress(int percent);
    bool searchAbort(bool doWarn = true);
    void closeAbortDialog();
    using ListViewAnchor = std::pair<bool,int>;
    ListViewAnchor getViewAnchor();
    void seeViewAnchor(ListViewAnchor& anchor);
    void bgCopyLoop(bool doAdd, int startLine, int endLine, int line);
    void matchViewInt(int line, int idx = -1);
    void adjustLineNumsInt(int top_l, int bottom_l);
    void saveFile(const QString& fileName, bool lnum_only);
    void cmdSaveFileAs(bool lnum_only);
    void cmdLoadFrom(bool);
    bool loadLineList(const QString& fileName, std::set<int>& line_list);
    void cmdOpenParserConfig(bool);
    void signalDlgParserClosed(bool changed);
    void cmdDisplayStats();

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
    static ParseSpec    s_parseSpec;

    struct SearchListShowCfg
    {
        bool            srchHall = true;
        bool            bookmarkMarkup = false;
        bool            lineIdx = false;
        bool            lineDelta = false;
        ParseColumns    custom;
    };
    static SearchListShowCfg s_prevShowCfg;

    SearchListView    * m_table = nullptr;
    SearchListModel   * m_model = nullptr;
    HighlightViewDelegate * m_draw = nullptr;
    SearchListDrawBok * m_drawBok = nullptr;
    SearchListUndo    * m_undo = nullptr;
    QProgressBar      * m_hipro = nullptr;
    StatusLine        * m_stline = nullptr;
    QMessageBox       * m_abortDialog = nullptr;
    DlgParser         * m_dlgParser = nullptr;
    SearchListShowCfg   m_showCfg;

    QAction           * m_menActUndo = nullptr;
    QAction           * m_menActRedo = nullptr;
    QAction           * m_menActAbort = nullptr;
    QAction           * m_menActRemove = nullptr;
    QAction           * m_actShowLineDelta = nullptr;
    QAction           * m_actShowCustomVal = nullptr;
    QAction           * m_actShowCustomValDelta = nullptr;
    QAction           * m_actShowCustomFrm = nullptr;
    QAction           * m_actShowCustomFrmDelta = nullptr;
    QAction           * m_actRootCustomValDelta = nullptr;
    QAction           * m_actRootCustomFrmDelta = nullptr;

    BgTask            * tid_search_list = nullptr;
    int                 m_ignoreSelCb = -1;

    static const int    COPY_LOOP_CHUNK_SZ = 40000;
    static const int    UNDO_LOOP_CHUNK_SZ = 10000;
    static const int    SEARCH_LOOP_TIME_LIMIT_MS = 100;
    static const int    MAX_SELECTION_SZ = 1000;
};

#endif /* _SEARCH_LIST_H */

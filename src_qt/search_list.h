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

#include <main_search.h>  // for SearchPar

class QTableView;
class QTextCursor;
class QJsonObject;

class Highlighter;
class MainSearch;
class MainText;
class MainWin;
class SearchListModel;
class SearchListDraw;

class SearchList : public QMainWindow
{
    Q_OBJECT

public:
    static SearchList * s_instance;
    static QByteArray s_winGeometry;
    static QByteArray s_winState;
    static std::vector<QRgb> s_defaultColPalette;

    static void connectWidgets(Highlighter * higl, MainSearch * search, MainWin * mainWin, MainText * mainText);
    static void openDialog(bool raiseWin);
    static QJsonObject getRcValues();
    static void setRcValues(const QJsonValue& val);
    static SearchList* getInstance(bool raiseWin = true);
    static void matchView(int line);

    // external interfaces
    void copyCurrentLine();
    void addMatches(int direction);
    void removeMatches(int direction);
    void searchMatches(bool do_add, int direction, const SearchPar& par);

private:
    // constructor can only be invoked via the static interface
    SearchList();
    ~SearchList();

    virtual void closeEvent(QCloseEvent *) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

    bool searchAbort(bool doWarn __attribute__((unused)) = true) { return true; } //dummy,TODO
    void addMainSelection(QTextCursor& c);
    void matchViewInt(int line, int idx = -1);
    void displayStats(bool);

private:
    static Highlighter * s_higl;
    static MainSearch  * s_search;
    static MainWin     * s_mainWin;
    static MainText    * s_mainText;

    QTableView        * m_table = nullptr;
    SearchListModel   * m_model = nullptr;
    SearchListDraw    * m_draw = nullptr;

    bool dlg_srch_highlight = false;
};

#endif /* _SEARCH_LIST_H */

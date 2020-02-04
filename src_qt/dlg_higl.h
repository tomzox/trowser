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

#include <QWidget>
#include <QMainWindow>
#include <QItemSelection>

class QTableView;
class QPushButton;
class Highlighter;
class MainSearch;
class MainWin;
class DlgHiglModel;
class DlgHidlFmtDraw;

class DlgHigl : public QMainWindow
{
    Q_OBJECT

public:
    static DlgHigl * s_instance;
    static QByteArray s_winGeometry;
    static QByteArray s_winState;
    static void openDialog(Highlighter * higl, MainSearch * search, MainWin * mainWin);
private:
    DlgHigl(Highlighter * higl, MainSearch * search, MainWin * mainWin);
    DlgHigl(const DlgHigl&) = delete;
    DlgHigl& operator=(const DlgHigl&) = delete;
    ~DlgHigl();

    void cmdSearch(bool is_fwd);
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void closeEvent(QCloseEvent *);

private:
    Highlighter * const m_higl;
    MainSearch * const  m_search;
    MainWin * const     m_mainWin;
    QTableView        * m_table;
    DlgHiglModel      * m_model;
    DlgHidlFmtDraw    * m_fmtDelegate;

    QPushButton       * m_f1_del;
    QPushButton       * m_f1_up;
    QPushButton       * m_f1_down;
    QPushButton       * m_f1_fmt;
    QPushButton       * m_f2_bn;
    QPushButton       * m_f2_bp;
    QPushButton       * m_f2_ball;
    QPushButton       * m_f2_ballb;
    QPushButton       * m_f2_balla;
};

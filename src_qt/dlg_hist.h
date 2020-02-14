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
#ifndef _DLG_HIST_H
#define _DLG_HIST_H

#include <QWidget>
#include <QMainWindow>
#include <QItemSelection>
#include <QSet>

class QTableView;
class QTextCursor;
class QDialogButtonBox;
class QPushButton;
class QAbstractButton;
class QModelIndex;
class QJsonObject;

class MainSearch;
class MainText;
class MainWin;
class DlgHistoryView;
class DlgHistoryModel;

class DlgHistory : public QMainWindow
{
    Q_OBJECT

public:
    static void connectWidgets(MainSearch * search, MainWin * mainWin, MainText * mainText);
    static QJsonObject getRcValues();
    static void setRcValues(const QJsonValue& val);
    static DlgHistory* getInstance();
    static void openDialog();
    static void signalHistoryChanged();

private:
    // constructor can only be invoked via the static interface
    DlgHistory();
    ~DlgHistory();

    virtual void closeEvent(QCloseEvent *) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void refreshContents();
    void showContextMenu(const QPoint& pos);
    void cmdClose(bool);
    void cmdButton(QAbstractButton * button);
    void cmdRemove(bool);
    void cmdCopyToMain(bool);
    void mouseTrigger(const QModelIndex& index);
    void cmdSearch(bool is_fwd);
    void cmdSearchList(int direction);

private:
    static MainSearch  * s_search;
    static MainWin     * s_mainWin;
    static MainText    * s_mainText;

    static DlgHistory * s_instance;
    static QByteArray   s_winGeometry;
    static QByteArray   s_winState;

    DlgHistoryView    * m_table = nullptr;
    DlgHistoryModel   * m_model = nullptr;

    QDialogButtonBox  * m_cmdButs = nullptr;
    QPushButton       * m_f2_bn = nullptr;
    QPushButton       * m_f2_bp = nullptr;
    QPushButton       * m_f2_ball = nullptr;
    QPushButton       * m_f2_ballb = nullptr;
    QPushButton       * m_f2_balla = nullptr;

    QSet<QString>       m_selPats;
};

#endif /* _DLG_HIST_H */

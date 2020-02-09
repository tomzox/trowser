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
#ifndef _DLG_HIGL_H
#define _DLG_HIGL_H

#include <QWidget>
#include <QMainWindow>
#include <QItemSelection>

class QTableView;
class QPushButton;
class QDialogButtonBox;
class QAbstractButton;
class Highlighter;
class MainSearch;
class MainWin;
class DlgHiglModel;
class DlgHidlFmtDraw;
class QJsonObject;

class DlgHigl : public QMainWindow
{
    Q_OBJECT

public:
    static DlgHigl * s_instance;
    static QByteArray s_winGeometry;
    static QByteArray s_winState;
    static std::vector<QRgb> s_defaultColPalette;

    static void openDialog(Highlighter * higl, MainSearch * search, MainWin * mainWin);
    static QJsonObject getRcValues();
    static void setRcValues(const QJsonValue& val);

private:
    // constructor can only be invoked via the static interface
    DlgHigl(Highlighter * higl, MainSearch * search, MainWin * mainWin);
    DlgHigl(const DlgHigl&) = delete;
    DlgHigl& operator=(const DlgHigl&) = delete;
    ~DlgHigl();

    virtual void closeEvent(QCloseEvent *) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void cbDataChanged(const QModelIndex &, const QModelIndex &);
    void showContextMenu(const QPoint& pos);
    void cmdButton(QAbstractButton * button);
    void cmdShiftUp(bool);
    void cmdShiftDown(bool);
    void cmdSearch(bool is_fwd);
    void cmdSearchList(int direction);
    void cmdAdd(bool);
    void cmdCopyToMain(bool);
    void cmdCopyFromMain(bool);
    void cmdRemove(bool);
    void cmdChangeBgColor(bool);
    void cmdChangeFgColor(bool);
    void cmdToggleFontUnderline(const QModelIndex& index, bool checked);
    void cmdToggleFontBold(const QModelIndex& index, bool checked);
    void cmdToggleFontItalic(const QModelIndex& index, bool checked);
    void cmdToggleFontOverstrike(const QModelIndex& index, bool checked);
    void cmdResetFont(const QModelIndex& index);
    void cmdChangeFont(bool);

private:
    Highlighter * const m_higl;
    MainSearch * const  m_search;
    MainWin * const     m_mainWin;
    QTableView        * m_table = nullptr;
    DlgHiglModel      * m_model = nullptr;
    DlgHidlFmtDraw    * m_fmtDelegate = nullptr;

    QDialogButtonBox  * m_cmdButs = nullptr;
    QPushButton       * m_f1_del = nullptr;
    QPushButton       * m_f1_up = nullptr;
    QPushButton       * m_f1_down = nullptr;
    QPushButton       * m_f1_fmt = nullptr;
    QPushButton       * m_f2_bn = nullptr;
    QPushButton       * m_f2_bp = nullptr;
    QPushButton       * m_f2_ball = nullptr;
    QPushButton       * m_f2_ballb = nullptr;
    QPushButton       * m_f2_balla = nullptr;
};

#endif /* _DLG_HIGL_H */

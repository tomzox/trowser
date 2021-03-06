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
 * Module description:
 *
 * This module implements the "search history" dialog. The dialog shows
 * the 40 most-recently used search expressions (order last-used at top).
 * The main intention is allowing to quickly repeat one or more searches
 * from the list. For each entry the pattern and reg-exp/case options
 * are shown. The options can be edited in the table.
 *
 * ----------------------------------------------------------------------------
 */

#include <QWidget>
#include <QApplication>
#include <QToolBar>
#include <QTableView>
#include <QAbstractItemModel>
#include <QItemSelection>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include <cstdio>
#include <string>
#include <vector>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "config_file.h"
#include "status_line.h"
#include "search_list.h"
#include "dlg_history.h"

// ----------------------------------------------------------------------------

/**
 * This class implements the "model" associated with the search history "view".
 * The model is a state-less wrapper around the external history list. This
 * means data queries are simply forwarded. Modifications of the underlying
 * list via the model are not supported. Instead, when the list is changed
 * externally, the view receives a signal and has the model to emit a
 * dataChanged() signal for forcing a complete redraw.
 */
class DlgHistoryModel : public QAbstractItemModel
{
public:
    enum TblColIdx { COL_IDX_REGEXP, COL_IDX_CASE, COL_IDX_PAT, COL_COUNT };

    DlgHistoryModel(MainSearch * search)
        : m_history(search->getHistory())
    {
    }
    virtual ~DlgHistoryModel()
    {
    }
    virtual QModelIndex index(int row, int column, const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        return createIndex(row, column);
    }
    virtual QModelIndex parent(const QModelIndex&) const override
    {
        return QModelIndex();
    }
    virtual int rowCount(const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        const std::vector<SearchPar>& hist = m_history->getHistory();
        return hist.size();
    }
    virtual int columnCount(const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        return COL_COUNT;
    }
    virtual Qt::ItemFlags flags(const QModelIndex& index __attribute__((unused))) const override
    {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    //virtual bool setData(const QModelIndex& index, const QVariant &value, int role = Qt::EditRole) override;
    //virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    //virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    const SearchPar& getSearchPar(const QModelIndex& index) const;
    const SearchPar& getSearchPar(int row) const;
    void forceRedraw();

private:
    SearchHistory * const m_history;
};

QVariant DlgHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
                case COL_IDX_PAT: return QVariant("Search string / pattern");
                case COL_IDX_REGEXP: return QVariant("Reg.Exp.?");
                case COL_IDX_CASE: return QVariant("Match case?");
                default: break;
            }
            qWarning("Invalid index:%d for headerData()", section);
        }
        else if (role == Qt::ToolTipRole)
        {
            switch (section)
            {
                case COL_IDX_PAT: return QVariant("<P>Shows the search text or regular expression. (Double-click to start editing the text.)</P>");
                case COL_IDX_REGEXP: return QVariant("<P>Shows \"true\" if the search string is a Regular Expression (Perl syntax), or \"false\" if it is a plain sub-string.</P>");
                case COL_IDX_CASE: return QVariant("<P>Shows \"true\" if text matches only when in the same case as in the given pattern.</P>");
                default: break;
            }
        }
    }
    else
    {
        if (role == Qt::DisplayRole)
        {
            const std::vector<SearchPar>& hist = m_history->getHistory();
            if (size_t(section) < hist.size())
                return QVariant(QString::number(section + 1));
        }
    }
    return QVariant();
}

QVariant DlgHistoryModel::data(const QModelIndex &index, int role) const
{
    const std::vector<SearchPar>& hist = m_history->getHistory();

    if (   (role == Qt::DisplayRole)
        && ((size_t)index.row() < hist.size()))
    {
        const SearchPar& el = hist.at(index.row());
        switch (index.column())
        {
            case COL_IDX_PAT: return QVariant(el.m_pat);
            case COL_IDX_REGEXP: return QVariant(el.m_opt_regexp);
            case COL_IDX_CASE: return QVariant(el.m_opt_case);
            case COL_COUNT:
                break;
        }
    }
    return QVariant();
}

const SearchPar& DlgHistoryModel::getSearchPar(const QModelIndex& index) const
{
    const std::vector<SearchPar>& hist = m_history->getHistory();
    return hist.at(index.row());
}

const SearchPar& DlgHistoryModel::getSearchPar(int row) const
{
    const std::vector<SearchPar>& hist = m_history->getHistory();
    return hist.at(row);
}

void DlgHistoryModel::forceRedraw()
{
    emit layoutChanged();
}

// ----------------------------------------------------------------------------

/**
 * This helper class overloads the standard QTableView class to allow
 * overriding several key bindings.
 */
class DlgHistoryView : public QTableView
{
public:
    DlgHistoryView(QWidget *parent)
        : QTableView(parent)
    {
    }
    virtual void keyPressEvent(QKeyEvent *e) override;
};

void DlgHistoryView::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        // Firstly make Home key behave same with and without CTRL
        // Secondly correct behavior so that target line is selected
        case Qt::Key_Home:
            if (model()->rowCount() != 0)
            {
                // following updates selection, anchor (for key up/down) and view at once
                // whereas scrollTo() and selectionModel()->select() fail to update anchor
                // NOTE: given column must not be hidden, else scrollTo() does nothing
                QModelIndex midx = model()->index(0, 0);
                this->setCurrentIndex(midx);
            }
            break;

        case Qt::Key_End:
            if (int count = model()->rowCount())
            {
                QModelIndex midx = model()->index(count - 1, 0);
                this->setCurrentIndex(midx);
            }
            break;

        default:
            QTableView::keyPressEvent(e);
            break;
    }
}


// ----------------------------------------------------------------------------

/**
 * This function creates the dialog window. The window consists of a table
 * showing the search history, and two toolbars.
 */
DlgHistory::DlgHistory()
{
    auto central_wid = new QWidget();
        setCentralWidget(central_wid);
    auto layout_top = new QVBoxLayout(central_wid);

    m_model = new DlgHistoryModel(s_search);

    // main dialog component: table view, showing the history list
    m_table = new DlgHistoryView(central_wid);
        m_table->setModel(m_model);
        m_table->horizontalHeader()->setSectionResizeMode(DlgHistoryModel::COL_IDX_PAT, QHeaderView::Stretch);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, &QAbstractItemView::customContextMenuRequested, this, &DlgHistory::showContextMenu);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DlgHistory::selectionChanged);
        layout_top->addWidget(m_table);

    // Standard command buttons at the bottom of the dialog window: only "Close"
    m_cmdButs = new QDialogButtonBox(QDialogButtonBox::Close,
                                     Qt::Horizontal, central_wid);
        connect(m_cmdButs, &QDialogButtonBox::clicked, this, &DlgHistory::cmdButton);
        layout_top->addWidget(m_cmdButs);

    // Status line overlay: used to display notification text
    m_stline = new StatusLine(m_table);

    // Find toolbar: functionality is equivalent to main window and highlight editor dialog
    auto f2 = new QToolBar("Find", this);
        f2->setObjectName("DlgHistory::Find"); // for saveState
        addToolBar(Qt::LeftToolBarArea, f2);
    auto f2_l = new QLabel("Find:", f2);
        f2->addWidget(f2_l);
    m_f2_bn = new QPushButton("&Next", f2);
        m_f2_bn->setToolTip("<P>Start searching forward in the main window for the first occurence of one of the selected patterns.</P>");
        m_f2_bn->setEnabled(false);
        m_f2_bn->setFocusPolicy(Qt::TabFocus);
        connect(m_f2_bn, &QPushButton::clicked, [=](){ cmdSearch(true); });
        f2->addWidget(m_f2_bn);
    m_f2_bp = new QPushButton("&Prev.", f2);
        m_f2_bp->setToolTip("<P>Start searching backward in the main window for the first occurence of one of the selected patterns.</P>");
        m_f2_bp->setEnabled(false);
        m_f2_bp->setFocusPolicy(Qt::TabFocus);
        connect(m_f2_bp, &QPushButton::clicked, [=](){ cmdSearch(false); });
        f2->addWidget(m_f2_bp);
    m_f2_ball = new QPushButton("&All", f2);
        m_f2_ball->setToolTip("<P>Open a new window showing all lines of the main text that contain a match on one of the selected patterns.</P>");
        m_f2_ball->setEnabled(false);
        m_f2_ball->setFocusPolicy(Qt::TabFocus);
        connect(m_f2_ball, &QPushButton::clicked, [=](){ cmdSearchList(0); });
        f2->addWidget(m_f2_ball);
    m_f2_ballb = new QPushButton("All below", f2);
        m_f2_ballb->setToolTip("<P>Open a new window showing all lines of the main text below the currently selected line that contain a match on one of the selected patterns.</P>");
        m_f2_ballb->setEnabled(false);
        m_f2_ballb->setFocusPolicy(Qt::TabFocus);
        m_f2_ballb->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_N));
        connect(m_f2_ballb, &QPushButton::clicked, [=](){ cmdSearchList(1); });
        f2->addWidget(m_f2_ballb);
    m_f2_balla = new QPushButton("All above", f2);
        m_f2_balla->setToolTip("<P>Open a new window showing all lines of the main text above the currently selected line that contain a match on one of the selected patterns.</P>");
        m_f2_balla->setEnabled(false);
        m_f2_balla->setFocusPolicy(Qt::TabFocus);
        m_f2_balla->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_P));
        connect(m_f2_balla, &QPushButton::clicked, [=](){ cmdSearchList(-1); });
        f2->addWidget(m_f2_balla);

    // global keyboard shortcuts
    auto act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(act, &QAction::triggered, this, &DlgHistory::cmdRemove);
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Escape));
        connect(act, &QAction::triggered, this, &DlgHistory::cmdClose);
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Ampersand));
        connect(act, &QAction::triggered, [=](){ s_search->searchHighlightClear(); });
        central_wid->addAction(act);
    // note this can only be used as long as editing is disabled
    connect(m_table, &QTableView::doubleClicked, this, &DlgHistory::mouseTrigger);

    connect(s_search->getHistory(), &SearchHistory::historyChanged, this, &DlgHistory::refreshContents);
    connect(s_mainWin, &MainWin::documentNameChanged, this, &DlgHistory::mainDocNameChanged);
    mainDocNameChanged();  // set window title initially

    if (!s_winGeometry.isEmpty())
        this->restoreGeometry(s_winGeometry);
    if (!s_winState.isEmpty())
        this->restoreState(s_winState);

    m_table->setFocus(Qt::ShortcutFocusReason);
    this->show();
}


/**
 * Destructor: Freeing resources not automatically deleted via widget tree
 */
DlgHistory::~DlgHistory()
{
    delete m_model;
}

/**
 * This overriding function is called when the dialog window receives the close
 * event. The function stops background processes and destroys the class
 * instance to release all resources. The event is always accepted.
 */
void DlgHistory::closeEvent(QCloseEvent * event)
{
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();
    ConfigFile::updateRcAfterIdle();

    event->accept();

    s_instance->deleteLater();
    s_instance = nullptr;
}


/**
 * This slot is connected to context menu requests, as custom context menu is
 * configured for the table view. The function creates and executes context
 * menu.
 */
void DlgHistory::showContextMenu(const QPoint& pos)
{
    auto menu = new QMenu("Search history actions", this);
    auto sel = m_table->selectionModel()->selectedRows();

    auto act = menu->addAction("Remove selected entries");
        act->setEnabled(sel.size() != 0);
        connect(act, &QAction::triggered, this, &DlgHistory::cmdRemove);
    menu->addSeparator();

    act = menu->addAction("Copy to the search entry field & search next");
        act->setEnabled(sel.size() != 0);
        connect(act, &QAction::triggered, this, &DlgHistory::cmdCopyToMain);

    menu->exec(mapToGlobal(pos));
}

/**
 * This slot is bound to changes of the selection in the color tags list.
 */
void DlgHistory::selectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
    auto sel = m_table->selectionModel()->selectedRows();
    bool enabled = (sel.size() > 0);

    m_f2_bn->setEnabled(enabled);
    m_f2_bp->setEnabled(enabled);
    m_f2_ball->setEnabled(enabled);
    m_f2_ballb->setEnabled(enabled);
    m_f2_balla->setEnabled(enabled);

    // keep copy of selected patterns to maintain selection across list updates
    m_selPats.clear();
    for (auto midx : sel)
    {
        m_selPats.insert(m_model->getSearchPar(midx).m_pat);
    }
}

/**
 * This slot is connected to notification of main document file changes. This
 * is used to update the dialog window title. (Note before this event, the
 * dialog is informed separately about discarding the content of the previous
 * document.)
 */
void DlgHistory::mainDocNameChanged()
{
    const QString& fileName = s_mainWin->getFilename();
    if (!fileName.isEmpty())
        this->setWindowTitle("Search history - " + fileName);
    else
        this->setWindowTitle("Search history - trowser");
}


/**
 * This slot is bound to the main command buttons: In this dialog this is only
 * the "Close" button.
 */
void DlgHistory::cmdButton(QAbstractButton * button)
{
    if (button == m_cmdButs->button(QDialogButtonBox::Close))
    {
        QCoreApplication::postEvent(this, new QCloseEvent());
    }
}


/**
 * This slot is connected to the Escape-key shortcut, which is equivalent to
 * clicking "Close".
 */
void DlgHistory::cmdClose(bool)
{
    QCoreApplication::postEvent(this, new QCloseEvent());
}


// ----------------------------------------------------------------------------

/**
 * This slot is connected to changes in the externally managed search history
 * list (i.e. items removed/added or modified). Note this function also may be
 * called by actions initiated via the dialog, such as removal of history
 * entries.  The function first forces a complete redraw of the contents. Then
 * it will re-select all previously selected items (ie.e. those with the same
 * pattern string); the previous selection is known from a copy that is taken
 * immediately in the callback upon any change of selection.
 *
 * Note this special handling is necessary because by default the selection
 * model would keep selection at the same indices, which now may refer to
 * completely different items.
 */
void DlgHistory::refreshContents()
{
    m_model->forceRedraw();

    QItemSelection sel;
    QSet<QString>  newSelPats;
    QModelIndex first;
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        if (m_selPats.find(m_model->getSearchPar(row).m_pat) != m_selPats.end())
        {
            QModelIndex midx = m_model->index(row, 0);
            sel.select(midx, midx);
            newSelPats.insert(m_model->getSearchPar(row).m_pat);

            if (first.isValid() == false)
                first = midx;
        }
    }
    if (sel != m_table->selectionModel()->selection())
    {
        // Cannot rely on selection update below to invoke the callback, so update here directly;
        // Must be done first as list gets updated during following selection change.
        m_selPats = newSelPats;

        if (first.isValid())
            m_table->setCurrentIndex(first);
        m_table->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}


/**
 * This slot is connected to the Deleta shortcut and "Remove selected lines"
 * command in the history list's context menu.  All currently selected entries
 * are removed from the history.
 */
void DlgHistory::cmdRemove(bool)
{
    QItemSelectionModel * sel = m_table->selectionModel();
    if (sel->hasSelection())
    {
        std::set<int> excluded;
        for (auto& midx : sel->selectedRows())
        {
            excluded.insert(midx.row());
        }
        s_search->getHistory()->removeMultiple(excluded);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No expression selected");
}


/**
 * This slot is connected to the "Copy to search field" entry in the history
 * list's context menu. The function copies the search string and options to
 * the respective input fields and checkboxes respectively in the main window
 * and starts a search.  (Note an almost identical menu entry exists in the tag
 * list dialog.)
 */
void DlgHistory::cmdCopyToMain(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        s_search->searchEnterOpt( m_model->getSearchPar(sel.front()) );
    }
}


/**
 * This slot is connected to double-click mouse events occuring on an entry in
 * the history list. The function starts a search with the selected expression
 * in the main window.
 */
void DlgHistory::mouseTrigger(const QModelIndex &index)
{
    s_search->searchEnterOpt( m_model->getSearchPar(index) );
}


/**
 * This slot is bound to the "Next" and "Prev" buttons in the "Find" toolbar.
 * This function starts a search in the main text content in the given
 * direction for all of the currently selected search patterns. When more than
 * one pattern is selected, the cursor is moved to the first match on any of
 * the patterns.
 */
void DlgHistory::cmdSearch(bool is_fwd)
{
    QItemSelectionModel * sel = m_table->selectionModel();
    if (sel->hasSelection())
    {
        m_stline->clearMessage("search");

        std::vector<SearchPar> patList;
        for (auto& row : sel->selectedRows())
        {
            patList.push_back(m_model->getSearchPar(row));
        }

        bool found = s_search->searchFirst(is_fwd, patList);
        if (!found)
        {
            QString msg = QString("No match until ")
                            + (is_fwd ? "end" : "start")
                            + " of file"
                            + ((patList.size() <= 1) ? "" : " for any selected pattern");
            m_stline->showWarning("search", msg);
        }
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No expression selected");
}

/**
 * This slot is bound to the "list all" button in the "Find" toolbar.  The
 * function opens the search result list window and starts a search for all
 * expressions which are currently selected in the history list (serializing
 * multiple searches is handled by the search list dialog).
 */
void DlgHistory::cmdSearchList(int direction)
{
    QItemSelectionModel * sel = m_table->selectionModel();
    if (sel->hasSelection())
    {
        m_stline->clearMessage("search");

        std::vector<SearchPar> patList;
        for (auto& row : sel->selectedRows())
        {
            patList.push_back(m_model->getSearchPar(row));
        }
        SearchList::getInstance(false)->searchMatches(true, direction, patList);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No expression selected");
}


// ----------------------------------------------------------------------------
// Static state & interface

DlgHistory  * DlgHistory::s_instance;
MainSearch  * DlgHistory::s_search;
MainWin     * DlgHistory::s_mainWin;
MainText    * DlgHistory::s_mainText;
QByteArray    DlgHistory::s_winGeometry;
QByteArray    DlgHistory::s_winState;

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this dialog. Currently this includes the dialog geometry and
 * tool-bar state and the color palettes. Note the highlight definitions are
 * not included here, as the dialog class only holds a temporary copy while
 * open.
 */
QJsonObject DlgHistory::getRcValues()  /*static*/
{
    QJsonObject obj;

    if (s_instance)
    {
      s_winGeometry = s_instance->saveGeometry();
      s_winState = s_instance->saveState();
    }

    obj.insert("win_geom", QJsonValue(QString(s_winGeometry.toHex())));
    obj.insert("win_state", QJsonValue(QString(s_winState.toHex())));

    return obj;
}

/**
 * This function is called during start-up to apply configuration variables.
 * The values are simply stored and applied when the dialog is actually opened.
 */
void DlgHistory::setRcValues(const QJsonValue& val)  /*static*/
{
    const QJsonObject obj = val.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        if (var == "win_geom")
        {
            s_winGeometry = QByteArray::fromHex(val.toString().toLatin1());
        }
        else if (var == "win_state")
        {
            s_winState = QByteArray::fromHex(val.toString().toLatin1());
        }
    }
}

/**
 * This static external interface function creates and shows the dialog window.
 * If the window is already open, it is only raised. There can only be one
 * instance of the dialog.
 */
void DlgHistory::openDialog() /*static*/
{
    if (s_instance == nullptr)
    {
        s_instance = new DlgHistory();
    }
    else
    {
        s_instance->activateWindow();
        s_instance->raise();
    }
}

/**
 * This external interface function is called once during start-up after all
 * classes are instantiated to establish the required connections.
 */
void DlgHistory::connectWidgets(MainWin * mainWin, MainSearch * search, MainText * mainText) /*static*/
{
    s_mainWin = mainWin;
    s_search = search;
    s_mainText = mainText;
}

DlgHistory* DlgHistory::getInstance()
{
    openDialog();
    return s_instance;
}

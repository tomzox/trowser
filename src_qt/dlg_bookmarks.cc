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
 * This module implements the "bookmark" dialog. The dialog shows a list of
 * text lines, selected by the user from the main document. When clicking on of
 * the lines, the respective line is shown & selected in the main window and
 * the search list dialog. New bookmarks can be added via the main window or
 * search window (keyboard shortcut "m", or via selection & a menu command.)
 * The bookmark list can thus be seen as another level of filtering above
 * search lists (i.e. the user selects a usually large number of lines for the
 * search list, and from among these selected for the bookmark list.)
 *
 * The dialog allows editing the list by removing entries, or changing the
 * bookmark label (which by default is simply a copy of the text of the line.)
 * Also the list can be save to or read from a file.
 */

#include <QWidget>
#include <QApplication>
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
#include "status_line.h"
#include "search_list.h"
#include "bookmarks.h"
#include "highlighter.h"
#include "highl_view_dlg.h"
#include "dlg_bookmarks.h"

// ----------------------------------------------------------------------------

class DlgBookmarkModel : public QAbstractItemModel, public HighlightViewModelIf
{
public:
    enum TblColIdx { COL_IDX_LINE, COL_IDX_TEXT, COL_COUNT };

    DlgBookmarkModel(MainText * mainText, Highlighter * higl, Bookmarks * bookmarks)
        : m_mainText(mainText)
        , m_higl(higl)
        , m_bookmarks(bookmarks)
    {
        m_bookmarks->getLineList(m_lines);
    }
    virtual ~DlgBookmarkModel()
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
        return m_lines.size();
    }
    virtual int columnCount(const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        return COL_COUNT;
    }
    virtual Qt::ItemFlags flags(const QModelIndex& index __attribute__((unused))) const override
    {
        if (index.column() == COL_IDX_TEXT)
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        else
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    virtual QVariant headerData(int section __attribute__((unused)), Qt::Orientation orientation __attribute__((unused)), int role __attribute__((unused))) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex& index, const QVariant &value, int role = Qt::EditRole) override;
    //virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    //virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    int lineCount() const
    {
        return m_lines.size();
    }
    int getLineIdx(int line) const
    {
        auto it = std::find(m_lines.begin(), m_lines.end(), line);
        return ((it != m_lines.end()) ? *it : -1);
    }
    bool getLineIdx(int line, int &idx) const
    {
        auto it = std::find(m_lines.begin(), m_lines.end(), line);
        idx = ((it != m_lines.end()) ? *it : -1);
        return (it != m_lines.end());
    }
    // reverse lookup of getLineIdx
    int getLineOfIdx(int row) const
    {
        return ((size_t(row) < m_lines.size()) ? m_lines[row] : -1);
    }
    void refreshContents();
    void forceRedraw(TblColIdx col, int line = -1);

    // implementation of HighlightViewModelIf interfaces
    virtual const HiglFmtSpec * getFmtSpec(const QModelIndex& index) const override
    {
        int line = getLineOfIdx(index.row());
        if (line >= 0)
            return m_higl->getFmtSpecForLine(line, true);
        else
            return nullptr;
    }
    virtual QVariant higlModelData(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        return data(index, role);
    }

private:
    MainText    * const m_mainText;
    Highlighter * const m_higl;
    Bookmarks   * const m_bookmarks;

    std::vector<int> m_lines;
};

QVariant DlgBookmarkModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            if (section < COL_COUNT)
            {
                switch (section)
                {
                    case COL_IDX_LINE: return QVariant("Line number");
                    case COL_IDX_TEXT: return QVariant("Content / Description");
                    case COL_COUNT: break;
                }
            }
        }
        else
        {
            if (size_t(section) < m_lines.size())
            {
                return QVariant(QString::number(section + 1));
            }
        }
    }
    return QVariant();
}

QVariant DlgBookmarkModel::data(const QModelIndex &index, int role) const
{
    if (   ((role == Qt::DisplayRole) || (role == Qt::EditRole))
        && (size_t(index.row()) < m_lines.size()))
    {
        int line = m_lines[index.row()];

        switch (index.column())
        {
            case COL_IDX_LINE: return QVariant(line + 1);
            case COL_IDX_TEXT: return QVariant(m_bookmarks->getText(line));
            case COL_COUNT: break;
        }
    }
    return QVariant();
}

bool DlgBookmarkModel::setData(const QModelIndex &index, const QVariant &value, int /*role*/)
{
    int row = index.row();
    int col = index.column();
    bool result = false;

    if (size_t(row) < m_lines.size())
    {
        if (col == COL_IDX_TEXT)
        {
            QString str = value.toString();
            int line = m_lines[row];

            if (!str.isEmpty() && (str != m_bookmarks->getText(line)))
            {
                m_bookmarks->setText(line, str);

                emit dataChanged(index, index);
                result = true;
            }
        }
    }
    return result;
}

// called when external state has changed that affects rendering
void DlgBookmarkModel::forceRedraw(TblColIdx col, int line)  // ATTN row/col order reverse of usual
{
    if (m_lines.size() != 0)
    {
        int idx;
        if (line < 0)
        {
            emit dataChanged(createIndex(0, col),
                             createIndex(m_lines.size() - 1, col));
        }
        else if (getLineIdx(line, idx))
        {
            auto midx = createIndex(idx, col);
            emit dataChanged(midx, midx);
        }
    }
}

// called after removal/insertion of bookmarks
// UNCLEAN but used due to loose coupling with bookmarks database
// (in particular items can be inserted/removed by other dialogs)
void DlgBookmarkModel::refreshContents()
{
    m_lines.clear();
    m_bookmarks->getLineList(m_lines);

    emit layoutChanged();
}

// ----------------------------------------------------------------------------

class DlgBookmarkView : public QTableView
{
public:
    DlgBookmarkView(QWidget *parent)
        : QTableView(parent)
    {
    }
    virtual void keyPressEvent(QKeyEvent *e) override;
};

void DlgBookmarkView::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        // Firstly make Home key behave same with and without CTRL
        // Secondly correct behavior so that target line is selected
        case Qt::Key_Home:
            if (model()->rowCount() != 0)
            {
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
 * This function creates the dialog window showing all currently defined
 * bookmarks.
 */
DlgBookmarks::DlgBookmarks()
{
    const QString& fileName = s_mainWin->getFilename();
    if (!fileName.isEmpty())
      this->setWindowTitle("Bookmark list - " + fileName);
    else
      this->setWindowTitle("Bookmark list");

    auto central_wid = new QWidget();
        setCentralWidget(central_wid);
    auto layout_top = new QVBoxLayout(central_wid);

    m_model = new DlgBookmarkModel(s_mainText, s_higl, s_bookmarks);
    m_draw = new HighlightViewDelegate(m_model, false,
                                       s_mainWin->getFontContent(),
                                       s_mainWin->getFgColDefault(),
                                       s_mainWin->getBgColDefault());

    QFontMetrics metrics1(s_mainWin->getFontContent());
    m_table = new DlgBookmarkView(central_wid);
        m_table->setModel(m_model);
        m_table->setShowGrid(false);
        m_table->horizontalHeader()->setSectionResizeMode(DlgBookmarkModel::COL_IDX_TEXT, QHeaderView::Stretch);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        QFontMetrics metrics2(m_table->font());
        m_table->verticalHeader()->setDefaultSectionSize(std::max(metrics1.height(), metrics2.height()));
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_table->setItemDelegateForColumn(DlgBookmarkModel::COL_IDX_TEXT, m_draw);
        connect(m_table, &QAbstractItemView::customContextMenuRequested, this, &DlgBookmarks::showContextMenu);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DlgBookmarks::selectionChanged);
        layout_top->addWidget(m_table);

    m_stline = new StatusLine(m_table);

    m_cmdButs = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, central_wid);
        connect(m_cmdButs, &QDialogButtonBox::clicked, this, &DlgBookmarks::cmdButton);
    auto fileMenu = new QMenu("File actions", this);
        connect(fileMenu, &QMenu::aboutToShow, [=](){ m_saveMenuAct->setEnabled(s_bookmarks->validFileName()); });
    m_saveMenuAct = fileMenu->addAction("Save");
        connect(m_saveMenuAct, &QAction::triggered, [=](){ s_bookmarks->saveFileAs(this, true); });
    auto act = fileMenu->addAction("Save as...");
        connect(act, &QAction::triggered, [=](){ s_bookmarks->saveFileAs(this, false); });
    act = fileMenu->addAction("Load...");
        connect(act, &QAction::triggered, [=](){ s_bookmarks->readFileFrom(this); });
    auto m_cmdButFile = m_cmdButs->addButton("File...", QDialogButtonBox::ActionRole);
        m_cmdButFile->setMenu(fileMenu);
    layout_top->addWidget(m_cmdButs);

    connect(s_mainWin, &MainWin::textFontChanged, this, &DlgBookmarks::mainFontChanged);

    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(act, &QAction::triggered, this, &DlgBookmarks::cmdRemove);
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Escape));
        connect(act, &QAction::triggered, this, &DlgBookmarks::cmdClose);
        central_wid->addAction(act);

    if (!s_winGeometry.isEmpty())
        this->restoreGeometry(s_winGeometry);
    if (!s_winState.isEmpty())
        this->restoreState(s_winState);

    m_table->setFocus(Qt::ShortcutFocusReason);
    this->show();
}

DlgBookmarks::~DlgBookmarks()
{
    delete m_model;
    delete m_draw;
}

/**
 * This overriding function is called when the dialog window receives the close
 * event. The function stops background processes and destroys the class
 * instance to release all resources. The event is always accepted.
 */
void DlgBookmarks::closeEvent(QCloseEvent * event)
{
    // TODO? override resizeEvent(QResizeEvent *event) to call updateRcAfterIdle() (needed in case user terminates app via CTRL-C while window still open)
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();
    s_mainWin->updateRcAfterIdle();

    event->accept();

    s_instance->deleteLater();
    s_instance = nullptr;
}


/**
 * This slot is connected to the ESCAPE key shortcut. It closes the dialog in
 * the same way as via the X button in the window title bar.
 */
void DlgBookmarks::cmdClose(bool)
{
    QCoreApplication::postEvent(this, new QCloseEvent());
}


/**
 * This slot is connected to notification of font changes in the main text
 * window, as the same font is used for displaying text content in the dialog.
 * The function resizes row height and then forces a redraw. Note the new font
 * need not be passed to the view delegate as it gets passed a reference to the
 * font so that it picks up the new font automatically.
 */
void DlgBookmarks::mainFontChanged()
{
    QFontMetrics metrics1(s_mainWin->getFontContent());
    QFontMetrics metrics2(m_table->font());
    m_table->verticalHeader()->setDefaultSectionSize(std::max(metrics1.height(), metrics2.height()));

    m_model->forceRedraw(DlgBookmarkModel::COL_IDX_TEXT);
}


/**
 * This slot is bound to the main command buttons: In this dialog this is only
 * the "Close" button.
 */
void DlgBookmarks::cmdButton(QAbstractButton * button)
{
    if (button == m_cmdButs->button(QDialogButtonBox::Close))
    {
        QCoreApplication::postEvent(this, new QCloseEvent());
    }
}


/**
 * This function pops up a context menu for the bookmarks list.
 */
void DlgBookmarks::showContextMenu(const QPoint& pos)
{
    auto menu = new QMenu("Bookmark actions", this);
    auto sel = m_table->selectionModel()->selectedRows();

    auto act = menu->addAction("Remove selected entries");
        act->setEnabled(sel.size() != 0);
        connect(act, &QAction::triggered, this, &DlgBookmarks::cmdRemove);
    menu->addSeparator();

    act = menu->addAction("Rename bookmark");
        act->setEnabled(sel.size() != 0);
        if (sel.size() != 0)
            connect(act, &QAction::triggered, [=](){ m_table->edit(m_model->index(sel.front().row(), DlgBookmarkModel::COL_IDX_TEXT)); });

    menu->exec(mapToGlobal(pos));
}


/**
 * This external interface function is called out of the main window's
 * highlight loop for every line to which a search-match highlight is applied.
 * If the search list dialog is not open the function does nothing. Else it
 * forces a redraw of the text column in the given line so that the changed
 * highlighting is applied. (The model internally queries the main window
 * highlighting database so no local state change is required.)
 */
void DlgBookmarks::signalHighlightLine(int line)  /*static*/
{
    if (s_instance != nullptr)
    {
        //TODO filter search highlight?
        s_instance->m_model->forceRedraw(DlgBookmarkModel::COL_IDX_TEXT, line);
    }
}


/**
 * This function is bound to the "Toggle highlight" checkbutton in the
 * search list dialog's menu.  The function enables or disables search highlight.
 */
void DlgBookmarks::signalHighlightReconfigured()  /*static*/
{
    if (s_instance != nullptr)
    {
        s_instance->m_model->forceRedraw(DlgBookmarkModel::COL_IDX_TEXT);
    }
}


/**
 * This callback is invoked when the search history list has changed externally
 * (i.e. items removed/added or modified).  The function first forces a
 * complete redraw of the contents. Then it will re-select all previously
 * selected items, specifically those with the same pattern string.
 *
 * Note this special handling is necessary because by default the selection
 * model would keep selection at the same indices, which now may refer to
 * completely different items.
 */
void DlgBookmarks::refreshContents()
{
    m_model->refreshContents();

    QItemSelection sel;
    //QSet<QString>  newSelPats;
    QModelIndex first;
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        if (m_selPats.find(m_model->getLineOfIdx(row)) != m_selPats.end())
        {
            QModelIndex midx = m_model->index(row, 0);
            sel.select(midx, midx);
            //newSelPats.insert(m_model->getLineOfIdx(row));

            if (first.isValid() == false)
                first = midx;
        }
    }
    if (sel != m_table->selectionModel()->selection())
    {
        // Cannot rely on selection update below to invoke the callback, so update here directly;
        // Must be done first as list gets updated during following selection change.
        //m_selPats = newSelPats;

        if (first.isValid())
            m_table->setCurrentIndex(first);
        m_table->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}


void DlgBookmarks::signalBookmarkListChanged()  /*static*/
{
    if (s_instance != nullptr)
    {
        s_instance->refreshContents();
    }
}


/**
 * This function adjusts the view in the bookmark list so that the given text
 * line becomes visible.
 */
void DlgBookmarks::matchViewInt(int line)
{
    int idx = m_model->getLineIdx(line);
    if (idx != -1)
    {
        // ignore selection chage callback triggered by following sel. changes
        m_ignoreSelCb = idx;

        // move selection onto the line; clear selection if line is not in the list
        QModelIndex midx = m_model->index(idx, 0);
        m_table->setCurrentIndex(midx);
    }
}


void DlgBookmarks::matchView(int line) /*static*/
{
    if (s_instance != 0)
    {
        s_instance->matchViewInt(line);
    }
}


/**
 * This function is bound to changes of the selection in the color tags list.
 */
void DlgBookmarks::selectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        if (m_ignoreSelCb == sel.front().row())
        {
            // FIXME WA for ignoring callback triggered by matchViewInt()
        }
        else
        {
            int line = m_model->getLineOfIdx(sel.front().row());
            if (line >= 0)
            {
                s_search->highlightFixedLine(line);
                SearchList::matchView(line);
            }
        }
    }
    m_ignoreSelCb = -1;

    // keep copy of selected patterns to maintain selection across list updates
    m_selPats.clear();
    for (auto midx : m_table->selectionModel()->selectedRows())
    {
        m_selPats.insert(m_model->getLineOfIdx(midx.row()));
    }
}

/**
 * This function is bound to the "Remove selected lines" command in the
 * search history list dialog's context menu.  All currently selected text
 * lines are removed from the search list.
 */
void DlgBookmarks::cmdRemove(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() != 0)
    {
        m_stline->clearMessage("bookmarks");

        std::vector<int> lineList;
        lineList.reserve(sel.size());

        for (auto midx : sel)
        {
            lineList.push_back(m_model->getLineOfIdx(midx.row()));
        }
        s_bookmarks->removeLines(lineList);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("bookmarks", "No bookmarks selected for removal");
}

// ----------------------------------------------------------------------------
// Static state & interface

DlgBookmarks * DlgBookmarks::s_instance;
QByteArray    DlgBookmarks::s_winGeometry;
QByteArray    DlgBookmarks::s_winState;

MainWin     * DlgBookmarks::s_mainWin;
MainText    * DlgBookmarks::s_mainText;
MainSearch  * DlgBookmarks::s_search;
Highlighter * DlgBookmarks::s_higl;
Bookmarks   * DlgBookmarks::s_bookmarks;

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this dialog. Currently this includes the dialog geometry and
 * tool-bar state and the color palettes. Note the highlight definitions are
 * not included here, as the dialog class only holds a temporary copy while
 * open.
 */
QJsonObject DlgBookmarks::getRcValues()  /*static*/
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
void DlgBookmarks::setRcValues(const QJsonValue& val)  /*static*/
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
void DlgBookmarks::openDialog() /*static*/
{
    if (s_instance == nullptr)
    {
        s_instance = new DlgBookmarks();
    }
    else
    {
        s_instance->activateWindow();
        s_instance->raise();
    }
}

void DlgBookmarks::connectWidgets(MainWin * mainWin, MainSearch * search, MainText * mainText,
                                  Highlighter * higl, Bookmarks * bookmarks) /*static*/
{
    s_mainWin = mainWin;
    s_mainText = mainText;
    s_search = search;
    s_higl = higl;
    s_bookmarks = bookmarks;
}

DlgBookmarks* DlgBookmarks::getInstance()
{
    openDialog();
    return s_instance;
}

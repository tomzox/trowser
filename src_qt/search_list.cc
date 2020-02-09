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

#include <QWidget>
#include <QTableView>
#include <QAbstractItemModel>
#include <QItemSelection>
#include <QHeaderView>
#include <QPainter>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>

#include <cstdio>
#include <string>
#include <vector>
#include <initializer_list>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "search_list.h"

// ----------------------------------------------------------------------------

class SearchUndoRedo
{
public:
    SearchUndoRedo(bool f, const std::initializer_list<int>& v)
        : isAdded(f)
        , lines(v)
        {}
    SearchUndoRedo(bool f, const std::vector<int>& v)
        : isAdded(f)
        , lines(v)
        {}
    bool             isAdded;
    std::vector<int> lines;
};

class SearchListModel : public QAbstractItemModel
{
public:
    SearchListModel(MainText * mainText)
        : m_mainText(mainText)
    {
    }
    virtual ~SearchListModel()
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
        return dlg_srch_lines.size();
    }
    virtual int columnCount(const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        return 1;
    }
    virtual Qt::ItemFlags flags(const QModelIndex& index __attribute__((unused))) const override
    {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    virtual QVariant headerData(int section __attribute__((unused)), Qt::Orientation orientation __attribute__((unused)), int role __attribute__((unused))) const override
    {
        return QVariant();
    }
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    //virtual bool setData(const QModelIndex& index, const QVariant &value, int role = Qt::EditRole) override;
    //virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    //virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    int getLineIdx(int ins_line) const;
    bool getLineIdx(int line, int& idx) const
    {
        idx = getLineIdx(line);
        return ((size_t(idx) < dlg_srch_lines.size()) && (dlg_srch_lines[idx] == line));
    }
    bool isIdxValid(int line, int idx) const
    {
        return    (idx >= 0)
               && (size_t(idx) < dlg_srch_lines.size())
               && (dlg_srch_lines[idx] == line);
    }
    int lineCount() const
    {
        return dlg_srch_lines.size();
    }
    void insertLine(int line, int row = -1);
    void insertLines(const std::vector<int>& line_list, const std::vector<int>& idx_list);

private:
    MainText * const            m_mainText;
    std::vector<int>            dlg_srch_lines;
    std::vector<SearchUndoRedo> dlg_srch_undo;
    std::vector<SearchUndoRedo> dlg_srch_redo;
};

QVariant SearchListModel::data(const QModelIndex &index, int role) const
{
    if (   (role == Qt::DisplayRole)
        && ((size_t)index.row() < dlg_srch_lines.size()))
    {
        QTextBlock block = m_mainText->document()->findBlockByNumber(dlg_srch_lines[index.row()]);
        return QVariant(block.text());
    }
    return QVariant();
}

#if 0
bool SearchListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    bool result = false;

    if (size_t(row) + size_t(count) <= dlg_srch_lines.size())
    {
        this->beginRemoveRows(parent, row, row + count - 1);
        //dlg_srch_lines.erase(dlg_srch_lines.begin() + row,
        //                dlg_srch_lines.begin() + (row + count));
        this->endRemoveRows();
        result = true;
    }
    return result;
}
#endif

void SearchListModel::insertLine(int line, int row)
{
    if (row < 0)
        row = getLineIdx(line);

    if ((size_t(row) >= dlg_srch_lines.size()) || (dlg_srch_lines[row] != line))
    {
        this->beginInsertRows(QModelIndex(), row, row);
        dlg_srch_lines.insert(dlg_srch_lines.begin() + row, line);
        this->endInsertRows();

        dlg_srch_undo.push_back(SearchUndoRedo(true, {line}));
        dlg_srch_redo.clear();
    }
}

void SearchListModel::insertLines(const std::vector<int>& line_list, const std::vector<int>& idx_list)
{
    Q_ASSERT(line_list.size() == idx_list.size());

    dlg_srch_lines.reserve(dlg_srch_lines.size() + line_list.size());

    int idx = 0;
    while (size_t(idx) < line_list.size())
    {
        int count = 1;
        while ((size_t(idx + count) < idx_list.size()) && (idx_list[idx] + count == idx_list[idx + count]))
            ++count;

        int row = idx_list[idx];
        this->beginInsertRows(QModelIndex(), row, row + count - 1);
        dlg_srch_lines.insert(dlg_srch_lines.begin() + row,
                              line_list.begin() + idx,
                              line_list.begin() + idx + count);
        this->endInsertRows();

        idx += count;
    }

    dlg_srch_undo.push_back(SearchUndoRedo(true, line_list));
    dlg_srch_redo.clear();
}


/**
 * Helper function which performs a binary search in the sorted line index
 * list for the first value which is larger or equal to the given value.
 * Returns the index of the element, or the length of the list if all
 * values in the list are smaller.
 */
int SearchListModel::getLineIdx(int ins_line) const
{
    int end = dlg_srch_lines.size();
    int min = -1;
    int max = end;
    if (end > 0)
    {
        int idx = end >> 1;
        end -= 1;
        while (true)
        {
            int el = dlg_srch_lines[idx];
            if (el < ins_line)
            {
                min = idx;
                idx = (idx + max) >> 1;
                if ((idx >= max) || (idx <= min))
                    break;
            }
            else if (el > ins_line)
            {
                max = idx;
                idx = (min + idx) >> 1;
                if (idx <= min)
                    break;
            }
            else
            {
                max = idx;
                break;
            }
        }
    }
    return max;
}



// ----------------------------------------------------------------------------

class SearchListDraw : public QAbstractItemDelegate
{
public:
    SearchListDraw(SearchListModel * model, const QFont& fontDdefault, const QColor& fg, const QColor& bg)
        : m_model(model)
        , m_fontDefault(fontDdefault)
        , m_fgColDefault(fg)
        , m_bgColDefault(bg)
        {}
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    const int TXT_MARGIN = 3;
    SearchListModel * const m_model;
    const QFont& m_fontDefault;
    const QColor& m_fgColDefault;
    const QColor& m_bgColDefault;
};

void SearchListDraw::paint(QPainter *pt, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    HiglFmtSpec f;
    const HiglFmtSpec * fmtSpec = &f; //TODO
    if (fmtSpec != nullptr)
    {
        QFont font(m_fontDefault);
        if (!fmtSpec->m_font.isEmpty())
        {
            font.fromString(fmtSpec->m_font);
        }
        if (fmtSpec->m_underline)
            font.setUnderline(true);
        if (fmtSpec->m_bold)
            font.setWeight(QFont::Bold);
        if (fmtSpec->m_italic)
            font.setItalic(true);
        if (fmtSpec->m_overstrike)
            font.setStrikeOut(true);

        int w = option.rect.width();
        int h = option.rect.height();

        pt->save();
        pt->translate(option.rect.topLeft());
        pt->setClipRect(QRectF(0, 0, w, h));
        pt->setFont(font);

        if (option.state & QStyle::State_Selected)
        {
            // TODO simply reverse colors for selection
            pt->fillRect(0, 0, w, h, m_fgColDefault);
            pt->setPen(m_bgColDefault);
        }
        else
        {
            if (   (fmtSpec->m_bgCol != HiglFmtSpec::INVALID_COLOR)
                && (fmtSpec->m_bgStyle != Qt::NoBrush))
                pt->fillRect(0, 0, w, h, QBrush(QColor(fmtSpec->m_bgCol), fmtSpec->m_bgStyle));
            else if (fmtSpec->m_bgCol != HiglFmtSpec::INVALID_COLOR)
                pt->fillRect(0, 0, w, h, QColor(fmtSpec->m_bgCol));
            else if (fmtSpec->m_bgStyle != Qt::NoBrush)
                pt->fillRect(0, 0, w, h, fmtSpec->m_bgStyle);
            else
                pt->fillRect(0, 0, w, h, m_bgColDefault);

            if (   (fmtSpec->m_fgCol != HiglFmtSpec::INVALID_COLOR)
                && (fmtSpec->m_fgStyle != Qt::NoBrush))
                pt->setBrush(QBrush(QColor(fmtSpec->m_fgCol), fmtSpec->m_fgStyle));
            else if (fmtSpec->m_fgCol != HiglFmtSpec::INVALID_COLOR)
                pt->setPen(QColor(fmtSpec->m_fgCol));
            else if (fmtSpec->m_fgStyle != Qt::NoBrush)
                pt->setBrush(fmtSpec->m_fgStyle);
            else
                pt->setPen(m_fgColDefault);
        }

        QFontMetricsF metrics(font);
        auto data = m_model->data(index);
        pt->drawText(TXT_MARGIN, TXT_MARGIN + metrics.ascent(),
                     data.toString());
        pt->restore();
    }
}

QSize SearchListDraw::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    //TODO other fonts
    QFontMetricsF metrics(m_fontDefault);
    return QSize(1, metrics.height());
}

// ----------------------------------------------------------------------------

/**
 * This function creates a dialog window which collects text lines matching one
 * or more search expressions. The user can also freely add or remove lines
 * from the list.
 */
SearchList::SearchList()
{
  //PreemptBgTasks()
    this->setWindowTitle("Search matches"); //TODO + cur_filename
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);
        layout_top->setContentsMargins(0, 0, 0, 0);

    m_model = new SearchListModel(s_mainText);
    m_draw = new SearchListDraw(m_model,
                                s_mainWin->getFontContent(),
                                s_mainWin->getFgColDefault(),
                                s_mainWin->getBgColDefault());

    m_table = new QTableView(central_wid);
        m_table->setModel(m_model);
        m_table->setShowGrid(false);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setVisible(false);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->setItemDelegate(m_draw);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchList::selectionChanged);
        layout_top->addWidget(m_table);

    auto men = menuBar()->addMenu("Control");
    auto act = men->addAction("Load line numbers...");
        //command=SearchList_LoadFrom)
    act = men->addAction("Save text as...");
        //command=lambda:SearchList_SaveFileAs(0))
    act = men->addAction("Save line numbers...");
        //command=lambda:SearchList_SaveFileAs(1))
    men->addSeparator();
    act = men->addAction("Clear all");
        //command=SearchList_Clear)
    act = men->addAction("Close");
        //command=wt.dlg_srch.destroy)

    men = menuBar()->addMenu("Edit");
    act = men->addAction("Undo");
        //command=SearchList_Undo
        //accelerator="u")
    act = men->addAction("Redo");
        //command=SearchList_Redo
        //accelerator="^r")
    men->addSeparator();
    act = men->addAction("Import selected lines from main window");
        //command=SearchList_CopyCurrentLine)
    men->addSeparator();
    act = men->addAction("Remove selected lines");
        //accelerator="Del");
        //command=SearchList_RemoveSelection)
    men->addSeparator();
    act = men->addAction("Add main window search matches");
        //command=lambda:SearchList_AddMatches(0))
    act = men->addAction("Remove main window search matches");
        //command=lambda:SearchList_RemoveMatches(0))

    men = menuBar()->addMenu("Search");
    act = men->addAction("Search history...");
        //command=SearchHistory_Open)
    act = men->addAction("Edit highlight patterns...");
        //command=TagList_OpenDialog)
    men->addSeparator();
    act = men->addAction("Insert all search matches...");
        //command=lambda:SearchAll(1, 0)
        //accelerator="ALT-a")
    act = men->addAction("Insert all matches above...");
        //command=lambda:SearchAll(1 -1)
        //accelerator="ALT-P")
    act = men->addAction("Insert all matches below...");
        //command=lambda:SearchAll(1, 1)
        //accelerator="ALT-N")
    men->addSeparator();
    act = men->addAction("Clear search highlight");
        //command=SearchHighlightClear
        //accelerator="&")


    men = menuBar()->addMenu("Options");
    act = men->addAction("Highlight all matches");
        act->setCheckable(true);
        //command=SearchList_ToggleHighlight, variable=dlg_srch_highlight
        //accelerator="ALT-h")
#if 0
    act = men->addAction("Show frame number");
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_show_fn
        //accelerator="ALT-f")
    act = men->addAction("Show tick number");
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_show_tick
        //accelerator="ALT-t")
    act = men->addAction("Show tick num. delta");
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_tick_delta
        //accelerator="ALT-d")
    men->addSeparator();
    act = men->addAction("Select line as origin for tick delta");
        //command=SearchList_SetFnRoot
        //accelerator="ALT-0")
#endif

    setCentralWidget(central_wid);
    if (!s_winGeometry.isEmpty())
        this->restoreGeometry(s_winGeometry);
    if (!s_winState.isEmpty())
        this->restoreState(s_winState);
    m_table->setFocus(Qt::ShortcutFocusReason);

    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
        connect(act, &QAction::triggered, this, &SearchList::displayStats);
        central_wid->addAction(act);
#if 0
    // create/reset variables to default values
    dlg_srch_shown = true
    SearchList_Init()
    dlg_srch_show_fn = BooleanVar(tk, false)
    dlg_srch_show_tick = BooleanVar(tk, false)
    dlg_srch_tick_delta = BooleanVar(tk, false)
    dlg_srch_highlight = BooleanVar(tk, false)

    wt.dlg_srch_f1_l.bind("<ButtonRelease-3>", lambda e:BindCallAndBreak(lambda:SearchList_ContextMenu(e.x, e.y)))
    wt.dlg_srch_f1_l.bind("<Delete>", lambda e:BindCallAndBreak(SearchList_RemoveSelection))
    wt.dlg_srch_f1_l.bind("<Control-plus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(1)))
    wt.dlg_srch_f1_l.bind("<Control-minus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(-1)))
    wt.dlg_srch_f1_l.bind("<Control-Key-g>", lambda e:BindCallKeyClrBreak(lambda:SearchList_DisplayStats()))
    KeyCmdBind(wt.dlg_srch_f1_l, "/", lambda:SearchEnter(1, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "?", lambda:SearchEnter(0, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "n", lambda:SearchList_SearchNext(1))
    KeyCmdBind(wt.dlg_srch_f1_l, "N", lambda:SearchList_SearchNext(0))
    KeyCmdBind(wt.dlg_srch_f1_l, "&", SearchHighlightClear)
    KeyCmdBind(wt.dlg_srch_f1_l, "m", SearchList_ToggleMark)
    KeyCmdBind(wt.dlg_srch_f1_l, "u", SearchList_Undo)
    wt.dlg_srch_f1_l.bind("<Control-Key-r>", lambda e: SearchList_Redo())
    wt.dlg_srch_f1_l.bind("<space>", lambda e:SearchList_SelectionChange(dlg_srch_sel.TextSel_GetSelection()))
    wt.dlg_srch_f1_l.bind("<Escape>", lambda e:SearchList_SearchAbort(0))
    wt.dlg_srch_f1_l.bind("<Alt-Key-h>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("highlight")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-f>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_fn")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-t>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_tick")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-d>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("tick_delta")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-0>", lambda e:BindCallAndBreak(SearchList_SetFnRoot))
    wt.dlg_srch_f1_l.bind("<Alt-Key-n>", lambda e:BindCallAndBreak(lambda:SearchNext(1)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-p>", lambda e:BindCallAndBreak(lambda:SearchNext(0)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-a>", lambda e:BindCallAndBreak(lambda:SearchAll(0, 0)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-N>", lambda e:BindCallAndBreak(lambda:SearchAll(0, 1)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-P>", lambda e:BindCallAndBreak(lambda:SearchAll(0, -1)))

    wt.dlg_srch_ctxmen = Menu(wt.dlg_srch, tearoff=0)

    SearchList_CreateHighlightTags()
#endif

    this->show();
    //ResumeBgTasks()
}

SearchList::~SearchList()
{
    //TODO SearchList_SearchAbort(0)
    delete m_model;
    delete m_draw;
}

/**
 * This function is bound to destruction events on the search list dialog window.
 * The function stops background processes and releases all dialog resources.
 */
void SearchList::closeEvent(QCloseEvent * event)
{
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();

    event->accept();

    s_instance->deleteLater();
    s_instance = nullptr;
}

/**
 * Wrapper functions to simplify external interfaces
 */
void SearchList::addMatches(int direction)
{
    SearchPar par = s_search->getCurSearchParams();
    searchMatches(true, direction, par);
}


void SearchList::removeMatches(int direction)
{
    SearchPar par = s_search->getCurSearchParams();
    searchMatches(false, direction, par);
}


/**
 * This function is the external interface to the search list for adding
 * or removing lines matching the given search pattern.  The search is
 * performed in a background task, i.e. it's not completed when this
 * function returns.
 */
//void SearchList::searchMatches(bool do_add, const QString& pat, bool is_re, bool use_case, int direction)  /*static*/
void SearchList::searchMatches(bool /*do_add*/, int /*direction*/, const SearchPar& par)
{
    if (!par.m_pat.isEmpty())
    {
#if 0 //TODO
        if (SearchExprCheck(par.m_pat, par.opt_regexp, true))
        {
            hl = [pat, is_re, use_case]
            pat_list = [hl]
            startSearchAll(pat_list, do_add, direction)
        }
#endif
    }
}

/**
 * This function inserts all selected lines in the main window into the list.
 */
void SearchList::addMainSelection(QTextCursor& c)
{
    if (searchAbort())
    {
        int endPos;
        if (c.position() < c.anchor())
        {
          endPos = c.anchor();
        }
        else
        {
          endPos = c.position();
          c.setPosition(c.anchor());
        }

        std::vector<int> line_list;
        std::vector<int> idx_list;
        QItemSelection sel;

        while (c.position() <= endPos)
        {
            int line = c.blockNumber();
            int idx;
            if (m_model->getLineIdx(line, idx) == false)
            {
                idx += line_list.size();
                line_list.push_back(line);
                idx_list.push_back(idx);

                QModelIndex midx = m_model->index(idx_list.back(), 0);
                sel.select(midx, midx);
            }
            c.movePosition(QTextCursor::NextBlock);
            if (c.atEnd())
                break;
        }

        if (line_list.size() > 0)
        {
            m_model->insertLines(line_list, idx_list);

            m_table->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

            QModelIndex midx = m_model->index(idx_list.back(), 0);
            m_table->scrollTo(midx);
        }
    }
}

/**
 * This function inserts either the line in the main window holding the cursor
 * or all selected lines into the search result list.  It's bound to the "i" key
 * press event in the main window.
 */
void SearchList::copyCurrentLine()
{
    if (searchAbort())
    {
        auto c = s_mainText->textCursor();
        // ignore selection if not visible (because it can be irritating when "i"
        // inserts some random line instead of the one holding the cursor)
        if (c.anchor() != c.position())
        {
            // selection exists: add all selected lines
            addMainSelection(c);
        }
        else
        {
            // get line number of the cursor position
            int line = c.block().blockNumber();
            int idx;
            if (m_model->getLineIdx(line, idx) == false)
            {
                // line is not yet included
                m_model->insertLine(line, idx);
            }

            // make line visible & select it
            matchViewInt(line, idx);
        }
    }
}

/**
 * This function adjusts the view in the search result list so that the given
 * main window's text line becomes visible.
 */
void SearchList::matchViewInt(int line, int idx)
{
    if (idx < 0)
      idx = m_model->getLineIdx(line);

    if ((idx >= m_model->lineCount()) && (m_model->lineCount() > 0))
      idx -= 1;
    if (idx < m_model->lineCount())
    {
        QModelIndex midx = m_model->index(idx, 0);
        m_table->scrollTo(midx);
        m_table->selectionModel()->select(midx, QItemSelectionModel::Clear);

        // move selection onto the line; clear selection if line is not in the list
        if (m_model->isIdxValid(line, idx))
        {
            m_table->selectionModel()->select(midx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}

void SearchList::matchView(int line) /*static*/
{
    if (s_instance != 0)
    {
        s_instance->matchViewInt(line);
    }
}


/**
 * This function is a callback for selection changes in the search list dialog.
 * If a single line is selected, the view in the main window is changed to
 * display the respective line.
 */
void SearchList::selectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
    }
#if 0
  if (sel.size() == 1)
  {
    idx = sel[0]
    if (idx < dlg_srch_lines.size())
    {
      Mark_Line(dlg_srch_lines[idx])
#endif
}


/**
 * This function is bound to CTRL-g in the search list and displays stats
 * about the content of the search result list.
 */
void SearchList::displayStats(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    QString msg;
    if (sel.size() == 1)
    {
        int line_idx = sel.front().row() + 1;
        QTextStream(&msg) << "line " << line_idx << " of "
                          << m_model->lineCount() << " in the search list";
    }
    else
    {
        msg = QString::number(m_model->lineCount()) + " lines in the search list";
    }
    s_mainWin->showError(this, msg);
}

// ----------------------------------------------------------------------------
// Static state & interface

SearchList  * SearchList::s_instance = nullptr;
Highlighter * SearchList::s_higl;
MainSearch  * SearchList::s_search;
MainWin     * SearchList::s_mainWin;
MainText    * SearchList::s_mainText;
QByteArray    SearchList::s_winGeometry;
QByteArray    SearchList::s_winState;

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this dialog. Currently this includes the dialog geometry and
 * tool-bar state and the color palettes. Note the highlight definitions are
 * not included here, as the dialog class only holds a temporary copy while
 * open.
 */
QJsonObject SearchList::getRcValues()  /*static*/
{
    QJsonObject obj;

    obj.insert("win_geom", QJsonValue(QString(s_winGeometry.toHex())));
    obj.insert("win_state", QJsonValue(QString(s_winState.toHex())));

    return obj;
}

/**
 * This function is called during start-up to apply configuration variables.
 * The values are simply stored and applied when the dialog is actually opened.
 */
void SearchList::setRcValues(const QJsonValue& val)  /*static*/
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
void SearchList::openDialog(bool raiseWin) /*static*/
{
    if (s_instance == nullptr)
    {
        s_instance = new SearchList();
    }
    else if (raiseWin)
    {
        s_instance->activateWindow();
        s_instance->raise();
    }
}

void SearchList::connectWidgets(Highlighter * higl, MainSearch * search, MainWin * mainWin, MainText * mainText) /*static*/
{
    s_higl = higl;
    s_search = search;
    s_mainWin = mainWin;
    s_mainText = mainText;
}

SearchList* SearchList::getInstance(bool raiseWin)
{
    openDialog(raiseWin);
    return s_instance;
}

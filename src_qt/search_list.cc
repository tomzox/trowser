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

/*
 *  TODO:
 *  - BUG: undo broken: missing entries after "search all"
 *  - searchNext
 *  - bookmarking
 *  - bg task abort dialog
 *  - performance optimization line list used by SearchListModel
 *  - line list load/save
 *  - frame number pattern parsing
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
#include <QProgressBar>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>

#include <cstdio>
#include <string>
#include <vector>
#include <initializer_list>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "search_list.h"
#include "dlg_higl.h"

// ----------------------------------------------------------------------------

class SearchListModel : public QAbstractItemModel
{
public:
    enum TblColIdx { COL_IDX_BOOK, COL_IDX_LINE, COL_IDX_LINE_D, COL_IDX_TXT, COL_COUNT };

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
        return COL_COUNT;
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
    int getLineOfIdx(int idx) const
    {
        return ((size_t(idx) < dlg_srch_lines.size()) ? dlg_srch_lines[idx] : -1);
    }
    int lineCount() const
    {
        return dlg_srch_lines.size();
    }
    void insertLine(int line, int row = -1);
    void insertLinePreSorted(const std::vector<int>& line_list, const std::vector<int>& idx_list);
    void insertLines(const std::vector<int>::const_iterator lines_begin, const std::vector<int>::const_iterator lines_end);
    void removeLinePreSorted(const std::vector<int>& idx_list);
    void removeLines(const std::vector<int>::const_iterator lines_begin, const std::vector<int>::const_iterator lines_end);
    void removeAll(std::vector<int>& removedLines);

    void setLineDeltaRoot(int line);
    void forceRedraw(int line = -1);

private:
    MainText * const            m_mainText;
    int                         m_rootLineIdx = 0;
    // TODO? https://en.wikipedia.org/wiki/Skip_list#Indexable_skiplist
    // simple alternative: list of length-limited vectors (e.g. max 1000)
    std::vector<int>            dlg_srch_lines;
};

QVariant SearchListModel::data(const QModelIndex &index, int role) const
{
    if (   (role == Qt::DisplayRole)
        && ((size_t)index.row() < dlg_srch_lines.size()))
    {
        int line = dlg_srch_lines[index.row()];
        switch (index.column())
        {
            case COL_IDX_LINE:
                return QVariant(QString::number(line));
            case COL_IDX_LINE_D:
                return QVariant(QString::number(line - m_rootLineIdx));
            case COL_IDX_TXT:
            {
                QTextBlock block = m_mainText->document()->findBlockByNumber(line);
                return QVariant(block.text());
            }
            case COL_IDX_BOOK:
            case COL_COUNT:
                break;
        }
    }
    return QVariant();
}

void SearchListModel::insertLine(int line, int row)
{
    if (row < 0)
        row = getLineIdx(line);

    if ((size_t(row) >= dlg_srch_lines.size()) || (dlg_srch_lines[row] != line))
    {
        this->beginInsertRows(QModelIndex(), row, row);
        dlg_srch_lines.insert(dlg_srch_lines.begin() + row, line);
        this->endInsertRows();
    }
}

// ATTN: indices must be pre-compensated for insertion
// i.e. when inserting two lines at index N, index list must indicate N and N+1 repectively
void SearchListModel::insertLinePreSorted(const std::vector<int>& line_list,
                                          const std::vector<int>& idx_list)
{
    Q_ASSERT(line_list.size() == idx_list.size());

    dlg_srch_lines.reserve(dlg_srch_lines.size() + line_list.size());

    int idx = 0;
    int prev = -1;
    while (size_t(idx) < idx_list.size())
    {
        Q_ASSERT(idx_list[idx] > prev); prev = idx_list[idx];
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
}

void SearchListModel::insertLines(const std::vector<int>::const_iterator lines_begin,
                                  const std::vector<int>::const_iterator lines_end)
{
    for (auto it = lines_begin; it != lines_end; ++it)
    {
        // TODO optimize by removing consecutive indices in one call
        int idx;
        if (getLineIdx(*it, idx) == false)
        {
            // insert line into display
            this->beginInsertRows(QModelIndex(), idx, idx);
            dlg_srch_lines.insert(dlg_srch_lines.begin() + idx, *it);
            this->endInsertRows();
        }
    }
}

void SearchListModel::removeLinePreSorted(const std::vector<int>& idx_list)
{
    int prev = dlg_srch_lines.size();
    int idx = 0;
    while (size_t(idx) < idx_list.size())
    {
        // detect consecutive index values in descending order(!)
        Q_ASSERT(idx_list[idx] < prev); prev = idx_list[idx];
        int count = 1;
        while ((size_t(idx + count) < idx_list.size()) && (idx_list[idx] == idx_list[idx + count] + count))
            ++count;

        int row = idx_list[idx + count - 1];
        this->beginRemoveRows(QModelIndex(), row, row + count - 1);
        dlg_srch_lines.erase(dlg_srch_lines.begin() + row,
                             dlg_srch_lines.begin() + row + count);
        this->endRemoveRows();

        idx += count;
    }
}

void SearchListModel::removeLines(const std::vector<int>::const_iterator lines_begin,
                                  const std::vector<int>::const_iterator lines_end)
{
    for (auto it = lines_begin; it != lines_end; ++it)
    {
        // TODO optimize by removing consecutive indices in one call
        int idx;
        if (getLineIdx(*it, idx))
        {
            // remove line from display
            this->beginRemoveRows(QModelIndex(), idx, idx);
            dlg_srch_lines.erase(dlg_srch_lines.begin() + idx);
            this->endRemoveRows();
        }
    }
}

void SearchListModel::removeAll(std::vector<int>& removedLines)
{
    if (dlg_srch_lines.size() != 0)
    {
        this->beginRemoveRows(QModelIndex(), 0, dlg_srch_lines.size() - 1);
        removedLines = std::move(dlg_srch_lines);
        this->endRemoveRows();
    }
    Q_ASSERT(dlg_srch_lines.size() == 0);
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

void SearchListModel::setLineDeltaRoot(int line)
{
    m_rootLineIdx = line;

    if (dlg_srch_lines.size() != 0)
    {
        emit dataChanged(createIndex(COL_IDX_LINE_D, 0),
                         createIndex(COL_IDX_LINE_D, dlg_srch_lines.size() - 1));
    }
}

// called when highlighting options have changed
void SearchListModel::forceRedraw(int line)
{
    if (dlg_srch_lines.size() != 0)
    {
        int idx;
        if (line < 0)
        {
            emit dataChanged(createIndex(COL_IDX_TXT, 0),
                             createIndex(COL_IDX_TXT, dlg_srch_lines.size() - 1));
        }
        else if (getLineIdx(line, idx))
        {
            auto midx = createIndex(COL_IDX_TXT, 0);
            emit dataChanged(midx, midx);
        }
    }
}


// ----------------------------------------------------------------------------

class UndoRedoItem
{
public:
    using OpCode = enum { OPCODE_ADDED = 1, OPCODE_REMOVED = -1,
                          OPCODE_ADDED_BG = 2, OPCODE_REMOVED_BG = -2 };

    UndoRedoItem(OpCode opc, const std::initializer_list<int>& v)
        : opcode(opc)
        , lines(v)
        {}
    UndoRedoItem(OpCode opc, const std::vector<int>& v)
        : opcode(opc)
        , lines(v)
        {}
    UndoRedoItem(OpCode opc, std::vector<int>&& v)
        : opcode(opc)
        , lines(std::move(v))
        {}
public:
    OpCode           opcode;
    std::vector<int> lines;
};

class SearchListUndo
{
public:
    SearchListUndo() = default;
    void appendChange(bool do_add, const std::vector<int>& lines);
    void prepareBgChange();
    void appendBgChange(bool forUndo, bool do_add,
                        std::vector<int>::const_iterator lines_begin,
                        std::vector<int>::const_iterator lines_end);
    void finalizeBgChange(bool forUndo);
    void adjustLineNums(int top_l, int bottom_l);

    bool hasUndo() const { return dlg_srch_undo.size() != 0; }
    bool hasRedo() const { return dlg_srch_redo.size() != 0; }
    std::pair<bool,int>&& describeFirstOp(bool forUndo);
    bool popUndo(bool forUndo, std::vector<int>& lines);

private:
    std::vector<UndoRedoItem> dlg_srch_undo;
    std::vector<UndoRedoItem> dlg_srch_redo;
};

void SearchListUndo::appendChange(bool do_add, const std::vector<int>& lines)
{
    auto opcode = do_add ? UndoRedoItem::OPCODE_ADDED
                         : UndoRedoItem::OPCODE_REMOVED;

    dlg_srch_undo.push_back(UndoRedoItem(opcode, lines));
    dlg_srch_redo.clear();
}

void SearchListUndo::prepareBgChange()
{
    dlg_srch_redo.clear();
}

/**
 * This function is during background tasks which fill the search match dialog
 * after adding new matches. The function adds the respective line numbers to
 * the undo list. If there's already an undo item for the current search, the
 * numbers are merged into it.
 */
void SearchListUndo::appendBgChange(bool forUndo, bool do_add,
                                    std::vector<int>::const_iterator lines_begin,
                                    std::vector<int>::const_iterator lines_end)
{
    std::vector<UndoRedoItem>& undo_list = forUndo ? dlg_srch_undo : dlg_srch_redo;
    auto opcode = do_add ? UndoRedoItem::OPCODE_ADDED_BG
                         : UndoRedoItem::OPCODE_REMOVED_BG;
    if (undo_list.size() != 0)
    {
        UndoRedoItem& prev_undo = undo_list.back();
        if (prev_undo.opcode == opcode)
          prev_undo.lines.insert(prev_undo.lines.end(), lines_begin, lines_end);
        else
          undo_list.emplace_back(UndoRedoItem(opcode, std::vector<int>(lines_begin, lines_end)));
    }
    else
      undo_list.emplace_back(UndoRedoItem(opcode, std::vector<int>(lines_begin, lines_end)));
}


/**
 * This function is invoked at the end of background tasks which fill the
 * search list window to mark the entry on the undo list as closed (so that
 * future search matches go into a new undo element.)
 */
void SearchListUndo::finalizeBgChange(bool forUndo)
{
    std::vector<UndoRedoItem>& undo_list = forUndo ? dlg_srch_undo : dlg_srch_redo;

    if (undo_list.size() != 0)
    {
        auto prev_op = undo_list.back().opcode;
        if (prev_op == UndoRedoItem::OPCODE_ADDED_BG)
        {
            prev_op = UndoRedoItem::OPCODE_ADDED;
        }
        else if (prev_op == UndoRedoItem::OPCODE_REMOVED_BG)
        {
            prev_op = UndoRedoItem::OPCODE_REMOVED;
        }
    }
}

void SearchListUndo::adjustLineNums(int top_l, int bottom_l)
{
    std::vector<UndoRedoItem> tmp2;
    for (const auto& cmd : dlg_srch_undo)
    {
        std::vector<int> tmpl;
        tmpl.reserve(dlg_srch_undo.size());

        for (int line : cmd.lines)
        {
            if ((line >= top_l) && ((line < bottom_l) || (bottom_l == 0)))
            {
                tmpl.push_back(line - top_l + 1);
            }
        }

        if (tmpl.size() > 0)
        {
            tmp2.emplace_back(UndoRedoItem(cmd.opcode, std::move(tmpl)));
        }
    }

    dlg_srch_undo = std::move(tmp2);
    dlg_srch_redo.clear();
}

bool SearchListUndo::popUndo(bool forUndo, std::vector<int>& lines)
{
    std::vector<UndoRedoItem>& undo_list = forUndo ? dlg_srch_undo : dlg_srch_redo;

    if (undo_list.size() != 0)
    {
        bool doAdd = (   (undo_list.back().opcode == UndoRedoItem::OPCODE_ADDED)
                      || (undo_list.back().opcode == UndoRedoItem::OPCODE_ADDED_BG));
        lines = std::move(undo_list.back().lines);

        undo_list.pop_back();

        return doAdd;
    }
    Q_ASSERT(false);
    return false;
}

std::pair<bool,int>&& SearchListUndo::describeFirstOp(bool forUndo)
{
    std::vector<UndoRedoItem>& undo_list = forUndo ? dlg_srch_undo : dlg_srch_redo;
    int count = 0;
    bool doAdd = false;

    if (undo_list.size() != 0)
    {
        doAdd = (   (undo_list.back().opcode == UndoRedoItem::OPCODE_ADDED)
                      || (undo_list.back().opcode == UndoRedoItem::OPCODE_ADDED_BG));
        count = undo_list.back().lines.size();
    }
    return std::move(std::make_pair(doAdd, count));
}

// ----------------------------------------------------------------------------

class SearchListDraw : public QAbstractItemDelegate
{
public:
    SearchListDraw(SearchListModel * model, Highlighter * higl,
                   const QFont& fontDdefault, const QColor& fg, const QColor& bg)
        : m_model(model)
        , m_higl(higl)
        , m_fontDefault(fontDdefault)
        , m_fgColDefault(fg)
        , m_bgColDefault(bg)
        {}
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    const int TXT_MARGIN = 3;
    SearchListModel * const m_model;
    Highlighter * const m_higl;
    const QFont& m_fontDefault;
    const QColor& m_fgColDefault;
    const QColor& m_bgColDefault;
};

void SearchListDraw::paint(QPainter *pt, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int line = m_model->getLineOfIdx(index.row());
    const HiglFmtSpec * fmtSpec = m_higl->getFmtSpecForLine(line);

    QFont font(m_fontDefault);
    if (fmtSpec != nullptr)
    {
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
    }

    int w = option.rect.width();
    int h = option.rect.height();

    pt->save();
    pt->translate(option.rect.topLeft());
    pt->setClipRect(QRectF(0, 0, w, h));
    pt->setFont(font);

    if (option.state & QStyle::State_Selected)
    {
        pt->fillRect(0, 0, w, h, option.palette.color(QPalette::Highlight));
        pt->setPen(option.palette.color(QPalette::HighlightedText));
    }
    else if (fmtSpec != nullptr)
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
    else
    {
        pt->fillRect(0, 0, w, h, m_bgColDefault);
        pt->setPen(m_fgColDefault);
    }

    QFontMetricsF metrics(font);
    auto data = m_model->data(index);
    pt->drawText(TXT_MARGIN, metrics.ascent(), data.toString());

    pt->restore();
}

QSize SearchListDraw::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    //TODO other fonts
    QFontMetricsF metrics(m_fontDefault);
    return QSize(1, metrics.height());
}

// ----------------------------------------------------------------------------

class SearchListView : public QTableView
{
public:
    SearchListView(QWidget *parent)
        : QTableView(parent)
    {
        this->resizeColumnToContents(SearchListModel::COL_IDX_BOOK);
    }
    virtual void keyPressEvent(QKeyEvent *e) override;
    virtual int sizeHintForColumn(int column) const override;
    void setDocLineCount(int lineCount);

private:
    const int TXT_MARGIN = 5;
    int m_docLineCount = 0;
};

int SearchListView::sizeHintForColumn(int column) const
{
    if (   (column == SearchListModel::COL_IDX_LINE)
        || (column == SearchListModel::COL_IDX_LINE_D))
    {
        int maxLine = (column == SearchListModel::COL_IDX_LINE)
                         ? m_docLineCount : -m_docLineCount;

        QFontMetricsF metrics(viewOptions().font);
        int width = metrics.boundingRect(QString::number(maxLine)).width();
        return width + TXT_MARGIN * 2;
    }
    else
        return 100;  // never reached
}

void SearchListView::setDocLineCount(int lineCount)
{
    m_docLineCount = lineCount;

    this->resizeColumnToContents(SearchListModel::COL_IDX_LINE);
    this->resizeColumnToContents(SearchListModel::COL_IDX_LINE_D);
}

void SearchListView::keyPressEvent(QKeyEvent *e)
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
                QModelIndex midx = model()->index(SearchListModel::COL_IDX_BOOK, 0);
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
 * This function creates a dialog window which collects text lines matching one
 * or more search expressions. The user can also freely add or remove lines
 * from the list.
 */
SearchList::SearchList()
{
  //PreemptBgTasks()
    this->setWindowTitle("Search matches"); //TODO + cur_filename
    auto central_wid = new QWidget();
        setCentralWidget(central_wid);
    auto layout_top = new QVBoxLayout(central_wid);
        layout_top->setContentsMargins(0, 0, 0, 0);

    tid_search_list = new ATimer(central_wid);

    m_model = new SearchListModel(s_mainText);
    m_draw = new SearchListDraw(m_model, s_higl,
                                s_mainWin->getFontContent(),
                                s_mainWin->getFgColDefault(),
                                s_mainWin->getBgColDefault());
    m_undo = new SearchListUndo();

    QFontMetricsF metrics(s_mainWin->getFontContent());
    m_table = new SearchListView(central_wid);
        m_table->setModel(m_model);
        m_table->setShowGrid(false);
        m_table->horizontalHeader()->setVisible(false);
        m_table->horizontalHeader()->setSectionResizeMode(SearchListModel::COL_IDX_TXT, QHeaderView::Stretch);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_table->verticalHeader()->setDefaultSectionSize(metrics.height());
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setItemDelegateForColumn(SearchListModel::COL_IDX_TXT, m_draw);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_table->setColumnWidth(SearchListModel::COL_IDX_BOOK, 10); // TODO with of image
        m_table->setDocLineCount(s_mainText->document()->lineCount());
        configureColumnVisibility();
        connect(m_table, &QAbstractItemView::customContextMenuRequested, this, &SearchList::showContextMenu);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchList::selectionChanged);
        layout_top->addWidget(m_table);

    m_hipro = new QProgressBar(m_table);
        m_hipro->setOrientation(Qt::Horizontal);
        m_hipro->setTextVisible(true);
        m_hipro->setMinimum(0);
        m_hipro->setMaximum(100);
        m_hipro->setVisible(false);

    if (!s_winGeometry.isEmpty())
        this->restoreGeometry(s_winGeometry);
    if (!s_winState.isEmpty())
        this->restoreState(s_winState);

    populateMenus();
    auto act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
        connect(act, &QAction::triggered, this, &SearchList::displayStats);
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Escape));
        connect(act, &QAction::triggered, [=](){ searchAbort(false); });
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_0));
        connect(act, &QAction::triggered, this, &SearchList::cmdSetDeltaColRoot);
        central_wid->addAction(act);

#if 0
    wt.dlg_srch_f1_l.bind("<ButtonRelease-3>", lambda e:BindCallAndBreak(lambda:SearchList_ContextMenu(e.x, e.y)))
    wt.dlg_srch_f1_l.bind("<Control-plus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(1)))
    wt.dlg_srch_f1_l.bind("<Control-minus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(-1)))
    KeyCmdBind(wt.dlg_srch_f1_l, "/", lambda:SearchEnter(1, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "?", lambda:SearchEnter(0, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "n", lambda:SearchList_SearchNext(1))
    KeyCmdBind(wt.dlg_srch_f1_l, "N", lambda:SearchList_SearchNext(0))
    KeyCmdBind(wt.dlg_srch_f1_l, "m", SearchList_ToggleMark)
    wt.dlg_srch_f1_l.bind("<space>", lambda e:SearchList_SelectionChange(dlg_srch_sel.TextSel_GetSelection()))
    wt.dlg_srch_f1_l.bind("<Alt-Key-h>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("highlight")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-f>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_fn")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-t>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_tick")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-d>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("tick_delta")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-n>", lambda e:BindCallAndBreak(lambda:SearchNext(1)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-p>", lambda e:BindCallAndBreak(lambda:SearchNext(0)))
    //TODO navigation key controls equivalent legacy "TextSel" class
#endif

    m_table->setFocus(Qt::ShortcutFocusReason);
    this->show();
    //ResumeBgTasks()
}

SearchList::~SearchList()
{
    //TODO searchAbort(false);
    delete m_model;
    delete m_draw;
    delete m_undo;
}

/**
 * This overriding function is called when the dialog window receives the close
 * event. The function stops background processes and destroys the class
 * instance to release all resources. The event is always accepted.
 */
void SearchList::closeEvent(QCloseEvent * event)
{
    // TODO? override resizeEvent(QResizeEvent *event) to call updateRcAfterIdle() (needed in case user terminates app via CTRL-C while window still open)
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();
    s_mainWin->updateRcAfterIdle();

    event->accept();

    s_instance->deleteLater();
    s_instance = nullptr;
}

void SearchList::cmdClose(bool)
{
    QCoreApplication::postEvent(this, new QCloseEvent());
}

void SearchList::populateMenus()
{
    auto men = menuBar()->addMenu("Control");
    auto act = men->addAction("Load line numbers...");
        //command=SearchList_LoadFrom)
    act = men->addAction("Save text as...");
        //command=lambda:SearchList_SaveFileAs(0))
    act = men->addAction("Save line numbers...");
        //command=lambda:SearchList_SaveFileAs(1))
    men->addSeparator();
    act = men->addAction("Clear all");
        connect(act, &QAction::triggered, this, &SearchList::cmdClearAll);
    act = men->addAction("Close");
        connect(act, &QAction::triggered, this, &SearchList::cmdClose);

    men = menuBar()->addMenu("Edit");
        connect(men, &QMenu::aboutToShow, this, &SearchList::editMenuAboutToShow);
    m_menActUndo = men->addAction("Undo");
        m_menActUndo->setShortcut(QKeySequence(Qt::Key_U));
        connect(m_menActUndo, &QAction::triggered, this, &SearchList::cmdUndo);
    m_menActRedo = men->addAction("Redo");
        m_menActRedo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
        connect(m_menActRedo, &QAction::triggered, this, &SearchList::cmdRedo);
    men->addSeparator();
    act = men->addAction("Import selected lines from main window");
        connect(act, &QAction::triggered, this, &SearchList::copyCurrentLine);
    men->addSeparator();
    act = men->addAction("Remove selected lines");
        act->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(act, &QAction::triggered, this, &SearchList::cmdRemoveSelection);
    men->addSeparator();
    act = men->addAction("Add main window search matches");
        connect(act, &QAction::triggered, [=](){ addMatches(0); });
    act = men->addAction("Remove main window search matches");
        connect(act, &QAction::triggered, [=](){ removeMatches(0); });

    men = menuBar()->addMenu("Search");
    act = men->addAction("Search history...");
        //command=SearchHistory_Open)
    act = men->addAction("Edit highlight patterns...");
        connect(act, &QAction::triggered, [=](){ DlgHigl::openDialog(s_higl, s_search, s_mainWin); });
    men->addSeparator();
    act = men->addAction("Insert all search matches...");
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_A));
        connect(act, &QAction::triggered, [=](){ s_search->searchAll(true, 0); });
    act = men->addAction("Insert all matches above...");
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_P));
        connect(act, &QAction::triggered, [=](){ s_search->searchAll(true, -1); });
    act = men->addAction("Insert all matches below...");
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_N));
        connect(act, &QAction::triggered, [=](){ s_search->searchAll(true, 1); });
    men->addSeparator();
    act = men->addAction("Clear search highlight");
        act->setShortcut(QKeySequence(Qt::Key_Ampersand));
        connect(act, &QAction::triggered, s_search, &MainSearch::searchHighlightClear);


    men = menuBar()->addMenu("Options");
    //act = men->addAction("Highlight all matches");
    //    act->setCheckable(true);
    //    act->setChecked(m_showSrchHall);
    //    act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_H));
    //    connect(act, &QAction::triggered, this, &SearchList::cmdToggleSearchHighlight);
    act = men->addAction("Show line number");
        act->setCheckable(true);
        act->setChecked(m_showLineIdx);
        connect(act, &QAction::triggered, this, &SearchList::cmdToggleShowLineNumber);
    m_actShowLineDelta = men->addAction("Show line number delta");
        m_actShowLineDelta->setCheckable(true);
        m_actShowLineDelta->setChecked(m_showLineDelta);
        connect(m_actShowLineDelta, &QAction::triggered, this, &SearchList::cmdToggleShowLineDelta);
#if 0
    act = men->addAction("Show frame number");
        act->setCheckable(true);
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_show_fn
        //accelerator="ALT-f")
    act = men->addAction("Show tick number");
        act->setCheckable(true);
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_show_tick
        //accelerator="ALT-t")
    act = men->addAction("Show tick num. delta");
        act->setCheckable(true);
        //command=SearchList_ToggleFrameNo, variable=dlg_srch_tick_delta
        //accelerator="ALT-d")
#endif
    men->addSeparator();
    act = men->addAction("Select line as origin for line number delta");
        connect(act, &QAction::triggered, this, &SearchList::cmdSetLineIdxRoot);
        // shortcut ALT-0 is shared with other columns and thus has different callback
#if 0
    act = men->addAction("Select line as origin for tick delta");
        //command=SearchList_SetFnRoot
#endif
}

/**
 * This function is called when the "edit" menu in the search list dialog is opened.
 */
void SearchList::editMenuAboutToShow()
{
    if (m_undo->hasUndo())
    {
        std::pair<bool,int> info = m_undo->describeFirstOp(true);
        QString txt;
        QTextStream(&txt) << "Undo (" << (info.first ? "addition" : "removal")
                          << " of " << info.second << " lines)";
        m_menActUndo->setText(txt);
        m_menActUndo->setEnabled(true);
    }
    else
    {
        m_menActUndo->setText("Undo");
        m_menActUndo->setEnabled(false);
    }

    // code is copy/paste from above except for "Redo" instead of "Undo"
    if (m_undo->hasRedo())
    {
        std::pair<bool,int> info = m_undo->describeFirstOp(false);
        QString txt;
        QTextStream(&txt) << "Redo (" << (info.first ? "addition" : "removal")
                          << " of " << info.second << " lines)";
        m_menActRedo->setText(txt);
        m_menActRedo->setEnabled(true);
    }
    else
    {
        m_menActRedo->setText("Redo");
        m_menActRedo->setEnabled(false);
    }
}


/**
 * This function pops up a context menu for the search list dialog.
 */
void SearchList::showContextMenu(const QPoint& pos)
{
    auto menu = new QMenu("Search list actions", this);
    auto sel = m_table->selectionModel()->selectedRows();

    auto act = menu->addAction("Select line as origin for line number delta");
        act->setEnabled(m_showLineDelta && (sel.size() == 1));
        connect(act, &QAction::triggered, this, &SearchList::cmdSetLineIdxRoot);
#if 0
    auto act = menu->addAction("Select line as origin for tick delta");
    act->setEnabled(false);
    if ((sel.size() == 1) && ((tick_pat_sep != "") || (tick_pat_num != "")))
    {
        int line = m_model->getLineOfIdx(sel.front().row());
        fn = ParseFrameTickNo("$line.0", dlg_srch_fn_cache);
        if (fn != "")
        {
            connect(act, &QAction::triggered, this, &SearchList::setFnRoot);
            act->setEnabled(true);
        }
    }
#endif

    menu->addSeparator();
    act = menu->addAction("Remove selected lines");
    if (sel.size() > 0)
    {
        connect(act, &QAction::triggered, this, &SearchList::cmdRemoveSelection);
        act->setEnabled(true);
    }
    else
        act->setEnabled(false);

    menu->exec(mapToGlobal(pos));
}

void SearchList::configureColumnVisibility()
{
    if (m_showLineDelta)
        m_table->showColumn(SearchListModel::COL_IDX_LINE_D);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_LINE_D);

    if (m_showLineIdx)
        m_table->showColumn(SearchListModel::COL_IDX_LINE);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_LINE);
}

#if 0  // needed? requires filtering Hall ID from fmtSpec
void SearchList::cmdToggleSearchHighlight(bool checked)
{
    m_showSrchHall = checked;
    m_model->showSearchHall(m_showSrchHall);
}
#endif

void SearchList::cmdToggleShowLineNumber(bool checked)
{
    m_showLineIdx = checked;
    configureColumnVisibility();
}

void SearchList::cmdToggleShowLineDelta(bool checked)
{
    m_showLineDelta = checked;
    configureColumnVisibility();
}

void SearchList::cmdSetLineIdxRoot(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        int line = m_model->getLineOfIdx(sel.front().row());
        m_model->setLineDeltaRoot(line);
    }
}


/**
 * This function is bound to ALT-0 in the search result list and to the
 * "Select root FN" context menu command. The function sets the currently
 * selected line as origin for frame number delta calculations and enables
 * frame number delta display, which requires a complete refresh of the list.
 */
void SearchList::cmdSetDeltaColRoot(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        int line = m_model->getLineOfIdx(sel.front().row());

        // when multiple columns are supported: auto-enable last used option
        if (!m_showLineDelta) // && !m_tickFrameDelta
        {
            m_actShowLineDelta->activate(QAction::Trigger);
        }

        if (m_showLineDelta)
        {
            m_model->setLineDeltaRoot(line);
        }
#if 0
        else if (m_tickFrameDelta)
        {
            //if ((tick_pat_sep != "") || (tick_pat_num != ""))
            // extract the frame number from the text in the main window around the referenced line
            fn = ParseFrameTickNo("%d.0" % line, dlg_srch_fn_cache)
            if (fn != "")
            {
                dlg_srch_tick_delta = 1
                dlg_srch_tick_root = fn.split(" ")[0]
                SearchList_Refill()
            }
            else
                s_mainWin->showError(this, "Parsing did not yield a number for this line");
        }
#endif
    }
    else
        s_mainWin->showError(this, "Select a text line as origin for delta display");
}


/**
 * This function removes all content in the search list.
 */
void SearchList::cmdClearAll()
{
    searchAbort(false);

    if (m_model->lineCount() > 0)
    {
        std::vector<int> lines;
        m_model->removeAll(lines);

        m_undo->appendChange(false, lines);
    }
}


/**
 * This function is bound to the "Remove selected lines" command in the
 * search list dialog's context menu.  All currently selected text lines
 * are removed from the search list.
 */
void SearchList::cmdRemoveSelection()
{
    if (searchAbort())
    {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.size() != 0)
        {
            std::vector<int> line_list;
            std::vector<int> idx_list;
            line_list.reserve(sel.size());
            idx_list.reserve(sel.size());

            // reverse-sort index list so that indices do not need adaption during removal
            std::sort(sel.begin(), sel.end(),
                      [](const QModelIndex& a, const QModelIndex&b)->bool {return a.row() > b.row();});

            for (auto& midx : sel)
            {
                line_list.push_back(m_model->getLineOfIdx(midx.row()));
                idx_list.push_back(midx.row());
            }
            m_model->removeLinePreSorted(idx_list);
            m_undo->appendChange(false, line_list);
        }
    }
}


/**
 * This function is bound to the "Undo" menu command any keyboard shortcut.
 * This reverts the last modification of the line list (i.e. last removal or
 * addition, either via search or manually.)
 */
void SearchList::cmdUndo()
{
    s_mainWin->clearMessage(this);

    if (m_undo->hasUndo())
    {
        if (searchAbort())
        {
            std::vector<int> lines;
            bool doAdd = m_undo->popUndo(true, lines);

            tid_search_list->after(10, [=](){ bgUndoRedoLoop(doAdd, lines, -1, 0); });
        }
    }
    else
    {
        s_mainWin->showError(this, "Already at oldest change in search list");
    }
}


void SearchList::extUndo()  /*static*/
{
    if (s_instance != 0)
        s_instance->cmdUndo();
}


/**
 * This function is bound to the "Redo" menu command any keyboard shortcut.
 * This reverts the last "undo", if any.
 */
void SearchList::cmdRedo()
{
    s_mainWin->clearMessage(this);
    if (m_undo->hasRedo())
    {
        if (searchAbort())
        {
            std::vector<int> lines;
            bool doAdd = m_undo->popUndo(false, lines);

            tid_search_list->after(10, [=](){ bgUndoRedoLoop(doAdd, lines, 1, 0); });
        }
    }
    else
    {
        s_mainWin->showError(this, "Already at newest change in search list");
    }
}


void SearchList::extRedo()  /*static*/
{
    if (s_instance != 0)
        s_instance->cmdRedo();
}


/**
 * This function acts as background process for undo and redo operations.
 * Each iteration of this task works on at most 250-500 lines.
 */
void SearchList::bgUndoRedoLoop(bool doAdd, const std::vector<int>& lines, int mode, int off)
{
#if 0
    if (block_bg_tasks || (tid_search_inc != nullptr) || (tid_search_hall != nullptr))
    {
        // background tasks are suspended - re-schedule with timer
        tid_search_list = tk.after(100, lambda: SearchList_BgUndoRedoLoop(doAdd, lines, mode, off))
    }
    else
#endif
    {
        auto anchor = getViewAnchor();
        const size_t CHUNK_SIZE = 400;
        auto lines_begin = lines.cbegin() + off;
        auto lines_end = ((lines.size() >= size_t(off) + CHUNK_SIZE)
                                ? (lines.cbegin() + off) : lines.cend());
        off += CHUNK_SIZE;

        applyUndoRedo(doAdd, mode, lines_begin, lines_end);

        if (mode < 0)  //FIXME replace with bool
          m_undo->appendBgChange(false, doAdd, lines_begin, lines_end);
        else
          m_undo->appendBgChange(true, doAdd, lines_begin, lines_end);

        // select previously selected line again
        seeViewAnchor(anchor);

        if (lines_end != lines.end())
        {
            // create or update the progress bar
            searchProgress(100 * off / lines.size());

            tid_search_list->after(0, [=](){ bgUndoRedoLoop(doAdd, lines, mode, off); });
        }
        else
        {
            m_undo->finalizeBgChange(mode >= 0);

            //TODO SafeDestroy(wt.srch_abrt)
            searchProgress(100);
        }
    }
}


/**
 * This function performs a command for "undo" and "redo".
 */
void SearchList::applyUndoRedo(bool doAdd, int mode,
                               std::vector<int>::const_iterator lines_begin,
                               std::vector<int>::const_iterator lines_end)
{
    int op = (doAdd ? 1 : -1);
    if (op * mode < 0)
    {
        // undo insertion, i.e. delete lines again
        m_model->removeLines(lines_begin, lines_end);
    }
    else if (op * mode > 0)
    {
        // re-insert previously removed lines
        m_model->insertLines(lines_begin, lines_end);
    }
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
void SearchList::searchMatches(bool do_add, int direction, const SearchPar& par)
{
    if (!par.m_pat.isEmpty())
    {
        if (s_search->searchExprCheck(par.m_pat, par.m_opt_regexp, true))
        {
            startSearchAll(std::vector<SearchPar>({par}), do_add, direction);
        }
    }
}

void SearchList::searchMatches(bool do_add, int direction, const std::vector<SearchPar>& pat_list)
{
    startSearchAll(pat_list, do_add, direction);
}

/**
 * This function starts the search in the main text content for all matches
 * to a given pattern.  Matching lines are either inserted or removed from the
 * search list. The search is performed in the background and NOT finished when
 * this function returns.  Possibly still running older searches are aborted.
 */
void SearchList::startSearchAll(const std::vector<SearchPar>& pat_list, bool do_add, int direction)
{
    if (searchAbort())
    {
        int textPos = ((direction == 0) ? 0 : s_mainText->textCursor().position());

        // reset redo list
        m_undo->prepareBgChange();

        tid_search_list->after(10, [=](){ bgSearchLoop(pat_list, do_add, direction, textPos, 0, 0); });
    }
}


/**
 * This function acts as background process to fill the search list window.
 * The search loop continues for at most 100ms, then the function re-schedules
 * itself as idle task.
 */
void SearchList::bgSearchLoop(const std::vector<SearchPar> pat_list, bool do_add, int direction, int textPos, int pat_idx, int loop_cnt)
{
#if 0 //TODO
    if (block_bg_tasks || (tid_search_inc != nullptr) || (tid_search_hall != nullptr))
    {
      // background tasks are suspended - re-schedule with timer
      tid_search_list = tk.after(100, lambda line=line: SearchList_BgSearchLoop(pat_list, do_add, direction, textPos, pat_idx, 0))
    }
    else
#endif
    if (loop_cnt > 10)
    {
        tid_search_list->after(10, [=](){ bgSearchLoop(pat_list, do_add, direction, textPos, pat_idx, 0); });
    }
    else
    {
        auto anchor = getViewAnchor();
        qint64 start_t = QDateTime::currentMSecsSinceEpoch();
        const SearchPar& pat = pat_list[pat_idx];
        std::vector<int> line_list;
        std::vector<int> idx_list;
        int idxOff = 0;

        while (true)
        {
            QTextBlock block;
            int matchPos, matchLen;
            if (s_mainText->findInBlocks(pat, textPos, direction>=0, matchPos, matchLen, &block))
            {
                int line = block.blockNumber();
                int idx;
                bool found = m_model->getLineIdx(line, idx);
                if (do_add && !found)
                {
                    line_list.push_back(line);
                    idx_list.push_back(idx + idxOff);
                    idxOff += 1;
                }
                else if (!do_add && found)
                {
                    line_list.push_back(line);
                    idx_list.push_back(idx);
                }
                textPos = block.position() + block.length();
            }
            else if (matchPos >= 0)
            {
                textPos = matchPos;
            }
            else
            {
                textPos = -1;
                break;
            }

            // limit the runtime of the loop - return start line number for the next invocation
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - start_t > 100)
                break;
        }

        if (line_list.size() > 0)
        {
            if (do_add)
            {
                if (direction < 0)
                {
                    std::reverse(line_list.begin(), line_list.end());
                    std::reverse(idx_list.begin(), idx_list.end());
                }
                m_model->insertLinePreSorted(line_list, idx_list);
            }
            else
            {
                if (direction >= 0)
                {
                    std::reverse(idx_list.begin(), idx_list.end());
                }
                m_model->removeLinePreSorted(idx_list);
            }

            m_undo->appendBgChange(true, do_add, line_list.begin(), line_list.end());
            // select previously selected line again
            seeViewAnchor(anchor);
        }

        if (textPos >= 0)
        {
            // create or update the progress bar
            QTextBlock lastBlock = s_mainText->document()->lastBlock();
            int textLength = std::max(lastBlock.position() + lastBlock.length(), 1);
            double ratio;
            if (direction == 0)
            {
                ratio = double(textPos) / textLength;
            }
            else if (direction < 0)
            {
                // FIXME base should be original start position not end
                //thresh = int(wt.f1_t.index("insert").split(".")[0])
                //ratio = 1 - (textPos / thresh)
                ratio = 1 - (double(textPos) / textLength);
            }
            else
            {
                // FIXME base should be original start position not 0
                //thresh = int(wt.f1_t.index("insert").split(".")[0])
                //ratio = textPos / (max_line - thresh)
                ratio = double(textPos) / textLength;
            }
            searchProgress(100 * (ratio + pat_idx) / pat_list.size());

            loop_cnt += 1;
            tid_search_list->after(0, [=](){ bgSearchLoop(pat_list, do_add, direction, textPos, pat_idx, loop_cnt); });
        }
        else  // done
        {
            m_undo->finalizeBgChange(true);
            pat_idx += 1;
            if (size_t(pat_idx) < pat_list.size())
            {
                loop_cnt += 1;
                int textPos = ((direction == 0) ? 0 : s_mainText->textCursor().position());

                tid_search_list->after(0, [=](){ bgSearchLoop(pat_list, do_add, direction, textPos, pat_idx, loop_cnt); });
            }
            else
            {
                searchProgress(100);
                //TODO SafeDestroy(wt.srch_abrt)
            }
        }
    }
}


/**
 * This function is called by the background search processes to display or
 * update the progress bar. The given percent value has to be in range 0 to
 * 100. The progress bar is removed when the value reaches 100.
 */
void SearchList::searchProgress(int percent)
{
    if ((m_hipro->isVisible() == false) && (percent < 100))
    {
        m_table->setCursor(Qt::BusyCursor);
        m_hipro->setValue(percent);
        m_hipro->setVisible(true);
    }
    else if (percent < 100)
    {
        m_hipro->setValue(percent);
    }
    else
    {
        m_hipro->setVisible(false);
        m_table->setCursor(Qt::ArrowCursor);
    }
}

/**
 * This helper function is called before modifications of the search result
 * list by the various background tasks to determine a line which can serve
 * as "anchor" for the view, i.e. which will be made visible again after the
 * insertions or removals (which may lead to scrolling.)
 */
SearchList::ListViewAnchor&& SearchList::getViewAnchor()
{
    bool haveSel = false;
    int line = -1;

    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() != 0)
    {
        // keep selection visible
        line = m_model->getLineOfIdx(sel.front().row());
        haveSel = true;
    }
    else
    {
        // no selection - check if line near cursor in main win is visible
        auto midx = m_table->indexAt(m_table->rect().topLeft());
        if (midx.row() < m_model->lineCount())
            line = m_model->getLineOfIdx(midx.row());
    }
    return std::move(std::make_pair(haveSel, line));
}


/**
 * This helper function is called after modifications of the search result
 * list by the various background tasks to make the previously determined
 * "anchor" line visible and to adjust the selection.
 */
void SearchList::seeViewAnchor(ListViewAnchor& anchor)
{
    if (anchor.second >= 0)
    {
        int idx;
        if (m_model->getLineIdx(anchor.second, idx))
        {
            QModelIndex midx = m_model->index(idx, 0);
            m_table->scrollTo(midx);
            if (anchor.first)
            {
                m_ignoreSelCb = idx;
                m_table->selectionModel()->select(midx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            return;
        }
    }
    m_table->selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);
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
                if (m_ignoreSelCb == -1)
                    m_ignoreSelCb = idx_list.back();
            }
            c.movePosition(QTextCursor::NextBlock);
            if (c.atEnd())
                break;
        }

        if (line_list.size() > 0)
        {
            m_model->insertLinePreSorted(line_list, idx_list);
            m_undo->appendChange(true, line_list);

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
            m_undo->appendChange(true, std::vector<int>{line});

            // make line visible & select it
            matchViewInt(line, idx);
        }
    }
}

/**
 * This function is called out of the main window's highlight loop for every line
 * to which a search-match highlight is applied.
 */
void SearchList::signalHighlightLine(int line)  /*static*/
{
    if (s_instance != nullptr)
    {
        //TODO if (m_showSrchHall || (tid_high_init != nullptr))
        s_instance->m_model->forceRedraw(line);
    }
}


/**
 * This function is bound to the "Toggle highlight" checkbutton in the
 * search list dialog's menu.  The function enables or disables search highlight.
 */
void SearchList::signalHighlightReconfigured()  /*static*/
{
    if (s_instance != nullptr)
    {
        s_instance->m_model->forceRedraw();
    }
}

/**
 * This function adjusts the view in the search result list so that the given
 * text line becomes visible.
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

        // ignore selection chage callback triggered by following sel. changes
        m_ignoreSelCb = idx;

        // move selection onto the line; clear selection if line is not in the list
        if (m_model->isIdxValid(line, idx))
            m_table->selectionModel()->select(midx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        else
            m_table->selectionModel()->select(midx, QItemSelectionModel::Clear);
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
            }
        }
    }
    m_ignoreSelCb = -1;
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

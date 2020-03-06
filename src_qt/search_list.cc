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
 * This module implements the search list dialog, which is used to show a
 * user-selected sub-set of text lines in the mmain window. The user can add or
 * remove text lines via "search all" in the main window (or the find toolbar
 * in several other dialogs). Lines can also be added "manually" via the
 * "Insert" key and menu entries in the main window. Lines can also be removed
 * via the "Delete" key within the list dialog. Any changes to the list are
 * covered by an undo/redo mechanism.
 *
 * The search list is designed specifically to support a large number of lines.
 * Therefore addition and removal as well as undo/redo are broken into steps of
 * may 100 ms within a timer-driven background task. Only one such change can
 * be going on at a time; the user will be asked to wait for the previous
 * change to complete via a dialog when making further changes.
 *
 * Finally the dialog offers saving the entire contents, or just the line
 * indices to a file, or inversely add line numbers listed in a file to the
 * dialog.
 *
 * By default the dialog only shows a copy of the text for each line. Via the
 * menu the user can add columns showing the line number, or a line number
 * delta, or user-defined columns with content extracted from adjacent text
 * lines via regular expression pattern matching & capturing.
 *
 * Currently only one instance of this class can exists. Attempts to open
 * another instance will just raise the window of the existing instance.
 * Technically however multiple instances are possible, and could be useful.
 * This would however require extending the user-interface in other dialogs so
 * that the user can select which instance operations such as "search all"
 * should work on.
 */

#include <QWidget>
#include <QTableView>
#include <QAbstractItemModel>
#include <QItemSelection>
#include <QHeaderView>
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>
#include <QByteArray>
#include <QFileDialog>
#include <QDebug>

#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <initializer_list>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "status_line.h"
#include "config_file.h"
#include "bg_task.h"
#include "text_block_find.h"
#include "bookmarks.h"
#include "highlighter.h"
#include "highl_view_dlg.h"
#include "search_list.h"
#include "parse_frame.h"
#include "dlg_bookmarks.h"
#include "dlg_parser.h"
#include "dlg_higl.h"
#include "dlg_history.h"

// ----------------------------------------------------------------------------

/**
 * This class implements the "model" associated with the search list "view".
 * The central part of this model is a plain array of line indices, which
 * represent a sub-set of the lines (i.e. paragraphs) of the main text
 * document. The model is designed for table view. The right-most column is
 * rendered as a copy of the respective main text line (i.e. the actual text
 * content of that line is not stored in the model).  Other columns can
 * optionally be made visible to display additional related information,
 * specifically: A flag when the line has been bookmarked; the line number;
 * line number delta to a user-selected "root" line; and several columns with
 * user-defined content that is obtained from an external ParseFrame class.
 *
 * In addition to the standard QAbstractItemModel interface, the model provides
 * an interface HighlightViewModelIf which is used by the delegate rendering
 * the text content for obtaining the data and mark-up configuration.
 */
class SearchListModel : public QAbstractItemModel, public HighlightViewModelIf
{
public:
    enum TblColIdx { COL_IDX_BOOK, COL_IDX_LINE, COL_IDX_LINE_D,
                     COL_IDX_CUST_VAL, COL_IDX_CUST_VAL_DELTA, COL_IDX_CUST_FRM, COL_IDX_CUST_FRM_DELTA,
                     COL_IDX_TXT, COL_COUNT };

    SearchListModel(MainText * mainText, Highlighter * higl, const ParseSpec& parseSpec,
                    bool showSrchHall, bool showBookmarkMarkup)
        : m_mainText(mainText)
        , m_higl(higl)
        , m_parser(ParseFrame::create(m_mainText->document(), parseSpec))  // may return nullptr
        , m_showSrchHall(showSrchHall)
        , m_showBookmarkMarkup(showBookmarkMarkup)
    {
    }
    virtual ~SearchListModel() = default;
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
        if (orientation == Qt::Horizontal)
        {
            if (role == Qt::DisplayRole)
            {
                switch (section)
                {
                    case COL_IDX_BOOK: return QVariant("BM.");
                    case COL_IDX_LINE: return QVariant("#");
                    case COL_IDX_LINE_D: return QVariant("\xCE\x94");  // UTF-8 0xCE94: Greek delta
                    case COL_IDX_CUST_VAL: return QVariant(m_parser.get() ? m_parser->getHeader(ParseColumnFlags::Val) : "");
                    case COL_IDX_CUST_VAL_DELTA: return QVariant(m_parser.get() ? ("\xCE\x94" + m_parser->getHeader(ParseColumnFlags::Val)) : "");
                    case COL_IDX_CUST_FRM: return QVariant(m_parser.get() ? m_parser->getHeader(ParseColumnFlags::Frm) : "");
                    case COL_IDX_CUST_FRM_DELTA: return QVariant(m_parser.get() ? ("\xCE\x94" + m_parser->getHeader(ParseColumnFlags::Frm)) : "");
                    case COL_IDX_TXT: return QVariant("Text");
                    case COL_COUNT: break;
                }
            }
            else if (role == Qt::ToolTipRole)
            {
                switch (section)
                {
                    case COL_IDX_BOOK: return QVariant("Bookmarked lines are marked with a blue dot in this column.");
                    case COL_IDX_LINE: return QVariant("Number of each line in the main window");
                    case COL_IDX_LINE_D: return QVariant("Line number delta to a selected base line");
                    case COL_IDX_CUST_VAL: return QVariant("Value extracted from text content as per \"custom column configuration\"");
                    case COL_IDX_CUST_VAL_DELTA: return QVariant("Delta between extracted value of each line to that of a selected line");
                    case COL_IDX_CUST_FRM: return QVariant("Frame boundary value extracted from text content as per \"custom column configuration\"");
                    case COL_IDX_CUST_FRM_DELTA: return QVariant("Delta between frame boundary value of each line to that of a selected line");
                    case COL_IDX_TXT: return QVariant("Copy of the text in the main window");
                    case COL_COUNT: break;
                }
            }
            // Qt::SizeHintRole
        }
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
    void insertLinePreSorted(const std::vector<int>& line_list, std::vector<int>& idx_list);
    void insertLines(const std::vector<int>& line_list);
    void removeLinePreSorted(const std::vector<int>& idx_list);
    void removeLines(const std::vector<int>& line_list);
    void removeAll(std::vector<int>& removedLines);

    void setLineDeltaRoot(int line);
    bool setCustomDeltaRoot(TblColIdx col, int line);
    void forceRedraw(TblColIdx col, int line = -1);
    const std::vector<int>& exportLineList() const
    {
        return dlg_srch_lines;
    }
    void adjustLineNums(int top_l, int bottom_l);
    void showSearchHall(bool enable)
    {
        m_showSrchHall = enable;
    }
    void showBookmarkMarkup(bool enable)
    {
        m_showBookmarkMarkup = enable;
    }
    void setCustomColCfg(const ParseSpec& parseSpec)
    {
        m_parser = ParseFrame::create(m_mainText->document(), parseSpec);
        if (dlg_srch_lines.size() != 0)
        {
            emit dataChanged(createIndex(0, COL_IDX_CUST_VAL),
                             createIndex(dlg_srch_lines.size() - 1, COL_IDX_CUST_FRM_DELTA));
        }
    }
    ParseColumns getCustomColumns() const
    {
        return (m_parser.get() ? m_parser->getColumns() : ParseColumns(0));
    }

    // implementation of HighlightViewModelIf interfaces
    virtual const HiglFmtSpec * getFmtSpec(const QModelIndex& index) const override
    {
        int line = getLineOfIdx(index.row());
        if (line >= 0)
            return m_higl->getFmtSpecForLine(line, !m_showBookmarkMarkup, !m_showSrchHall);
        else
            return nullptr;
    }
    virtual QVariant higlModelData(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        return data(index, role);
    }

private:
    MainText * const            m_mainText;
    Highlighter * const         m_higl;
    ParseFramePtr               m_parser;
    bool                        m_showSrchHall = true;
    bool                        m_showBookmarkMarkup = false;
    int                         m_rootLineIdx = 0;
    int                         m_rootCustVal = 0;
    int                         m_rootCustFrm = 0;
    std::vector<int>            dlg_srch_lines;

};

QVariant SearchListModel::data(const QModelIndex &index, int role) const
{
    if (   (role == Qt::DisplayRole)
        && ((size_t)index.row() < dlg_srch_lines.size()))
    {
        int line = dlg_srch_lines[index.row()];
        bool ok;

        switch (index.column())
        {
            case COL_IDX_LINE:
                return QVariant(QString::number(line + 1));
            case COL_IDX_LINE_D:
                return QVariant(QString::number(line - m_rootLineIdx));
            case COL_IDX_CUST_VAL:
                if (m_parser.get() != nullptr)
                    return QVariant(m_parser->parseFrame(line, 0));
                break;
            case COL_IDX_CUST_VAL_DELTA:
                if (m_parser.get() != nullptr)
                {
                    int val = m_parser->parseFrame(line, 0).toInt(&ok);
                    if (ok)
                        return QVariant(QString::number(val - m_rootCustVal));
                }
                break;
            case COL_IDX_CUST_FRM:
                if (m_parser.get() != nullptr)
                    return QVariant(m_parser->parseFrame(line, 1));
                break;
            case COL_IDX_CUST_FRM_DELTA:
                if (m_parser.get() != nullptr)
                {
                    int val = m_parser->parseFrame(line, 1).toInt(&ok);
                    if (ok)
                        return QVariant(QString::number(val - m_rootCustFrm));
                }
                break;
            case COL_IDX_TXT:
                return QVariant(m_mainText->document()->findBlockByNumber(line).text());
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
                                          std::vector<int>& idx_list)
{
    Q_ASSERT(line_list.size() == idx_list.size());

    dlg_srch_lines.reserve(dlg_srch_lines.size() + line_list.size());

    // adjust insertion indices for shift caused by preceding insertions
    for (size_t idx = 1; idx < idx_list.size(); ++idx)
        idx_list[idx] += idx;

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

void SearchListModel::insertLines(const std::vector<int>& line_list)
{
    int prev = -1;
    for (size_t line_idx = 0; line_idx < line_list.size(); ++line_idx)
    {
        // assert input list is sorted in ascending order
        Q_ASSERT(line_list[line_idx] > prev); prev = line_list[line_idx];

        int row;
        if (getLineIdx(line_list[line_idx], row) == false)
        {
            // detect line values mapped to consecutive index values
            size_t count = 1;
            if (size_t(row) < dlg_srch_lines.size())
                while (   (line_idx + count < line_list.size())
                       && (dlg_srch_lines[row] > line_list[line_idx + count]))
                    ++count;
            else
                count = line_list.size() - line_idx;

            // insert lines into display & list
            this->beginInsertRows(QModelIndex(), row, row + count - 1);
            dlg_srch_lines.insert(dlg_srch_lines.begin() + row,
                                  line_list.begin() + line_idx,
                                  line_list.begin() + line_idx + count);
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

void SearchListModel::removeLines(const std::vector<int>& line_list)
{
    int prev = -1;
    for (size_t line_idx = 0; line_idx < line_list.size(); ++line_idx)
    {
        // assert input list is sorted in ascending order
        Q_ASSERT(line_list[line_idx] > prev); prev = line_list[line_idx];

        int row;
        if (getLineIdx(line_list[line_idx], row))
        {
            // detect line values mapped to consecutive index values
            size_t count = 1;
            while (   (line_idx + count < line_list.size())
                   && (dlg_srch_lines[row + count] == line_list[line_idx + count]))
                ++count;

            // remove lines from display & list
            this->beginRemoveRows(QModelIndex(), row, row + count - 1);
            dlg_srch_lines.erase(dlg_srch_lines.begin() + row,
                                 dlg_srch_lines.begin() + row + count);
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
        emit dataChanged(createIndex(0, COL_IDX_LINE_D),
                         createIndex(dlg_srch_lines.size() - 1, COL_IDX_LINE_D));
    }
}

bool SearchListModel::setCustomDeltaRoot(TblColIdx col, int line)
{
    Q_ASSERT((col == COL_IDX_CUST_VAL_DELTA) || (col == COL_IDX_CUST_FRM_DELTA));
    bool result = false;

    int val = m_parser->parseFrame(line, ((col == COL_IDX_CUST_VAL_DELTA) ? 0 : 1)).toInt(&result);
    if (result)
    {
        if (col == COL_IDX_CUST_VAL_DELTA)
            m_rootCustVal = val;
        else
            m_rootCustFrm = val;

        if (dlg_srch_lines.size() != 0)
        {
            emit dataChanged(createIndex(0, col),
                             createIndex(dlg_srch_lines.size() - 1, col));
        }
    }
    return result;
}

// called when external state has changed that affects rendering
void SearchListModel::forceRedraw(TblColIdx col, int line)  // ATTN row/col order reverse of usual
{
    if (dlg_srch_lines.size() != 0)
    {
        int idx;
        if (line < 0)
        {
            emit dataChanged(createIndex(0, col),
                             createIndex(dlg_srch_lines.size() - 1, col));
        }
        else if (getLineIdx(line, idx))
        {
            auto midx = createIndex(idx, col);
            emit dataChanged(midx, midx);
        }
    }
}

void SearchListModel::adjustLineNums(int top_l, int bottom_l)
{
    if (bottom_l >= 0)
    {
        // delete from [bottom_l ... end[
        int idx = getLineIdx(bottom_l);
        if (size_t(idx) < dlg_srch_lines.size())
        {
            this->beginRemoveRows(QModelIndex(), idx, dlg_srch_lines.size() - 1);
            dlg_srch_lines.erase(dlg_srch_lines.begin() + idx, dlg_srch_lines.end());
            this->endRemoveRows();
        }
    }

    if (top_l > 0)
    {
        // delete lines from [0 ... topl[
        int idx = getLineIdx(top_l);
        if (idx > 0)
        {
            this->beginRemoveRows(QModelIndex(), 0, idx - 1);
            dlg_srch_lines.erase(dlg_srch_lines.begin(), dlg_srch_lines.begin() + idx);
            this->endRemoveRows();
        }

        // now actually adjust remaining line numbers
        if (dlg_srch_lines.size() != 0)
        {
            for (int& line : dlg_srch_lines)
                line -= top_l;

            emit dataChanged(createIndex(0, COL_IDX_LINE),
                             createIndex(dlg_srch_lines.size() - 1, COL_IDX_LINE));
        }
    }

    if (m_parser != nullptr)
    {
        m_parser->clearCache();
    }
}


// ----------------------------------------------------------------------------

/**
 * This struct describes one entry in the undo/redo lists.
 */
class UndoRedoItem
{
public:
    UndoRedoItem(bool doAdd, const std::initializer_list<int>& v)
        : isAdded(doAdd)
        , lines(v)
        {}
    UndoRedoItem(bool doAdd, const std::vector<int>& v)
        : isAdded(doAdd)
        , lines(v)
        {}
    UndoRedoItem(bool doAdd, std::vector<int>&& v)
        : isAdded(doAdd)
        , lines(std::move(v))
        {}
public:
    bool             isAdded;
    std::vector<int> lines;
};

/**
 * The SearchListUndo class manages the undo/redo list for the search list
 * dialog. Any changes to the content of the search list are recorded to the
 * undo list.  Changes can be either additions or removals. Changes done by
 * background tasks of the search list class are usually split into many parts
 * for performance reasons; the UndoRedoItem class offers special interfaces
 * for these cases to allow concatenating them to a single entry in the undo
 * list.
 *
 * When a change is undone, the respective change description is moved from the
 * undo list to the redo list. For performance reasons any undo/redo may be
 * split in many steps. In this phase entries are kept in both undo and redo
 * list, but portions of the contained line number lists are moved from one
 * list to the other, always in sync with changes to the model. When the undo
 * or redo is completed the obsolete entry is removed. Is the process is
 * aborted in the middle, the partial entries remain in both lists.
 *
 * Note this class is coupled tightly with the state of the model. To ensure
 * consistency there's an invariant function checking all undo entries combined
 * result exactly in the current content of the model.
 */
class SearchListUndo
{
public:
    SearchListUndo() = default;
    ~SearchListUndo() = default;
    void appendChange(bool doAdd, const std::vector<int>& lines);

    void prepareBgChange(bool doAdd);
    void appendBgChange(bool doAdd,
                        std::vector<int>::const_iterator lines_begin,
                        std::vector<int>::const_iterator lines_end);
    void finalizeBgChange(bool doAdd);
    void abortBgChange();

    void prepareUndoRedo(bool isRedo);
    bool popUndoRedo(bool isRedo, bool *retDoAdd, std::vector<int>& retLines, size_t maxCount);
    void finalizeUndoRedo(bool isRedo);

    void adjustLineNums(int top_l, int bottom_l);

    bool hasUndo(int *count = nullptr) const;
    bool hasRedo(int *count = nullptr) const;
    std::pair<bool,int> describeFirstOp(bool forUndo);

private:
    std::vector<UndoRedoItem> dlg_srch_undo;
    std::vector<UndoRedoItem> dlg_srch_redo;
    int  m_bgDstIdx = -1;
    bool m_bgForUndo = false;
    bool m_bgDoAdd = false;

#ifndef QT_NO_DEBUG
    void invariant() const;
    SearchListModel * m_modelDebug = nullptr;
public:
    void connectModelForDebug(SearchListModel * model) { m_modelDebug = model; }
#define SEARCH_LIST_UNDO_INVARIANT() do{ invariant(); }while(0)
#else
#define SEARCH_LIST_UNDO_INVARIANT() do{}while(0)
#endif
};


/**
 * This is the basic interface for adding to the "undo" list. It is used by
 * small "atomic" changes, usually via user interaction (i.e. adding a few
 * selected text lines). The redo list is cleared, so that any changes that
 * were previously undone are lost.
 */
void SearchListUndo::appendChange(bool doAdd, const std::vector<int>& lines)
{
    Q_ASSERT(m_bgDstIdx < 0);

    dlg_srch_undo.push_back(UndoRedoItem(doAdd, lines));
    dlg_srch_redo.clear();

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This interface has to be called at the start of background tasks to prepare
 * combining subsequent changes to a single entry. Target is always the undo
 * list; the redo list is cleared.
 */
void SearchListUndo::prepareBgChange(bool doAdd)
{
    std::vector<UndoRedoItem>& undo_list = dlg_srch_undo;
    Q_ASSERT(m_bgDstIdx < 0);

    m_bgDstIdx = undo_list.size();
    m_bgDoAdd = doAdd;
    m_bgForUndo = true;

    dlg_srch_redo.clear();
}


/**
 * This function is during background tasks which fill the search match dialog
 * after adding new matches. The function adds the respective line numbers to
 * the undo list. If there's already an undo item for the current search, the
 * numbers are merged into it.
 */
void SearchListUndo::appendBgChange(bool doAdd,
                                    std::vector<int>::const_iterator lines_begin,
                                    std::vector<int>::const_iterator lines_end)
{
    std::vector<UndoRedoItem>& undo_list = dlg_srch_undo;

    Q_ASSERT((m_bgDstIdx >= 0) && (size_t(m_bgDstIdx) <= undo_list.size()));
    Q_ASSERT(m_bgDoAdd == doAdd);
    Q_ASSERT(m_bgForUndo == true);

    if (size_t(m_bgDstIdx) == undo_list.size())
        undo_list.emplace_back(UndoRedoItem(doAdd, std::vector<int>(lines_begin, lines_end)));
    else
        undo_list.back().lines.insert(undo_list.back().lines.end(), lines_begin, lines_end);

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This function is invoked at the end of background tasks which fill the
 * search list window to mark the entry on the undo list as closed (so that
 * future search matches go into a new undo element.)
 *
 * Note it's allowed that there actually were no changes (e.g. no matching
 * lines found in text), so that no item may have been added to the list.
 */
void SearchListUndo::finalizeBgChange(bool doAdd)
{
    std::vector<UndoRedoItem>& undo_list = dlg_srch_undo;

    Q_ASSERT((m_bgDstIdx >= 0) && (size_t(m_bgDstIdx) <= undo_list.size()));
    Q_ASSERT(m_bgDoAdd == doAdd);
    Q_ASSERT(m_bgForUndo == true);

    m_bgDstIdx = -1;

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This function aborts either kind of ongoing changes via background tasks,
 * i.e. either additions via "search all", or undo/redo. The function just
 * resets the "background state", but leaves undo/redo lists unchanged. This is
 * possible because each step done by background tasks keeps model and
 * undo/redo lists in sync.
 */
void SearchListUndo::abortBgChange()
{
    Q_ASSERT(m_bgDstIdx >= 0);

    m_bgDstIdx = -1;

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This function is called upon start of an undo/redo that will be executed via
 * background tasks. It prepares for combination of all following changes into
 * a single entry in the opposite list. The function does not actually add an
 * entry yet, as empty entries are not allowed in the lists.
 */
void SearchListUndo::prepareUndoRedo(bool isRedo)
{
    std::vector<UndoRedoItem>& src_list = isRedo ? dlg_srch_redo : dlg_srch_undo;
    std::vector<UndoRedoItem>& dst_list = isRedo ? dlg_srch_undo : dlg_srch_redo;

    Q_ASSERT(m_bgDstIdx < 0);
    Q_ASSERT(src_list.size() != 0);

    if (src_list.size() != 0)
    {
        m_bgDstIdx = dst_list.size();
        m_bgDoAdd = src_list.back().isAdded;
        m_bgForUndo = !isRedo;
    }
}

/**
 * This function "pops" a set of line numbers from the last entry of the undo
 * or redo list (the actual list to be used is indicated via parameter). These
 * line numbers are added to the last entry of the opposite list (or a new
 * entry is created, if this is the first call after prepareUndoRedo) and then
 * returns the same set of line numbers to the caller.  The caller has to apply
 * the change to the model (so that it remains in sync with the undo list).
 */
bool SearchListUndo::popUndoRedo(bool isRedo, bool *retDoAdd, std::vector<int>& retLines, size_t maxCount)
{
    std::vector<UndoRedoItem>& src_list = isRedo ? dlg_srch_redo : dlg_srch_undo;
    std::vector<UndoRedoItem>& dst_list = isRedo ? dlg_srch_undo : dlg_srch_redo;

    Q_ASSERT((m_bgDstIdx >= 0) && (size_t(m_bgDstIdx) <= dst_list.size()));
    Q_ASSERT(m_bgForUndo == !isRedo);

    if (src_list.size() != 0)
    {
        UndoRedoItem& src_op = src_list.back();
        bool done;

        *retDoAdd = src_op.isAdded;

        if (size_t(m_bgDstIdx) == dst_list.size())
        {
            dst_list.emplace_back(UndoRedoItem(src_op.isAdded, std::vector<int>()));
            dst_list.back().lines.reserve(src_op.lines.size());
        }
        UndoRedoItem& dst_op = dst_list.back();

        if (src_op.lines.size() > maxCount)
        {
#if 0
            // NOTE could be more efficient by removing chunks from the end of the list
            // however that would require sorting the destination list upon "finalizing"

            dst_op.lines.insert(dst_op.lines.end(), src_op.lines.begin() + start,
                                                    src_op.lines.end());
            retLines.insert(retLines.end(), src_op.lines.begin() + start,
                                            src_op.lines.end());
            src_op.lines.erase(src_op.lines.begin() + start, src_op.lines.end());
#else
            dst_op.lines.insert(dst_op.lines.end(), src_op.lines.begin(),
                                                    src_op.lines.begin() + maxCount);
            retLines.insert(retLines.end(), src_op.lines.begin(),
                                            src_op.lines.begin() + maxCount);
            src_op.lines.erase(src_op.lines.begin(),
                               src_op.lines.begin() + maxCount);
#endif
        }
        else
        {
            dst_op.lines.insert(dst_op.lines.end(), src_op.lines.begin(), src_op.lines.end());
            retLines = std::move(src_op.lines);
            src_list.pop_back();
            done = true;
        }
        return done;
    }
    Q_ASSERT(false);
    return true;
}


/**
 * This function has to be invoked at the end of background tasks which perform
 * undo/redo. The function resets the background task-related state variables.
 */
void SearchListUndo::finalizeUndoRedo(bool isRedo)
{
    std::vector<UndoRedoItem>& dst_list = isRedo ? dlg_srch_undo : dlg_srch_redo;

    Q_ASSERT((m_bgDstIdx >= 0) && (size_t(m_bgDstIdx) == dst_list.size() - 1));
    Q_ASSERT(m_bgForUndo == !isRedo);

    m_bgDstIdx = -1;

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This function is called when portions of the text in the main window have
 * been deleted to update references to text lines. The function removes all
 * references to removed lines from the undo/redo lists and prunes remaining
 * empty entries. Remaining line numbers may be shifted, if lines at the top
 * of the document were removed.
 *
 * @param top_l     First line which is NOT deleted, or 0 to delete nothing at top
 * @param bottom_l  This line and all below are removed, or -1 if none
 */
void SearchListUndo::adjustLineNums(int top_l, int bottom_l)
{
    std::vector<UndoRedoItem> tmp2;
    for (const auto& cmd : dlg_srch_undo)
    {
        std::vector<int> tmpl;
        tmpl.reserve(cmd.lines.size());

        for (int line : cmd.lines)
        {
            if ((line >= top_l) && ((line < bottom_l) || (bottom_l < 0)))
            {
                tmpl.push_back(line - top_l);
            }
        }

        if (tmpl.size() > 0)
        {
            tmp2.emplace_back(UndoRedoItem(cmd.isAdded, std::move(tmpl)));
        }
    }
    dlg_srch_undo = std::move(tmp2);
    dlg_srch_redo.clear();

    SEARCH_LIST_UNDO_INVARIANT();
}


/**
 * This query function indicates if there are any entries in the undo list, and
 * optionally the number of lines in the last entry.
 */
bool SearchListUndo::hasUndo(int *count) const
{
    if (count != nullptr)
        *count = ((dlg_srch_undo.size() != 0) ? dlg_srch_undo.back().lines.size() : 0);

    return dlg_srch_undo.size() != 0;
}


/**
 * This query function indicates if there are any entries in the redo list, and
 * optionally the number of lines in the last entry.
 */
bool SearchListUndo::hasRedo(int *count) const
{
    if (count != nullptr)
        *count = ((dlg_srch_redo.size() != 0) ? dlg_srch_redo.back().lines.size() : 0);

    return dlg_srch_redo.size() != 0;
}


/**
 * This function describes the changes represented by the last entry in either
 * the undo or redo lists. This can be used to update the text of the undo/redo
 * menu entries.
 */
std::pair<bool,int> SearchListUndo::describeFirstOp(bool forUndo)
{
    std::vector<UndoRedoItem>& undo_list = forUndo ? dlg_srch_undo : dlg_srch_redo;
    int count = 0;
    bool doAdd = false;

    if (undo_list.size() != 0)
    {
        auto prev_op = undo_list.back();

        doAdd = prev_op.isAdded;
        count = prev_op.lines.size();
    }
    return std::make_pair(doAdd, count);
}

#ifndef QT_NO_DEBUG
/**
 * This function verifies internal consistency and concistency with the model
 * data.
 *
 * Note this function assumes that undo list is updated immediately after any
 * change to the model content. This is relevant in particular when changes are
 * split in many steps via background tasks - in this case the updo/redo list
 * has to be updated after each step.
 */
void SearchListUndo::invariant() const
{
    // convert input list into set & check that it is sorted & free of duplicates
    const std::vector<int>& inLines = m_modelDebug->exportLineList();
    std::set<int> lines;

    // --- Verify UNDO list: apply undo to set of lines starting with empty buffer ---

    for (auto it = dlg_srch_undo.cbegin(); it != dlg_srch_undo.cend(); ++it)
    {
        int prev = -1;
        if (it->isAdded)
        {
            for (int line : it->lines)
            {
                Q_ASSERT(line > prev); prev = line;
                Q_ASSERT(lines.find(line) == lines.end());
                lines.insert(line);
            }
        }
        else
        {
            for (int line : it->lines)
            {
                Q_ASSERT(line > prev); prev = line;
                auto xit = lines.find(line);
                if (xit != lines.end())
                    lines.erase(xit);
                else
                    Q_ASSERT(false);
            }
        }
        Q_ASSERT(it->lines.size() != 0);
    }

    // after applying all changes the result shall be identical to current buffer contents
    Q_ASSERT(lines.size() == inLines.size());
    auto inIt = inLines.cbegin();
    for (int line : inLines)
    {
        Q_ASSERT(*(inIt++) == line);
    }

    // --- Verify REDO list: apply redo current buffer content in forward order ---

    for (auto it = dlg_srch_redo.crbegin(); it != dlg_srch_redo.crend(); ++it)
    {
        int prev = -1;
        if (it->isAdded)
        {
            for (int line : it->lines)
            {
                Q_ASSERT(line > prev); prev = line;
                Q_ASSERT(lines.find(line) == lines.end());
                lines.insert(line);
            }
        }
        else
        {
            for (int line : it->lines)
            {
                Q_ASSERT(line > prev); prev = line;
                auto xit = lines.find(line);
                if (xit != lines.end())
                    lines.erase(xit);
                else
                    Q_ASSERT(false);
            }
        }
        Q_ASSERT(it->lines.size() != 0);
    }
}
#endif

// ----------------------------------------------------------------------------

/**
 * This helper class is an "item delegate" for rendering the content of the
 * bookmarks column of the search list. The content is either empty or a blue
 * dot if the respective line is bookmarked.
 *
 * Note the dot replaces the mark-up applies to bookmarks in the main window,
 * which is not applies in the search list.  Actually text mark-up is used in
 * the main window only because the plain-text widget does not support marking
 * the line properly. (Handling could be made consistent in Qt 5.12 with
 * QTextBlockFormat::setMarker().)
 */
class SearchListDrawBok : public QAbstractItemDelegate
{
public:
    SearchListDrawBok(SearchListModel * model, MainText * mainText, Bookmarks * bookmarks);
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    int widthHint() const;
private:
    const int IMG_MARGIN = 1;
    static constexpr const char * const s_gif =  // Base64 coded GIF (89a) with alpha channel, size 7x7
        "R0lGODlhBwAHAMIAAAAAuPj8+Hh8+JiYmDAw+AAAAAAAAAAAACH"
        "5BAEAAAEALAAAAAAHAAcAAAMUGDGsSwSMJ0RkpEIG4F2d5DBTkAAAOw==";
    SearchListModel * const m_model;
    MainText * const m_mainText;
    Bookmarks * const m_bookmarks;
    QPixmap m_img;
};

SearchListDrawBok::SearchListDrawBok(SearchListModel * model, MainText * mainText, Bookmarks * bookmarks)
        : m_model(model)
        , m_mainText(mainText)
        , m_bookmarks(bookmarks)
{
    QByteArray imgData = QByteArray::fromBase64(QByteArray(s_gif));
    if (m_img.loadFromData(imgData, "GIF") == false)
        qDebug() << "failed to load internal bookmark image data\n";
}

void SearchListDrawBok::paint(QPainter *pt, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int w = option.rect.width();
    int h = option.rect.height();

    pt->save();
    pt->translate(option.rect.topLeft());
    pt->setClipRect(QRectF(0, 0, w, h));

    if (option.state & QStyle::State_Selected)
        pt->fillRect(0, 0, w, h, option.palette.color(QPalette::Highlight));
    else
        pt->fillRect(0, 0, w, h, option.palette.color(QPalette::Base));

    int line = m_model->getLineOfIdx(index.row());
    if (m_bookmarks->isBookmarked(line))
    {
        int xoff = IMG_MARGIN;
        int yoff = (h - m_img.height() - IMG_MARGIN*2) / 2;
        pt->drawPixmap(QPoint(xoff, yoff), m_img);
    }

    pt->restore();
}

QSize SearchListDrawBok::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    return QSize(m_img.width() + IMG_MARGIN*2,
                 m_img.height() + IMG_MARGIN*2);
}

int SearchListDrawBok::widthHint() const
{
    return m_img.width() + IMG_MARGIN*2;
}

// ----------------------------------------------------------------------------

/**
 * This helper class overloads the standard QTableView class to allow adding
 * key bindings.
 */
class SearchListView : public QTableView
{
public:
    SearchListView(QWidget *parent)
        : QTableView(parent)
    {
        this->resizeColumnToContents(SearchListModel::COL_IDX_BOOK);
    }
    virtual void keyPressEvent(QKeyEvent *e) override;
    void addKeyBinding(Qt::KeyboardModifier mod, Qt::Key key, const std::function<void()>& cb);

private:
    const int TXT_MARGIN = 5;
    std::unordered_map<uint32_t,const std::function<void()>> m_keyCb;

};

void SearchListView::addKeyBinding(Qt::KeyboardModifier mod, Qt::Key key, const std::function<void()>& cb)
{
    Q_ASSERT((mod & ~0x0F000000) == 0);  // SHIFT, CTRL, ALT
    Q_ASSERT((key & ~0x0FFFFFFF) == 0);

    uint32_t val = (uint32_t(mod) << 4) | uint32_t(key);
    m_keyCb.emplace(std::make_pair(val, cb));
}

void SearchListView::keyPressEvent(QKeyEvent *e)
{
    // check if this key is overridden by a registered callback
    auto it = m_keyCb.find((uint32_t(e->modifiers()) << 4) | uint32_t(e->key()));
    if (it != m_keyCb.end()) {
        (it->second)();
        return;
    }

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

        case Qt::Key_Down:
            // FIXME should move cursor to top-most line when scrolling out of view
            if (e->modifiers() == Qt::ControlModifier)
                verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->singleStep() );
            else
                QTableView::keyPressEvent(e);
            break;

        case Qt::Key_Up:
            if (e->modifiers() == Qt::ControlModifier)
                verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->singleStep() );
            else
                QTableView::keyPressEvent(e);
            break;

        case Qt::Key_Tab:
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
    auto central_wid = new QWidget();
        setCentralWidget(central_wid);
    auto layout_top = new QVBoxLayout(central_wid);
        layout_top->setContentsMargins(0, 0, 0, 0);

    tid_search_list = new BgTask(central_wid, BG_PRIO_SEARCH_LIST);

    m_showCfg = s_prevShowCfg;
    m_model = new SearchListModel(s_mainText, s_higl, s_parseSpec,
                                  m_showCfg.srchHall, m_showCfg.bookmarkMarkup);
    m_draw = new HighlightViewDelegate(m_model, false,
                                       s_mainText->getFontContent(),
                                       s_mainText->getFgColDefault(),
                                       s_mainText->getBgColDefault());
    m_drawBok = new SearchListDrawBok(m_model, s_mainText, s_bookmarks);
    m_undo = new SearchListUndo();
#ifndef QT_NO_DEBUG
    m_undo->connectModelForDebug(m_model);
#endif

    // main dialog component: table view, showing the selected lines of text
    QFontMetrics metrics(s_mainText->getFontContent());
    m_table = new SearchListView(central_wid);
        m_table->setModel(m_model);
        m_table->setShowGrid(false);
        m_table->horizontalHeader()->setVisible(false);
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setSectionResizeMode(SearchListModel::COL_IDX_TXT, QHeaderView::Stretch);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_table->verticalHeader()->setDefaultSectionSize(metrics.height());
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setItemDelegateForColumn(SearchListModel::COL_IDX_BOOK, m_drawBok);
        m_table->setItemDelegateForColumn(SearchListModel::COL_IDX_TXT, m_draw);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_table->setColumnWidth(SearchListModel::COL_IDX_BOOK, m_drawBok->widthHint());
        configureColumnVisibility();
        connect(m_table, &QAbstractItemView::customContextMenuRequested, this, &SearchList::showContextMenu);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchList::selectionChanged);
        layout_top->addWidget(m_table);

    // Progress bar overlay: made visible during long operations via background tasks
    m_hipro = new QProgressBar(m_table);
        m_hipro->setOrientation(Qt::Horizontal);
        m_hipro->setTextVisible(true);
        m_hipro->setMinimum(0);
        m_hipro->setMaximum(100);
        m_hipro->setVisible(false);

    // Status line overlay: used to display notification text
    m_stline = new StatusLine(m_table);

    if (!s_winGeometry.isEmpty() && !s_winState.isEmpty())
    {
        this->restoreGeometry(s_winGeometry);
        this->restoreState(s_winState);
    }
    else
    {
        // set reasonable default size when starting without RC file
        QDesktopWidget dw;
        this->resize(dw.width()*0.75, dw.height()*0.33);
    }

    connect(s_mainText, &MainText::textFontChanged, this, &SearchList::mainFontChanged);
    connect(s_mainWin, &MainWin::documentNameChanged, this, &SearchList::mainDocNameChanged);
    mainDocNameChanged();  // set window title initially

    populateMenus();

    // overriding key bindings of the table view widget
    m_table->addKeyBinding(Qt::ControlModifier, Qt::Key_G, [=](){ SearchList::cmdDisplayStats(); });
    m_table->addKeyBinding(Qt::NoModifier,      Qt::Key_M, [=](){ cmdToggleBookmark(); });
    m_table->addKeyBinding(Qt::NoModifier,      Qt::Key_N, [=](){ cmdSearchNext(true); });
    m_table->addKeyBinding(Qt::ShiftModifier,   Qt::Key_N, [=](){ cmdSearchNext(false); });
    m_table->addKeyBinding(Qt::NoModifier,      Qt::Key_Slash, [=](){ cmdNewSearch(true); });
    m_table->addKeyBinding(Qt::ShiftModifier,   Qt::Key_Slash, [=](){ cmdNewSearch(true); });  // don't care SHIFT or not
    m_table->addKeyBinding(Qt::NoModifier,      Qt::Key_Question, [=](){ cmdNewSearch(false); });
    m_table->addKeyBinding(Qt::ShiftModifier,   Qt::Key_Question, [=](){ cmdNewSearch(false); });  // don't care SHIFT or not
    m_table->addKeyBinding(Qt::NoModifier,      Qt::Key_U, [=](){ cmdUndo(); });
    m_table->addKeyBinding(Qt::ControlModifier, Qt::Key_R, [=](){ cmdRedo(); });

    // global keyboard shortcuts
    auto act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT | Qt::Key_0));
        connect(act, &QAction::triggered, this, &SearchList::cmdSetDeltaColRoot);
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT | Qt::Key_N));
        connect(act, &QAction::triggered, [=](){ this->cmdSearchNext(true); });
        central_wid->addAction(act);
    act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::ALT | Qt::Key_P));
        connect(act, &QAction::triggered, [=](){ this->cmdSearchNext(false); });
        central_wid->addAction(act);

#if 0
    wt.dlg_srch_f1_l.bind("<Control-plus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(1)))
    wt.dlg_srch_f1_l.bind("<Control-minus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(-1)))
    wt.dlg_srch_f1_l.bind("<space>", lambda e:SearchList_SelectionChange(dlg_srch_sel.TextSel_GetSelection()))
    wt.dlg_srch_f1_l.bind("<Alt-Key-h>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("highlight")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-f>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_fn")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-t>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_tick")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-d>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("tick_delta")))
#endif

    m_table->setFocus(Qt::ShortcutFocusReason);
    this->show();
}


/**
 * Destructor: Freeing resources not automatically deleted via widget tree
 */
SearchList::~SearchList()
{
    s_prevShowCfg = m_showCfg;
    searchAbort(false);
    delete m_model;
    delete m_draw;
    delete m_drawBok;
    delete m_undo;
    delete tid_search_list;
}


/**
 * This overriding function is called when the dialog window receives the close
 * event. The function stops background processes and destroys the class
 * instance to release all resources. The event is always accepted.
 */
void SearchList::closeEvent(QCloseEvent * event)
{
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();
    ConfigFile::updateRcAfterIdle();

    event->accept();

    s_instance->deleteLater();
    s_instance = nullptr;
}


/**
 * This slot is connected to the "Close" menu action. It closes the dialog in
 * the same way as via the X button in the window title bar.
 */
void SearchList::cmdClose(bool)
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
void SearchList::mainFontChanged()
{
    QFontMetrics metrics(s_mainText->getFontContent());
    m_table->verticalHeader()->setDefaultSectionSize(metrics.height());

    m_model->forceRedraw(SearchListModel::COL_IDX_TXT);
}

/**
 * This slot is connected to notification of main document file changes. This
 * is used to update the dialog window title. (Note before this event, the
 * dialog is informed separately about discarding the content of the previous
 * document.)
 */
void SearchList::mainDocNameChanged()
{
    const QString& fileName = s_mainWin->getFilename();
    if (!fileName.isEmpty())
        this->setWindowTitle("Search matches - " + fileName);
    else
        this->setWindowTitle("Search matches - trowser");
}


/**
 * This sub-function of the constructor populates the menu bar with actions.
 */
void SearchList::populateMenus()
{
    auto men = menuBar()->addMenu("&Control");
    auto act = men->addAction("Load line numbers...");
        connect(act, &QAction::triggered, this, &SearchList::cmdLoadFrom);
    act = men->addAction("Save text as...");
        connect(act, &QAction::triggered, [=](){ cmdSaveFileAs(false); });
    act = men->addAction("Save line numbers...");
        connect(act, &QAction::triggered, [=](){ cmdSaveFileAs(true); });
    men->addSeparator();
    act = men->addAction("Clear all");
        connect(act, &QAction::triggered, this, &SearchList::cmdClearAll);
    act = men->addAction("Close");
        connect(act, &QAction::triggered, this, &SearchList::cmdClose);

    men = menuBar()->addMenu("&Edit");
        connect(men, &QMenu::aboutToShow, this, &SearchList::editMenuAboutToShow);
    m_menActUndo = men->addAction("Undo");
        m_menActUndo->setShortcut(QKeySequence(Qt::Key_U));
        connect(m_menActUndo, &QAction::triggered, this, &SearchList::cmdUndo);
    m_menActRedo = men->addAction("Redo");
        m_menActRedo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
        connect(m_menActRedo, &QAction::triggered, this, &SearchList::cmdRedo);
    m_menActAbort = men->addAction("Abort ongoing operation");
        m_menActAbort->setShortcut(QKeySequence(Qt::Key_Escape));
        connect(m_menActAbort, &QAction::triggered, [=](){ searchAbort(false); });
    men->addSeparator();
    act = men->addAction("Import lines selected in main window");
        connect(act, &QAction::triggered, [=](){ copyCurrentLine(true); });
    act = men->addAction("Remove lines selected in main window");
        connect(act, &QAction::triggered, [=](){ copyCurrentLine(false); });
    men->addSeparator();
    m_menActRemove = men->addAction("Remove selected lines");
        m_menActRemove->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(m_menActRemove, &QAction::triggered, this, &SearchList::cmdRemoveSelection);
    men->addSeparator();
    act = men->addAction("Add main window search matches");
        connect(act, &QAction::triggered, [=](){ addMatches(0); });
    act = men->addAction("Remove main window search matches");
        connect(act, &QAction::triggered, [=](){ removeMatches(0); });

    men = menuBar()->addMenu("&Search");
    act = men->addAction("Search history...");
        connect(act, &QAction::triggered, [=](){ DlgHistory::openDialog(); });
    act = men->addAction("Edit highlight patterns...");
        connect(act, &QAction::triggered, [=](){ DlgHigl::openDialog(s_higl, s_search, s_mainText, s_mainWin); });
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


    men = menuBar()->addMenu("&Options");
    act = men->addAction("Highlight all search matches");
        act->setCheckable(true);
        act->setChecked(m_showCfg.srchHall);
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_H));
        connect(act, &QAction::triggered, this, &SearchList::cmdToggleSearchHighlight);
    act = men->addAction("Show bookmark mark-up");
        act->setCheckable(true);
        act->setChecked(m_showCfg.bookmarkMarkup);
        connect(act, &QAction::triggered, this, &SearchList::cmdToggleBookmarkMarkup);
    men->addSeparator();
    act = men->addAction("Show line number");
        act->setCheckable(true);
        act->setChecked(m_showCfg.lineIdx);
        connect(act, &QAction::triggered, this, &SearchList::cmdToggleShowLineNumber);
    m_actShowLineDelta = men->addAction("Show line number delta");
        m_actShowLineDelta->setCheckable(true);
        m_actShowLineDelta->setChecked(m_showCfg.lineDelta);
        connect(m_actShowLineDelta, &QAction::triggered, this, &SearchList::cmdToggleShowLineDelta);

    m_actShowCustomVal = men->addAction("Show custom column");
        m_actShowCustomVal->setCheckable(true);
        m_actShowCustomVal->setVisible(false);
        connect(m_actShowCustomVal, &QAction::triggered, [=](bool v){ cmdToggleShowCustom(ParseColumnFlags::Val, v); });
    m_actShowCustomValDelta = men->addAction("Show custom column");
        m_actShowCustomValDelta->setCheckable(true);
        m_actShowCustomValDelta->setVisible(false);
        connect(m_actShowCustomValDelta, &QAction::triggered, [=](bool v){ cmdToggleShowCustom(ParseColumnFlags::ValDelta, v); });
    m_actShowCustomFrm = men->addAction("Show custom column");
        m_actShowCustomFrm->setCheckable(true);
        m_actShowCustomFrm->setVisible(false);
        connect(m_actShowCustomFrm, &QAction::triggered, [=](bool v){ cmdToggleShowCustom(ParseColumnFlags::Frm, v); });
    m_actShowCustomFrmDelta = men->addAction("Show custom column");
        m_actShowCustomFrmDelta->setCheckable(true);
        m_actShowCustomFrmDelta->setVisible(false);
        connect(m_actShowCustomFrmDelta, &QAction::triggered, [=](bool v){ cmdToggleShowCustom(ParseColumnFlags::FrmDelta, v); });

    men->addSeparator();
    act = men->addAction("Select line as origin for line number delta");
        connect(act, &QAction::triggered, this, &SearchList::cmdSetLineIdxRoot);
    m_actRootCustomValDelta = men->addAction("Select line as origin for custom val");
        connect(m_actRootCustomValDelta, &QAction::triggered, [=](){ cmdSetCustomColRoot(ParseColumnFlags::ValDelta); });
    m_actRootCustomFrmDelta = men->addAction("Select line as origin for custom frame boundary");
        connect(m_actRootCustomFrmDelta, &QAction::triggered, [=](){ cmdSetCustomColRoot(ParseColumnFlags::FrmDelta); });
        // shortcut ALT-0 is shared with other columns and thus has different callback

    men->addSeparator();
    act = men->addAction("Configure custom column with parsed values...");
        connect(act, &QAction::triggered, this, &SearchList::cmdOpenParserConfig);

    configureCustomMenuActions();
}


/**
 * This function adapts the text and state of menu entries related to custom
 * column configuration. This function is called initially and after
 * configuration changes.
 */
void SearchList::configureCustomMenuActions()
{
    auto custFlags = m_model->getCustomColumns();

    m_actShowCustomVal->setText("Show custom: " + s_parseSpec.getHeader(ParseColumnFlags::Val));
    m_actShowCustomVal->setChecked(m_showCfg.custom & ParseColumnFlags::Val);
    m_actShowCustomVal->setVisible(custFlags & ParseColumnFlags::Val);

    m_actShowCustomValDelta->setText("Show custom: Delta " + s_parseSpec.getHeader(ParseColumnFlags::Val));
    m_actShowCustomValDelta->setChecked(m_showCfg.custom & ParseColumnFlags::ValDelta);
    m_actShowCustomValDelta->setVisible(custFlags & ParseColumnFlags::ValDelta);

    m_actShowCustomFrm->setText("Show custom: " + s_parseSpec.getHeader(ParseColumnFlags::Frm));
    m_actShowCustomFrm->setChecked(m_showCfg.custom & ParseColumnFlags::Frm);
    m_actShowCustomFrm->setVisible(custFlags & ParseColumnFlags::Frm);

    m_actShowCustomFrmDelta->setText("Show custom: Delta " + s_parseSpec.getHeader(ParseColumnFlags::Frm));
    m_actShowCustomFrmDelta->setChecked(m_showCfg.custom & ParseColumnFlags::FrmDelta);
    m_actShowCustomFrmDelta->setVisible(custFlags & ParseColumnFlags::FrmDelta);

    m_actRootCustomValDelta->setText("Select line as origin for custom: Delta " + s_parseSpec.getHeader(ParseColumnFlags::Val));
    m_actRootCustomValDelta->setVisible(custFlags & ParseColumnFlags::ValDelta);

    m_actRootCustomFrmDelta->setText("Select line as origin for custom: Delta " + s_parseSpec.getHeader(ParseColumnFlags::Frm));
    m_actRootCustomFrmDelta->setVisible(custFlags & ParseColumnFlags::FrmDelta);
}


/**
 * This function is called when the "edit" menu in the search list dialog is opened.
 */
void SearchList::editMenuAboutToShow()
{
    if (m_undo->hasUndo())
    {
        auto info = m_undo->describeFirstOp(true);
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

    // following is copy/paste from above except for "Redo" instead of "Undo"
    if (m_undo->hasRedo())
    {
        auto info = m_undo->describeFirstOp(false);
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

    m_menActAbort->setEnabled(tid_search_list->isActive());
    m_menActRemove->setEnabled(m_table->selectionModel()->hasSelection());
}


/**
 * This slot is connected to context menu requests, as custom context menu is
 * configured for the table view. The function creates and executes context
 * menu.
 */
void SearchList::showContextMenu(const QPoint& pos)
{
    auto menu = new QMenu("Search list actions", this);
    auto sel = m_table->selectionModel()->selectedRows();
    bool sel_size_1 = (sel.size() == 1);

    auto act = menu->addAction("Select line as origin for line number delta");
        act->setEnabled(sel_size_1);  // && m_showCfg.lineDelta
        connect(act, &QAction::triggered, this, &SearchList::cmdSetLineIdxRoot);

    if (m_actRootCustomValDelta->isVisible())
    {
        act = menu->addAction(m_actRootCustomValDelta->text());
        act->setEnabled(sel_size_1); // && (m_showCfg.custom & ParseColumnFlags::ValDelta)
        connect(act, &QAction::triggered, [=](){ cmdSetCustomColRoot(ParseColumnFlags::ValDelta); });
    }
    if (m_actRootCustomFrmDelta->isVisible())
    {
        // Note: error if this line has no numerical value is handled by callback
        act = menu->addAction(m_actRootCustomFrmDelta->text());
        act->setEnabled(sel_size_1); // && (m_showCfg.custom & ParseColumnFlags::FrmDelta)
        connect(act, &QAction::triggered, [=](){ cmdSetCustomColRoot(ParseColumnFlags::FrmDelta); });
    }

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


/**
 * This function configures the model after changes to column visbility.
 * Additionally it controls visibility of the horizontal header, depending on
 * the number of non-standard columns being shown.
 */
void SearchList::configureColumnVisibility()
{
    if (m_showCfg.lineIdx)
        m_table->showColumn(SearchListModel::COL_IDX_LINE);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_LINE);

    if (m_showCfg.lineDelta)
        m_table->showColumn(SearchListModel::COL_IDX_LINE_D);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_LINE_D);

    if (m_showCfg.custom & ParseColumnFlags::Val)
        m_table->showColumn(SearchListModel::COL_IDX_CUST_VAL);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_CUST_VAL);

    if (m_showCfg.custom & ParseColumnFlags::ValDelta)
        m_table->showColumn(SearchListModel::COL_IDX_CUST_VAL_DELTA);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_CUST_VAL_DELTA);

    if (m_showCfg.custom & ParseColumnFlags::Frm)
        m_table->showColumn(SearchListModel::COL_IDX_CUST_FRM);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_CUST_FRM);

    if (m_showCfg.custom & ParseColumnFlags::FrmDelta)
        m_table->showColumn(SearchListModel::COL_IDX_CUST_FRM_DELTA);
    else
        m_table->hideColumn(SearchListModel::COL_IDX_CUST_FRM_DELTA);

    // any non-default columns shown -> display header
    if (m_showCfg.lineDelta || m_showCfg.lineIdx || (m_showCfg.custom != ParseColumns(0)))
        m_table->horizontalHeader()->setVisible(true);
    else
        m_table->horizontalHeader()->setVisible(false);
}

void SearchList::cmdToggleSearchHighlight(bool checked)
{
    m_showCfg.srchHall = checked;
    m_model->showSearchHall(m_showCfg.srchHall);
    m_model->forceRedraw(SearchListModel::COL_IDX_TXT);
}

void SearchList::cmdToggleBookmarkMarkup(bool checked)
{
    m_showCfg.bookmarkMarkup = checked;
    m_model->showBookmarkMarkup(m_showCfg.bookmarkMarkup);
    m_model->forceRedraw(SearchListModel::COL_IDX_BOOK);
}

void SearchList::cmdToggleShowLineNumber(bool checked)
{
    m_showCfg.lineIdx = checked;
    configureColumnVisibility();
}

void SearchList::cmdToggleShowLineDelta(bool checked)
{
    m_showCfg.lineDelta = checked;
    configureColumnVisibility();
}

void SearchList::cmdToggleShowCustom(ParseColumnFlags col, bool checked)
{
    if (checked)
        m_showCfg.custom |= col;
    else
        m_showCfg.custom ^= (m_showCfg.custom & col);

    configureColumnVisibility();
}

void SearchList::cmdSetLineIdxRoot(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        int line = m_model->getLineOfIdx(sel.front().row());
        m_model->setLineDeltaRoot(line);

        // auto-enable the column if not shown yet
        if (m_showCfg.lineDelta == false)
            m_actShowLineDelta->activate(QAction::Trigger);
    }
    else
        m_stline->showError("menu", "Please select a line in the list first");
}

void SearchList::cmdSetCustomColRoot(ParseColumnFlags col)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        int line = m_model->getLineOfIdx(sel.front().row());
        auto tblCol = (col == ParseColumnFlags::ValDelta)
                        ? SearchListModel::COL_IDX_CUST_VAL_DELTA
                        : SearchListModel::COL_IDX_CUST_FRM_DELTA;

        if (m_model->setCustomDeltaRoot(tblCol, line))
        {
            // auto-enable the column if not shown yet
            if ((m_showCfg.custom & col) == 0)
            {
                if (col == ParseColumnFlags::ValDelta)
                    m_actShowCustomValDelta->activate(QAction::Trigger);
                else
                    m_actShowCustomFrmDelta->activate(QAction::Trigger);
            }
        }
        else
            m_stline->showError("menu", "Parsing did not yield a numerical value for this line");
    }
    else
        m_stline->showError("menu", "Please select a line in the list first");
}


/**
 * This function is bound to ALT-0 in the search result list and to the
 * "Select root FN" context menu command. The function sets the currently
 * selected line as origin for frame number delta calculations and enables
 * frame number delta display, which requires a complete refresh of the list.
 */
void SearchList::cmdSetDeltaColRoot(bool)
{
    bool done = false;

    // auto-enable line index if no delta column is configured
    if (m_showCfg.lineDelta || (m_model->getCustomColumns() == ParseColumns(0)))
    {
        cmdSetLineIdxRoot(true);   // dummy parameter
        done = true;
    }

    if (m_showCfg.custom & ParseColumnFlags::ValDelta)
    {
        cmdSetCustomColRoot(ParseColumnFlags::ValDelta);
        done = true;
    }
    if (m_showCfg.custom & ParseColumnFlags::FrmDelta)
    {
        cmdSetCustomColRoot(ParseColumnFlags::FrmDelta);
        done = true;
    }

    if (done == false)
    {
        m_stline->showError("menu", "Please enable a delta column via Options menu first");
    }
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
 * This function is bound to the "m" key in the search list dialog. The
 * function adds or removes a bookmark on the currently selected line (but only
 * if exactly one line is selected.)
 */
void SearchList::cmdToggleBookmark()
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        int line = m_model->getLineOfIdx(sel.front().row());
        s_bookmarks->toggleBookmark(line);
    }
    else
    {
        if (sel.size() == 0)
            m_stline->showError("search", "No line is selected - cannot place bookmark");
        else
            m_stline->showError("search", "More than one line is selected - no bookmark set");
    }
}


/**
 * This helper function searches for the given pattern in the document, but
 * only within blocks that are also included in the search list. The search
 * start at the line following the given one in the given direction.
 */
int SearchList::searchAtomicInList(const SearchPar& pat, int line, bool is_fwd)
{
    QTextBlock block = s_mainText->document()->findBlockByNumber(line);
    int textPos = is_fwd ? (block.position() + block.length())
                         : (block.position() - 1);
    if (textPos >= 0)
    {
        auto finder = MainTextFind::create(s_mainText->document(), pat, is_fwd, textPos);

        while (!finder->isDone())
        {
            QTextBlock block;
            int matchPos, matchLen;

            // FIXME optimize to search only within blocks of lines that are actually in the list
            if (finder->findNext(matchPos, matchLen, &block))
            {
                int line = block.blockNumber();
                int idx;
                if (m_model->getLineIdx(line, idx))
                    return idx;
            }
        }
    }
    return -1;
}

/**
 * This function is bound to "n", "N" in the search filter dialog. The function
 * starts a search for the last-used search pattern, but only considers matches
 * on lines in the search list.
 */
void SearchList::cmdSearchNext(bool is_fwd)
{
    m_stline->clearMessage("search");

    SearchPar par = s_search->getCurSearchParams();
    if (!par.m_pat.isEmpty() && s_search->searchExprCheck(par, false))
    {
        auto midx = m_table->currentIndex();
        if (midx.isValid())
        {
            int line = m_model->getLineOfIdx(midx.row());
            int idx = searchAtomicInList(par, line, is_fwd);
            if (idx >= 0)
            {
                s_mainText->cursorJumpPushPos();

                m_table->selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);
                QModelIndex midx = m_model->index(idx, 0);
                m_table->setCurrentIndex(midx);
            }
            else if (is_fwd)
                m_stline->showWarning("search", "No match until end of search result list");
            else
                m_stline->showWarning("search", "No match until start of search result list");
        }
        else
            m_stline->showWarning("search", "Select a line in the list where to start searching");
    }
    else
        m_stline->showError("search", "No (valid) pattern defined for search repeat");
}


/**
 * This function is bound to "/", "?" in the search filter dialog. The function
 * changes focus to the pattern entry field in the main window. After entering
 * a pattern, search is started in the main window (i.e. not the search list!)
 * and focus returns to the search list dialog.
 */
void SearchList::cmdNewSearch(bool is_fwd)
{
    s_search->searchEnter(is_fwd, m_table);
    s_mainWin->activateWindow();
}


/**
 * This function is bound to the "Undo" menu command any keyboard shortcut.
 * This reverts the last modification of the line list (i.e. last removal or
 * addition, either via search or manually.)
 */
void SearchList::cmdUndo()
{
    m_stline->clearMessage("search");

    int origCount;
    if (m_undo->hasUndo(&origCount))
    {
        if (searchAbort())
        {
            bool isRedo = false;
            m_undo->prepareUndoRedo(isRedo);
            tid_search_list->start([=](){ bgUndoRedoLoop(isRedo, origCount); });
        }
    }
    else
    {
        m_stline->showError("search", "Already at oldest change in search list");
    }
}


/**
 * This external interface is intended for handling "undo" key binding in the
 * main text window. The call is ignored when the dialog window is not
 * currently open.
 */
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
    m_stline->clearMessage("search");
    int origCount;
    if (m_undo->hasRedo(&origCount))
    {
        if (searchAbort())
        {
            bool isRedo = true;
            m_undo->prepareUndoRedo(isRedo);
            tid_search_list->start([=](){ bgUndoRedoLoop(isRedo, origCount); });
        }
    }
    else
    {
        m_stline->showError("search", "Already at newest change in search list");
    }
}


/**
 * This external interface is intended for handling "redo" key binding in the
 * main text window. The call is ignored when the dialog window is not
 * currently open.
 */
void SearchList::extRedo()  /*static*/
{
    if (s_instance != 0)
        s_instance->cmdRedo();
}


/**
 * This function acts as background process for undo and redo operations.
 * Each iteration of this task works on at most 250-500 lines.
 */
void SearchList::bgUndoRedoLoop(bool isRedo, int origCount)
{
    auto anchor = getViewAnchor();

    const size_t CHUNK_SIZE = 1000;
    std::vector<int> lines;
    bool doAdd;
    bool done = m_undo->popUndoRedo(isRedo, &doAdd, lines, CHUNK_SIZE);

    if (doAdd != isRedo)
    {
        // undo insertion OR redo removal -> delete lines
        m_model->removeLines(lines);
    }
    else
    {
        // undo addition OR redo insertion -> add lines
        m_model->insertLines(lines);
    }

    // select previously selected line again
    seeViewAnchor(anchor);

    if (!done)
    {
        // create or update the progress bar
        int restCount;
        isRedo ? m_undo->hasRedo(&restCount) : m_undo->hasUndo(&restCount);  //ternary result unused
        searchProgress(100 * (origCount - restCount) / origCount);

        tid_search_list->start([=](){ bgUndoRedoLoop(isRedo, origCount); });
    }
    else
    {
        m_undo->finalizeUndoRedo(isRedo);

        closeAbortDialog();
        searchProgress(100);
    }
}


/**
 * Wrapper functions for simplifying external interfaces
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
 * This function is the external interface to the search list for adding or
 * removing lines matching the given search pattern.  The search is performed
 * in a background task, i.e. it's not completed when this function returns.
 */
void SearchList::searchMatches(bool do_add, int direction, const SearchPar& par)
{
    if (!par.m_pat.isEmpty())
    {
        if (s_search->searchExprCheck(par, true))
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
        m_undo->prepareBgChange(do_add);

        tid_search_list->start([=](){ bgSearchLoop(pat_list, do_add, direction, textPos, 0); });
    }
}


/**
 * This function acts as background process to fill the search list window.
 * The search loop continues for at most 100ms, then the function re-schedules
 * itself as idle task.
 */
void SearchList::bgSearchLoop(const std::vector<SearchPar> pat_list, bool do_add, int direction, int textPos, int pat_idx)
{
    auto anchor = getViewAnchor();
    qint64 start_t = QDateTime::currentMSecsSinceEpoch();
    std::vector<int> line_list;
    std::vector<int> idx_list;
    auto finder = MainTextFind::create(s_mainText->document(), pat_list[pat_idx], direction>=0, textPos);

    while (!finder->isDone())
    {
        QTextBlock block;
        int matchPos, matchLen;
        if (finder->findNext(matchPos, matchLen, &block))
        {
            int line = block.blockNumber();
            int idx;
            bool found = m_model->getLineIdx(line, idx);
            if (do_add && !found)
            {
                line_list.push_back(line);
                idx_list.push_back(idx);
            }
            else if (!do_add && found)
            {
                line_list.push_back(line);
                idx_list.push_back(idx);
            }
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

        m_undo->appendBgChange(do_add, line_list.begin(), line_list.end());
        // select previously selected line again
        seeViewAnchor(anchor);
    }

    if (!finder->isDone())
    {
        textPos = finder->nextStartPos();

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
            int thresh = std::min(s_mainText->textCursor().position(), 1);
            ratio = 1 - (double(textPos) / thresh);
        }
        else
        {
            int thresh = s_mainText->textCursor().position();
            if (thresh < textLength)
                ratio = double(textPos) / (textLength - thresh);
            else
                ratio = 1;
        }
        searchProgress(100 * (ratio + pat_idx) / pat_list.size());

        tid_search_list->start([=](){ bgSearchLoop(pat_list, do_add, direction, textPos, pat_idx); });
    }
    else  // done
    {
        m_undo->finalizeBgChange(do_add);
        pat_idx += 1;
        if (size_t(pat_idx) < pat_list.size())
        {
            m_undo->prepareBgChange(do_add);
            int textPos = ((direction == 0) ? 0 : s_mainText->textCursor().position());

            tid_search_list->start([=](){ bgSearchLoop(pat_list, do_add, direction, textPos, pat_idx); });
        }
        else
        {
            searchProgress(100);
            closeAbortDialog();
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
 * This function stops a possibly ongoing background search in the search
 * list dialog. Optionally the user is asked it he really wants to abort.
 * The function returns 0 and does not abort the background action if the
 * user selects "Cancel", else it returns 1.  The caller MUST check the
 * return value if parameter "do_warn" is TRUE.
 */
bool SearchList::searchAbort(bool do_warn)
{
    bool cancel_new = false;

    if (tid_search_list->isActive())
    {
        if (do_warn)
        {
            // should never happen as dialog is modal (i.e. user cannot trigger another action)
            Q_ASSERT(m_abortDialog == nullptr);

            m_abortDialog = new
                QMessageBox(QMessageBox::Question,
                            "Confirm abort of search",
                            "This command will abort the ongoing search operation.\n"
                                "Please confirm, or wait until this message disappears.",
                            QMessageBox::NoButton,
                            this);
            auto cancelBut = m_abortDialog->addButton(QMessageBox::Cancel);
                cancelBut->setText("Cancel new");
            auto abortBut = m_abortDialog->addButton(QMessageBox::Ok);
                abortBut->setText("Abort ongoing");
            m_abortDialog->setDefaultButton(QMessageBox::NoButton);
            m_abortDialog->setEscapeButton(QMessageBox::Cancel);

            QAbstractButton * m_abortButton = nullptr;
            connect(m_abortDialog, &QMessageBox::buttonClicked,
                    [=, &m_abortButton](QAbstractButton * b){ m_abortButton = b; });

            m_abortDialog->exec();
            // do not use exec result as it indicates cancel also after external deletion
            cancel_new = (m_abortButton == cancelBut);

            // ATTN m_abortDialog may be NULL here due to background activity
            if (m_abortDialog != nullptr)
            {
                m_abortDialog->deleteLater();
                m_abortDialog = nullptr;
            }
        }
        else
            closeAbortDialog();
    }

    if (!cancel_new && tid_search_list->isActive())
    {
        // reset undo/redo state of any kind of bg task (i.e. search or undo/redo)
        // note partially done undo/redo may remain (i.e. former single entry is split in two)
        m_undo->abortBgChange();

        // stop the background process
        tid_search_list->stop();

        // remove the progress bar
        searchProgress(100);

        m_stline->showWarning("search", "Search list operation was aborted");
    }

    return !cancel_new;
}


void SearchList::closeAbortDialog()
{
    if (m_abortDialog != nullptr)
    {
        QCoreApplication::postEvent(m_abortDialog, new QCloseEvent());
        m_abortDialog->deleteLater();
        m_abortDialog = nullptr;
    }
}


/**
 * This helper function is called before modifications of the search result
 * list by the various background tasks to determine a line which can serve
 * as "anchor" for the view, i.e. which will be made visible again after the
 * insertions or removals (which may lead to scrolling.)
 */
SearchList::ListViewAnchor SearchList::getViewAnchor()
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
    return std::make_pair(haveSel, line);
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
 * This function inserts or removes lines selected in the main window (without
 * selection that's the line holding the cursor) into the search result list.
 * It's bound to "i" and Delete key presses in the main window respectively.
 */
void SearchList::copyCurrentLine(bool doAdd)
{
    if (searchAbort())
    {
        m_table->selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);

        auto c = s_mainText->textCursor();
        int endPos;
        if (c.position() <= c.anchor())
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

        while (c.position() <= endPos)
        {
            int line = c.blockNumber();
            int idx;
            bool found = m_model->getLineIdx(line, idx);
            if (doAdd && !found)
            {
                line_list.push_back(line);
                idx_list.push_back(idx);
            }
            else if (!doAdd && found)
            {
                line_list.push_back(line);
                idx_list.push_back(idx);
            }

            c.movePosition(QTextCursor::NextBlock);
            if (c.atEnd())
                break;
        }

        if (line_list.size() > 0)
        {
            if (doAdd)
            {
                m_model->insertLinePreSorted(line_list, idx_list);
            }
            else
            {
                std::reverse(idx_list.begin(), idx_list.end());
                m_model->removeLinePreSorted(idx_list);
            }

            m_undo->appendChange(doAdd, line_list);

            if (doAdd && (line_list.size() <= 1000))
            {
                QItemSelection sel;
                for (int idx : idx_list)  // NOTE idx_list modified by insertLinePreSorted()
                {
                    QModelIndex midx = m_model->index(idx, 0);
                    sel.select(midx, midx);
                }
                m_ignoreSelCb = idx_list.front();

                m_table->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }

            QModelIndex midx = m_model->index(idx_list.back(), 0);
            m_table->scrollTo(midx);
        }
    }
}


/**
 * This external interface function is called out of the bookmark manager when
 * a bookmark is added or removed for the given line.  If the search list
 * dialog is not open the function does nothing. Else it forces a redraw of the
 * bookmark column in the given line, or all lines in case the parameter is -1.
 */
void SearchList::signalBookmarkLine(int line)  /*static*/
{
    if (s_instance != nullptr)
    {
        // note line parameter may be -1 for "all"
        s_instance->m_model->forceRedraw(SearchListModel::COL_IDX_BOOK, line);

        // actually needed only if (m_showCfg.bookmarkMarkup)
        s_instance->m_model->forceRedraw(SearchListModel::COL_IDX_TXT, line);
    }
}


/**
 * This external interface function is called out of the main window's
 * highlight loop for every line to which a search-match highlight is applied.
 * The function forces a redraw of the text column in the given line so that
 * the changed highlighting is applied. (The model internally queries the main
 * window highlighting database so no local state change is required.)
 */
void SearchList::signalHighlightLine(int line)  /*static*/
{
    if (s_instance != nullptr)
    {
        // not checking "m_showCfg.srchHall" - call will have no effect
        s_instance->m_model->forceRedraw(SearchListModel::COL_IDX_TXT, line);
    }
}

/**
 * This external interface function is called by the highlighter module after
 * bulk changes to the highlight pattern list. The function forces a redraw of
 * the text content, so that it is redrawn with up-to date highlighting.
 */
void SearchList::signalHighlightReconfigured()  /*static*/
{
    if (s_instance != nullptr)
    {
        s_instance->m_model->forceRedraw(SearchListModel::COL_IDX_TXT);
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
        {
            m_table->selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);
            m_ignoreSelCb = idx; // repeat due to callback caused by clearing selection

            QModelIndex midx = m_model->index(idx, 0);
            m_table->setCurrentIndex(midx);
        }
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
            // WA for ignoring callback triggered by matchViewInt()
        }
        else
        {
            int line = m_model->getLineOfIdx(sel.front().row());
            if (line >= 0)
            {
                s_search->highlightFixedLine(line);
                DlgBookmarks::matchView(line);
            }
        }
    }
    m_ignoreSelCb = -1;
}


/**
 * This function is called via the static external interface for active
 * instances when portions of the text in the main window have been deleted to
 * update references to text lines. Parameter meaning:
 *
 * @param top_l     First line which is NOT deleted, or 0 to delete nothing at top
 * @param bottom_l  This line and all below are removed, or -1 if none
 */
void SearchList::adjustLineNumsInt(int top_l, int bottom_l)
{
    searchAbort(false);

    m_model->adjustLineNums(top_l, bottom_l);
    m_undo->adjustLineNums(top_l, bottom_l);
}


/**
 * This static external interface notifies about removal of text from the main
 * text document. The call is forwarded to active instances of the class.
 */
void SearchList::adjustLineNums(int top_l, int bottom_l)  /*static*/
{
    if (s_instance != nullptr)
    {
        s_instance->adjustLineNumsInt(top_l, bottom_l);
    }
}


/**
 * This helper function stores all text lines in the search result window into
 * a file. Depending on the option, either only file line number or line number
 * and respective text is stored. The user is notified about I/O errors via
 * modal dialog.
 *
 * NOTE: Line numbering starts at 1 for the first line in the buffer,
 * equivalently to the content shown in the line number column.
 */
void SearchList::saveFile(const QString& fileName, bool lnum_only)
{
    auto fh = std::make_unique<QFile>(QString(fileName));
    if (fh->open(QFile::WriteOnly | QFile::Text))
    {
        QTextStream out(fh.get());

        if (lnum_only)
        {
            // save line numbers only (i.e. line numbers in main window)
            for (int line : m_model->exportLineList())
                out << (line + 1) << '\n';
        }
        else
        {
            auto doc = s_mainText->document();

            // save text content
            for (int line : m_model->exportLineList())
            {
                QTextBlock block = doc->findBlockByNumber(line);
                out << (line + 1) << '\t' << block.text() << '\n';
            }
        }
        out << flush;
        if (out.status() != QTextStream::Ok)
        {
            QString msg = QString("Error writing file ") + fileName + ": " + fh->errorString();
            QMessageBox::warning(this, "trowser", msg, QMessageBox::Ok);
        }
    }
    else
    {
        QString msg = QString("Error creating file ") + fileName + ": " + fh->errorString();
        QMessageBox::critical(this, "trowser", msg, QMessageBox::Ok);
    }
}


/**
 * This function is called by the "Save as..." menu etries in the search
 * dialog.  The user is asked to select an output file; if he does so the list
 * content is written into it, in the format selected by the user. The function
 * internally remembers the last used directory & file name to preselect them
 * when saving next.
 */
void SearchList::cmdSaveFileAs(bool lnum_only)
{
    if (m_model->lineCount() > 0)
    {
        QString fileName = QFileDialog::getSaveFileName(this,
                               "Save line number list to file",
                               s_prevFileName,
                               "Any (*);;Text files (*.txt)");
        if (fileName.isEmpty() == false)
        {
            s_prevFileName = fileName;
            saveFile(fileName, lnum_only);
        }
    }
    else
    {
        QMessageBox::warning(this, "trowser", "Nothing to save: Search list is empty", QMessageBox::Ok);
    }
}


/**
 * This helper function reads and returns a list of line numbers from a file.
 * Numbers in the file must be in range 1 to number of lines in the buffer.
 * The user is notified about parser or range errors, but given the option to
 * ignore such errors. Duplicates are ignored silently.
 */
bool SearchList::loadLineList(const QString& fileName, std::set<int>& line_list)
{
    bool result = false;
    auto fh = std::make_unique<QFile>(QString(fileName));
    if (fh->open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(fh.get());

        int max_line = s_mainText->document()->blockCount();
        int skipped = 0;
        int synerr = 0;
        QString line_str;

        while (in.readLineInto(&line_str))
        {
            static const QRegularExpression re1("^(\\d+)(?:[^\\d\\w]|$)");
            static const QRegularExpression re2("^\\s*(?:#.*)?$");
            auto mat1 = re1.match(line_str);
            if (mat1.hasMatch())
            {
                bool ok;
                int line = mat1.captured(1).toInt(&ok, 10);

                if (ok && (line > 0) && (line < max_line))
                {
                    // OK - insert number into the list
                    // (hint at end of list, as likely numbers are sorted)
                    line_list.insert(line_list.end(), line - 1);
                }
                else if (ok)
                    skipped += 1;
                else
                    synerr += 1;
            }
            else  // invalid - error unless empty or comment "#"
            {
                auto mat2 = re2.match(line_str);
                if (mat2.hasMatch() == false)
                    synerr += 1;
            }
        }

        // error handling
        if (in.status() != QTextStream::Ok)
        {
            QString msg = QString("Error while reading file ") + fileName + ": " + fh->errorString();
            QMessageBox::warning(this, "trowser", msg, QMessageBox::Ok);
        }
        else if ((skipped > 0) || (synerr > 0))
        {
            QString msg("Found ");
            QTextStream msgBld(&msg);
            if (skipped > 0)
            {
                msgBld << skipped << " lines with a value outside of the allowed range 1 .. " << max_line;
            }
            if (synerr > 0)
            {
                if (skipped)
                    msgBld << " and ";
                msgBld << synerr << " non-empty lines without a numeric value";
            }
            if (line_list.size() == 0)
            {
                msgBld << ". No valid numbers found in the file.";
                QMessageBox::critical(this, "trowser", msg, QMessageBox::Ok);
            }
            else
            {
                auto answer = QMessageBox::warning(this, "trowser", msg, QMessageBox::Ignore | QMessageBox::Cancel);
                if (answer == QMessageBox::Ignore)
                    result = true;
            }
        }
        else if (line_list.size() == 0)
        {
            QString msg("No line numbers found in the file.");
            QMessageBox::critical(this, "trowser", msg, QMessageBox::Ok);
        }
        else
        {
            result = true;
        }
    }
    else
    {
        QString msg = QString("Error opening file ") + fileName + ": " + fh->errorString();
        QMessageBox::critical(this, "trowser", msg, QMessageBox::Ok);
    }
    return result;
}


/**
 * This function is called by menu entry "Load line numbers..." in the search
 * result dialog. The user is asked to select an input file; if he does so a
 * list of line numbers is extracted from it and lines with these numbers is
 * copied from the main window.
 */
void SearchList::cmdLoadFrom(bool)
{
    if (searchAbort())
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                               "Load line number list from file",
                               s_prevFileName,
                               "Any (*);;Text files (*.txt)");
        if (fileName.isEmpty() == false)
        {
            s_prevFileName = fileName;

            // using set to de-duplicate & sort
            std::set<int> inLines;

            if (loadLineList(fileName, inLines))
            {
                std::vector<int> line_list;
                std::vector<int> idx_list;

                line_list.reserve(inLines.size());
                idx_list.reserve(inLines.size());

                // filter lines already in the list: needed for undo list
                for (int line : inLines)
                {
                    int idx;
                    if (m_model->getLineIdx(line, idx) == false)
                    {
                        line_list.push_back(line);
                        idx_list.push_back(idx);
                    }
                }

                if (line_list.size() != 0)
                {
                    auto anchor = getViewAnchor();
                    m_model->insertLinePreSorted(line_list, idx_list);
                    m_undo->appendChange(true, line_list);
                    seeViewAnchor(anchor);

                    QString msg = QString("Inserted ") + QString::number(line_list.size()) + " lines.";
                    m_stline->showPlain("file", msg);
                }
                else
                {
                    QMessageBox::warning(this, "trowser", "All read lines were already in the list", QMessageBox::Ok);
                }
            }
        }
    }
}


/**
 * This function is bound to CTRL-g in the search list and displays stats
 * about the content of the search result list.
 */
void SearchList::cmdDisplayStats()
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
    m_stline->showPlain("file", msg);
}


/**
 * This slot is connected to the column configuration menu entry and opens the
 * external configuration dialog. The dialog is non-modal, so the function
 * returns immediately. When the user closes the dialog, a signal is raised
 * that is connected to the following slot for processing the changes.
 */
void SearchList::cmdOpenParserConfig(bool)
{
    if (m_dlgParser == nullptr)
    {
        m_dlgParser = new DlgParser(s_mainWin, s_mainText, s_parseSpec);
        connect(m_dlgParser, &DlgParser::dlgClosed, this, &SearchList::signalDlgParserClosed);
    }
    else
    {
        m_dlgParser->activateWindow();
        m_dlgParser->raise();
    }
}

/**
 * This slot is connected to the completion signal sent by the column
 * configuration dialog when closed. If the "changed" parameter is TRUE, the
 * function stores and applies the new configuraiton. In any case the dialog
 * object is deleted.
 */
void SearchList::signalDlgParserClosed(bool changed)
{
    if (m_dlgParser != nullptr)
    {
        if (changed)
        {
            m_dlgParser->getSpec(s_parseSpec);

            // apply the configuration in the model
            m_model->setCustomColCfg(s_parseSpec);

            // hide columns that are no longer enabled in configuration
            m_showCfg.custom &= m_model->getCustomColumns();
            configureColumnVisibility();

            // update static copy as parser config is static
            s_prevShowCfg = m_showCfg;

            // set visbility & text of menu entries related to custom columns
            configureCustomMenuActions();
        }
        m_dlgParser->deleteLater();
        m_dlgParser = nullptr;
    }
}


// ----------------------------------------------------------------------------
// Static state & interface

SearchList  * SearchList::s_instance = nullptr;
QByteArray    SearchList::s_winGeometry;
QByteArray    SearchList::s_winState;
QString       SearchList::s_prevFileName(".");
ParseSpec     SearchList::s_parseSpec;
SearchList::SearchListShowCfg SearchList::s_prevShowCfg;

Highlighter * SearchList::s_higl;
MainSearch  * SearchList::s_search;
MainWin     * SearchList::s_mainWin;
MainText    * SearchList::s_mainText;
Bookmarks   * SearchList::s_bookmarks;

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
        s_prevShowCfg = s_instance->m_showCfg;
    }

    obj.insert("win_geom", QJsonValue(QString(s_winGeometry.toHex())));
    obj.insert("win_state", QJsonValue(QString(s_winState.toHex())));

    obj.insert("show_search_highlight", QJsonValue(s_prevShowCfg.srchHall));
    obj.insert("show_bookmark_markup", QJsonValue(s_prevShowCfg.bookmarkMarkup));
    obj.insert("show_col_line_idx", QJsonValue(s_prevShowCfg.lineIdx));
    obj.insert("show_col_line_idx_delta", QJsonValue(s_prevShowCfg.lineDelta));
    obj.insert("show_col_custom_val", QJsonValue((s_prevShowCfg.custom & ParseColumnFlags::Val) != 0));
    obj.insert("show_col_custom_val_delta", QJsonValue((s_prevShowCfg.custom & ParseColumnFlags::ValDelta) != 0));
    obj.insert("show_col_custom_frame", QJsonValue((s_prevShowCfg.custom & ParseColumnFlags::Frm) != 0));
    obj.insert("show_col_custom_frame_delta", QJsonValue((s_prevShowCfg.custom & ParseColumnFlags::FrmDelta) != 0));

    obj.insert("custom_column_cfg", s_parseSpec.getRcValues());

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
        else if (var == "show_search_highlight")
        {
            s_prevShowCfg.srchHall = val.toBool();
        }
        else if (var == "show_bookmark_markup")
        {
            s_prevShowCfg.bookmarkMarkup = val.toBool();
        }
        else if (var == "show_col_line_idx")
        {
            s_prevShowCfg.lineIdx = val.toBool();
        }
        else if (var == "show_col_line_idx_delta")
        {
            s_prevShowCfg.lineDelta = val.toBool();
        }
        else if (var == "show_col_custom_val")
        {
            if (val.toBool())
                s_prevShowCfg.custom |= ParseColumnFlags::Val;
        }
        else if (var == "show_col_custom_val_delta")
        {
            if (val.toBool())
                s_prevShowCfg.custom |= ParseColumnFlags::ValDelta;
        }
        else if (var == "show_col_custom_frame")
        {
            if (val.toBool())
                s_prevShowCfg.custom |= ParseColumnFlags::Frm;
        }
        else if (var == "show_col_custom_frame_delta")
        {
            if (val.toBool())
                s_prevShowCfg.custom |= ParseColumnFlags::FrmDelta;
        }
        else if (var == "custom_column_cfg")
        {
            s_parseSpec = ParseSpec(val);
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

/**
 * This external interface function is called once during start-up after all
 * classes are instantiated to establish the required connections.
 */
void SearchList::connectWidgets(MainWin * mainWin, MainSearch * search, MainText * mainText,
                                Highlighter * higl, Bookmarks * bookmarks) /*static*/
{
    s_mainWin = mainWin;
    s_search = search;
    s_mainText = mainText;
    s_higl = higl;
    s_bookmarks = bookmarks;
}

bool SearchList::isDialogOpen()
{
    return (s_instance != nullptr);
}

SearchList* SearchList::getInstance(bool raiseWin)
{
    openDialog(raiseWin);
    return s_instance;
}

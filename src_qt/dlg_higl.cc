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
#include <QStyle>
#include <QMenu>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QFontDialog>
#include <QColorDialog>
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
#include "highlighter.h"
#include "search_list.h"
#include "highl_view_dlg.h"
#include "dlg_markup.h"
#include "dlg_higl.h"

// ----------------------------------------------------------------------------

class DlgHiglModel : public QAbstractItemModel, public HighlightViewModelIf
{
public:
    enum TblColIdx { COL_IDX_PAT, COL_IDX_REGEXP, COL_IDX_CASE, COL_IDX_FMT, COL_COUNT };

    DlgHiglModel(Highlighter * higl);
    virtual ~DlgHiglModel() {}
    const SearchPar& getSearchPar(const QModelIndex& index) const;
    virtual const HiglFmtSpec * getFmtSpec(const QModelIndex& index) const override;
    HiglId getFmtId(const QModelIndex& index) const;
    int getIdxForId(HiglId id) const;
    bool isModified() const { return m_modified; }
    void saveData(Highlighter * higl);

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex&) const override { return QModelIndex(); }
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex& index, const QVariant &value, int role = Qt::EditRole) override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                          const QModelIndex &destinationParent, int destinationChild) override;

    virtual bool insertRowData(int row, const SearchPar& pat, const HiglFmtSpec& fmtSpec); /*non-override*/
    void setFmtData(int row, const HiglFmtSpec& fmtSpec);
    void forceRedraw(TblColIdx col);

    // implementation of HighlightViewModelIf interfaces
    virtual QVariant higlModelData(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    std::vector<HiglPatExport>  m_patList;
    HiglId m_firstFreeId;
    bool m_modified;
};

DlgHiglModel::DlgHiglModel(Highlighter * higl)
{
    // get a copy of the pattern & highlighting format list
    higl->getPatList(m_patList);
    m_firstFreeId = m_patList.size();
    m_modified = false;
}

void DlgHiglModel::saveData(Highlighter * higl)
{
    higl->setPatList(m_patList);
    m_modified = false;
}

const HiglFmtSpec * DlgHiglModel::getFmtSpec(const QModelIndex& index) const
{
    if ((size_t)index.row() < m_patList.size())
        return &m_patList.at(index.row()).m_fmtSpec;
    return nullptr;
}

const SearchPar& DlgHiglModel::getSearchPar(const QModelIndex& index) const
{
    return m_patList.at(index.row()).m_srch;
}

HiglId DlgHiglModel::getFmtId(const QModelIndex& index) const
{
    return m_patList.at(index.row()).m_id;
}

int DlgHiglModel::getIdxForId(HiglId id) const
{
    for (size_t idx = 0; idx < m_patList.size(); ++idx)
        if (m_patList[idx].m_id == id)
            return idx;
    return -1;
}

QModelIndex DlgHiglModel::index(int row, int column, const QModelIndex& /*parent*/) const
{
    return createIndex(row, column);
}

int DlgHiglModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_patList.size();
}

int DlgHiglModel::columnCount(const QModelIndex& /*parent*/) const
{
    return COL_COUNT;
}

QVariant DlgHiglModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case COL_IDX_PAT: return QVariant("Pattern");
                case COL_IDX_REGEXP: return QVariant("Reg.Exp.?");
                case COL_IDX_CASE: return QVariant("Match case?");
                case COL_IDX_FMT: return QVariant("Format");
                default: break;
            }
            qWarning("Invalid index:%d for headerData()", section);
        }
        else if ((size_t)section < m_patList.size())
        {
            return QVariant(QString::number(section + 1));
        }
    }
    return QVariant();
}

QVariant DlgHiglModel::data(const QModelIndex &index, int role) const
{
    if (   ((role == Qt::DisplayRole) || (role == Qt::EditRole))
        && ((size_t)index.row() < m_patList.size()))
    {
        const SearchPar& el = m_patList.at(index.row()).m_srch;
        switch (index.column())
        {
            case COL_IDX_PAT: return QVariant(el.m_pat);
            case COL_IDX_REGEXP: return QVariant(el.m_opt_regexp);
            case COL_IDX_CASE: return QVariant(el.m_opt_case);
            case COL_IDX_FMT: return QVariant();  // content rendered by item delegate
            default: break;
        }
        qWarning("Invalid index:%d for data()", index.column());
    }
    return QVariant();
}

Qt::ItemFlags DlgHiglModel::flags(const QModelIndex &index) const
{
    if ((size_t)index.row() < m_patList.size())
    {
        switch (index.column())
        {
            case COL_IDX_PAT: return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
            case COL_IDX_REGEXP: /* fall-through */
            case COL_IDX_CASE: return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable;
            case COL_IDX_FMT: return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            default: break;
        }
    }
    return Qt::NoItemFlags;
}

bool DlgHiglModel::setData(const QModelIndex &index, const QVariant &value, int /*role*/)
{
    int row = index.row();
    int col = index.column();
    bool result = false;

    if ((size_t)row < m_patList.size())
    {
        SearchPar& el = m_patList.at(row).m_srch;

        if (col == COL_IDX_PAT)
        {
            QString str = value.toString();
            // TODO also validate reg.exp. if applicable; display warning
            if (!str.isEmpty() && (str != el.m_pat))
            {
                el.m_pat = str;
                m_modified = true;
                result = true;
            }
        }
        else if (col == COL_IDX_REGEXP)
        {
            if (el.m_opt_regexp != value.toBool())
            {
                el.m_opt_regexp = value.toBool();
                m_modified = true;
                result = true;
            }
        }
        else if (col == COL_IDX_CASE)
        {
            if (el.m_opt_case != value.toBool())
            {
                el.m_opt_case = value.toBool();
                m_modified = true;
                result = true;
            }
        }
        if (result)
            emit dataChanged(index, index);
    }
    return result;
}

void DlgHiglModel::setFmtData(int row, const HiglFmtSpec& fmtSpec)
{
    if ((size_t)row < m_patList.size())
    {
        m_patList.at(row).m_fmtSpec = fmtSpec;
        m_modified = true;

        QModelIndex index = createIndex(row, COL_IDX_FMT);
        emit dataChanged(index, index);
    }
}

bool DlgHiglModel::removeRows(int row, int count, const QModelIndex& parent)
{
    bool result = false;

    if (size_t(row) + size_t(count) <= m_patList.size())
    {
        this->beginRemoveRows(parent, row, row + count - 1);
        m_patList.erase(m_patList.begin() + row,
                        m_patList.begin() + (row + count));
        this->endRemoveRows();
        m_modified = true;
        result = true;
    }
    return result;
}

bool DlgHiglModel::moveRows(const QModelIndex& srcParent, int srcRow, int count,
                            const QModelIndex& dstParent, int dstRow)
{
    bool result = false;
    if (size_t(srcRow) + size_t(count) <= m_patList.size())
    {
        Q_ASSERT(count > 0);
        Q_ASSERT((dstRow < srcRow) || (dstRow >= srcRow + count));

        if (this->beginMoveRows(srcParent, srcRow, srcRow + count - 1, dstParent, dstRow))
        {
            std::vector<HiglPatExport> tmp;
            tmp.insert(tmp.begin(), m_patList.begin() + srcRow,
                                    m_patList.begin() + srcRow + count);

            m_patList.insert(m_patList.begin() + dstRow, tmp.begin(), tmp.end());

            // remove the original; adapt index change due to insertion, if needed
            int off = ((dstRow < srcRow) ? count : 0);
            m_patList.erase( m_patList.begin() + srcRow + off,
                             m_patList.begin() + srcRow + count + off);

            this->endMoveRows();
            m_modified = true;
            result = true;
        }
        else
            qCritical("DlgHiglModel::moveRows failed in beginMoveRows");
    }
    return result;
}

bool DlgHiglModel::insertRowData(int row, const SearchPar& pat, const HiglFmtSpec& fmtSpec)
{
    bool result = false;
    if (size_t(row) <= m_patList.size())  // == OK: insert at end
    {
        HiglPatExport w;
        w.m_srch = pat;
        w.m_fmtSpec = fmtSpec;
        w.m_id = ++m_firstFreeId;

        this->beginInsertRows(QModelIndex(), row, row);
        m_patList.insert(m_patList.begin() + row, w);
        this->endInsertRows();

        m_modified = true;
        result = true;
    }
    return result;
}


// called when external state has changed that affects rendering
void DlgHiglModel::forceRedraw(TblColIdx col)  // ATTN row/col order reverse of usual
{
    if (m_patList.size() != 0)
    {
        emit dataChanged(createIndex(0, col),
                         createIndex(m_patList.size() - 1, col));
    }
}

// implementation of HighlightViewModelIf interfaces
QVariant DlgHiglModel::higlModelData(const QModelIndex& /*index*/, int /*role*/) const
{
    return QVariant("...Sample text...");
}


// ----------------------------------------------------------------------------

class DlgHiglView : public QTableView
{
public:
    DlgHiglView(QWidget *parent)
        : QTableView(parent)
    {
    }
    virtual void keyPressEvent(QKeyEvent *e) override;
};

void DlgHiglView::keyPressEvent(QKeyEvent *e)
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
 * This function creates the highlight mark-up edit dialog.
 */
DlgHigl::DlgHigl(Highlighter * higl, MainSearch * search, MainWin * mainWin)
    : m_higl(higl)
    , m_search(search)
    , m_mainWin(mainWin)
{
    this->setWindowTitle("Highlight patterns - trowser");
    auto style = m_mainWin->getAppStyle();
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    m_model = new DlgHiglModel(higl);
    m_fmtDelegate = new HighlightViewDelegate(m_model, true,
                                              m_mainWin->getFontContent(),
                                              m_mainWin->getFgColDefault(),
                                              m_mainWin->getBgColDefault());

    m_table = new DlgHiglView(central_wid);
        m_table->setModel(m_model);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setSectionResizeMode(DlgHiglModel::COL_IDX_PAT, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(DlgHiglModel::COL_IDX_FMT, QHeaderView::ResizeToContents);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);  // use delegate's sizeHint()
        m_table->setItemDelegateForColumn(DlgHiglModel::COL_IDX_FMT, m_fmtDelegate);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, &QAbstractItemView::customContextMenuRequested, this, &DlgHigl::showContextMenu);
        connect(m_table, &QAbstractItemView::doubleClicked, [=](const QModelIndex & midx){
            if (midx.column() == DlgHiglModel::COL_IDX_FMT) cmdEditFormat(true);  // else use "inline" editor
        });
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DlgHigl::selectionChanged);
        layout_top->addWidget(m_table);

    m_stline = new StatusLine(m_table);

    m_cmdButs = new QDialogButtonBox(QDialogButtonBox::Cancel |
                                     QDialogButtonBox::Apply |
                                     QDialogButtonBox::Ok,
                                     Qt::Horizontal, central_wid);
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(false);
        connect(m_cmdButs, &QDialogButtonBox::clicked, this, &DlgHigl::cmdButton);
        connect(m_model, &DlgHiglModel::dataChanged, this, &DlgHigl::cbDataChanged);
        layout_top->addWidget(m_cmdButs);

    connect(m_mainWin, &MainWin::textFontChanged, this, &DlgHigl::mainFontChanged);

    //
    // "Edit" toolbar: functionality for adding/removing/sorting the list
    //
    auto f1 = new QToolBar("Edit", this);
        f1->setObjectName("DlgHigl::Edit"); // for saveState
        addToolBar(Qt::TopToolBarArea, f1);
    auto f1_l = new QLabel("Edit:", f1);
        f1->addWidget(f1_l);
    auto m_f1_add = new QPushButton(style->standardIcon(QStyle::SP_FileIcon), "Add new item", f1);
        connect(m_f1_add, &QPushButton::clicked, this, &DlgHigl::cmdAdd);
        f1->addWidget(m_f1_add);
    m_f1_del = new QPushButton(style->standardIcon(QStyle::SP_TrashIcon), "Remove...", f1);
        m_f1_del->setEnabled(false);
        m_f1_del->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(m_f1_del, &QPushButton::clicked, this, &DlgHigl::cmdRemove);
        f1->addWidget(m_f1_del);
    m_f1_up = new QPushButton(style->standardIcon(QStyle::SP_ArrowUp), "Move up", f1);
        m_f1_up->setEnabled(false);
        connect(m_f1_up, &QPushButton::clicked, this, &DlgHigl::cmdShiftUp);
        f1->addWidget(m_f1_up);
    m_f1_down = new QPushButton(style->standardIcon(QStyle::SP_ArrowDown), "Move down", f1);
        m_f1_down->setEnabled(false);
        connect(m_f1_down, &QPushButton::clicked, this, &DlgHigl::cmdShiftDown);
        f1->addWidget(m_f1_down);
    m_f1_fmt = new QPushButton("Format...", f1);
        m_f1_fmt->setEnabled(false);
        connect(m_f1_fmt, &QPushButton::clicked, this, &DlgHigl::cmdEditFormat);
        f1->addWidget(m_f1_fmt);

    //
    // "Find" toolbar: functionality for applying search patterns
    //
    auto f2 = new QToolBar("Find", this);
        f2->setObjectName("DlgHigl::Find"); // for saveState
        addToolBar(Qt::RightToolBarArea, f2);
    auto f2_l = new QLabel("Find:", f2);
        f2->addWidget(f2_l);
    m_f2_bn = new QPushButton("&Next", f2);
        m_f2_bn->setEnabled(false);
        connect(m_f2_bn, &QPushButton::clicked, [=](){ cmdSearch(true); });
        f2->addWidget(m_f2_bn);
    m_f2_bp = new QPushButton("&Prev.", f2);
        m_f2_bp->setEnabled(false);
        connect(m_f2_bp, &QPushButton::clicked, [=](){ cmdSearch(false); });
        f2->addWidget(m_f2_bp);
    m_f2_ball = new QPushButton("&All", f2);
        m_f2_ball->setEnabled(false);
        connect(m_f2_ball, &QPushButton::clicked, [=](){ cmdSearchList(0); });
        f2->addWidget(m_f2_ball);
    m_f2_ballb = new QPushButton("All below", f2);
        m_f2_ballb->setEnabled(false);
        m_f2_ballb->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_N));
        connect(m_f2_ballb, &QPushButton::clicked, [=](){ cmdSearchList(1); });
        f2->addWidget(m_f2_ballb);
    m_f2_balla = new QPushButton("All above", f2);
        m_f2_balla->setEnabled(false);
        m_f2_balla->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_P));
        connect(m_f2_balla, &QPushButton::clicked, [=](){ cmdSearchList(-1); });
        f2->addWidget(m_f2_balla);

    auto act = new QAction(central_wid);
        act->setShortcut(QKeySequence(Qt::Key_Ampersand));
        connect(act, &QAction::triggered, [=](){ m_search->searchHighlightClear(); });
        central_wid->addAction(act);

    setCentralWidget(central_wid);
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
DlgHigl::~DlgHigl()
{
    delete m_model;
    delete m_fmtDelegate;
}

/**
 * This slot is connected to notification of font changes in the main text
 * window, as the same font is used for displaying text content in the dialog.
 * The function resizes row height and then forces a redraw. Note the new font
 * need not be passed to the view delegate as it gets passed a reference to the
 * font so that it picks up the new font automatically.
 */
void DlgHigl::mainFontChanged()
{
    QFontMetrics metrics1(m_mainWin->getFontContent());
    QFontMetrics metrics2(m_table->font());
    m_table->verticalHeader()->setDefaultSectionSize(2*3 + std::max(metrics1.height(), metrics2.height()));

    m_model->forceRedraw(DlgHiglModel::COL_IDX_FMT);
}

/**
 * This function is bound to changes of the selection in the color tags list.
 */
void DlgHigl::selectionChanged(const QItemSelection& /*selected*/, const QItemSelection& /*deselected*/)
{
    auto sel = m_table->selectionModel()->selectedRows();
    bool enabled = (sel.size() > 0);

    m_f1_del->setEnabled(enabled);
    m_f1_up->setEnabled(enabled);
    m_f1_down->setEnabled(enabled);
    m_f1_fmt->setEnabled(sel.size() == 1);

    m_f2_bn->setEnabled(enabled);
    m_f2_bp->setEnabled(enabled);
    m_f2_ball->setEnabled(enabled);
    m_f2_ballb->setEnabled(enabled);
    m_f2_balla->setEnabled(enabled);
}


/**
 * This slot is connected to a signal sent by the model whenever data is
 * modified.  The function enables the "Apply" button, which allows saving
 * the changes as well as visually indicates the status to the user.
 */
void DlgHigl::cbDataChanged(const QModelIndex &, const QModelIndex &)
{
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
}

/**
 * This function is bound to right mouse clicks in the highlight tag list and pops
 * up a context menu. The menu is populated and shown at the given coordinates.
 */
void DlgHigl::showContextMenu(const QPoint& pos)
{
    auto menu = new QMenu("Pattern list actions", this);

    auto searchPar = m_search->getCurSearchParams();
    auto sel = m_table->selectionModel()->selectedRows();
    bool state_find_sel_1 = (!searchPar.m_pat.isEmpty() && (sel.size() == 1));
    bool state_sel_1 = (sel.size() == 1);
    bool state_sel_n0 = (sel.size() > 0);

    auto act = menu->addAction("Change background color");
        act->setEnabled(state_sel_1);
        connect(act, &QAction::triggered, [=](){ cmdChangeBgFgColor(true); });
    act = menu->addAction("Change foreground color");
        act->setEnabled(state_sel_1);
        connect(act, &QAction::triggered, [=](){ cmdChangeBgFgColor(false); });
    QMenu *sub = menu->addMenu("Font options");
    if (state_sel_1)
    {
        const HiglFmtSpec * pFmtSpec = m_model->getFmtSpec(sel.front());
        bool optEna = pFmtSpec->m_font.isEmpty();

        act = sub->addAction("Bold");
            act->setEnabled(optEna);
            act->setCheckable(true);
            act->setChecked(pFmtSpec->m_bold);
            connect(act, &QAction::triggered, [=](){ cmdToggleFontBold(sel.front(), act->isChecked()); });
        act = sub->addAction("Italic");
            act->setEnabled(optEna);
            act->setCheckable(true);
            act->setChecked(pFmtSpec->m_italic);
            connect(act, &QAction::triggered, [=](){ cmdToggleFontItalic(sel.front(), act->isChecked()); });
        act = sub->addAction("Underline");
            act->setEnabled(optEna);
            act->setCheckable(true);
            act->setChecked(pFmtSpec->m_underline);
            connect(act, &QAction::triggered, [=](){ cmdToggleFontUnderline(sel.front(), act->isChecked()); });
        act = sub->addAction("Strikeout");
            act->setEnabled(optEna);
            act->setCheckable(true);
            act->setChecked(pFmtSpec->m_strikeout);
            connect(act, &QAction::triggered, [=](){ cmdToggleFontStrikeout(sel.front(), act->isChecked()); });
        act = sub->addAction("Overline");
            act->setEnabled(optEna);
            act->setCheckable(true);
            act->setChecked(pFmtSpec->m_overline);
            connect(act, &QAction::triggered, [=](){ cmdToggleFontOverline(sel.front(), act->isChecked()); });
        act = sub->addSeparator();
        act = sub->addAction("Reset font to default");
            act->setEnabled(!optEna);
            connect(act, &QAction::triggered, [=](){ cmdResetFont(sel.front()); });
        act = sub->addAction("Font family && options...");
            connect(act, &QAction::triggered, this, &DlgHigl::cmdChangeFont);
    }
    else
    {
        sub->setEnabled(false);
    }
    act = menu->addAction("More format options...");
        act->setEnabled(state_sel_1);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdEditFormat);
    menu->addSeparator();
    act = menu->addAction("Copy pattern to main window");
        act->setEnabled(state_sel_1);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdCopyToMain);
    act = menu->addAction("Copy pattern from main window");
        act->setEnabled(state_find_sel_1);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdCopyFromMain);
    menu->addSeparator();
    act = menu->addAction("Duplicate this entry");
        act->setEnabled(state_sel_1);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdDuplicate);
    act = menu->addAction("Remove selected entries");
        act->setEnabled(state_sel_n0);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdRemove);
    act = menu->addAction("Move Up");
        act->setEnabled(state_sel_n0);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdShiftUp);
    act = menu->addAction("Move Down");
        act->setEnabled(state_sel_n0);
        connect(act, &QAction::triggered, this, &DlgHigl::cmdShiftDown);

    menu->exec(mapToGlobal(pos));
}

/**
 * This function is bound to destruction of the dialog window. The event is
 * also sent artificially upon the "Cancel" button.
 */
void DlgHigl::closeEvent(QCloseEvent * event)
{
    if (m_model->isModified())
    {
        auto answer = QMessageBox::question(this, "trowser", "Discard unsaved changes in the pattern highlight list?",
                                            QMessageBox::Ok | QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
        {
            if (event)
                event->ignore();
            return;
        }
    }
    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();

    if (event)
        event->accept();
    else
        this->close();

    s_instance->deleteLater();
    s_instance = nullptr;
}


/**
 * This function is bound to all of the grouped main command buttons: Ok,
 * Apply, Cancel. As usual, Ok will save & close, Cancel will confirm &
 * discard & close, and Apply will only save.
 */
void DlgHigl::cmdButton(QAbstractButton * button)
{
    if (button == m_cmdButs->button(QDialogButtonBox::Cancel))
    {
        QCoreApplication::postEvent(this, new QCloseEvent());
    }
    else if (button == m_cmdButs->button(QDialogButtonBox::Ok))
    {
        if (m_model->isModified())
            m_model->saveData(m_higl);
        QCoreApplication::postEvent(this, new QCloseEvent());
    }
    else if (button == m_cmdButs->button(QDialogButtonBox::Apply))
    {
        if (m_model->isModified())
        {
            m_model->saveData(m_higl);
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(false);
        }
    }
}


/**
 * This function is invoked by the "Add new item" entry in the highlight
 * list's toolbar and context menu. The new entry is given the current or
 * last used search parameters from the main menu and a default highlight
 * format with a hard-coded background color.
 */
void DlgHigl::cmdAdd(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    int row = ((sel.size() > 0) ? sel.front().row() : m_model->rowCount());

    auto pat = m_search->getCurSearchParams();
    if (pat.m_pat.isEmpty())
        pat.m_pat = "Enter pattern...";

    HiglFmtSpec fmtSpec;
    fmtSpec.m_bgCol = 0xfffaee0a; //TODO s_colFind

    m_model->insertRowData(row, pat, fmtSpec);

    // select item and make it visible
    QModelIndex midx = m_model->index(row, DlgHiglModel::COL_IDX_PAT);
    m_table->selectionModel()->select(midx, QItemSelectionModel::Clear);
    m_table->setCurrentIndex(midx);
    m_table->scrollTo(midx);

    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
}


/**
 * This function is invoked by the "Duplicate entry" command in the highlight
 * list's context menu. (The command is not available via toolbar.) It creates
 * a new entry with the same search pattern and highlighting format as the
 * selected one.
 */
void DlgHigl::cmdDuplicate(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        auto midx = sel.front();
        m_model->insertRowData(midx.row(), m_model->getSearchPar(midx),
                                           *m_model->getFmtSpec(midx));

        // select item and make it visible
        m_table->selectionModel()->select(midx, QItemSelectionModel::Clear);
        m_table->setCurrentIndex(midx);

        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No pattern is selected in the list");
}


/**
 * This function is invoked by the "Remove entry" command in the highlight
 * list's toolbar and context menu. It removes all selected entries without
 * asking for confirmation (the user can still "Cancel" the dialog to undo
 * the deletion.)
 */
void DlgHigl::cmdRemove(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() > 0)
    {
        for (auto& midx : sel)
        {
            auto it = findDlgMarkup(m_model->getFmtId(midx), m_dlgMarkup);
            if (it != m_dlgMarkup.end())
                m_dlgMarkup.erase(it);
        }

        // reverse-sort index list so that indices do not need adaption during removal
        std::sort(sel.begin(), sel.end(),
                  [](const QModelIndex& a, const QModelIndex&b)->bool {return a.row() > b.row();});

        for (auto& midx : sel)
        {
            m_model->removeRows(midx.row(), 1);
        }
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No pattern is selected in the list");
}


/**
 * This function is bound to the "Move up" button.  Each selected item (where
 * selection may be non-consecutive) is shifted up by one row. If the first
 * selected item is already at top, the function does nothing.
 */
void DlgHigl::cmdShiftUp(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() > 0)
    {
        // sort index list so that indices do not need adaption during removal
        std::sort(sel.begin(), sel.end(),
                  [](const QModelIndex& a, const QModelIndex&b)->bool {return a.row() < b.row();});

        if (sel.front().row() > 0)  // first element is already at top
        {
            for (auto& midx : sel)
            {
                m_model->moveRows(QModelIndex(), midx.row(), 1,
                                  QModelIndex(), midx.row() - 1);
            }
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
        else
            m_stline->showWarning("edit", "Item is already at the top - cannot move up");
    }
}


/**
 * This function is bound to the "down" button next to the color highlight
 * list.  Each selected item is shifted down by one line.
 */
void DlgHigl::cmdShiftDown(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() > 0)
    {
        // reverse-sort index list so that indices do not need adaption during removal
        std::sort(sel.begin(), sel.end(),
                  [](const QModelIndex& a, const QModelIndex&b)->bool {return a.row() > b.row();});

        if (sel.front().row() + 1 < m_model->rowCount())  // lowest element is already at bottom
        {
            for (auto& midx : sel)
            {
                // second +1 needed due to semantics of QAbstractItemModel::beginMoveRows
                m_model->moveRows(QModelIndex(), midx.row(), 1,
                                  QModelIndex(), midx.row() + 1 + 1);
            }
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
        else
            m_stline->showWarning("edit", "Item is already at the bottom - cannot move down");
    }
}


/**
 * This helper function searches the list of pointers to mark-up editor dialogs
 * for one working for the given highlight id. It returns a list iterator that
 * has value end() as usual when no such item was found.
 */
DlgHigl::DlgMarkupPtrList::iterator DlgHigl::findDlgMarkup(HiglId id, DlgHigl::DlgMarkupPtrList& v)
{
    for (auto it = v.begin(); it != v.end(); ++it)
        if ((*it)->getFmtId() == id)
            return it;
    return v.end();
}

/**
 * This function is invoked by the "Format..." command in the highlight list's
 * toolbar and context menu. It can also be used via double-click on a list
 * item. The function opens the mark-up editor for the selected entry.
 */
void DlgHigl::cmdEditFormat(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() > 0)
    {
        const HiglFmtSpec * fmtSpec = m_model->getFmtSpec(sel.front());
        const SearchPar& pat = m_model->getSearchPar(sel.front());
        HiglId id = m_model->getFmtId(sel.front());

        auto it = findDlgMarkup(id, m_dlgMarkup);
        if (it == m_dlgMarkup.end())
        {
            QString title = QString("Pattern /") + pat.m_pat + "/ mark-up";
            auto ptr = std::make_unique<DlgMarkup>(id, title, fmtSpec, m_higl, m_mainWin);

            connect(ptr.get(), &DlgMarkup::closeReq, this, &DlgHigl::signalMarkupCloseReq);
            connect(ptr.get(), &DlgMarkup::applyReq, this, &DlgHigl::signalMarkupApplyReq);

            m_dlgMarkup.push_back(std::move(ptr));
        }
        else  // dialog for that pattern is already open
        {
            (*it)->activateWindow();
            (*it)->raise();
        }
    }
}

/**
 * This slot is connected to the signal sent when a mark-up editor dialog is
 * closed. The function releases the dialog resources.
 */
void DlgHigl::signalMarkupCloseReq(HiglId id)
{
    auto it = findDlgMarkup(id, m_dlgMarkup);
    if (it != m_dlgMarkup.end())
    {
        m_dlgMarkup.erase(it);
    }
}

/**
 * This slot is connected to the signal sent by the mark-up editor upon "Apply"
 * or "Ok". In case of the former, parameter "immediate" indicates that the
 * highlight list shall be applied to the main window immediately. Else, the
 * new format provided by the editor is only stored internally until its own
 * "Apply" or "Ok" buttons are clicked.
 */
void DlgHigl::signalMarkupApplyReq(HiglId id, bool immediate)
{
    int idx = m_model->getIdxForId(id);
    auto it = findDlgMarkup(id, m_dlgMarkup);
    if ((it != m_dlgMarkup.end()) && (idx >= 0))
    {
        m_model->setFmtData(idx, (*it)->getFmtSpec());
        if (immediate)
        {
            m_model->saveData(m_higl);
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(false);
        }
        else
        {
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
        }
        (*it)->resetModified();
    }
}


/**
 * This function is bound to the next/prev buttons below the highlight tags
 * list. The function searches for the next line which is tagged with one of
 * the selected highlighting tags (i.e. no text search is performed!) When
 * multiple tags are searched for, the closest match is used.
 */
void DlgHigl::cmdSearch(bool is_fwd)
{
    QItemSelectionModel * sel = m_table->selectionModel();

    if (sel->hasSelection())
    {
        m_stline->clearMessage("search");

        std::vector<SearchPar> patList;
        for (auto& midx : sel->selectedRows())
        {
            patList.push_back(m_model->getSearchPar(midx));
        }

        bool found = m_search->searchFirst(is_fwd, patList);
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
        m_stline->showError("search", "No pattern is selected in the list");
}

/**
 * This function is bound to the "All", "All below" and "All above" buttons in
 * the color tags dialog.  The function opens the search result window and adds
 * all lines matching the pattern for the currently selected color tags.
 */
void DlgHigl::cmdSearchList(int direction)
{
    QItemSelectionModel * sel = m_table->selectionModel();

    if (sel->hasSelection())
    {
        m_stline->clearMessage("search");

        std::vector<SearchPar> patList;
        for (auto& midx : sel->selectedRows())
        {
            patList.push_back(m_model->getSearchPar(midx));
        }
        SearchList::getInstance(false)->searchMatches(true, direction, patList);
    }
    else  // should never occur as button (incl. shortcut) gets disabled
        m_stline->showError("search", "No pattern is selected in the list");
}

/**
 * This slot is connected to the "Change background/foreground color" entries
 * in the context menu; the parameter indicates which color to modify.  The
 * function opens the color selection dialog and when closed with Ok updates
 * the respective color in the formst.
 */
void DlgHigl::cmdChangeBgFgColor(bool isBg)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        const HiglFmtSpec * pFmtSpec = m_model->getFmtSpec(sel.front());
        QColor col = QColor(isBg ? pFmtSpec->m_bgCol : pFmtSpec->m_fgCol);

        col = QColorDialog::getColor(col, this, "Background color selection");
        if (col.isValid())
        {
            HiglFmtSpec fmtSpec = *pFmtSpec;
            (isBg ? fmtSpec.m_bgCol : fmtSpec.m_fgCol) = col.rgba();

            m_model->setFmtData(sel.front().row(), fmtSpec);
        }
    }
}


void DlgHigl::cmdToggleFontUnderline(const QModelIndex& index, bool checked)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_underline = checked;
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdToggleFontBold(const QModelIndex& index, bool checked)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_bold = checked;
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdToggleFontItalic(const QModelIndex& index, bool checked)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_italic = checked;
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdToggleFontOverline(const QModelIndex& index, bool checked)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_overline = checked;
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdToggleFontStrikeout(const QModelIndex& index, bool checked)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_strikeout = checked;
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdResetFont(const QModelIndex& index)
{
    HiglFmtSpec fmtSpec = *m_model->getFmtSpec(index);
    fmtSpec.m_font.clear();
    m_model->setFmtData(index.row(), fmtSpec);
}

void DlgHigl::cmdChangeFont(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        HiglFmtSpec fmtSpec = *m_model->getFmtSpec(sel.front());
        QFont font;
        if (!fmtSpec.m_font.isEmpty())
            font.fromString(fmtSpec.m_font);
        else
            font = m_mainWin->getFontContent();

        bool ok;
        font = QFontDialog::getFont(&ok, font, this);
        if (ok)
        {
            fmtSpec.m_font = font.toString();
            m_model->setFmtData(sel.front().row(), fmtSpec);
        }
    }
}


/**
 * This function is invoked by the "Copy to main search field" command in the
 * highlight list's context menu.
 */
void DlgHigl::cmdCopyToMain(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.size() == 1)
    {
        m_search->searchEnterOpt( m_model->getSearchPar(sel.front()) );
    }
}


/**
 * This function is invoked by the "Update from main search field" command in
 * the highlight list's context menu. The function replaces the current pattern
 * and options with the current values of the entry field and checkboxes in the
 * main window.
 */
void DlgHigl::cmdCopyFromMain(bool)
{
    auto sel = m_table->selectionModel()->selectedRows();
    auto par = m_search->getCurSearchParams();
    if (!par.m_pat.isEmpty() && (sel.size() == 1))
    {
        int row = sel.front().row();

        m_model->setData(m_model->index(row, DlgHiglModel::COL_IDX_PAT), QVariant(par.m_pat));
        m_model->setData(m_model->index(row, DlgHiglModel::COL_IDX_REGEXP), QVariant(par.m_opt_regexp));
        m_model->setData(m_model->index(row, DlgHiglModel::COL_IDX_CASE), QVariant(par.m_opt_case));

        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
    //else: should never happen as button is disabled
}

// ----------------------------------------------------------------------------
// Static state & interface

/**
 * This table replaces the ugly list of "standard" colors provided by the Qt
 * color selection dialog. The colors are selected especially to be useful as
 * background with black foreground text, plus a few for use as foreground /
 * text color. Each group of 6 colors is shown in one column in the dialog.
 */
std::vector<QRgb> DlgHigl::s_defaultColPalette =
{
  QRgb(0xff'000000),     // light colors (for background)
  QRgb(0xff'666666),
  QRgb(0xff'999999),
  QRgb(0xff'bbbbbb),
  QRgb(0xff'dddddd),
  QRgb(0xff'ffffff),

  QRgb(0xff'4acbb5),
  QRgb(0xff'8bcbc0),
  QRgb(0xff'94ff80),
  QRgb(0xff'b4e79c),
  QRgb(0xff'bee1be),
  QRgb(0xff'bfffb3),

  QRgb(0xff'b7c1e3),
  QRgb(0xff'b3beff),
  QRgb(0xff'96d9ff),
  QRgb(0xff'b3fff3),
  QRgb(0xff'ccffff),
  QRgb(0xff'dab3d9),

  QRgb(0xff'c180ff),
  QRgb(0xff'dab3ff),
  QRgb(0xff'eba1ff),
  QRgb(0xff'e6b3ff),
  QRgb(0xff'e6ccff),
  QRgb(0xff'e6b3d9),

  QRgb(0xff'ff423f),
  QRgb(0xff'e73c39),
  QRgb(0xff'ff7342),
  QRgb(0xff'ffb439),
  QRgb(0xff'ffbf80),
  QRgb(0xff'ffcfa0),

  QRgb(0xff'ffd9b3),
  QRgb(0xff'f0b0a0),
  QRgb(0xff'ffb3be),
  QRgb(0xff'e9ff80),
  QRgb(0xff'eeee8e),
  QRgb(0xff'faee0a),

  QRgb(0xff'86589a),     // saturated colors (for foreground)
  QRgb(0xff'9a516d),
  QRgb(0xff'b86200),
  QRgb(0xff'b90000),
  QRgb(0xff'3d4291),
  QRgb(0xff'5e789a),

  QRgb(0xff'008800),
  QRgb(0xff'61949a),
  QRgb(0xff'007c66),
  QRgb(0xff'717100),
  QRgb(0xff'b8b800),
  QRgb(0xff'ffff00),
};

DlgHigl * DlgHigl::s_instance = nullptr;
QByteArray DlgHigl::s_winGeometry;
QByteArray DlgHigl::s_winState;


/**
 * This function has to be called by all dialogs using the color selection
 * dialog for initializing the palette of "standard" colors. This is needed as
 * setting the palette during start-up (while reading RC file) does not seem to
 * take effect.
 */
void DlgHigl::initColorPalette()  /*static*/
{
    int idx = 0;
    for (auto& col : s_defaultColPalette)
    {
        QColorDialog::setStandardColor(idx++, QColor(col));
    }
}

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this dialog. Currently this includes the dialog geometry and
 * tool-bar state and the color palettes. Note the highlight definitions are
 * not included here, as the dialog class only holds a temporary copy while
 * open.
 */
QJsonObject DlgHigl::getRcValues()  /*static*/
{
    QJsonObject obj;

    if (s_instance)
    {
      s_winGeometry = s_instance->saveGeometry();
      s_winState = s_instance->saveState();
    }

    obj.insert("win_geom", QJsonValue(QString(s_winGeometry.toHex())));
    obj.insert("win_state", QJsonValue(QString(s_winState.toHex())));

    QJsonArray arr1;
    for (auto& col : s_defaultColPalette)
    {
        arr1.push_back(QJsonValue(int(col)));
    }
    obj.insert("default_col_palette", arr1);

    QJsonArray arr2;
    for (int idx = 0; idx < QColorDialog::customCount(); ++idx)
    {
        arr2.push_back(QJsonValue(int( QColorDialog::customColor(idx).rgba() )));
    }
    obj.insert("custom_col_palette", arr2);

    return obj;
}

/**
 * This function is called during start-up to apply configuration variables.
 * The values are simply stored and applied when the dialog is actually opened.
 */
void DlgHigl::setRcValues(const QJsonValue& val)  /*static*/
{
    const QJsonObject obj = val.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        if (var == "default_col_palette")
        {
            QJsonArray arr = val.toArray();
            s_defaultColPalette.clear();
            int idx = 0;
            for (auto it = arr.begin(); it != arr.end(); ++it)
            {
                s_defaultColPalette.push_back(it->toInt());
                QColorDialog::setStandardColor(idx++, it->toInt());
            }
        }
        else if (var == "custom_col_palette")
        {
            QJsonArray arr = val.toArray();
            int idx = 0;
            for (auto it = arr.begin(); it != arr.end(); ++it)
            {
                QColorDialog::setCustomColor(idx++, QColor(QRgb(it->toInt())));
            }
        }
        else if (var == "win_geom")
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
void DlgHigl::openDialog(Highlighter * higl, MainSearch * search, MainWin * mainWin) /*static*/
{
    if (s_instance == nullptr)
    {
        initColorPalette();
        s_instance = new DlgHigl(higl, search, mainWin);
    }
    else
    {
        s_instance->activateWindow();
        s_instance->raise();
    }
}

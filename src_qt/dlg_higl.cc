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
#include <QTableView>
#include <QAbstractItemModel>
#include <QItemSelection>
#include <QHeaderView>
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QToolBar>
#include <QDialogButtonBox>
#include <QMenu>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

#include <cstdio>
#include <string>
#include <vector>

#include "main_win.h"
#include "dlg_higl.h"

// ----------------------------------------------------------------------------

class DlgHiglModel : public QAbstractItemModel
{
public:
    DlgHiglModel(Highlighter * higl);
    virtual ~DlgHiglModel() {}
    const SearchPar& getSearchPar(int idx) { return m_patList.at(idx).m_srch; }
    const HiglFmtSpec * getFmtSpec(const QModelIndex& index) const;
    bool isModified() const { return m_modified; }

    virtual QModelIndex index(int row, int column,
                              const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex&) const override { return QModelIndex(); }

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant &value, int role = Qt::EditRole) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
private:
    static const int COL_COUNT = 4;
    enum TblColIdx { COL_IDX_PAT, COL_IDX_REGEXP, COL_IDX_CASE, COL_IDX_PIX };

    std::vector<HiglPat>  m_patList;
    bool m_modified;
};

DlgHiglModel::DlgHiglModel(Highlighter * higl)
{
    // get a copy of the pattern & highlighting format list
    m_patList = higl->getPatList();
    m_modified = false;
}

const HiglFmtSpec * DlgHiglModel::getFmtSpec(const QModelIndex& index) const
{
    if ((size_t)index.row() < m_patList.size())
        return &m_patList.at(index.row()).m_fmtSpec;
    return nullptr;
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
            if (section < COL_COUNT)
            {
                switch (section)
                {
                    case COL_IDX_PAT: return QVariant("Pattern");
                    case COL_IDX_REGEXP: return QVariant("Reg.Exp.?");
                    case COL_IDX_CASE: return QVariant("Match case?");
                    case COL_IDX_PIX: return QVariant("Format");
                    default: break;
                }
            }
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
            case COL_IDX_PIX: /*fall-through*/
            default: break;
        }
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
            case COL_IDX_PIX: return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            default: break;
        }
    }
    return Qt::NoItemFlags;
}

bool DlgHiglModel::setData(const QModelIndex &index, const QVariant &value, int /*role*/)
{
    int row = index.row();
    int col = index.column();

    if ((size_t)row < m_patList.size())
    {
        SearchPar& el = m_patList.at(row).m_srch;

        if (col == 0)
        {
            QString str = value.toString();
            if (!str.isEmpty())  // TODO also validate reg.exp. if applicable; display warning
            {
                el.m_pat = value.toString();
                m_modified = true;
                return true;
            }
        }
        else if (col == 1)
        {
            el.m_opt_regexp = value.toBool();
            m_modified = true;
            return true;
        }
        else if (col == 2)
        {
            el.m_opt_case = value.toBool();
            m_modified = true;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------

class DlgHidlFmtDraw : public QAbstractItemDelegate
{
public:
    DlgHidlFmtDraw(DlgHiglModel * model, const QFont& fontDdefault, const QColor& fg, const QColor& bg)
        : m_model(model)
        , m_fontDefault(fontDdefault)
        , m_fgColDefault(fg)
        , m_bgColDefault(bg)
        {}
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    const char * const s_sampleText = "...Sample text...";
    const int TXT_MARGIN = 3;
    DlgHiglModel * const m_model;
    const QFont& m_fontDefault;
    const QColor& m_fgColDefault;
    const QColor& m_bgColDefault;
};

// could also use QTextDocument::drawContents(QPainter *p, const QRectF &rect)
void DlgHidlFmtDraw::paint(QPainter *pt, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const HiglFmtSpec * fmtSpec = m_model->getFmtSpec(index);
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

        // calculate size of pixmap as sample text dimensions plus margin
        QFontMetricsF metrics(font);
        auto txtRect = metrics.boundingRect(s_sampleText);
        int w = option.rect.width();
        int h = option.rect.height();
        int xoff = (w - txtRect.width()) / 2;
        int yoff = (h - txtRect.height()) / 2;

        pt->save();
        pt->translate(option.rect.topLeft());
        pt->setClipRect(QRectF(0, 0, w, h));
        pt->setFont(font);

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

        pt->drawText(xoff - txtRect.x() + TXT_MARGIN,
                     yoff + metrics.ascent() + TXT_MARGIN,
                     s_sampleText);
        pt->restore();
    }
}

QSize DlgHidlFmtDraw::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex &index) const
{
    const HiglFmtSpec * fmtSpec = m_model->getFmtSpec(index);
    if (fmtSpec != nullptr)
    {
        QFont font(m_fontDefault);
        if (!fmtSpec->m_font.isEmpty())
        {
            font.fromString(fmtSpec->m_font);
        }

        // calculate size of pixmap as sample text dimensions plus margin
        QFontMetricsF metrics(font);
        auto txtRect = metrics.boundingRect(s_sampleText);
        int pixWidth = int(txtRect.width() + TXT_MARGIN*2);
        int pixHeight = int(txtRect.height() + TXT_MARGIN*2);

        return QSize(pixWidth, pixHeight);
    }
    return QSize();
}

// ----------------------------------------------------------------------------

/**
 * This function creates or raises the color highlighting tags list dialog.
 * This dialog shows all currently defined tag assignments.
 */
DlgHigl::DlgHigl(Highlighter * higl, MainSearch * search, MainWin * mainWin)
    : m_higl(higl)
    , m_search(search)
    , m_mainWin(mainWin)
{
  //PreemptBgTasks()
    this->setWindowTitle("Color highlights list");
    auto style = m_mainWin->getAppStyle();
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);
        //layout_top->setContentsMargins(0, 0, 0, 0);

    m_model = new DlgHiglModel(higl);
    m_fmtDelegate = new DlgHidlFmtDraw(m_model,
                                       m_mainWin->getFontContent(),
                                       m_mainWin->getFgColDefault(),
                                       m_mainWin->getBgColDefault());

    m_table = new QTableView(central_wid);
        m_table->setModel(m_model);
        m_table->setCornerButtonEnabled(false);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_table->setItemDelegateForColumn(3, m_fmtDelegate);
        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DlgHigl::selectionChanged);
        layout_top->addWidget(m_table);

    auto bfrm = new QDialogButtonBox(QDialogButtonBox::Cancel |
                                     QDialogButtonBox::Apply |
                                     QDialogButtonBox::Ok,
                                     Qt::Horizontal, central_wid);
        layout_top->addWidget(bfrm);

    auto f1 = new QToolBar("Edit", this);
        f1->setObjectName("DlgHigl::Edit"); // for saveState
        addToolBar(Qt::TopToolBarArea, f1);
    auto f1_l = new QLabel("Edit:", f1);
        f1->addWidget(f1_l);
    auto m_f1_add = new QPushButton(QIcon::fromTheme("document-new"), "Add new item", f1);
        //command=lambda:this->CopyFromSearch(sel_el), state=state_find_sel_1)
        f1->addWidget(m_f1_add);
    m_f1_del = new QPushButton(QIcon::fromTheme("edit-delete"), "Remove...", f1);
        m_f1_del->setEnabled(false);
        //command=lambda:this->Remove(sel), state=state_sel_n0)
        f1->addWidget(m_f1_del);
    m_f1_up = new QPushButton(QIcon(style->standardPixmap(QStyle::SP_ArrowUp)), "Move up", f1);
        m_f1_up->setEnabled(false);
        //TODO command=this->ShiftUp, state=DISABLED)
        f1->addWidget(m_f1_up);
    m_f1_down = new QPushButton(QIcon(style->standardPixmap(QStyle::SP_ArrowDown)), "Move down", f1);
        m_f1_down->setEnabled(false);
        //TODO command=this->ShiftDown, state=DISABLED)
        f1->addWidget(m_f1_down);
    m_f1_fmt = new QPushButton("Format...", f1);
        m_f1_fmt->setEnabled(false);
        //command=lambda:Markup_OpenDialog(sel_el), state=state_sel_1)
        f1->addWidget(m_f1_fmt);

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
    m_f2_ball = new QPushButton("All", f2);
        m_f2_ball->setEnabled(false);
        //TODO connect this->SearchList(0)
        f2->addWidget(m_f2_ball);
    m_f2_ballb = new QPushButton("All below", f2);
        m_f2_ballb->setEnabled(false);
        //TODO connect this->SearchList(1)
        f2->addWidget(m_f2_ballb);
    m_f2_balla = new QPushButton("All above", f2);
        m_f2_balla->setEnabled(false);
        //TODO connect this->SearchList(-1)
        f2->addWidget(m_f2_balla);

    setCentralWidget(central_wid);
    if (!s_winGeometry.isEmpty())
        this->restoreGeometry(s_winGeometry);
    if (!s_winState.isEmpty())
        this->restoreState(s_winState);
    m_table->setFocus(Qt::ShortcutFocusReason);

    this->show();
    //ResumeBgTasks()
}

DlgHigl::~DlgHigl()
{
    delete m_model;
    delete m_fmtDelegate;
}

/**
 * This function is bound to destruction of the dialog window.
 */
void DlgHigl::closeEvent(QCloseEvent * event)
{
    if (m_model->isModified())
    {
        auto answer = QMessageBox::question(this, "trowser", "Discard unsaved changed in the pattern highlight list?",
                                            QMessageBox::Ok | QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
        {
            event->ignore();
            return;
        }
    }

    s_winGeometry = this->saveGeometry();
    s_winState = this->saveState();

    delete s_instance;
    s_instance = nullptr;
    event->accept();
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
        m_mainWin->clearMessage(this);

        std::vector<SearchPar> patList;
        for (auto& pat_idx : sel->selectedRows())
        {
            patList.push_back(m_model->getSearchPar(pat_idx.row()));
        }
        m_search->searchFirst(is_fwd, patList);
    }
    else
        m_mainWin->showError(this, "No pattern is selected in the list");
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

// ----------------------------------------------------------------------------

DlgHigl * DlgHigl::s_instance = nullptr;
QByteArray DlgHigl::s_winGeometry;
QByteArray DlgHigl::s_winState;

void DlgHigl::openDialog(Highlighter * higl, MainSearch * search, MainWin * mainWin) /*static*/
{
    if (s_instance == nullptr)
    {
        s_instance = new DlgHigl(higl, search, mainWin);
    }
    else
    {
        s_instance->activateWindow();
        s_instance->raise();
    }
}


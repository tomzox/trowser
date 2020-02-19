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
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QComboBox>
#include <QAbstractItemModel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFontDialog>
#include <QColorDialog>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "highlighter.h"
#include "dlg_markup.h"

// ----------------------------------------------------------------------------

static const char * const
    SAMPLE_STR_TXT = "Lorem ipsum dolor sit amet, consectetur adipisici elit, sed eiusmod tempor incidunt ut labore et dolore magna aliqua.\n"
                     "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquid ex ea commodi consequat.\n"
                     "Quis aute iure reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.\n"
                     "Excepteur sint obcaecat cupiditat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
static const unsigned SAMPLE_STR_MARKUP_BLK = 1;
static const unsigned SAMPLE_STR_LINE_CNT = 4;

static const Qt::BrushStyle s_brushStyles[] =
{
    Qt::NoBrush,               Qt::Dense1Pattern,          Qt::Dense2Pattern,
    Qt::Dense3Pattern,         Qt::Dense4Pattern,          Qt::Dense5Pattern,
    Qt::Dense6Pattern,         Qt::Dense7Pattern,          //Qt::SolidPattern,
    Qt::HorPattern,            Qt::VerPattern,             Qt::CrossPattern,
    Qt::BDiagPattern,          Qt::FDiagPattern,           Qt::DiagCrossPattern,
};
static const size_t s_brushStyleCnt = sizeof(s_brushStyles) / sizeof(s_brushStyles[0]);

// ----------------------------------------------------------------------------

class BrushStyleListModel : public QAbstractItemModel
{
public:
    BrushStyleListModel()
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
        return s_brushStyleCnt;
    }
    virtual int columnCount(const QModelIndex& parent __attribute__((unused)) = QModelIndex()) const override
    {
        return 1;
    }
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (size_t(index.row()) < s_brushStyleCnt)
        {
            if (role == Qt::DisplayRole)
            {
                switch (s_brushStyles[index.row()])
                {
                    case Qt::NoBrush:          return "None";
                    case Qt::Dense1Pattern:    return "Dense (90%)";
                    case Qt::Dense2Pattern:    return "Dense (75%)";
                    case Qt::Dense3Pattern:    return "Medium (60%)";
                    case Qt::Dense4Pattern:    return "Medium (50%)";
                    case Qt::Dense5Pattern:    return "Medium (40%)";
                    case Qt::Dense6Pattern:    return "Light (25%)";
                    case Qt::Dense7Pattern:    return "Light (10%)";
                    case Qt::HorPattern:       return "Horizontal";
                    case Qt::VerPattern:       return "Vertical";
                    case Qt::CrossPattern:     return "Cross";
                    case Qt::BDiagPattern:     return "Diagonal";
                    case Qt::FDiagPattern:     return "Diagonal";
                    case Qt::DiagCrossPattern: return "Cross-diag.";
                    default: break;
                }
            }
            else if ((role == Qt::DecorationRole) && (index.row() != 0))
            {
                QPixmap pix(16, 16);
                pix.fill(QColor(QRgb(0xffffffff)));
                QPainter pt(&pix);
                pt.fillRect(0, 0, 16, 16, QBrush(QColor(QRgb(0xff000000)),
                                                 s_brushStyles[index.row()]));
                return QVariant(pix);
            }
        }
        return QVariant();
    }

    int getIndexOfStyle(Qt::BrushStyle style)
    {
        for (size_t idx = 0; idx < s_brushStyleCnt; ++idx)
            if (s_brushStyles[idx] == style)
                return idx;
        return -1;
    }
};

// ----------------------------------------------------------------------------

/**
 * This function creates the color highlighting editor dialog.
 * This dialog shows all currently defined pattern definitions.
 */
DlgMarkup::DlgMarkup(HiglId id, const QString& name, const HiglFmtSpec * fmtSpec,
                     Highlighter * higl, MainWin * mainWin)
    : m_id(id)
    , m_fmtSpecOrig(*fmtSpec)
    , m_fmtSpec(*fmtSpec)
    , m_higl(higl)
    , m_mainWin(mainWin)
{
    this->setWindowTitle("Pattern /" + name + "/ mark-up");
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    QFontMetricsF metrics(m_mainWin->getFontContent());
    m_sampleWid = new QPlainTextEdit(central_wid);
        m_sampleWid->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_sampleWid->setFont(m_mainWin->getFontContent());
        m_sampleWid->setFocusPolicy(Qt::NoFocus);
        m_sampleWid->setPlainText(SAMPLE_STR_TXT);
        m_sampleWid->setReadOnly(true);
        m_sampleWid->setFixedHeight(metrics.height() * (SAMPLE_STR_LINE_CNT + 2));
        drawTextSample();
        layout_top->addWidget(m_sampleWid);

    m_brushStyleModel = new BrushStyleListModel();

    //
    // Background color & style
    //
    auto layout_grid = new QGridLayout();
        layout_grid->setColumnStretch(2, 1);
        layout_grid->setColumnStretch(4, 1);
    auto lab = new QLabel("Background:", central_wid);
        layout_grid->addWidget(lab, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lab = new QLabel("Color:", central_wid);
        layout_grid->addWidget(lab, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);
    m_bgColBut = new QPushButton("", central_wid);
        auto men = new QMenu("Color:", central_wid);
            auto act = men->addAction("Reset to no color");
                connect(act, &QAction::triggered,
                        [=](){ cmdResetColor(&m_fmtSpec.m_bgCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered,
                        [=](){ cmdSelectColor(&m_fmtSpec.m_bgCol, "Background"); });
        m_bgColBut->setMenu(men);
        m_bgColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_bgColBut, 0, 2);
    lab = new QLabel("Pattern:", central_wid);
        layout_grid->addWidget(lab, 0, 3, Qt::AlignLeft | Qt::AlignVCenter);
    m_bgStyleBut = new QComboBox(central_wid);
        m_bgStyleBut->setModel(m_brushStyleModel);
        m_bgStyleBut->setCurrentIndex(m_brushStyleModel->getIndexOfStyle(m_fmtSpec.m_bgStyle));
        connect(m_bgStyleBut, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [=](int idx){ cmdSelectStyle(&m_fmtSpec.m_bgStyle, idx); });
        m_bgStyleBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_bgStyleBut, 0, 4);

    //
    // Foreground (text) color & style
    //
    lab = new QLabel("Text:", central_wid);
        layout_grid->addWidget(lab, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lab = new QLabel("Color:", central_wid);
        layout_grid->addWidget(lab, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    m_fgColBut = new QPushButton("", central_wid);
        men = new QMenu("Color:", central_wid);
            act = men->addAction("Reset to no color");
                connect(act, &QAction::triggered,
                        [=](){ cmdResetColor(&m_fmtSpec.m_fgCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered,
                        [=](){ cmdSelectColor(&m_fmtSpec.m_fgCol, "Foreground"); });
        m_fgColBut->setMenu(men);
        m_fgColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_fgColBut, 1, 2);
    lab = new QLabel("Pattern:", central_wid);
        layout_grid->addWidget(lab, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
    m_fgStyleBut = new QComboBox(central_wid);
        m_fgStyleBut->setModel(m_brushStyleModel);
        m_fgStyleBut->setCurrentIndex(m_brushStyleModel->getIndexOfStyle(m_fmtSpec.m_fgStyle));
        connect(m_fgStyleBut, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [=](int idx){ cmdSelectStyle(&m_fmtSpec.m_fgStyle, idx); });
        m_fgStyleBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_fgStyleBut, 1, 4);

    //
    // Text outline
    //
    lab = new QLabel("Text outline:", central_wid);
        layout_grid->addWidget(lab, 2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lab = new QLabel("Color:", central_wid);
        layout_grid->addWidget(lab, 2, 1, Qt::AlignLeft | Qt::AlignVCenter);
    m_olColBut = new QPushButton("", central_wid);
        men = new QMenu("Color:", central_wid);
            act = men->addAction("Reset to no color");
                connect(act, &QAction::triggered,
                        [=](){ cmdResetColor(&m_fmtSpec.m_olCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered,
                        [=](){ cmdSelectColor(&m_fmtSpec.m_olCol, "Text outline"); });
        m_olColBut->setMenu(men);
        m_olColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_olColBut, 2, 2);

    //
    // Font family/size selection
    //
    lab = new QLabel("Font:", central_wid);
        layout_grid->addWidget(lab, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_curFontBut = new QPushButton("", central_wid);
        men = new QMenu("Font:", central_wid);
            act = men->addAction("Reset to use default");
                connect(act, &QAction::triggered, [=](){ cmdResetFont(); });
            act = men->addAction("Select font...");
                connect(act, &QAction::triggered, [=](){ cmdSelectFont(); });
        m_curFontBut->setMenu(men);
        m_curFontBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_curFontBut, 3, 1, 1, 4);

    //
    // Font option checkboxes
    //
    auto frm = new QFrame(central_wid);
        auto layout_rb = new QHBoxLayout(frm);
        auto rb = new QCheckBox("Underline", central_wid);
            rb->setChecked(m_fmtSpec.m_underline);
            connect(rb, &QCheckBox::stateChanged,
                    [=](int state){ cmdSetFontOption(&m_fmtSpec.m_underline, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Bold", central_wid);
            rb->setChecked(m_fmtSpec.m_bold);
            connect(rb, &QCheckBox::stateChanged,
                    [=](int state){ cmdSetFontOption(&m_fmtSpec.m_bold, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Italic", central_wid);
            rb->setChecked(m_fmtSpec.m_italic);
            connect(rb, &QCheckBox::stateChanged,
                    [=](int state){ cmdSetFontOption(&m_fmtSpec.m_italic, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Strike out", central_wid);
            rb->setChecked(m_fmtSpec.m_strikeout);
            connect(rb, &QCheckBox::stateChanged,
                    [=](int state){ cmdSetFontOption(&m_fmtSpec.m_strikeout, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        layout_grid->addWidget(frm, 4, 1, 1, 4);

    drawFontButtonText();
    drawButtonPixmaps();
    layout_top->addLayout(layout_grid);

    m_cmdButs = new QDialogButtonBox(QDialogButtonBox::Cancel |
                                     QDialogButtonBox::Apply |
                                     QDialogButtonBox::Ok,
                                     Qt::Horizontal, central_wid);
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(false);
        connect(m_cmdButs, &QDialogButtonBox::clicked, this, &DlgMarkup::cmdButton);
        layout_top->addWidget(m_cmdButs);

    connect(m_mainWin, &MainWin::textFontChanged, this, &DlgMarkup::mainFontChanged);

    setCentralWidget(central_wid);
    this->show();
}

DlgMarkup::~DlgMarkup()
{
    delete m_brushStyleModel;
}


/**
 * This slot is connected to notification of font changes in the main text
 * window, as the same font is used for displaying text content in the dialog.
 * The function triggers a redraw of the sample text.
 */
void DlgMarkup::mainFontChanged()
{
    QFontMetricsF metrics(m_mainWin->getFontContent());
    m_sampleWid->setFixedHeight(metrics.height() * (SAMPLE_STR_LINE_CNT + 2));
    m_sampleWid->setFont(m_mainWin->getFontContent());

    drawTextSample();
}


/**
 * This function is bound to destruction of the dialog window. The event is
 * also sent artificially upon the "Cancel" button.
 */
void DlgMarkup::closeEvent(QCloseEvent * event)
{
    if (m_isModified)
    {
        auto answer = QMessageBox::question(this, "trowser", "Discard unsaved changes to mark-up?",
                                            QMessageBox::Ok | QMessageBox::Cancel);
        if (answer != QMessageBox::Ok)
        {
            if (event)
                event->ignore();
            return;
        }
    }
    event->accept();

    emit closeReq(m_id);
}


/**
 * This function is bound to each of the main command buttons: Ok, Apply,
 * Cancel.
 */
void DlgMarkup::cmdButton(QAbstractButton * button)
{
    if (button == m_cmdButs->button(QDialogButtonBox::Cancel))
    {
        QCoreApplication::postEvent(this, new QCloseEvent());
    }
    else if (button == m_cmdButs->button(QDialogButtonBox::Ok))
    {
        if (m_isModified)
            emit applyReq(m_id, false);
        this->close();
    }
    else if (button == m_cmdButs->button(QDialogButtonBox::Apply))
    {
        if (m_isModified)
            emit applyReq(m_id, true);
    }
}

void DlgMarkup::resetModified()
{
    m_fmtSpecOrig = m_fmtSpec;
    m_isModified = false;
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);
}

void DlgMarkup::cmdSetFontOption(bool *option, bool value)
{
    *option = value;

    m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

    drawTextSample();
}

void DlgMarkup::cmdResetFont()
{
    m_fmtSpec.m_font.clear();

    m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

    drawFontButtonText();
    drawTextSample();
}

void DlgMarkup::cmdSelectFont()
{
    QFont font;
    if (!m_fmtSpec.m_font.isEmpty())
        font.fromString(m_fmtSpec.m_font);
    else
        font = m_mainWin->getFontContent();

    bool ok;
    font = QFontDialog::getFont(&ok, font, this);
    if (ok)
    {
        m_fmtSpec.m_font = font.toString();

        m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

        drawFontButtonText();
        drawTextSample();
    }
}

void DlgMarkup::cmdResetColor(QRgb * col)
{
    *col = HiglFmtSpec::INVALID_COLOR;

    m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

    drawTextSample();
    drawButtonPixmaps();
}

void DlgMarkup::cmdSelectColor(QRgb * col, const QString& desc)
{
    auto newCol = QColorDialog::getColor(QColor(*col), this, desc + " color selection");
    if (newCol.isValid())
    {
        *col = newCol.rgba();

        m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

        drawTextSample();
        drawButtonPixmaps();
    }
}

void DlgMarkup::cmdSelectStyle(Qt::BrushStyle * style, int idx)
{
    if (size_t(idx) < s_brushStyleCnt)
    {
        *style = s_brushStyles[idx];

        m_isModified = !(m_fmtSpec == m_fmtSpecOrig);
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(m_isModified);

        drawTextSample();
    }
}

void DlgMarkup::drawFontButtonText()
{
    if (!m_fmtSpec.m_font.isEmpty())
    {
        auto desc = m_fmtSpec.m_font.split(',');
        m_curFontBut->setText(desc.at(0) + " (" + desc.at(1) + ")");
    }
    else
        m_curFontBut->setText("[default]");
}

void DlgMarkup::drawTextSample()
{
    QTextCharFormat charFmt;
    m_higl->configFmt(charFmt, m_fmtSpec);

    QTextBlock blk = m_sampleWid->document()->findBlockByNumber(SAMPLE_STR_MARKUP_BLK);
    QTextCursor c(blk);
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    c.setCharFormat(charFmt);
}

void DlgMarkup::drawButtonPixmaps()
{
    QPixmap pix(16, 16);

    drawButtonPixmap(m_bgColBut, m_fmtSpec.m_bgCol, Qt::NoBrush, pix);
    drawButtonPixmap(m_fgColBut, m_fmtSpec.m_fgCol, Qt::NoBrush, pix);
    drawButtonPixmap(m_olColBut, m_fmtSpec.m_olCol, Qt::NoBrush, pix);
}

void DlgMarkup::drawButtonPixmap(QPushButton *but, QRgb col, Qt::BrushStyle style, QPixmap& pix)
{
    QPainter pt(&pix);
    auto& pal = but->palette();

    if (col != HiglFmtSpec::INVALID_COLOR)
    {
        pt.fillRect(0, 0, pix.width(), pix.height(), QColor(col));
        pt.setPen(pal.color(QPalette::WindowText));
        pt.drawRect(0, 0, pix.width() - 1, pix.height() - 1);
        but->setIcon(QIcon(pix));
    }
    else if (style != Qt::NoBrush)
    {
        pt.fillRect(0, 0, pix.width(), pix.height(),
                    QBrush(QColor(pal.color(QPalette::WindowText)), style));
        but->setIcon(QIcon(pix));
    }
    else
    {
        but->setIcon(QIcon());
    }
}

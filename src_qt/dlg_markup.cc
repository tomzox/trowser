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
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFontDialog>
#include <QColorDialog>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "highlighter.h"
#include "dlg_markup.h"

#define SAMPLE_STR_0 "\n"
#define SAMPLE_STR_1 "Text line above\n"
#define SAMPLE_STR_2 "Text sample ... sample text\n"
#define SAMPLE_STR_3 "Line below\n"
#define SAMPLE_STR_CNT 4

// ----------------------------------------------------------------------------

/**
 * This function creates the color highlighting editor dialog.
 * This dialog shows all currently defined pattern definitions.
 */
DlgMarkup::DlgMarkup(const QString& name, const HiglFmtSpec * fmtSpec,
                     Highlighter * higl, MainWin * mainWin)
    : m_fmtSpec(*fmtSpec)
    , m_higl(higl)
    , m_mainWin(mainWin)
{
    this->setWindowTitle("Pattern /" + name + "/ mark-up");
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    QFontMetricsF metrics(m_mainWin->getFontContent());
    m_sampleWid = new QPlainTextEdit(central_wid);
        m_sampleWid->setPlainText(SAMPLE_STR_0 SAMPLE_STR_1 SAMPLE_STR_2 SAMPLE_STR_3);
        m_sampleWid->setFont(m_mainWin->getFontContent());
        m_sampleWid->setFocusPolicy(Qt::NoFocus);
        m_sampleWid->setReadOnly(true);
        m_sampleWid->setFixedHeight(metrics.height() * (SAMPLE_STR_CNT + 2));
        //m_sampleWid->setFixedWidth(metrics.averageCharWidth() * (strlen(SAMPLE_STR_2) + 10));
        drawTextSample();
        layout_top->addWidget(m_sampleWid);

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
                connect(act, &QAction::triggered, [=](){ cmdResetColor(&m_fmtSpec.m_bgCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered, [=](){ cmdSelectColor(&m_fmtSpec.m_bgCol, "Background"); });
        m_bgColBut->setMenu(men);
        m_bgColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_bgColBut, 0, 2);
    lab = new QLabel("Pattern:", central_wid);
        layout_grid->addWidget(lab, 0, 3, Qt::AlignLeft | Qt::AlignVCenter);
    m_bgStyleBut = new QPushButton("", central_wid);
        //TODO m_bgStyleBut->setMenu(men);
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
                connect(act, &QAction::triggered, [=](){ cmdResetColor(&m_fmtSpec.m_fgCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered, [=](){ cmdSelectColor(&m_fmtSpec.m_fgCol, "Foreground"); });
        m_fgColBut->setMenu(men);
        m_fgColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_fgColBut, 1, 2);
    lab = new QLabel("Pattern:", central_wid);
        layout_grid->addWidget(lab, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
    m_fgStyleBut = new QPushButton("", central_wid);
        //TODO m_fgStyleBut->setMenu(men);
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
                connect(act, &QAction::triggered, [=](){ cmdResetColor(&m_fmtSpec.m_olCol); });
            act = men->addAction("Select color...");
                connect(act, &QAction::triggered, [=](){ cmdSelectColor(&m_fmtSpec.m_olCol, "Text outline"); });
        m_olColBut->setMenu(men);
        m_olColBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_olColBut, 2, 2);

    //
    // Font selection
    //
    lab = new QLabel("Font:", central_wid);
        layout_grid->addWidget(lab, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_curFontBut = new QPushButton("", central_wid);
        men = new QMenu("Font:", central_wid);
            act = men->addAction("Reset to use default");
                connect(act, &QAction::triggered, [=](){ cmdSelectFont(true); });
            act = men->addAction("Select font...");
                connect(act, &QAction::triggered, [=](){ cmdSelectFont(false); });
        m_curFontBut->setMenu(men);
        m_curFontBut->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        layout_grid->addWidget(m_curFontBut, 3, 1, 1, 4);

    //
    // Font option checkboxes
    //
    auto frm = new QFrame(central_wid);
        auto layout_rb = new QHBoxLayout(frm);
        auto rb = new QCheckBox("Underline", central_wid);
            connect(rb, &QCheckBox::stateChanged, [=](int state){ cmdSetFontOption(&m_fmtSpec.m_underline, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Bold", central_wid);
            connect(rb, &QCheckBox::stateChanged, [=](int state){ cmdSetFontOption(&m_fmtSpec.m_bold, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Italic", central_wid);
            connect(rb, &QCheckBox::stateChanged, [=](int state){ cmdSetFontOption(&m_fmtSpec.m_italic, (state == Qt::Checked)); });
            layout_rb->addWidget(rb);
        rb = new QCheckBox("Strike out", central_wid);
            connect(rb, &QCheckBox::stateChanged, [=](int state){ cmdSetFontOption(&m_fmtSpec.m_strikeout, (state == Qt::Checked)); });
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
    // deletion all done by QWidget
}


/**
 * This slot is connected to notification of font changes in the main text
 * window, as the same font is used for displaying text content in the dialog.
 * The function triggers a redraw of the sample text.
 */
void DlgMarkup::mainFontChanged()
{
    QFontMetricsF metrics(m_mainWin->getFontContent());
    m_sampleWid->setFixedHeight(metrics.height() * (SAMPLE_STR_CNT + 2));
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

    emit closeReq();
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
            emit applyReq(false);
        this->close();
    }
    else if (button == m_cmdButs->button(QDialogButtonBox::Apply))
    {
        if (m_isModified)
            emit applyReq(true);
    }
}

void DlgMarkup::resetModified()
{
    m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(false);
    m_isModified = false;
}

void DlgMarkup::cmdSetFontOption(bool *option, bool value)
{
    if (*option != value)
    {
        *option = value;

        m_isModified = true;
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
    drawTextSample();
}

void DlgMarkup::cmdSelectFont(bool doReset)
{
    if (doReset)
    {
        m_fmtSpec.m_font.clear();
    }
    else
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
            if (m_fmtSpec.m_font != font.toString())
            {
                m_fmtSpec.m_font = font.toString();

                m_isModified = true;
                m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
            }
        }
    }
    drawFontButtonText();
    drawTextSample();
}

void DlgMarkup::cmdResetColor(QRgb * col)
{
    if (*col != HiglFmtSpec::INVALID_COLOR)
    {
        *col = HiglFmtSpec::INVALID_COLOR;
        m_isModified = true;
        m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
    }

    drawTextSample();
    drawButtonPixmaps();
}

void DlgMarkup::cmdSelectColor(QRgb * col, const QString& desc)
{
    auto newCol = QColorDialog::getColor(QColor(*col), this, desc + " color selection");
    if (newCol.isValid())
    {
        if (*col != newCol.rgba())
        {
            *col = newCol.rgba();

            m_isModified = true;
            m_cmdButs->button(QDialogButtonBox::Apply)->setEnabled(true);
        }

        drawTextSample();
        drawButtonPixmaps();
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

    QTextBlock blk = m_sampleWid->document()->findBlockByNumber(2);
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
    drawButtonPixmap(m_bgStyleBut, HiglFmtSpec::INVALID_COLOR, m_fmtSpec.m_bgStyle, pix);
    drawButtonPixmap(m_fgStyleBut, HiglFmtSpec::INVALID_COLOR, m_fmtSpec.m_fgStyle, pix);
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

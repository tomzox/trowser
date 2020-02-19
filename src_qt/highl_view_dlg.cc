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
#include <QAbstractItemModel>
#include <QPainter>
#include <QDebug>

#include <cstdio>
#include <string>

#include "highlighter.h"
#include "highl_view_dlg.h"

// ----------------------------------------------------------------------------

void HighlightViewDelegate::paint(QPainter *pt, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const HiglFmtSpec * fmtSpec = m_model->getFmtSpec(index);

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
        if (fmtSpec->m_strikeout)
            font.setStrikeOut(true);
    }

    auto data = m_model->higlModelData(index);
    QFontMetricsF metrics(font);
    int w = option.rect.width();
    int h = option.rect.height();
    int xoff = 0;
    int yoff = 0;
    if (m_centering)
    {   
        auto txtRect = metrics.boundingRect(data.toString());
        xoff = (w - txtRect.width()) / 2 - txtRect.x();
        yoff = (h - txtRect.height()) / 2 + TXT_MARGIN;
    }

    pt->save();
    pt->translate(option.rect.topLeft());
    pt->setClipRect(QRectF(0, 0, w, h));
    pt->setFont(font);

    if ((option.state & QStyle::State_Selected) && !m_centering)
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

        if (fmtSpec->m_olCol != HiglFmtSpec::INVALID_COLOR)
        {
            QPainterPath txtPath;
            txtPath.addText(TXT_MARGIN + xoff, metrics.ascent() + yoff,
                            font, data.toString());
            pt->setPen(QColor(fmtSpec->m_olCol));
            pt->drawPath(txtPath);
        }

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

    pt->drawText(TXT_MARGIN + xoff, metrics.ascent() + yoff, data.toString());

    pt->restore();
}

QSize HighlightViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex &index) const
{
    const HiglFmtSpec * fmtSpec = m_model->getFmtSpec(index);
    if (fmtSpec != nullptr)
    {
        QFont font(m_fontDefault);
        if (!fmtSpec->m_font.isEmpty())
        {
            font.fromString(fmtSpec->m_font);
        }

        if (m_centering)
        {
            // calculate size of pixmap as sample text dimensions plus margin
            auto data = m_model->higlModelData(index);
            QFontMetricsF metrics(font);
            auto txtRect = metrics.boundingRect(data.toString());
            int pixWidth = int(txtRect.width() + TXT_MARGIN*2);
            int pixHeight = int(txtRect.height() + TXT_MARGIN*2);

            return QSize(pixWidth, pixHeight);
        }
        else
        {
            QFontMetricsF metrics(font);
            return QSize(1, metrics.height());
        }
    }
    return QSize();
}

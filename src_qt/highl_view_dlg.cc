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
    int line = m_model->higlModelGetLineOfIdx(index.row());
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
        if (fmtSpec->m_strikeout)
            font.setStrikeOut(true);
    }

    int w = option.rect.width();
    int h = option.rect.height();
    QFontMetricsF metrics(font);
    auto data = m_model->higlModelData(index);

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

        if (fmtSpec->m_olCol != HiglFmtSpec::INVALID_COLOR)
        {
            QPainterPath txtPath;
            txtPath.addText(TXT_MARGIN, metrics.ascent(), font, data.toString());
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

    pt->drawText(TXT_MARGIN, metrics.ascent(), data.toString());

    pt->restore();
}

QSize HighlightViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    //TODO other fonts
    QFontMetricsF metrics(m_fontDefault);
    return QSize(1, metrics.height());
}

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
#ifndef _HIGL_VIEW_DLG_H
#define _HIGL_VIEW_DLG_H

#include <QAbstractItemDelegate>

class QStyleOptionViewItem;
class QModelIndex;
class QPainter;
class QFont;
class QColor;

class HiglFmtSpec;

// ----------------------------------------------------------------------------

class HighlightViewModelIf  // : class QAbstractItemModel
{
public:
    HighlightViewModelIf() = default;
    virtual const HiglFmtSpec * getFmtSpec(const QModelIndex& index) const = 0;
    virtual QVariant higlModelData(const QModelIndex& index, int role = Qt::DisplayRole) const = 0;
};

// ----------------------------------------------------------------------------

class HighlightViewDelegate : public QAbstractItemDelegate
{
public:
    HighlightViewDelegate(HighlightViewModelIf * model, bool centering,
                          const QFont& fontDdefault, const QColor& fg, const QColor& bg)
        : m_model(model)
        , m_centering(centering)
        , m_fontDefault(fontDdefault)
        , m_fgColDefault(fg)
        , m_bgColDefault(bg)
        {}
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    const int TXT_MARGIN = 3;
    HighlightViewModelIf * const m_model;
    bool m_centering;
    const QFont& m_fontDefault;
    const QColor& m_fgColDefault;
    const QColor& m_bgColDefault;
};

#endif /* _HIGL_VIEW_DLG_H */

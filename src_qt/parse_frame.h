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
#ifndef _PARSE_FRAME_H
#define _PARSE_FRAME_H

#include <QString>

#include <memory>

class QTextDocument;
class QJsonObject;
class QJsonValue;

// ----------------------------------------------------------------------------

enum class ParseColumnFlags
{
    Val = 1, ValDelta = 2, Frm = 4, FrmDelta = 8
};
using ParseColumns = QFlags<ParseColumnFlags>;

class ParseSpec
{
public:
    ParseSpec() = default;
    ParseSpec(const QJsonValue& val);
    QJsonObject getRcValues() const;
    ParseColumns getColumns() const;
    const QString& getHeader(ParseColumnFlags col) const;

public:
    QString     m_valPat;
    QString     m_valHeader;
    bool        m_valDelta = true;

    QString     m_frmPat;
    QString     m_frmHeader;
    bool        m_frmFwd = false;
    bool        m_frmCapture = false;
    bool        m_frmDelta = true;

    uint32_t    m_range = 5000;
};

bool operator==(const ParseSpec& lhs, const ParseSpec& rhs);
bool operator!=(const ParseSpec& lhs, const ParseSpec& rhs);

// ----------------------------------------------------------------------------

struct ParseFrameInfo
{
    int frmStart;
    int valMatch;
    int frmEnd;
};

// ----------------------------------------------------------------------------

class ParseFrame;
using ParseFramePtr = std::unique_ptr<ParseFrame>;

class ParseFrame
{
public:
    static ParseFramePtr create(QTextDocument* doc, const ParseSpec& spec);

    virtual ~ParseFrame() = default;
    virtual bool isValid() const = 0;
    virtual QString parseFrame(int line, int col) = 0;
    virtual void clearCache() = 0;
    virtual ParseFrameInfo getMatchInfo() const = 0;

    ParseColumns getColumns() const { return m_spec.getColumns(); }
    const QString& getHeader(ParseColumnFlags col) const { return m_spec.getHeader(col); };

protected:
    ParseFrame(QTextDocument* doc, const ParseSpec& spec);

    QTextDocument * const m_doc;
    const ParseSpec m_spec;
};

#endif /* _PARSE_FRAME_H */

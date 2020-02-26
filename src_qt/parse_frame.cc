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

#include <QTextDocument>
#include <QTextBlock>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QDebug>

#include <cstdio>
#include <string>
#include <utility>
#include <map>

#include "parse_frame.h"

// ----------------------------------------------------------------------------

class ParseFrameCacheEntry
{
public:
    int startLine;  // always equal the key of this entry
    int lastLine;
    QString val;
    QString frm;
};

using ParseFrameCacheMap = std::map<int,ParseFrameCacheEntry>;

// ----------------------------------------------------------------------------

/**
 * Constructor for parser using backward-search only
 */
class ParseFrameLinear : public ParseFrame
{
public:
    ParseFrameLinear(QTextDocument* doc, const ParseSpec& spec);
    virtual bool isValid() const override;
    virtual QString parseFrame(int line, int col) override;
    virtual void clearCache() override { m_cache.clear(); }
    virtual ParseFrameInfo getMatchInfo() const override;

private:
    const QRegularExpression tick_pat_num;
    const QRegularExpression tick_pat_sep;

    ParseFrameCacheMap m_cache;

    int m_prevFrmStart = -1;
    int m_prevValMatch = -1;
};

ParseFrameLinear::ParseFrameLinear(QTextDocument* doc, const ParseSpec& spec)
    : ParseFrame(doc, spec)
    , tick_pat_num(spec.m_valPat)
    , tick_pat_sep(spec.m_frmPat)
{
}

bool ParseFrameLinear::isValid() const
{
    return tick_pat_num.isValid();
}

ParseFrameInfo ParseFrameLinear::getMatchInfo() const
{
    return {m_prevFrmStart, m_prevValMatch, -1};
}

QString ParseFrameLinear::parseFrame(int line, int col)
{
    // search for cache entry of frame starting exactly at (or after) the given line
    ParseFrameCacheEntry * cached = nullptr;
    auto cache_it = m_cache.lower_bound(line);
    if ((cache_it != m_cache.end()) && (cache_it->second.startLine == line))
    {
        cached = &cache_it->second;
        //printf("XXX|linear CACHE hit line:%d => [%s,%s]\n", line, cached->val.toLatin1().data(), cached->frm.toLatin1().data());
        return ((col == 0) ? cached->val : cached->frm);
    }
    // most of the time we need to go back one entry for the frame enclosing the requested line
    else if (cache_it != m_cache.begin())
    {
        auto cache_prev = cache_it;
        cached = &((--cache_prev)->second);
        if ((line >= cached->startLine) && (line <= cached->lastLine))
        {
            //printf("XXX|linear CACHE hit line:%d [%d...%d] => [%s,%s]\n", line, cached->startLine, cached->lastLine, cached->val.toLatin1().data(),cached->frm.toLatin1().data());
            return ((col == 0) ? cached->val : cached->frm);
        }
    }

    // start searching backwards from the given line
    QTextBlock blkOfLine = m_doc->findBlockByNumber(line);
    QTextBlock blk1 = blkOfLine;
    QString valStr;
    uint32_t limit = m_spec.m_range;  // underflow if zero is expected
    while (blk1.isValid() && --limit)
    {
        if (cached && (cached->lastLine == blk1.blockNumber()))
        {
            // extend cache entry to include the currently requested line
            //printf("XXX|linear CACHE extend line:%d [%d...%d] => [%s,%s]\n", line, cached->startLine, cached->lastLine, cached->val.toLatin1().data(),cached->frm.toLatin1().data());
            cached->lastLine = line;
            return ((col == 0) ? cached->val : cached->frm);
        }
        Q_ASSERT(!cached || (cached->lastLine < blk1.blockNumber()));

        QString text = blk1.text();
        auto mat = tick_pat_num.match(text);
        if (mat.hasMatch())
        {
            m_prevValMatch = blk1.blockNumber();
            m_prevFrmStart = -1;

            if (mat.lastCapturedIndex() >= 1)
                valStr = mat.captured(1);

            if (!m_spec.m_frmCapture)
            {
                // add result to the cache
                m_cache.emplace_hint(cache_it, std::make_pair(m_prevValMatch,
                                         ParseFrameCacheEntry{m_prevValMatch, line,
                                                              valStr, QString()}));
                //printf("XXX|linear MATCH hit line:%d [%d...%d] => [%s,]\n", line, m_prevFrmStart, m_prevValMatch, valStr.toLatin1().data());
                return ((col == 0) ? valStr : QString());
            }
        }
        if (!m_spec.m_frmPat.isEmpty())
        {
            mat = tick_pat_sep.match(text);
            if (mat.hasMatch())
            {
                m_prevFrmStart = blk1.blockNumber();

                // add result to the cache
                m_cache.emplace_hint(cache_it, std::make_pair(m_prevFrmStart,
                                         ParseFrameCacheEntry{m_prevFrmStart, line,
                                                              valStr,
                                                              (  (m_spec.m_frmCapture && (mat.lastCapturedIndex() >= 1))
                                                               ? mat.captured(1) : QString()) }));
                //printf("XXX|linear FRAME hit line:%d [%d...%d] => [%s,%s]\n", line, m_prevFrmStart, m_prevValMatch, valStr.toLatin1().data(), mat.captured(1).toLatin1().data());
                return ((col == 0) ? valStr : QString());
            }
        }
        blk1 = blk1.previous();
    }
    // TODO empty cache entry: HOWEVER must not return that unless line - startLine > limit
    return QString();
}

// ----------------------------------------------------------------------------

/**
 * Constructor for parser using frame-based search
 */
class ParseFrameRange : public ParseFrame
{
public:
    ParseFrameRange(QTextDocument* doc, const ParseSpec& spec);
    virtual bool isValid() const override;
    virtual QString parseFrame(int line, int col) override;
    virtual void clearCache() override { m_cache.clear(); }
    virtual ParseFrameInfo getMatchInfo() const override;

private:
    const QRegularExpression tick_pat_num;
    const QRegularExpression tick_pat_sep;

    ParseFrameCacheMap m_cache;

    int m_prevFrmStart = -1;
    int m_prevValMatch = -1;
    int m_prevFrmEnd = -1;
    QString m_prevResult;
};

ParseFrameRange::ParseFrameRange(QTextDocument* doc, const ParseSpec& spec)
    : ParseFrame(doc, spec)
    , tick_pat_num(spec.m_valPat)
    , tick_pat_sep(spec.m_frmPat)
{
}

bool ParseFrameRange::isValid() const
{
    return tick_pat_num.isValid() && tick_pat_sep.isValid();
}

ParseFrameInfo ParseFrameRange::getMatchInfo() const
{
    return { m_prevFrmStart, m_prevValMatch, m_prevFrmEnd };
}


/**
 * This function retrieves the "frame number" (timestamp) to which a given line
 * of text belongs via pattern matching. Two methods for retrieving the number
 * can be used, depending on which patterns are defined.
 */
QString ParseFrameRange::parseFrame(int line, int col)
{
    // Query the cache before parsing for the frame number
    auto cache_it = m_cache.lower_bound(line);
    if ((cache_it != m_cache.end()) && (cache_it->second.startLine == line))
    {
        auto cached = &cache_it->second;
        //printf("XXX|range CACHE hit line:%d => [%s,%s]\n", line, cached->val.toLatin1().data(), cached->frm.toLatin1().data());
        return ((col == 0) ? cached->val : cached->frm);
    }
    // most of the time we need to go back one entry for the frame enclosing the requested line
    else if (cache_it != m_cache.begin())
    {
        auto cache_prev = cache_it;
        auto cached = &((--cache_prev)->second);
        if ((line >= cached->startLine) && (line < cached->lastLine))  // not <= unlike Linear
        {
            //printf("XXX|range CACHE hit line:%d [%d...%d] => [%s,%s]\n", line, cached->startLine, cached->lastLine, cached->val.toLatin1().data(),cached->frm.toLatin1().data());
            return ((col == 0) ? cached->val : cached->frm);
        }
    }
    QString prefix;

    // determine frame number by searching forwards and backwards for frame boundaries
    // marked by a frame separator pattern; then within these boundaries search for FN
    QString tick_no;
    QString fn;
    int frameStartLine = -1;
    int frameEndLine = -1;
    int valMatchLine = -1;
    QString frmStr;
    QString valStr;

    // Step #1: search backwards for frame boundary pattern & value extraction
    QTextBlock blkOfLine = m_doc->findBlockByNumber(line);
    QTextBlock blk1 = blkOfLine;
    uint32_t limit = m_spec.m_range;
    while (blk1.isValid() && --limit)
    {
        QString text = blk1.text();
        auto mat = tick_pat_sep.match(text);
        if (mat.hasMatch())
        {
            if (mat.lastCapturedIndex() >= 1)
                frmStr = mat.captured(1);
            frameStartLine = blk1.blockNumber();
            break;
        }
        if (valMatchLine < 0)
        {
            mat = tick_pat_num.match(text);
            if (mat.hasMatch())
            {
                if (mat.lastCapturedIndex() >= 1)
                    valStr = mat.captured(1);
                valMatchLine = blk1.blockNumber();
            }
        }
        blk1 = blk1.previous();
    }
    if (!blk1.isValid())
    {
        frameStartLine = 0;
    }

    // Step #2: search forward for frame boundary pattern & value extraction
    QTextBlock blk2 = blkOfLine.next();
    limit = m_spec.m_range;
    while (blk2.isValid() && --limit)
    {
        QString text = blk2.text();
        auto mat = tick_pat_sep.match(text);
        if (mat.hasMatch())
        {
            frameEndLine = blk2.blockNumber();
            break;
        }
        if (valMatchLine < 0)
        {
            mat = tick_pat_num.match(text);
            if (mat.hasMatch())
            {
                if (mat.lastCapturedIndex() >= 1)
                    valStr = mat.captured(1);
                valMatchLine = blk2.blockNumber();
            }
        }
        blk2 = blk2.next();
    }
    if (frameEndLine < 0)
        frameEndLine = (blk2.isValid() ? blk2.blockNumber() : (m_doc->lastBlock().blockNumber() + 1));

    if ((valMatchLine >= 0) || (frameStartLine >= 0))
    {
        if (frameStartLine < 0)
            frameStartLine = valMatchLine;

        // add result to the cache
        m_cache.emplace_hint(cache_it, std::make_pair(frameStartLine,
                                 ParseFrameCacheEntry{frameStartLine, frameEndLine,
                                                      valStr,
                                                      ((m_spec.m_frmCapture ? frmStr : QString())) }));
        //printf("XXX|range MATCH line %d [%d...%d...%d] => [%s,%s]\n", line, frameStartLine, valMatchLine, frameEndLine, valStr.toLatin1().data(), frmStr.toLatin1().data());

        // remember the extent of the current frame
        m_prevFrmStart = frameStartLine;
        m_prevValMatch = valMatchLine;
        m_prevFrmEnd = frameEndLine;

        return ((col == 0) ? valStr : (m_spec.m_frmCapture ? frmStr : QString()));
    }
    // TODO empty cache entry: HOWEVER must not return that unless line - startLine > limit
    return QString();
}

// ----------------------------------------------------------------------------

bool operator==(const ParseSpec& lhs, const ParseSpec& rhs)
{
    return    (lhs.m_valPat == rhs.m_valPat)
           && (lhs.m_valHeader == rhs.m_valHeader)
           && (lhs.m_valDelta == rhs.m_valDelta)
           && (lhs.m_frmPat == rhs.m_frmPat)
           && (lhs.m_frmHeader == rhs.m_frmHeader)
           && (lhs.m_frmFwd == rhs.m_frmFwd)
           && (lhs.m_frmCapture == rhs.m_frmCapture)
           && (lhs.m_frmDelta == rhs.m_frmDelta)
           && (lhs.m_range == rhs.m_range);
}

bool operator!=(const ParseSpec& lhs, const ParseSpec& rhs)
{
    return !(lhs == rhs);
}

QJsonObject ParseSpec::getRcValues() const
{
    QJsonObject obj;

    obj.insert("val_pattern", QJsonValue(m_valPat));
    obj.insert("val_col_header", QJsonValue(m_valHeader));
    obj.insert("val_enable_delta", QJsonValue(m_valDelta));

    obj.insert("frame_pattern", QJsonValue(m_frmPat));
    obj.insert("frame_col_header", QJsonValue(m_frmHeader));
    obj.insert("frame_search_forward", QJsonValue(m_frmFwd));
    obj.insert("frame_capture_val", QJsonValue(m_frmCapture));
    obj.insert("frame_enable_delta", QJsonValue(m_frmDelta));

    obj.insert("limit_range", QJsonValue(int(m_range)));

    return obj;
}

ParseSpec::ParseSpec(const QJsonValue& val)
{
    const QJsonObject obj = val.toObject();

    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        if (var == "val_pattern")
        {
            m_valPat = val.toString();
        }
        else if (var == "val_col_header")
        {
            m_valHeader = val.toString();
        }
        else if (var == "val_enable_delta")
        {
            m_valDelta = val.toBool();
        }

        else if (var == "frame_pattern")
        {
            m_frmPat = val.toString();
        }
        else if (var == "frame_col_header")
        {
            m_frmHeader = val.toString();
        }
        else if (var == "frame_search_forward")
        {
            m_frmFwd = val.toBool();
        }
        else if (var == "frame_capture_val")
        {
            m_frmCapture = val.toBool();
        }
        else if (var == "frame_enable_delta")
        {
            m_frmDelta = val.toBool();
        }

        else if (var == "limit_range")
        {
            m_range = val.toInt();
        }
    }
}

ParseColumns ParseSpec::getColumns() const
{
    ParseColumns result = ParseColumnFlags::Val;
    if (m_valDelta)
        result |= ParseColumnFlags::ValDelta;

    if (!m_frmPat.isEmpty())
    {
        if (m_frmCapture)
            result |= ParseColumnFlags::Frm;
        if (m_frmCapture && m_frmDelta)
            result |= ParseColumnFlags::FrmDelta;
    }
    return result;
}

const QString& ParseSpec::getHeader(ParseColumnFlags col) const
{
    if (col == ParseColumnFlags::Val)
        return m_valHeader;
    else
        return m_frmHeader;
}


// ----------------------------------------------------------------------------

/**
 * Constructor for shared base class
 */
ParseFrame::ParseFrame(QTextDocument* doc, const ParseSpec& spec)
    : m_doc(doc)
    , m_spec(spec)
{
    // nop
}


/**
 * This factory function creates and returns a "finder" instance for the given
 * search type (i.e. regular expression or sub-string search)
 */
ParseFramePtr ParseFrame::create(QTextDocument* doc, const ParseSpec& spec)  /*static*/
{
    ParseFramePtr ptr;

    if (!spec.m_frmPat.isEmpty() && spec.m_frmFwd)
    {
        ptr = std::make_unique<ParseFrameRange>(doc, spec);
    }
    else if (!spec.m_valPat.isEmpty())
    {
        ptr = std::make_unique<ParseFrameLinear>(doc, spec);
    }

    if (ptr->isValid() == false)
        ptr.reset();

    return ptr;
}

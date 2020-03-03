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
 * This module contains the static factory function and internal sub-classes
 * that implement the abstract ParseFrame interface. This interface is used by
 * the search list window for proving content for its custom columns. The
 * ParseFrame constructor is provided an instance of the ParseSpec data
 * structure (which is created by the DlgParser class, and stored within the
 * persistent confiruation parameters of the search list dialog). Based on this
 * configuration, the static builder interfaces instantiates one of the two
 * sub-classes: The simpler ParseFrameLinear class, or the class
 * ParseFrameRange.
 *
 * Both classes extract data for the custom column by searching from a given
 * line backwards for a match on a regular expression; the expression has to
 * contain a capure group (i.e. parenthesis). The search can be limited by an
 * additional pattern, or a via range of lines searched. When a match is found,
 * the captured text is returned for display in the custom column. Class
 * ParseFrameRange will also search in forward direction, if backward search
 * did not produce a result.
 *
 * Both classes employ a cache that stores the result of a search for each
 * line, or more correctly: There is a cache entry for ranges of text that will
 * produce the same result.
 *
 * Examples where these classes could be useful are given in the description of
 * class DlgParser.
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

/**
 * This class is used by both sub-classes in this module for defining cache entries.
 */
class ParseFrameCacheEntry
{
public:
    int startLine;      // always equal the key of this entry
    int lastLine;       // when used by ParseFrameLinear: last line this entry is valid for
                        // when used by ParseFrameRange: first following line this entry is not valid for
    QString val;        // value extracted from mandatory first pattern
    QString frm;        // value extracted from optional frame boundary pattern, or empty
};

using ParseFrameCacheMap = std::map<int,ParseFrameCacheEntry>;

// ----------------------------------------------------------------------------

/**
 * Parser class using backwards-search only
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
 * Parser class using frame-based search range
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

/*
 * Member functions of the ParseSpec class, which is a parameter container.
 * The class serves for persistent storage of parameters as well as for
 * parameter for construction of ParseFrame instances.
 */

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

/**
 * This function returns a description of the parser configuration
 * ("specification") in form of a JSON object.
 */
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

/**
 * This function constructs a parser specification from the given JSON object.
 */
ParseSpec::ParseSpec(const QJsonValue& val)
{
    const QJsonObject obj = val.toObject();

    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        const QString& var = it.key();
        const QJsonValue& val = it.value();

        // parameters for basic "value extraction"
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
        // parameters for searching frame boundaries (and optionally capturing a 2nd value)
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
        // common parameter: search range limit
        else if (var == "limit_range")
        {
            m_range = val.toInt();
        }
    }
}

/**
 * This function returns a bitmap that indicates which of possible 4 different
 * customizable search list columns are enabled according to the given
 * parameter set.
 */
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

/**
 * This function returns the user-defined column header for the given column
 * from the given parameter set. The result is undefined if the given column
 * is not enabled in the parameter set. Note for delta columns, the caller is
 * responsible to prepend the delta symbol (or equivalent marker).
 */
const QString& ParseSpec::getHeader(ParseColumnFlags col) const
{
    if (col == ParseColumnFlags::Val)
        return m_valHeader;
    else
        return m_frmHeader;
}


// ----------------------------------------------------------------------------

/**
 * Constructor for the shared abstract base class: The base class stores the
 * common constant input parameters provided as parameters to the factory
 * method.
 */
ParseFrame::ParseFrame(QTextDocument* doc, const ParseSpec& spec)
    : m_doc(doc)
    , m_spec(spec)
{
    // nop
}


/**
 * This static factory function creates and returns a "parser" instance for the
 * given configuration data-set. The function may also return a NULL pointer if
 * the given configuration is invalid.
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
    // else: no pattern given at all -> return nullptr

    if (ptr && !ptr->isValid())
        ptr.reset();  // invalid pattern -> return nullptr

    return ptr;
}

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
 * This module implements the sub-classes that implement the abstract
 * MainTextFind interface. This interface is used by various classes that
 * employ text search in the main text document. The classes replace the use of
 * the "find" interfaces of QTextDocument, which are unusable for very large
 * documents due to not supporting a limit on the search range (i.e. they will
 * block the user interface for an indeterminate amount of time).
 *
 * There are two sub-classes, of which the first is instantiated for search via
 * regular expression, and the second for plain sub-string search. The classes
 * work similar an interator, which means they are one instantiated with
 * parameters and then keep internal state for repeated calls of their
 * findNext() interface. The search returns if either a match is found, the end
 * of the document is reached, or a maximum number of lines has been searched.
 * The first is sidtinguished by means of the return value, the latter two by
 * means of the isDone() query interface.
 *
 * Note this class currently returns only the first match per line. The next
 * call of findNext() will proceed to the next line. This is done for
 * performance reasons, as none of the current users cares for more than one
 * match per line (i.e. all but one actually only care about the line number as
 * a result, but not the exact position of matching text.)
 */

#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>

#include <cstdio>
#include <string>

#include "main_search.h"  // SearchPar
#include "text_block_find.h"

// ----------------------------------------------------------------------------

/**
 * Constructor for shared base class
 */
MainTextFind::MainTextFind(const QTextDocument* doc, int startPos, bool isFwd)
    : m_isFwd(isFwd)
    , m_blk(doc->findBlock(startPos))
    , m_startOff(startPos - m_blk.position())
{
}

// ----------------------------------------------------------------------------

/**
 * Constructor for search via regular expression.
 */
class MainTextFindRegExp : public MainTextFind
{
public:
    MainTextFindRegExp(const QTextDocument* doc, const SearchPar& par, bool isFwd, int startPos)
        : MainTextFind(doc, startPos, isFwd)
        , m_reFlags(par.m_opt_case ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption)
        , m_expr(par.m_pat, m_reFlags)
    {
        if (m_expr.isValid() == false)
            m_blk = QTextBlock();  // invalidate
    }
    virtual bool findNext(int& matchPos, int& matchLen, QTextBlock *matchBlock = nullptr) override;
    virtual ~MainTextFindRegExp() = default;
private:
    const QRegularExpression::PatternOptions m_reFlags;
    const QRegularExpression m_expr;
};

/**
 * Search in text document's blocks via regular expression
 */
bool MainTextFindRegExp::findNext(int& matchPos, int& matchLen, QTextBlock *matchBlock)
{
    if (!m_blk.isValid())
        return false;

    int cnt = MAX_ITERATIONS;

    if (m_isFwd)
    {
        while (m_blk.isValid() && --cnt)
        {
            QRegularExpressionMatch mat;
            QString text = m_blk.text();
            int idx = text.indexOf(m_expr, m_startOff, &mat);
            if (idx >= 0)
            {
                matchPos = m_blk.position() + idx;
                matchLen = mat.captured(0).length();
                if (matchLen == 0) // may occur for "^" et.al.
                    matchLen = 1;
                if (matchBlock)
                    *matchBlock = m_blk;
                m_blk = m_blk.next();
                return true;
            }
            m_blk = m_blk.next();
            m_startOff = 0;
        }
    }
    else  /* !isFwd */
    {
        while (m_blk.isValid() && --cnt)
        {
            QRegularExpressionMatch mat;
            QString text = m_blk.text();
            int idx = text.lastIndexOf(m_expr, m_startOff, &mat);
            if (idx >= 0)
            {
                matchPos = m_blk.position() + idx;
                matchLen = mat.captured(0).length();
                if (matchLen == 0)
                    matchLen = 1;
                if (matchBlock)
                    *matchBlock = m_blk;
                m_blk = m_blk.previous();
                return true;
            }
            m_blk = m_blk.previous();
            m_startOff = -1;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------

/**
 * Constructor for sub-string search.
 */
class MainTextFindSubStr : public MainTextFind
{
public:
    MainTextFindSubStr(const QTextDocument* doc, const SearchPar& par, bool isFwd, int startPos)
        : MainTextFind(doc, startPos, isFwd)
        , m_subStr(par.m_pat)
        , m_cmpFlags(par.m_opt_case ? Qt::CaseSensitive : Qt::CaseInsensitive)
    {
        if (par.m_pat.isEmpty())
            m_blk = QTextBlock();  // invalidate
    }
    virtual bool findNext(int& matchPos, int& matchLen, QTextBlock *matchBlock = nullptr) override;
    virtual ~MainTextFindSubStr() = default;
private:
    const QString& m_subStr;
    const Qt::CaseSensitivity m_cmpFlags;
};

/**
 * Search in text document's blocks via sub-string search
 */
bool MainTextFindSubStr::findNext(int& matchPos, int& matchLen, QTextBlock *matchBlock)
{
    if (!m_blk.isValid())
        return false;

    int cnt = MAX_ITERATIONS;
    if (m_isFwd)
    {
        while (m_blk.isValid() && --cnt)
        {
            QString text = m_blk.text();
            int idx = text.indexOf(m_subStr, m_startOff, m_cmpFlags);
            if (idx >= 0)
            {
                matchPos = m_blk.position() + idx;
                matchLen = m_subStr.length();
                if (matchBlock)
                    *matchBlock = m_blk;
                m_blk = m_blk.next();
                return true;
            }
            m_blk = m_blk.next();
            m_startOff = 0;
        }
    }
    else  /* !isFwd */
    {
        while (m_blk.isValid() && --cnt)
        {
            QString text = m_blk.text();
            int idx = text.lastIndexOf(m_subStr, m_startOff, m_cmpFlags);
            if (idx >= 0)
            {
                matchPos = m_blk.position() + idx;
                matchLen = m_subStr.length();
                if (matchBlock)
                    *matchBlock = m_blk;
                m_blk = m_blk.previous();
                return true;
            }
            m_blk = m_blk.previous();
            m_startOff = -1;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------

/**
 * This factory function creates and returns a "finder" instance for the given
 * search type (i.e. regular expression or sub-string search)
 */
std::unique_ptr<MainTextFind>
        MainTextFind::create(const QTextDocument* doc, const SearchPar& par, bool isFwd, int startPos)
{
    if (par.m_opt_regexp)
    {
        return std::make_unique<MainTextFindRegExp>(doc, par, isFwd, startPos);
    }
    else
    {
        return std::make_unique<MainTextFindSubStr>(doc, par, isFwd, startPos);
    }
}


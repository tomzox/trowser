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
            int idx = text.indexOf(m_expr, 0, &mat);
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
        }
    }
    else  /* !isFwd */
    {
        while (m_blk.isValid() && --cnt)
        {
            QRegularExpressionMatch mat;
            QString text = m_blk.text();
            int idx = text.lastIndexOf(m_expr, -1, &mat);
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
            int idx = text.indexOf(m_subStr, 0, m_cmpFlags);
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
        }
    }
    else  /* !isFwd */
    {
        while (m_blk.isValid() && --cnt)
        {
            QString text = m_blk.text();
            int idx = text.lastIndexOf(m_subStr, -1, m_cmpFlags);
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


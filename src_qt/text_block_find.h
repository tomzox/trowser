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
#ifndef _TEXT_BLOCK_FIND_H
#define _TEXT_BLOCK_FIND_H

#include <QTextBlock>

#include <memory>

class QTextDocument;
class SearchPar;

// ----------------------------------------------------------------------------

class MainTextFind
{
public:
    static std::unique_ptr<MainTextFind>
        create(const QTextDocument* doc, const SearchPar& par, bool isFwd, int startPos);

    bool isDone() const { return !m_blk.isValid(); }
    int nextStartPos() const { return m_blk.position(); }
    virtual bool findNext(int& matchPos, int& matchLen, QTextBlock *matchBlock = nullptr) = 0;
    virtual ~MainTextFind() = default;

protected:
    MainTextFind(const QTextDocument* doc, int startPos, bool isFwd);

    static const unsigned MAX_ITERATIONS = 50000;  // in unit of blocks
    const bool m_isFwd;
    QTextBlock m_blk;
};

#endif /* _TEXT_BLOCK_FIND_H */

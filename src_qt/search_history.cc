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
 * This module implements the SearchHistory class, which is a relatively simple
 * wrapper around a stack that stores the N last used search strings. The class
 * is embedded within the MainSearch class; it also has interfaces to support
 * the search history dialog.
 *
 * The search history stores both the pattern and the reg.exp. and case-match
 * options for each entry. The most-used interface by MainSearch is simply
 * retrieving the parameters of the last search. But there are also interfaces
 * that support iterating through the stack, while filtering for patterns with
 * a given prefix. This is supported by means of the iterator sub-class, which
 * maintains the state of iteration.
 *
 * The module also defines the widely used SearchPar class, which is just a
 * data structure containing the parameters of a search.
 */

#include <QObject>
#include <QJsonArray>

#include <cstdio>
#include <string>
#include <vector>

#include "main_win.h"
#include "main_search.h"
#include "config_file.h"
#include "search_history.h"

// ----------------------------------------------------------------------------

/**
 * This function is called when writing the config file to retrieve persistent
 * settings of this class, namely the search history.
 */
QJsonArray SearchHistory::getRcValues()
{
    QJsonArray arr;
    for (auto& hl : m_history)
    {
        QJsonArray el{ QJsonValue(hl.m_pat),
                       QJsonValue(hl.m_opt_regexp),
                       QJsonValue(hl.m_opt_case) };
        arr.push_back(el);
    }
    return arr;
}

/**
 * This function is called during start-up to apply configuration variables.
 * The function is the inverse of getRcValues()
 */
void SearchHistory::setRcValues(const QJsonArray& arr)
{
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        const QJsonArray hl = it->toArray();
        m_history.emplace_back(SearchPar(hl.at(0).toString(),
                                         hl.at(1).toBool(),
                                         hl.at(2).toBool()));
    }
}

/**
 * This function allows removing a number of entries from the search stack in
 * bulk. Entries are addressed by their index. This function is intended only
 * for the search history dialog.
 */
void SearchHistory::removeMultiple(const std::set<int>& excluded)
{
    if (excluded.size() != 0)
    {
        std::vector<SearchPar> tmp;
        tmp.reserve(TLB_HIST_MAXLEN + 1);

        for (size_t idx = 0; idx < m_history.size(); ++idx)
        {
            if (excluded.find(idx) == excluded.end())
                tmp.push_back(m_history[idx]);
        }
        m_history = tmp;

        ConfigFile::updateRcAfterIdle();
        emit historyChanged();
    }
}

/**
 * This convenience function adds all the given patterns to the search history,
 * or moves them to the top if already in the list. (This function is used when
 * starting searches via the search history list or highlight editor.)
 */
void SearchHistory::addMultiple(const std::vector<SearchPar>& patList)
{
    // add individually in reverse order so that they end up in the given order
    for (auto it = patList.rbegin(); it != patList.rend(); ++it)
        addEntry(*it);
}

/**
 * This function add the given search parameter set to the search history
 * stack.  If the same string is already on the stack, it is moved to the top.
 * Note: top of the stack is the front of the list.
 */
void SearchHistory::addEntry(const SearchPar& par)
{
    if (!par.m_pat.isEmpty())
    {
        // search for the expression in the history (options not compared)
        // remove the element if already in the list
        for (auto it = m_history.begin(); it != m_history.end(); ++it)
        {
            if (it->m_pat == par.m_pat)
            {
                m_history.erase(it);
                break;
            }
        }

        // insert the element at the top of the stack
        m_history.insert(m_history.begin(), par);

        // maintain max. stack depth
        if (m_history.size() > TLB_HIST_MAXLEN)
        {
            m_history.erase(m_history.begin() + TLB_HIST_MAXLEN, m_history.end());
        }

        ConfigFile::updateRcAfterIdle();
        emit historyChanged();
    }
}

/**
 * This function removes the entry indicated by the iterator from the history
 * stack. Afterwards the iterator points to the element in the stack following
 * the removed one; if the erased element was the last the iterator becomes
 * invalid.
 */
const QString& SearchHistory::removeEntry(iterator& it)
{
    const QString result;

    if (it.isValid(m_history.size()))
    {
        m_history.erase(m_history.begin() + it.pos);

        if (it.prevUp == false)
            it.pos -= 1;

        if (!it.isValid(m_history.size()))
        {
            it.reset();
        }

        ConfigFile::updateRcAfterIdle();
        emit historyChanged();
    }

    if (it.isValid(m_history.size()))
        return m_history.at(it.pos).m_pat;
    else
        return it.prefix;
}

/**
 * This function is used to conditionally reset the iterator in case the user
 * modified the search pattern prefix so that it no longer matches the entry
 * the iterator points at. In that case the next call of iterNext() will
 * restart at the top of the history stack (with a new prefix, too)
 */
void SearchHistory::trackIter(iterator& it, const QString& pat)
{
    if (it.isValid(m_history.size()))
    {
        if (pat != m_history[it.pos].m_pat)
        {
            it.reset();
        }
    }
}

/**
 * This function returns the search pattern of the next entry in the given
 * direction in the history stack. Note the given prefix is only used upon
 * start of a new iteration. When the end of the stack is reached (or no
 * matching pattern is found), the function returns the original prefix (i.e.
 * search does not wrap immediately, but it restart at the opposite end upon
 * the next call)
 */
const QString& SearchHistory::iterNext(iterator& it, const QString& prefix, bool is_up)
{
    if (m_history.size() > 0)
    {
        if (!it.isValid(m_history.size()))
        {
            // initialize iterator: store given prefix
            it.init(prefix, m_history.size(), is_up);
        }
        else if (is_up)
        {
            if (size_t(it.pos) + 1 < m_history.size())
                it.pos += 1;
            else
                it.pos = -1;
        }
        else
        {
            if (it.pos >= 0)
                it.pos -= 1;
        }

        // advance to the next item
        it.pos = searchPrefix(it, is_up ? 1 : -1);
        it.prevUp = is_up;
    }

    if (it.isValid(m_history.size()))
        return m_history.at(it.pos).m_pat;
    else
        return it.prefix;  // return initial search pattern before starting iteration
}


/**
 * This internal helper function searches the search history stack in the given
 * direction for a search pattern with a given prefix. The search will end when
 * the top or bottom of the stack is reached (i.e. no wrapping) and in this
 * case return an invalid index.
 */
int SearchHistory::searchPrefix(iterator& it, int step)
{
    for (int idx = it.pos; (idx >= 0) && (size_t(idx) < m_history.size()); idx += step)
    {
        if (m_history[idx].m_pat.startsWith(it.prefix))
            return idx;
    }
    return -1;
}

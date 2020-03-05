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
#ifndef _SEARCH_HISTORY_H
#define _SEARCH_HISTORY_H

#include <QObject>
#include <QString>

#include <set>

class QJsonArray;

class MainWin;

// ----------------------------------------------------------------------------

/**
 * This is a small container class, used for storing parameters for a text
 * search, which is: the search string, and two flags indicating if the string
 * is a regular expression and if it shall match case respectively. The class
 * is used for storing the active parameter set, previous sets in the search
 * history, and widely as function parameter while executing searches.
 */
class SearchPar
{
public:
    SearchPar() : m_opt_regexp(false), m_opt_case(false) {}
    SearchPar(const QString& pat, bool opt_regexp, bool opt_case)
        : m_pat(pat)
        , m_opt_regexp(opt_regexp)
        , m_opt_case(opt_case)
    {
    }
    bool operator==(const SearchPar& rhs) const
    {
        return (   (m_pat == rhs.m_pat)
                && (m_opt_regexp == rhs.m_opt_regexp)
                && (m_opt_case == rhs.m_opt_case));
    }
    bool operator!=(const SearchPar& rhs) const { return !(*this == rhs); }
    void reset() { m_pat.clear(); m_opt_regexp = false; m_opt_case = false; }

public:
    QString m_pat;
    bool  m_opt_regexp;
    bool  m_opt_case;
};

// ----------------------------------------------------------------------------

class SearchHistory : public QObject
{
    Q_OBJECT
public:
    /**
     * Public sub-class, used for iterating through the history stack.
     */
    class iterator
    {
    public:
        iterator() = default;
        void reset()
        {
            pos = -1;
            prefix.clear();
        }
        friend class SearchHistory;

    private:
        bool isValid(size_t size) const
        {
            return (pos >= 0) && (size_t(pos) < size);
        }
        void init(const QString& pat, size_t histLen, bool is_up)
        {
            prefix = pat;
            pos = (is_up ? 0 : (histLen - 1));
        }
    private:
        int           pos = -1;
        bool          prevUp = false;
        QString       prefix;
    };
    //  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --

    SearchHistory(MainWin * mainWin)
        : m_mainWin(mainWin)
    {
        m_history.reserve(TLB_HIST_MAXLEN + 1);
    }
    QJsonArray getRcValues();
    void setRcValues(const QJsonArray& arr);

    const std::vector<SearchPar>& getHistory() const { return m_history; }
    void removeMultiple(const std::set<int>& excluded);

    bool isEmpty() const { return m_history.empty(); }
    const SearchPar& front() const { return m_history.front(); }

    void addEntry(const SearchPar& par);
    const QString& removeEntry(iterator& it);
    void trackIter(iterator& it, const QString& pat);
    const QString& iterNext(iterator& it, const QString& prefix, bool is_up);

signals:
    void historyChanged();

private:
    int  searchPrefix(iterator& it, int step);

private:
    static const uint TLB_HIST_MAXLEN = 50;
    MainWin * const m_mainWin;

    std::vector<SearchPar> m_history;
};

#endif /* _SEARCH_HISTORY_H */

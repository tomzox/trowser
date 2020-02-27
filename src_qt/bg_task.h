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
#ifndef _BG_TASK_H
#define _BG_TASK_H

#include <QObject>
#include <QPointer>

class QTimer;

// ----------------------------------------------------------------------------

typedef enum
{
    BG_PRIO_SEARCH_LIST,
    BG_PRIO_SEARCH_INC,
    BG_PRIO_HIGHLIGHT_SEARCH,
    BG_PRIO_HIGHLIGHT_INIT,
} BG_TASK_PRIO;

class BgTask : public QObject
{
public:
    BgTask(QObject * parent, BG_TASK_PRIO priority);
    ~BgTask();

    bool isActive() const { return m_isActive; }
    void start(const std::function<void()>& callback);
    void start(unsigned delay, const std::function<void()>& callback);
    void stop();

private:
    static void schedTimerExpired();
    void privateTimerExpired();

private:
    static std::list<BgTask*> s_queue;
    static QTimer * s_timer;

    const QPointer<QObject> m_parent;
    const BG_TASK_PRIO m_priority;

    std::function<void()> m_callback;
    bool m_isActive;
    QTimer * s_privateTimer;
};

#endif // _BG_TASK_H

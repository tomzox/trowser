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
 * This module implements a facility which supports scheduling background tasks
 * via "idle" timer out of the main event loop (i.e. not using threads).  In
 * essence, the class provides simply a single-shot timer for each call of the
 * start() method. The user modules are responsible to split processing that
 * takes a longer time than say 100ms (i.e. an acceptible delay on processing
 * interactive input) into separate steps. For each step the BgTask is started,
 * and then reschedules itself for the next step out of the callback function.
 *
 * The main purpose of having a central facility is scheduling only a single
 * one of the pending tasks, namely the one with highest priority. Only once
 * that tasks stops rescheduling itself, lower-priority tasks handlers will be
 * processed.
 *
 * ----------------------------------------------------------------------------
 */

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QDebug>

#include <list>
#include <algorithm>

#include "bg_task.h"

// ----------------------------------------------------------------------------
// Static member variables

std::list<BgTask*> BgTask::s_queue;
QTimer * BgTask::s_timer;

// ----------------------------------------------------------------------------

BgTask::BgTask(QObject * parent, BG_TASK_PRIO priority)
    : m_parent(parent)
    , m_priority(priority)
    , m_isActive(false)
    , s_privateTimer(nullptr)
{
    if (s_timer == nullptr)
    {
        s_timer = new QTimer(parent);
        s_timer->setSingleShot(true);
        s_timer->setInterval(0);
        connect(s_timer, &QTimer::timeout, [](){ BgTask::schedTimerExpired(); });
    }
}

BgTask::~BgTask()
{
    // s_timer is shared (static) hence not deleted
    stop();

    if (s_privateTimer)
    {
        s_privateTimer->stop();
        delete s_privateTimer;
    }
}


/**
 * This internal static function is installed as handler for the timer used for
 * scheduling all pending background tasks. The function extracts the task of
 * highest priority from the queue and invokes its handler. If any tasks are
 * remaining (maybe added within the callback function), the timer is
 * restarted.
 */
void BgTask::schedTimerExpired()  /*static*/
{
    auto it = std::min_element(s_queue.begin(), s_queue.end(),
                               [](BgTask* a, BgTask* b) bool { return a->m_priority < b->m_priority; });
    if (it != s_queue.end())
    {
        BgTask * next = *it;
        s_queue.erase(it);

        Q_ASSERT(!next->m_parent.isNull());  // timer not stopped before destruction
        next->m_isActive = false;

        // restart timer to schedule next remaining task, if any
        if (!s_queue.empty())
        {
            s_timer->start();
        }

        // FINALLY invoke the task handler function (may call start() recursively)
        next->m_callback();
    }
    else
        Q_ASSERT(false);
}


/**
 * This function adds the given background task instance to the list of pending
 * tasks. The given handler function will get called out of the event handler
 * once other events and tasks with higher priority have finished.
 */
void BgTask::start(const std::function<void()>& callback)
{
    Q_ASSERT(!m_parent.isNull());  // timer not stopped before destruction

    m_callback = callback;

    if (m_isActive == false)
    {
        Q_ASSERT(std::find(s_queue.begin(), s_queue.end(), this) == s_queue.end());

        s_queue.push_back(this);
        m_isActive = true;

        if (!s_timer->isActive())
        {
            s_timer->start();
        }
    }
}


/**
 * This function stops the given background task instance from being scheduled.
 * If the task is not currently pending, the function does nothing.
 */
void BgTask::stop()
{
    auto it = std::find(s_queue.begin(), s_queue.end(), this);
    if (it != s_queue.end())
    {
        Q_ASSERT(!m_parent.isNull());  // timer not stopped before destruction
        s_queue.erase(it);
        m_isActive = false;

        if (s_queue.empty())
        {
            s_timer->stop();
        }
    }
    if (s_privateTimer != nullptr)
    {
        s_privateTimer->stop();
    }
}

/**
 * This overloaded function is for convenience: The function starts the
 * background task with an initial delay. This is achieved by using an extra
 * timer; only when this timer expires, the task is added to the regular
 * scheduling queue. The regular stop() function handles the initial delay and
 * later scheduling transparently (i.e. task is cancelled regardless of the
 * phase of scheduling it is in).
 */
void BgTask::start(unsigned delay, const std::function<void()>& callback)
{
    Q_ASSERT(!m_parent.isNull());  // timer not stopped before destruction

    if (m_isActive)
        stop();

    if (s_privateTimer == nullptr)
    {
        s_privateTimer = new QTimer(m_parent);
        s_privateTimer->setSingleShot(true);
        connect(s_privateTimer, &QTimer::timeout, [=](){ privateTimerExpired(); });
    }
    else
        s_privateTimer->stop();

    m_callback = callback;

    s_privateTimer->setInterval(delay);
    s_privateTimer->start();
}

/**
 * This internal function is installed as handler for the private timer used for
 * initial delay of scheduling a task. The function adds the task to the common
 * scheduling queue.
 */
void BgTask::privateTimerExpired()
{
    start(m_callback);
}

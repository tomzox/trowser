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
 * This module implements an alternative to QMainWindow's status line. The
 * reason for this is that text messages in the default status line at the
 * bottom of the window are not very well noticable and thus not usable for
 * showing error or warning feedback to user actions. Further the default
 * status line permanently wastes on-screen space. This alternative displays
 * the message as a temporary overlay to the respective toplevel's main widget
 * and gives the text a background color that makes it noticable.
 *
 * ----------------------------------------------------------------------------
 */

#include <QWidget>
#include <QApplication>
#include <QLabel>
#include <QTimer>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "status_line.h"

// ----------------------------------------------------------------------------

/**
 * This statis member variable stores a pointer to the currently displayed
 * instance of a status line across all top-level windows. It is set to NULL
 * when no status line is active. This pointer is used to ensure only one
 * instance is visible at all times.
 */
StatusLine * StatusLine::s_activeInstance = nullptr;

StatusLine::StatusLine(QWidget * parent)
    : QWidget(parent)
{
    m_parent = parent;

    m_lab = new QLabel(parent);
        m_lab->setFrameShape(QFrame::StyledPanel);
        m_lab->setLineWidth(2);
        m_lab->setAutoFillBackground(true);
        m_lab->setMargin(5);
        m_lab->setFocusPolicy(Qt::NoFocus);
        m_lab->setVisible(false);

    m_timStLine = new QTimer(this);
        m_timStLine->setSingleShot(true);
        m_timStLine->setInterval(DISPLAY_DURATION);
        connect(m_timStLine, &QTimer::timeout, this, &StatusLine::expireStatusMsg);
};

StatusLine::~StatusLine()
{
    if (s_activeInstance == this)
        s_activeInstance = nullptr;

    m_lab->setVisible(false);
    m_timStLine->stop();
}

void StatusLine::showStatusMsg(const QString& msg, QRgb col)
{
    if (s_activeInstance != nullptr)
        s_activeInstance->expireStatusMsg();

    // withdraw to force recalculation of geometry
    m_lab->setVisible(false);

    QPalette stline_pal(m_lab->palette());
    stline_pal.setColor(QPalette::Window, col);

    m_lab->setPalette(stline_pal);
    m_lab->setText(msg);
    m_lab->move(10, m_parent->height() - m_lab->height() - 20);
    m_lab->setVisible(true);

    s_activeInstance = this;
    m_timStLine->start();
}

void StatusLine::showPlain(const QString& topic, const QString& msg)
{
    auto pal = QApplication::palette(m_lab);

    showStatusMsg(msg, pal.color(QPalette::Window).rgba());
    m_activeTopic = topic;
}

void StatusLine::showWarning(const QString& topic, const QString& msg)
{
    showStatusMsg(msg, s_colStWarning);
    m_activeTopic = topic;
}

void StatusLine::showError(const QString& topic, const QString& msg)
{
    showStatusMsg(msg, s_colStError);
    m_activeTopic = topic;
}

void StatusLine::clearMessage(const QString& topic)
{
    if (topic == m_activeTopic)
    {
        if (s_activeInstance == this)
            s_activeInstance = nullptr;
        m_timStLine->stop();

        m_lab->setVisible(false);
        m_lab->setText("");
    }
}

void StatusLine::expireStatusMsg()
{
    if (s_activeInstance == this)
        s_activeInstance = nullptr;
    m_timStLine->stop();

    m_lab->setVisible(false);
    m_lab->setText("");
}

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
#ifndef _STATUS_LINE_H
#define _STATUS_LINE_H

#include <QWidget>
#include <QString>
#include <QRgb>

class QTimer;
class QLabel;

// ----------------------------------------------------------------------------

class StatusLine: public QWidget
{
    Q_OBJECT

public:
    StatusLine(QWidget * parent);
    ~StatusLine();
    void showWarning(const QString& topic, const QString& msg);
    void showError(const QString& topic, const QString& msg);
    void showPlain(const QString& topic, const QString& msg);
    void clearMessage(const QString& topic);

private:
    void showStatusMsg(const QString& msg, QRgb col);
    void expireStatusMsg();

private:
    static const int DISPLAY_DURATION = 4000;
    static constexpr QRgb s_colStError{0xffff6b6b};
    static constexpr QRgb s_colStWarning{0xffffcc5d};

    static StatusLine * s_activeInstance;

    QWidget     * m_parent = nullptr;
    QLabel      * m_lab = nullptr;
    QTimer      * m_timStLine = nullptr;
    QString       m_activeTopic;
};

#endif // _STATUS_LINE_H

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
#ifndef _MAIN_WIN_H
#define _MAIN_WIN_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTimer>
#include <QRgb>

#include <vector>

class QApplication;
class QStyle;
class QMenu;
class QLabel;
class QJsonObject;

class MainText;
class MainSearch;
class Highlighter;
class Bookmarks;

#ifdef __GNUC__
#define condLikely(x)       __builtin_expect(!!(x), 1)
#define condUnlikely(x)     __builtin_expect(!!(x), 0)
#else
#define condLikely(x)       (x)
#define condUnlikely(x)     (x)
#endif

// ----------------------------------------------------------------------------

class ATimer : public QTimer
{
public:
    ATimer(QWidget * parent = nullptr);
    void after(int delay, const std::function<void()>& callback);
    void reschedule(int delay);
};

// ----------------------------------------------------------------------------

class StatusMsg : public QWidget
{
    Q_OBJECT

public:
    StatusMsg(QWidget * parent);
    void showWarning(QWidget * widget, const QString& msg);
    void showError(QWidget * widget, const QString& msg);
    void showPlain(QWidget * widget, const QString& msg);
    void clearMessage(QWidget * widget);
private:
    void showStatusMsg(const QString& msg, QRgb col);
    void expireStatusMsg();

private:
    static const int DISPLAY_DURATION = 4000;
    static constexpr QRgb s_colStError{0xffff6b6b};
    static constexpr QRgb s_colStWarning{0xffffcc5d};

    QWidget     * m_parent;
    QLabel      * m_lab;
    QTimer      * m_timStLine;
    QWidget     * m_owner = nullptr;
};

// ----------------------------------------------------------------------------

class MainWin : public QMainWindow
{
    Q_OBJECT

public:
    MainWin(QApplication * app);
    ~MainWin();

    QWidget * focusWidget() const;
    QStyle * getAppStyle() const;
    const QString& getFilename() const { return m_curFileName; }
    const QFont& getFontContent() const { return m_fontContent; }
    const QColor& getFgColDefault() const;
    const QColor& getBgColDefault() const;

    void showError(QWidget * widget, const QString& msg) { m_stline->showError(widget, msg); }
    void showWarning(QWidget * widget, const QString& msg) { m_stline->showWarning(widget, msg); }
    void showPlain(QWidget * widget, const QString& msg) { m_stline->showPlain(widget, msg); }
    void clearMessage(QWidget * widget) { m_stline->clearMessage(widget); }
    void loadRcFile();
    void updateRcFile();
    void updateRcAfterIdle();
    void menuCmdDisplayLineNo();
    void keyCmdZoomFontSize(bool zoomIn);
    void LoadFile(const QString& fileName);

private slots:
    void menuCmdReload(bool checked);
    void menuCmdFileOpen(bool checked);
    void menuCmdFileQuit(bool checked);
    void menuCmdSelectFont(bool checked);
    void menuCmdToggleLineWrap(bool checked);
    void menuCmdGotoLine(bool checked);
    void menuCmdBookmarkDeleteAll(bool checked);
    void menuCmdAbout(bool checked);

private:
    void closeEvent(QCloseEvent *event);
    void menuCmdDiscard(bool is_fwd);
    void discardContent();
    void populateMenus();

private:
    QApplication* const m_mainApp;
    QMenu       * m_menubar_ctrl = nullptr;
    QMenu       * m_menubar_srch = nullptr;
    QMenu       * m_menubar_mark = nullptr;
    QMenu       * m_menubar_help = nullptr;

    MainText    * m_f1_t = nullptr;
    StatusMsg   * m_stline = nullptr;
    MainSearch  * m_search = nullptr;
    Highlighter * m_higl = nullptr;
    Bookmarks   * m_bookmarks = nullptr;

    QTimer      * m_timUpdateRc = nullptr;
    qint64        m_tsUpdateRc = 0;
    QFont         m_fontContent;
    QString       m_curFileName;
};

#endif // _MAIN_WIN_H

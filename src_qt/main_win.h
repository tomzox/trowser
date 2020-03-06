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

class QApplication;
class QTimer;

class MainText;
class MainSearch;
class StatusLine;
class Highlighter;
class Bookmarks;
class LoadPipe;

// ----------------------------------------------------------------------------

class MainWin : public QMainWindow
{
    Q_OBJECT

public:
    MainWin(QApplication * app);
    ~MainWin();

    QWidget * focusWidget() const;
    const QString& getFilename() const { return m_curFileName; }
    StatusLine * mainStatusLine() const { return m_stline; }

    void startLoading(const char * fileName);

signals:
    void documentNameChanged();

private:
    void closeEvent(QCloseEvent *event);
    void discardContent();
    void populateMenus();
    void menuCmdReload(bool checked);
    void menuCmdFileOpen(bool checked);
    void menuCmdFileQuit(bool checked);
    void menuCmdDiscard(bool is_fwd);
    void menuCmdSelectFont(bool checked);
    void menuCmdToggleLineWrap(bool checked);
    void menuCmdGotoLine(bool checked);
    void menuCmdBookmarkDeleteAll(bool checked);
    void menuCmdAbout(bool checked);
    void loadFromFile(const QString& fileName);
    void loadFromPipe();
    void loadPipeDone();

private:
    QApplication* const m_mainApp;

    MainText    * m_mainText = nullptr;
    StatusLine  * m_stline = nullptr;
    MainSearch  * m_search = nullptr;
    Highlighter * m_higl = nullptr;
    Bookmarks   * m_bookmarks = nullptr;
    LoadPipe    * m_loadPipe = nullptr;
    QAction     * m_actFileReload = nullptr;

    QString       m_curFileName;
};

#endif // _MAIN_WIN_H

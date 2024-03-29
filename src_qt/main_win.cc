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
 * TROWSER is a browser for line-oriented text files, e.g. debug traces.
 * This is a translation from the original implementation in Tcl/Tk.
 *
 * Module description:
 *
 * This module contains the main() entry point and command line parser, as well
 * as class MainWin which creates the main window and instantiates all other
 * classes required by the application.
 *
 * ----------------------------------------------------------------------------
 */

#include <QApplication>
#include <QDesktopWidget>
#include <QWidget>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QToolBar>
#include <QFontDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QProgressDialog>

#include <cstdio>
#include <string>
#include <unistd.h>

#include "config_file.h"
#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "bookmarks.h"
#include "status_line.h"
#include "search_list.h"
#include "load_pipe.h"
#include "dlg_higl.h"
#include "dlg_history.h"
#include "dlg_bookmarks.h"
#include "dlg_markup_sa.h"

// ----------------------------------------------------------------------------

/**
 * This constructor creates and shows the main window, containing a menu bar,
 * the text widget showing the main document and a "Find" toolbar. Additionally
 * the function instantiates all other classes required by the application.
 */
MainWin::MainWin(QApplication * app)
    : m_mainApp(app)
{
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    m_search = new MainSearch(this);
    m_bookmarks = new Bookmarks(this);

    m_mainText = new MainText(this, m_search, m_bookmarks, central_wid);
        m_mainText->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_mainText->setCursorWidth(2);
        m_mainText->setTabChangesFocus(true);   // enable Key_Tab moving keyboard focus
        layout_top->addWidget(m_mainText);

    auto f2 = new QToolBar("Find", this);
        f2->setObjectName("Toolbar::Find"); // for saveState
        addToolBar(Qt::BottomToolBarArea, f2);
    auto f2_l = new QLabel("Find:", f2);
        f2_l->setToolTip("Enter a search text or regular expression, then\n"
                         "press RETURN to search for it in the main window.\n"
                         "Use up/down keys for browsing your search history.");
        f2->addWidget(f2_l);
    auto f2_e = new MainFindEnt(m_search, f2);
        f2->addWidget(f2_e);
    auto f2_bn = new QPushButton("&Next", f2);
        f2_bn->setToolTip("Move the cursor to the next match on the search pattern.");
        f2_bn->setFocusPolicy(Qt::TabFocus);
        connect(f2_bn, &QPushButton::clicked, [=](){ m_search->searchNext(true); });
        f2->addWidget(f2_bn);
    auto f2_bp = new QPushButton("&Prev.", f2);
        f2_bp->setToolTip("Move the cursor to the previous match on the search pattern.");
        f2_bp->setFocusPolicy(Qt::TabFocus);
        connect(f2_bp, &QPushButton::clicked, [=](){ m_search->searchNext(false); });
        f2->addWidget(f2_bp);
    auto f2_bl = new QPushButton("&All", f2);
        f2_bl->setToolTip("Open a new window showing all lines that contain a match for the pattern.");
        f2_bl->setFocusPolicy(Qt::TabFocus);
        // FIXME first param should be "false" when used via shortcut
        connect(f2_bl, &QPushButton::clicked, [=](){ m_search->searchAll(true, 0); });
        f2->addWidget(f2_bl);
    f2->addSeparator();
    auto f2_hall = new QCheckBox("&Highlight all", f2);
        f2_hall->setToolTip("When checked, highlight all lines that contain a match on the search pattern.");
        f2_hall->setFocusPolicy(Qt::TabFocus);
        connect(f2_hall, &QCheckBox::stateChanged, m_search, &MainSearch::searchOptToggleHall);
        f2->addWidget(f2_hall);
    auto f2_mcase = new QCheckBox("&Match case", f2);
        f2_mcase->setToolTip("When checked, only match on letters in the case given in the pattern.\nElse ignore case.");
        f2_mcase->setFocusPolicy(Qt::TabFocus);
        connect(f2_mcase, &QCheckBox::stateChanged, m_search, &MainSearch::searchOptToggleCase);
        f2->addWidget(f2_mcase);
    auto f2_regexp = new QCheckBox("Reg.&Exp.", f2);
        f2_regexp->setToolTip("Evaluate the search string as regular expression in Perl syntax");
        f2_regexp->setFocusPolicy(Qt::TabFocus);
        connect(f2_regexp, &QCheckBox::stateChanged, m_search, &MainSearch::searchOptToggleRegExp);
        f2->addWidget(f2_regexp);

    m_stline = new StatusLine(m_mainText);
    m_higl = new Highlighter(m_mainText, this);
    m_search->connectWidgets(m_mainText, m_higl, f2_e, f2_hall, f2_mcase, f2_regexp);
    m_bookmarks->connectWidgets(m_mainText, m_higl);
    SearchList::connectWidgets(this, m_search, m_mainText, m_higl, m_bookmarks);
    DlgBookmarks::connectWidgets(this, m_search, m_mainText, m_higl, m_bookmarks);
    DlgHistory::connectWidgets(this, m_search, m_mainText);
    ConfigFile::connectWidgets(this, m_search, m_mainText, m_higl);

    layout_top->setContentsMargins(0, 0, 0, 0);
    this->setCentralWidget(central_wid);

    populateMenus();

    // shortcuts for functions which have no button widget
    QAction * act;
    act = new QAction(central_wid);  // move focus into search text entry field
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_F));
        connect(act, &QAction::triggered, [=](){ m_search->searchEnter(true); });
        central_wid->addAction(act);
    act = new QAction(central_wid);  // "search all below"
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_N));
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(false, 1); });
        central_wid->addAction(act);
    act = new QAction(central_wid);  // "search all above"
        act->setShortcut(QKeySequence(Qt::ALT + Qt::SHIFT + Qt::Key_P));
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(false, -1); });
        central_wid->addAction(act);

    m_mainText->setFocus(Qt::ShortcutFocusReason);
}

/**
 * Destructor: Freeing resources not automatically deleted via widget tree
 */
MainWin::~MainWin()
{
    delete m_search;
    delete m_bookmarks;
    delete m_stline;
    delete m_loadPipe;
}

/**
 * This sub-function of the constructor populates the menu bar with actions.
 */
void MainWin::populateMenus()
{
    QAction * act;

    auto menubar_ctrl = menuBar()->addMenu("&Control");
    act = menubar_ctrl->addAction("Open file...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileOpen);
    m_actFileReload = menubar_ctrl->addAction("Reload current file");
        connect(m_actFileReload, &QAction::triggered, this, &MainWin::menuCmdReload);
        m_actFileReload->setEnabled(false);
    menubar_ctrl->addSeparator();
    act = menubar_ctrl->addAction("Discard above cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(false); });
    act = menubar_ctrl->addAction("Discard below cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(true); });
    menubar_ctrl->addSeparator();
    act = menubar_ctrl->addAction("Toggle line wrap");
        act->setCheckable(true);
        connect(act, &QAction::toggled, this, &MainWin::menuCmdToggleLineWrap);
    act = menubar_ctrl->addAction("Font selection...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdSelectFont);
    QMenu *sub = menubar_ctrl->addMenu("Mark-up configuration");
        act = sub->addAction("Search results...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editSearchFmt(m_higl, m_mainText, this); });
        act = sub->addAction("Search increment...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editSearchIncFmt(m_higl, m_mainText, this); });
        act = sub->addAction("Bookmarks...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editBookmarkFmt(m_higl, m_mainText, this); });
    menubar_ctrl->addSeparator();
    act = menubar_ctrl->addAction("Quit");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileQuit);

    auto menubar_srch = menuBar()->addMenu("&Search");
    act = menubar_srch->addAction("Search history...");
        connect(act, &QAction::triggered, [=](){ DlgHistory::openDialog(); });
    act = menubar_srch->addAction("Edit highlight patterns...");
        connect(act, &QAction::triggered, [=](){ DlgHigl::openDialog(m_higl, m_search, m_mainText, this); });
    menubar_srch->addSeparator();
    act = menubar_srch->addAction("List all search matches...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 0); });
    act = menubar_srch->addAction("List all matches above...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, -1); });
    act = menubar_srch->addAction("List all matches below...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 1); });
    menubar_srch->addSeparator();
    act = menubar_srch->addAction("Clear search highlight");
        connect(act, &QAction::triggered, m_search, &MainSearch::searchHighlightClear);
    menubar_srch->addSeparator();
    act = menubar_srch->addAction("Goto line number...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdGotoLine);

    auto menubar_mark = menuBar()->addMenu("&Bookmarks");
    act = menubar_mark->addAction("Toggle bookmark");
        connect(act, &QAction::triggered, [=](){ m_mainText->toggleBookmark(); });
    act = menubar_mark->addAction("List bookmarks");
        connect(act, &QAction::triggered, [=](){ DlgBookmarks::openDialog(); });
    act = menubar_mark->addAction("Delete all bookmarks");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdBookmarkDeleteAll);
    menubar_mark->addSeparator();
    act = menubar_mark->addAction("Jump to prev. bookmark");
        connect(act, &QAction::triggered, [=](){ m_mainText->jumpToNextBookmark(false); });
    act = menubar_mark->addAction("Jump to next bookmark");
        connect(act, &QAction::triggered, [=](){ m_mainText->jumpToNextBookmark(true); });
    menubar_mark->addSeparator();
    act = menubar_mark->addAction("Read bookmarks from file...");
        connect(act, &QAction::triggered, [=](){ m_bookmarks->readFileFrom(this); });
    act = menubar_mark->addAction("Save bookmarks to file...");
        connect(act, &QAction::triggered, [=](){ m_bookmarks->saveFileAs(this, false); });

    auto menubar_help = menuBar()->addMenu("Help");
    act = menubar_help->addAction("About trowser...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdAbout);
    act = menubar_help->addAction("About Qt...");
        connect(act, &QAction::triggered, m_mainApp, &QApplication::aboutQt);
}

/**
 * This query function returns the widget that currently has keyboard focus, or
 * NULL if none.
 */
QWidget * MainWin::focusWidget() const
{
    return m_mainApp->focusWidget();
}


// ----------------------------------------------------------------------------

void MainWin::menuCmdAbout(bool)
{
    QString msg("<h2>TROWSER - A debug trace browser</h2>"
                "<P>"
                "trowser is a browser for large line-oriented text files with color "
                "highlighting, originally written 2007 in Tcl/Tk but in this version "
                "converted to C++ using Qt. trowser is designed especially for analysis "
                "of huge text files as generated by an application's debug/trace output. "
                "</P><P>\n"
                "Compared to traditional tools such as 'less', trowser adds color "
                "highlighting, a persistent search history, "
                "graphical bookmarking, separate search result (i.e. filter) windows and "
                "flexible skipping of input from pipes to STDIN.  Trowser has a graphical "
                "interface, but is designed to allow browsing via the keyboard at least to "
                "the same extent as less. Key bindings and the cursor positioning concept "
                "are derived mainly from vim.\n"
                "</P><P>\n"
                "Homepage &amp; Documentation: "
                "<A HREF=\"https://github.com/tomzox/trowser\">github.com/tomzox/trowser</A>\n"
                "</P><HR><P>\n"
                "Copyright (C) 2007-2010,2020 Th. Zoerner\n"
                "</P><P><small>\n"
                "This program is free software: you can redistribute it and/or modify it "
                "under the terms of the GNU General Public License as published by the "
                "Free Software Foundation, either version 3 of the License, or (at your "
                "option) any later version.\n"
                "</P><P><small>\n"
                "This program is distributed in the hope that it will be useful, but "
                "<B>without any warranty</B>; without even the implied warranty of <B>merchantability</B> "
                "or <B>fitness for a particular purpose</B>.  See the GNU General Public License "
                "for more details.\n"
                "</P><P><small>\n"
                "You should have received a copy of the GNU General Public License along "
                "with this program.  If not, see <A HREF=\"http://www.gnu.org/licenses/\">www.gnu.org/licenses</A>.</small>");

    QMessageBox::about(this, "About Trace Browser", msg);
}

/**
 * This function is bound to the "Goto line" menu command.  The function opens
 * a small modal dialog for entering a number in range of the number of lines
 * in the text buffer (starting at 1). When completed by the user the cursor is
 * set to the start of the specified line.
 */
void MainWin::menuCmdGotoLine(bool)
{
    int lineCount = m_mainText->document()->blockCount();
    bool ok = false;

    int line = QInputDialog::getInt(this, "Goto line number...",
                                    QString("Enter line number: (max. ")
                                        + QString::number(lineCount) + ")",
                                    1, 1, lineCount, 1, &ok);
    if (ok && (line > 0) && (line <= lineCount))
    {
        m_mainText->jumpToLine(line - 1);
    }
}

/**
 * This function deletes all bookmarks. It's called via the main menu.
 * The function is intended esp. if a large number of bookmarks was imported
 * previously from a file.
 */
void MainWin::menuCmdBookmarkDeleteAll(bool)
{
    int count = m_bookmarks->getCount();
    if (count > 0)
    {
        QString msg = QString("Really delete ") + QString::number(count)
                         + " bookmark" + ((count != 1) ? "s" : "") + "?";
        auto answer = QMessageBox::question(this, "trowser", msg,
                                            QMessageBox::Ok | QMessageBox::Cancel);
        if (answer == QMessageBox::Ok)
        {
            m_bookmarks->removeAll();
        }
    }
    else
        QMessageBox::warning(this, "trowser",
                             "Your bookmark list is already empty.", QMessageBox::Ok);
}

/**
 * This slot is bound to the "Font selection" menu entry. The function will
 * open the font selection dialog with the text widget's current font as
 * default. If the user selects a font, it is passed to the MainText class
 * which will reconfigure itself and notify other dialogs that show text
 * content.
 */
void MainWin::menuCmdSelectFont(bool)
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_mainText->getFontContent(), this);
    if (ok)
    {
        // apply selected font in main text window
        m_mainText->setFontContent(font);
    }
}

/**
 * This slot is bound to the "Toggle line wrap" menu entry. The new state is
 * forwarded to the text widget.
 */
void MainWin::menuCmdToggleLineWrap(bool checked)
{
    m_mainText->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth
                                        : QPlainTextEdit::NoWrap);
}

/**
 * This procedure discards all text content and aborts all ongoing
 * activity and timers. The function is called before new data is
 * loaded.
 */
void MainWin::discardContent()
{
    // discard the complete document contents
    m_mainText->clear();
    m_mainText->setCurrentCharFormat(QTextCharFormat());
    m_mainText->cursorJumpStackReset();

    m_search->searchReset();

    m_bookmarks->removeAll();
    m_higl->adjustLineNums(0, 0);
    SearchList::adjustLineNums(0, 0);
}

/*
 * This function is bound to the "Discard above/below" menu commands. The
 * parameter specifies if content above or below the cursor is discarded.
 */
void MainWin::menuCmdDiscard(bool is_fwd)
{
    auto c = m_mainText->textCursor();
    int curLine = c.block().blockNumber();
    int lineCount = m_mainText->document()->blockCount();
    int delCount = 0;

    if (is_fwd)
    {
        // delete everything below the line holding the cursor
        delCount = lineCount - curLine - 1;
        if (delCount <= 0)
        {
            QMessageBox::information(this, "trowser", "Already at the bottom");
            return;
        }
        c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        curLine = c.block().blockNumber();
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    }
    else
    {
        // delete everything above the line holding the cursor
        delCount = curLine;
        if (delCount <= 0)
        {
            QMessageBox::information(this, "trowser", "Already at the top");
            return;
        }
        c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        c.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
    }
    int perc = ((lineCount != 0) ? int(100.0 * delCount / lineCount) : 0);

    // ask for confirmation, as this cannot be undone
    QString msg;
    QTextStream(&msg) << "Please confirm removing " << delCount
                      << ((delCount == 1) ? " line" : " lines")
                      << " (" << perc << "%), as this cannot be undone.";
    if (!m_curFileName.isEmpty())
        msg += " (The file will not be modified.)";

    auto answer = QMessageBox::question(this, "trowser", msg,
                                        QMessageBox::Ok | QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;

    m_search->searchHighlightClear();

    // perform the removal
    c.removeSelectedText();

    m_search->searchReset();
    m_mainText->cursorJumpStackReset();

    int top_l = (is_fwd ? 0 : curLine);
    int bottom_l = (is_fwd ? curLine : -1);

    m_bookmarks->adjustLineNums(top_l, bottom_l);
    m_higl->adjustLineNums(top_l, bottom_l);
    SearchList::adjustLineNums(top_l, bottom_l);
}


/**
 * This function is bound to the "Reload current file" menu command.
 */
void MainWin::menuCmdReload(bool)
{
    if (m_loadPipe != nullptr)
    {
        discardContent();
        loadFromPipe();
    }
    else
    {
        if (m_bookmarks->offerSave(this))
        {
            discardContent();
            loadFromFile(m_curFileName);
        }
    }
}


/**
 * This function is bound to the "Load file" menu command. The function asks
 * the user to select a file which to read. The current browser contents are
 * discarded and all bookmarks, search results etc. are cleared. Then the
 * new content is loaded.
 */
void MainWin::menuCmdFileOpen(bool)
{
    // offer to save old bookmarks before discarding them below
    if (m_bookmarks->offerSave(this))
    {
        QString fileName = QFileDialog::getOpenFileName(this, "Open File",
                                m_curFileName, "Trace Files (out.*);;Any (*)");
        if (fileName.isEmpty() == false)
        {
            discardContent();
            loadFromFile(fileName);
        }
    }
}

/**
 * This function is installed as callback for destroy requests on the
 * main window to store the search history and bookmarks.
 */
void MainWin::menuCmdFileQuit(bool)
{
    closeEvent(nullptr);
}


// user closed main window via "X" button
void MainWin::closeEvent(QCloseEvent * event)
{
    if (m_bookmarks->offerSave(this))
    {
        ConfigFile::updateRcFile();

        // FIXME connect(quitButton, clicked(), m_mainApp, &QApplication::quit, Qt::QueuedConnection);
        m_mainApp->quit();
    }
    else if (event)
    {
        event->ignore();
    }
}


/**
 * This external interface is called upon start-up to load the given file or
 * stream into the text widget and display the main window.
 */
void MainWin::startLoading(const char * fileName)
{
    if (ConfigFile::isValid() == false)
    {
        // set reasonable default size when starting without RC file
        QDesktopWidget dw;
        this->resize(dw.width()*0.5, dw.height()*0.75);
    }
    this->show();

    if (strcmp(fileName, "-") == 0)
    {
        loadFromPipe();
    }
    else if (strlen(fileName) > 0)
    {
        loadFromFile(fileName);
    }
    else
    {
        // no file specified: show empty window
    }
}

/**
 * This function loads a text file (or parts of it) into the text widget.
 */
void MainWin::loadFromFile(const QString& fileName)
{
    QFile fh(fileName);
    if (fh.open(QFile::ReadOnly | QFile::Text))
    {
        LoadMode load_file_mode;
        size_t load_buf_size;
        ConfigFile::getFileLoadParams(load_file_mode, load_buf_size, false);

        if (load_buf_size > 0)
        {
            if (   (load_file_mode == LoadMode::Tail)
                && (size_t(fh.size()) > load_buf_size))
            {
                fh.seek(fh.size() - load_buf_size);
            }
            if (load_buf_size > size_t(fh.size()))
            {
                load_buf_size = fh.size();
            }
        }
        else
        {
            load_buf_size = fh.size();
        }

        size_t rest = load_buf_size;
        const size_t CHUNK_SIZE = 1024*1024;
        char * inBuf = new char[CHUNK_SIZE + 1];

        // display modal progress dialog
        auto progressWid =
                std::make_unique<QProgressDialog>("Loading text from file...", "Stop",
                                         0, (load_buf_size + CHUNK_SIZE-1) / CHUNK_SIZE,
                                         this, Qt::Dialog);
        progressWid->setWindowModality(Qt::WindowModal);
        progressWid->setMinimumDuration(1000);  // ms
        progressWid->show();

        while (rest > 0)
        {
            qint64 rdSize = fh.read(inBuf, std::min(rest, CHUNK_SIZE));
            if (rdSize > 0)
            {
                // insert the loaded chunk of text at the end of the document
                m_mainText->textCursor().movePosition(QTextCursor::End);
                inBuf[rdSize] = 0;
                m_mainText->insertPlainText(QString(inBuf));
                rest -= rdSize;
            }
            else if (rdSize < 0)
            {
                QString msg = QString("Error while reading the file: ") + fh.errorString();
                QMessageBox::warning(this, "trowser", msg, QMessageBox::Ok);
                break;
            }
            else  // EOF
                break;

            // update progress dialog (modal)
            progressWid->setValue((load_buf_size - rest + CHUNK_SIZE-1) / CHUNK_SIZE);
            QApplication::processEvents();
            if (progressWid->wasCanceled())
            {
                m_stline->showWarning("load", QString("Stopped loading after ")
                                        + QString::number(uint64_t(100)*(load_buf_size - rest)/load_buf_size)
                                        + "%");
                break;
            }
        }
        delete[] inBuf;

        // move cursor to start or end of document, depending on head/tail option
        auto c = m_mainText->textCursor();
        c.movePosition((load_file_mode == LoadMode::Head) ? QTextCursor::Start : QTextCursor::End);
        m_mainText->setTextCursor(c);
        m_mainText->ensureCursorVisible();

        // adapt text of the "reload file" menu entry, in case pipe was used previously
        m_actFileReload->setText("Reload current file");
        m_actFileReload->setEnabled(true);
        if (m_loadPipe != nullptr)
        {
            delete m_loadPipe;
            m_loadPipe = nullptr;
        }

        m_bookmarks->readFileAuto(this);

        // start color highlighting in the background
        m_higl->highlightInit();

        // Propagate document name change to dialog windows
        setWindowTitle(fileName + " - trowser");
        m_curFileName = fileName;
        emit documentNameChanged();
    }
    else
    {
        QMessageBox::critical(this, "trowser",
                              QString("Error opening file ") + fileName,
                              QMessageBox::Ok);
    }
}

/**
 * This function is called after start-up when loading data from STDIN, as
 * specified on the command line via pseudo-filename "-". It may also be called
 * again later via the "Continue loading" menu entry, as long as EOF was not
 * reached previously (which is prevented by disabling the respective menu
 * entry). This function only triggers showing a progress dialog and initiating
 * loading of data via the LoadPipe class. The function then returns to the
 * main loop while data is loaded in the background. After completion, actual
 * loading of that data into the display is triggered by a signal sent by the
 * LoadPipe class (see next function.)
 */
void MainWin::loadFromPipe()
{
    if (m_loadPipe == nullptr)
    {
        LoadMode load_file_mode;
        size_t load_buf_size;
        ConfigFile::getFileLoadParams(load_file_mode, load_buf_size, true);

        m_loadPipe = new LoadPipe(this, m_mainText, load_file_mode, load_buf_size);
        connect(m_loadPipe, &LoadPipe::pipeLoaded, this, &MainWin::loadPipeDone);

        if (isatty(0))
            fprintf(stderr, "trowser warning: input is a terminal - missing input redirection?\n");
    }
    else
        m_loadPipe->continueReading();

    m_mainText->viewport()->setCursor(Qt::BusyCursor);
}

/**
 * This slot is connected to a completion message sent by the LoadPipe class
 * when text data is available for display. The function retrieves the data,
 * inserts it to the text widget and then starts initial highlighting.
 */
void MainWin::loadPipeDone()
{
    std::vector<QByteArray> dataBuf;
    m_loadPipe->getLoadedData(dataBuf);

    // display modal progress dialog
    auto progressWid =
            std::make_unique<QProgressDialog>("Loading text from file...", "Stop",
                                              0, dataBuf.size(), this, Qt::Dialog);
    progressWid->setWindowModality(Qt::WindowModal);
    progressWid->show();


    // insert the data into the text widget
    size_t idx = 0;
    for (auto& data : dataBuf)
    {
        m_mainText->textCursor().movePosition(QTextCursor::End);
        m_mainText->insertPlainText(data);

        // update progress dialog (modal)
        if (++idx % 16 == 0)  // avoid slowing down loading process
        {
            progressWid->setValue(++idx);
            QApplication::processEvents();
            if (progressWid->wasCanceled())
            {
                m_stline->showWarning("load", QString("Stopped loading after ")
                                        + QString::number(100*idx/dataBuf.size())  // size>0 asserted
                                        + "%");
                break;
            }
        }
    }

    // store buffer size parameter specified in the dialog in the RC file
    LoadMode load_file_mode = m_loadPipe->getLoadMode();
    size_t load_buf_size = m_loadPipe->getLoadBufferSize();
    ConfigFile::updateFileLoadParams(load_file_mode, load_buf_size);

    // move cursor to start or end of document, depending on head/tail option
    auto c = m_mainText->textCursor();
    c.movePosition((load_file_mode == LoadMode::Head)
                        ? QTextCursor::Start : QTextCursor::End);
    m_mainText->setTextCursor(c);
    m_mainText->ensureCursorVisible();

    // finally initiate color highlighting etc.
    m_mainText->viewport()->setCursor(Qt::ArrowCursor);
    m_higl->highlightInit();

    // adapt text of the "reload file" menu entry
    m_actFileReload->setText("Continue loading STDIN...");
    m_actFileReload->setEnabled(!m_loadPipe->isEof());

    if (m_loadPipe->isEof())
    {
        delete m_loadPipe;
        m_loadPipe = nullptr;
    }
}

// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("trowser");
    bool load_initial_file = ParseArgv(argc, argv);

    MainWin main_win(&app);
    ConfigFile::loadRcFile();

    main_win.startLoading(load_initial_file ? argv[argc - 1] : "");

    return app.exec();
}

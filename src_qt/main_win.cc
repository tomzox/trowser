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
 * DESCRIPTION:  Browser for line-oriented text files, e.g. debug traces.
 * This is a translation from the original implementation in Tcl/Tk.
 *
 * ----------------------------------------------------------------------------
 */

#include <QApplication>
#include <QWidget>
#include <QMenu>
#include <QMenuBar>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QToolBar>
#include <QFontDialog>
#include <QRegularExpression>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "main_search.h"
#include "highlighter.h"
#include "bookmarks.h"
#include "status_line.h"
#include "search_list.h"
#include "dlg_higl.h"
#include "dlg_history.h"
#include "dlg_bookmarks.h"
#include "dlg_markup_sa.h"

static int load_file_mode = 0;
static int load_buf_size = 0x10000000;
static const char * myrcfile = ".trowserc.qt";
static const char * const DEFAULT_FONT_FAM = "DejaVu Sans Mono";
static const int DEFAULT_FONT_SZ = 9;

// ----------------------------------------------------------------------------

MainWin::MainWin(QApplication * app)
    : m_mainApp(app)
    , m_fontContent(DEFAULT_FONT_FAM, DEFAULT_FONT_SZ, false, false)
{
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    m_search = new MainSearch(this);
    m_bookmarks = new Bookmarks(this);

    m_f1_t = new MainText(this, m_search, m_bookmarks, central_wid);
        m_f1_t->setFont(m_fontContent);
        m_f1_t->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_f1_t->setCursorWidth(2);
        m_f1_t->setTabChangesFocus(true);   // enable Key_Tab moving keyboard focus
        layout_top->addWidget(m_f1_t);

    auto f2 = new QToolBar("Find", this);
        f2->setObjectName("Toolbar::Find"); // for saveState
        addToolBar(Qt::BottomToolBarArea, f2);
    auto f2_l = new QLabel("Find:", f2);
        f2_l->setToolTip("Enter a search text or regular expression, then\n"
                         "press RETURN to search for it in the main window.\n"
                         "Use key up/down for browsing your search history.");
        f2->addWidget(f2_l);
    auto f2_e = new MainFindEnt(m_search, f2);
        f2->addWidget(f2_e);
    auto f2_bn = new QPushButton("&Next", f2);
        f2_bn->setToolTip("Move the cursor to the next match on the search pattern.");
        connect(f2_bn, &QPushButton::clicked, [=](){ m_search->searchNext(true); });
        f2->addWidget(f2_bn);
    auto f2_bp = new QPushButton("&Prev.", f2);
        f2_bp->setToolTip("Move the cursor to the previous match on the search pattern.");
        connect(f2_bp, &QPushButton::clicked, [=](){ m_search->searchNext(false); });
        f2->addWidget(f2_bp);
    auto f2_bl = new QPushButton("&All", f2);
        f2_bl->setToolTip("Open a new window showing all lines that contain a match for the pattern.");
        // FIXME first param should be "false" when used via shortcut
        connect(f2_bl, &QPushButton::clicked, [=](){ m_search->searchAll(true, 0); });
        f2->addWidget(f2_bl);
    f2->addSeparator();
    auto f2_hall = new QCheckBox("Highlight all", f2);
        f2_hall->setToolTip("When checked, highlight all lines that contain a match on the search pattern.");
        // do not add shortcut "ALT-H" here as this would move focus when triggered
        connect(f2_hall, &QPushButton::clicked, [=](bool v){ m_search->searchOptToggleHall(v); });
        f2->addWidget(f2_hall);
    auto f2_mcase = new QCheckBox("Match case", f2);
        f2_mcase->setToolTip("When checked, only match on letters in the case given in the pattern.\nElse ignore case.");
        connect(f2_mcase, &QPushButton::clicked, [=](bool v){ m_search->searchOptToggleCase(v); });
        f2->addWidget(f2_mcase);
    auto f2_regexp = new QCheckBox("Reg.Exp.", f2);
        f2_regexp->setToolTip("Evaluate the search string as regular expression in Perl syntax");
        connect(f2_regexp, &QPushButton::clicked, [=](bool v){ m_search->searchOptToggleRegExp(v); });
        f2->addWidget(f2_regexp);

    m_stline = new StatusLine(m_f1_t);
    m_higl = new Highlighter(m_f1_t, this);
    m_search->connectWidgets(m_f1_t, m_higl, f2_e, f2_hall, f2_mcase, f2_regexp);
    m_bookmarks->connectWidgets(m_f1_t, m_higl);
    SearchList::connectWidgets(this, m_search, m_f1_t, m_higl, m_bookmarks);
    DlgBookmarks::connectWidgets(this, m_search, m_f1_t, m_higl, m_bookmarks);
    DlgHistory::connectWidgets(this, m_search, m_f1_t);

    layout_top->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(central_wid);

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
    act = new QAction(central_wid);  // toggle "highlight all" option
        act->setShortcut(QKeySequence(Qt::ALT + Qt::Key_H));
        connect(act, &QAction::triggered, [=](){ m_search->searchOptToggleHall(-1); });
        central_wid->addAction(act);

    m_f1_t->setFocus(Qt::ShortcutFocusReason);

    m_timUpdateRc = new QTimer(this);
    m_timUpdateRc->setSingleShot(true);
    m_timUpdateRc->setInterval(3000);
    connect(m_timUpdateRc, &QTimer::timeout, this, &MainWin::updateRcFile);
    m_tsUpdateRc = QDateTime::currentSecsSinceEpoch();
}

MainWin::~MainWin()
{
    delete m_search;
    delete m_bookmarks;
    delete m_stline;
}

void MainWin::populateMenus()
{
    QAction * act;

    m_menubar_ctrl = menuBar()->addMenu("&Control");
    act = m_menubar_ctrl->addAction("Open file...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileOpen);
    act = m_menubar_ctrl->addAction("Reload current file");
        //TODO "Continue loading STDIN..." when loading through pipe
        connect(act, &QAction::triggered, this, &MainWin::menuCmdReload);
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Discard above cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(false); });
    act = m_menubar_ctrl->addAction("Discard below cursor...");
        connect(act, &QAction::triggered, [=](){ MainWin::menuCmdDiscard(true); });
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Toggle line wrap");
        act->setCheckable(true);
        connect(act, &QAction::toggled, this, &MainWin::menuCmdToggleLineWrap);
    act = m_menubar_ctrl->addAction("Font selection...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdSelectFont);
    QMenu *sub = m_menubar_ctrl->addMenu("Mark-up configuration");
        act = sub->addAction("Search results...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editSearchFmt(m_higl, this); });
        act = sub->addAction("Search increment...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editSearchIncFmt(m_higl, this); });
        act = sub->addAction("Bookmarks...");
            connect(act, &QAction::triggered, [=](){ DlgMarkupSA::editBookmarkFmt(m_higl, this); });
    m_menubar_ctrl->addSeparator();
    act = m_menubar_ctrl->addAction("Quit");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdFileQuit);

    m_menubar_srch = menuBar()->addMenu("&Search");
    act = m_menubar_srch->addAction("Search history...");
        connect(act, &QAction::triggered, [=](){ DlgHistory::openDialog(); });
    act = m_menubar_srch->addAction("Edit highlight patterns...");
        connect(act, &QAction::triggered, [=](){ DlgHigl::openDialog(m_higl, m_search, this); });
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("List all search matches...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 0); });
    act = m_menubar_srch->addAction("List all matches above...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, -1); });
    act = m_menubar_srch->addAction("List all matches below...");
        connect(act, &QAction::triggered, [=](){ m_search->searchAll(true, 1); });
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("Clear search highlight");
        connect(act, &QAction::triggered, m_search, &MainSearch::searchHighlightClear);
    m_menubar_srch->addSeparator();
    act = m_menubar_srch->addAction("Goto line number...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdGotoLine);

    m_menubar_mark = menuBar()->addMenu("&Bookmarks");
    act = m_menubar_mark->addAction("Toggle bookmark");
        connect(act, &QAction::triggered, [=](){ m_f1_t->toggleBookmark(); });
    act = m_menubar_mark->addAction("List bookmarks");
        connect(act, &QAction::triggered, [=](){ DlgBookmarks::openDialog(); });
    act = m_menubar_mark->addAction("Delete all bookmarks");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdBookmarkDeleteAll);
    m_menubar_mark->addSeparator();
    act = m_menubar_mark->addAction("Jump to prev. bookmark");
        connect(act, &QAction::triggered, [=](){ m_f1_t->jumpToNextBookmark(false); });
    act = m_menubar_mark->addAction("Jump to next bookmark");
        connect(act, &QAction::triggered, [=](){ m_f1_t->jumpToNextBookmark(true); });
    m_menubar_mark->addSeparator();
    act = m_menubar_mark->addAction("Read bookmarks from file...");
        connect(act, &QAction::triggered, [=](){ m_bookmarks->readFileFrom(this); });
    act = m_menubar_mark->addAction("Save bookmarks to file...");
        connect(act, &QAction::triggered, [=](){ m_bookmarks->saveFileAs(this, false); });

    m_menubar_help = menuBar()->addMenu("Help");
    act = m_menubar_help->addAction("About trowser...");
        connect(act, &QAction::triggered, this, &MainWin::menuCmdAbout);
    act = m_menubar_help->addAction("About Qt...");
        connect(act, &QAction::triggered, m_mainApp, &QApplication::aboutQt);
}

QWidget * MainWin::focusWidget() const
{
    return m_mainApp->focusWidget();
}

QStyle * MainWin::getAppStyle() const
{
    return m_mainApp->style();
}

const QColor& MainWin::getFgColDefault() const
{
    auto& pal = m_f1_t->palette();
    return pal.color(QPalette::Text);
}

const QColor& MainWin::getBgColDefault() const
{
    auto& pal = m_f1_t->palette();
    return pal.color(QPalette::Base);
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
                "Homepage: <A HREF=\"https://github.com/tomzox/trowser\">github.com/tomzox/trowser</A>\n"
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
 * This function is bound to CTRL-G in the main window. It displays the
 * current line number and fraction of lines above the cursor in percent
 * (i.e. same as VIM)
 */
void MainWin::menuCmdDisplayLineNo()
{
    auto c = m_f1_t->textCursor();
    int line = c.block().blockNumber();
    int max = m_f1_t->document()->blockCount();

    QString msg = m_curFileName + ": line " + QString::number(line + 1) + " of "
                    + QString::number(max) + " lines";
    if (max > 1)
        msg += " (" + QString::number(int(100.0 * line / max + 0.5)) + "%)";

    m_stline->showPlain("line_query", msg);
}

/**
 * This function is bound to the "Goto line" menu command.  The function opens
 * a small modal dialog for entering a number in range of the number of lines
 * in the text buffer (starting at 1). When completed by the user the cursor is
 * set to the start of the specified line.
 */
void MainWin::menuCmdGotoLine(bool)
{
    int lineCount = m_f1_t->document()->blockCount();
    bool ok = false;

    int line = QInputDialog::getInt(this, "Goto line number...",
                                    QString("Enter line number: (max. ")
                                        + QString::number(lineCount) + ")",
                                    1, 1, lineCount, 1, &ok);
    if (ok && (line > 0) && (line <= lineCount))
    {
        m_f1_t->jumpToLine(line - 1);
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

void MainWin::menuCmdSelectFont(bool)
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_fontContent, this);
    if (ok)
    {
        m_fontContent = font;

        // apply font directly in main text window
        m_f1_t->setFont(m_fontContent);

        // notify dialogs
        emit textFontChanged();
    }
}

void MainWin::keyCmdZoomFontSize(bool zoomIn)
{
    if (zoomIn)
        m_f1_t->zoomIn();
    else
        m_f1_t->zoomOut();

    m_fontContent = m_f1_t->font();

    // notify dialogs
    emit textFontChanged();
}

void MainWin::menuCmdToggleLineWrap(bool checked)
{
    m_f1_t->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

/**
 * This procedure discards all text content and aborts all ongoing
 * activity and timers. The function is called before new data is
 * loaded.
 */
void MainWin::discardContent()
{
    // discard the complete document contents
    m_f1_t->clear();
    m_f1_t->setCurrentCharFormat(QTextCharFormat());
    m_f1_t->cursorJumpStackReset();

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
    auto c = m_f1_t->textCursor();
    int curLine = c.block().blockNumber();
    int lineCount = m_f1_t->document()->blockCount();
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
    m_f1_t->cursorJumpStackReset();

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
#if 0 //TODO
    if (load_pipe)
    {
        discardContent();
        load_pipe.LoadPipe_Start();
    }
    else
#endif
    {
        if (m_bookmarks->offerSave(this))
        {
            discardContent();
            LoadFile(m_curFileName);
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
        QString fileName = QFileDialog::getOpenFileName(this,
                             "Open File", ".", "Trace Files (out.*);;Any (*)");
        if (fileName.isEmpty() == false)
        {
            discardContent();
            LoadFile(fileName);
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
    updateRcFile();

    if (m_bookmarks->offerSave(this))
    {
        // FIXME connect(quitButton, clicked(), m_mainApp, &QApplication:quit, Qt::QueuedConnection);
        m_mainApp->quit();
    }
    else if (event)
    {
        event->ignore();
    }
}


/**
 * This function loads a text file (or parts of it) into the text widget.
 */
void MainWin::LoadFile(const QString& fileName)
{
    QFile fh(fileName);
    if (fh.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream in(&fh);

        // error handling
        if (in.status() != QTextStream::Ok)
        {
            QString msg = QString("Error while reading the file: ") + fh.errorString();
            QMessageBox::critical(this, "trowser", msg, QMessageBox::Ok);
            return;
        }

        m_f1_t->setPlainText(in.readAll());
        //m_f1_t->setReadOnly(true);

        setWindowTitle(fileName + " - trowser");
        m_curFileName = fileName;

        // TODO InitContent
        // - propagate window title change to search list & bookmark list

        m_bookmarks->readFileAuto(this);

        // start color highlighting in the background
        m_higl->highlightInit();

        emit documentNameChanged();
    }
    else
    {
        QMessageBox::critical(this, "trowser",
                              QString("Error opening file ") + fileName,
                              QMessageBox::Ok);
    }
}

// ----------------------------------------------------------------------------

/**
 * This function reads configuration variables from the rc file.
 * The function is called once during start-up.
 */
void MainWin::loadRcFile()
{
    QFile fh(myrcfile);
    if (fh.open(QFile::ReadOnly | QFile::Text))
    {
        QJsonParseError err;
        QTextStream readFile(&fh);
        auto txt = readFile.readAll();

        // skip comments at the start of the file
        int off = 0;
        while (off < txt.length())
        {
            static const QRegularExpression re1("\\s*(?:#.*)?(?:\n|$)");
            auto mat1 = re1.match(txt.midRef(off), 0, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
            if (!mat1.hasMatch())
                break;
            off += mat1.captured(0).length();
        }

        auto doc = QJsonDocument::fromJson(txt.midRef(off).toUtf8(), &err);
        if (!doc.isNull())
        {
            if (doc.isObject())
            {
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it)
                {
                    const QString& var = it.key();
                    const QJsonValue& val = it.value();

                    if (var == "main_search")               m_search->setRcValues(val.toObject());
                    else if (var == "highlight")            m_higl->setRcValues(val);
                    //else if (var == "tick_pat_sep")       tick_pat_sep = val;
                    //else if (var == "tick_pat_num")       tick_pat_num = val;
                    //else if (var == "tick_str_prefix")    tick_str_prefix = val;
                    else if (var == "font_content")         m_fontContent.fromString(val.toString());
                    //else if (var == "fmt_selection")      fmt_selection = val;
                    //else if (var == "fmt_find")           fmt_find = val;
                    //else if (var == "fmt_findinc")        fmt_findinc = val;
                    else if (var == "load_buf_size")        load_buf_size = val.toInt();
                    //else if (var == "rcfile_version")     rcfile_version = val;
                    //else if (var == "rc_compat_version")  rc_compat_version = val;
                    //else if (var == "rc_timestamp")       { /* nop */ }
                    else if (var == "main_win_state")       this->restoreState(QByteArray::fromHex(val.toString().toLatin1()));
                    else if (var == "main_win_geom")        this->restoreGeometry(QByteArray::fromHex(val.toString().toLatin1()));
                    else if (var == "search_list")          SearchList::setRcValues(val);
                    else if (var == "dlg_highlight")        DlgHigl::setRcValues(val);
                    else if (var == "dlg_history")          DlgHistory::setRcValues(val);
                    else if (var == "dlg_bookmarks")        DlgBookmarks::setRcValues(val);
                    else
                        fprintf(stderr, "trowser: ignoring unknown keyword in rcfile: %s\n", var.toLatin1().data());
                }
                m_f1_t->setFont(m_fontContent);
            }
        }
        else
        {
            fprintf(stderr, "Error parsing config file: %s\n", err.errorString().toLatin1().data());
        }
        fh.close();
    }
    else
    {
        // Application GUI is not initialized yet, so print to console
        fprintf(stderr, "trowser: warning: failed to load config file '%s': %s\n", myrcfile, strerror(errno));
    }
}

/**
 * This functions writes configuration variables into the rc file
 */
void MainWin::updateRcFile()
{
    static bool rc_file_error = false;

    m_timUpdateRc->stop();
    m_tsUpdateRc = QDateTime::currentSecsSinceEpoch();

    //expr {srand([clock clicks -milliseconds])}
    //append tmpfile $myrcfile "." [expr {int(rand() * 1000000)}] ".tmp"

    QJsonObject obj;

    // dump software version
    //puts $rcfile [list set rcfile_version $rcfile_version]
    //puts $rcfile [list set rc_compat_version $rcfile_compat]
    //puts $rcfile [list set rc_timestamp [clock seconds]]

    // frame number parser patterns
    //puts $rcfile [list set tick_pat_sep $tick_pat_sep]
    //puts $rcfile [list set tick_pat_num $tick_pat_num]
    //puts $rcfile [list set tick_str_prefix $tick_str_prefix]

    // dump search history
    obj.insert("main_search", m_search->getRcValues());

    // dump highlighting patterns
    obj.insert("highlight", m_higl->getRcValues());

    // dialog window geometry and other options
    obj.insert("dlg_highlight", DlgHigl::getRcValues());
    obj.insert("dlg_history", DlgHistory::getRcValues());
    obj.insert("dlg_bookmarks", DlgBookmarks::getRcValues());
    obj.insert("search_list", SearchList::getRcValues());

    obj.insert("main_win_geom", QJsonValue(QString(this->saveGeometry().toHex())));
    obj.insert("main_win_state", QJsonValue(QString(this->saveState().toHex())));

    // font and color settings
    obj.insert("font_content", QJsonValue(m_fontContent.toString()));
    //puts $rcfile [list set fmt_find $fmt_find]
    //puts $rcfile [list set fmt_findinc $fmt_findinc]
    //puts $rcfile [list set fmt_selection $fmt_selection]

    // misc (note the head/tail mode is omitted intentionally)
    obj.insert("load_buf_size", QJsonValue(load_buf_size));

    QJsonDocument doc;
    doc.setObject(obj);

    QFile fh(myrcfile);
    if (fh.open(QFile::WriteOnly | QFile::Text))
    {
        QTextStream out(&fh);

        out << "#\n"
               "# trowser configuration file\n"
               "#\n"
               "# This file is automatically generated - do not edit\n"
               "# Written at: " << QDateTime::currentDateTime().toString() << "\n"
               "#\n";

        out << doc.toJson();

        fh.close();
#if 0
        // copy attributes on the new file
        if {[catch {set att_perm [file attributes $myrcfile -permissions]}] == 0} {
            catch {file attributes $tmpfile -permissions $att_perm}
        }
        if {[catch {set att_grp [file attributes $myrcfile -group]}] == 0} {
            catch {file attributes $tmpfile -group $att_grp}
        }
        // move the new file over the old one
        if {[catch {file rename -force $tmpfile $myrcfile} errstr] != 0} {
            if {![info exists rc_file_error]} {
                tk_messageBox -type ok -default ok -icon error \
                              -message "Could not replace rc file $myrcfile: $errstr"
                set rc_file_error 1
            }
        } else {
            unset -nocomplain rc_file_error
        }

        //TODO catch write error
        {
            // write error - remove the file fragment, report to user
            catch {file delete $tmpfile}
            if {![info exists rc_file_error]} {
                tk_messageBox -type ok -default ok -icon error \
                              -message "Write error in file $myrcfile: $errstr"
                set rc_file_error 1
            }
        }
#endif
        rc_file_error = false;
    }
    else /* open error */
    {
        if (!rc_file_error)
        {
            QMessageBox::critical(this, "trowser",
                                  QString("Error writing config file ") + myrcfile,
                                  QMessageBox::Ok);
            rc_file_error = true;
        }
    }
}

/**
 * This function is used to trigger writing the RC file after changes.
 * The write is delayed by a few seconds to avoid writing the file multiple
 * times when multiple values are changed. This timer is restarted when
 * another change occurs during the delay, however only up to a limit.
 */
void MainWin::updateRcAfterIdle()
{
    if (   !m_timUpdateRc->isActive()
        || (QDateTime::currentSecsSinceEpoch() - m_tsUpdateRc) < 60)
    {
        m_timUpdateRc->start();
    }
}


// ----------------------------------------------------------------------------

/**
 * This function is called when the program is started with -help to list all
 * possible command line options.
 */
void PrintUsage(const char * const argv[], int argvn=-1, const char * reason=nullptr)
{
    if (reason != nullptr)
        fprintf(stderr, "%s: %s: %s\n", argv[0], reason, argv[argvn]);

    fprintf(stderr, "Usage: %s [options] {file|-}\n", argv[0]);

    if (argvn != -1)
    {
        fprintf(stderr, "Use -h or --help for a list of options\n");
    }
    else
    {
        fprintf(stderr, "The following options are available:\n"
                        "  --head=size\t\tLoad <size> bytes from the start of the file\n"
                        "  --tail=size\t\tLoad <size> bytes from the end of the file\n"
                        "  --rcfile=<path>\tUse alternate config file (default: ~/.trowserc)\n");
    }
    exit(1);
}


/**
 * This helper function checks if a command line flag which requires an
 * argument is followed by at least another word on the command line.
 */
void ParseArgvLenCheck(int argc, const char * const argv[], int arg_idx)
{
    if (arg_idx + 1 >= argc)
        PrintUsage(argv, arg_idx, "this option requires an argument");
}

/**
 * This helper function reads an integer value from a command line parameter
 */
int ParseArgInt(const char * const argv[], int arg_idx, const char * opt)
{
    int ival = 0;
    try
    {
        std::size_t pos;
        ival = std::stoi(opt, &pos);
        if (opt[pos] != 0)
            PrintUsage(argv, arg_idx, "numerical value is followed by garbage");
    }
    catch (const std::exception& ex)
    {
        PrintUsage(argv, arg_idx, "is not a numerical value");
    }
    return ival;
}

/**
 * This function parses and evaluates the command line arguments.
 */
void ParseArgv(int argc, const char * const argv[])
{
    bool file_seen = false;
    int arg_idx = 1;

    while (arg_idx < argc)
    {
        const char * arg = argv[arg_idx];

        if ((arg[0] == '-') && (arg[1] != 0))
        {
            if (strcmp(arg, "-t") == 0)
            {
                ParseArgvLenCheck(argc, argv, arg_idx);
                arg_idx += 1;
                load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                load_file_mode = 1;
            }
            else if (strncmp(arg, "--tail", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    load_file_mode = 1;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --tail=10000000)");
            }
            else if (strcmp(arg, "-h") == 0)
            {
                ParseArgvLenCheck(argc, argv, arg_idx);
                arg_idx += 1;
                load_buf_size = ParseArgInt(argv, arg_idx, argv[arg_idx]);
                load_file_mode = 0;
            }
            else if (strncmp(arg, "--head", 6) == 0)
            {
                if ((arg[6] == '=') && (arg[6+1] != 0))
                {
                    load_buf_size = ParseArgInt(argv, arg_idx, arg + 6+1);
                    load_file_mode = 0;
                }
                else
                    PrintUsage(argv, arg_idx, "requires a numerical argument (e.g. --head=10000000)");
            }
            else if (strcmp(arg, "-r") == 0)
            {
                if (arg_idx + 1 < argc)
                {
                    arg_idx += 1;
                    myrcfile = argv[arg_idx];
                }
                else
                    PrintUsage(argv, arg_idx, "this option requires an argument");
            }
            else if (strncmp(arg, "--rcfile", 8) == 0)
            {
                if ((arg[8] == '=') && (arg[9] != 0))
                    myrcfile = arg + 8+1;
                else
                    PrintUsage(argv, arg_idx, "requires a path argument (e.g. --rcfile=foo/bar)");
            }
            else if (strcmp(arg, "-?") == 0 || strcmp(arg, "--help") == 0)
            {
                PrintUsage(argv);
            }
            else
                PrintUsage(argv, arg_idx, "unknown option");
        }
        else
        {
            if (arg_idx + 1 >= argc)
            {
                file_seen = true;
            }
            else
            {
                arg_idx += 1;
                PrintUsage(argv, arg_idx, "only one file name expected");
            }
        }
        arg_idx += 1;
    }

    if (!file_seen)
    {
        fprintf(stderr, "File name missing (use \"-\" for stdin)\n");
        PrintUsage(argv);
    }
}

// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("trowser");
    ParseArgv(argc, argv);

    MainWin main(&app);
    main.loadRcFile();
    main.LoadFile(argv[argc - 1]);

    main.show();
    return app.exec();
}

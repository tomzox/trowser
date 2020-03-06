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
 * This module implements a wrapper class that serves as owner of instances of
 * the mark-up configuration dialog. There are static interfaces that
 * instantiated the wrapper class for each of the pre-defined special-purpose
 * mark-up formats. The wrapper class will only create the dialog, then wait
 * for its completion signal, upon which it saves the results if applicable,
 * and destroys the dialog and itself. The static interfaces ensure that there
 * is only one instance of the wrapper and respective dialog for a given
 * purpose.
 */

#include <QWidget>

#include <cstdio>
#include <string>
#include <memory>

#include "main_win.h"
#include "highlighter.h"
#include "config_file.h"
#include "dlg_higl.h"
#include "dlg_markup.h"
#include "dlg_markup_sa.h"

// ----------------------------------------------------------------------------

/**
 * The constructor creates an instance of the mark-up configuration dialog for
 * the given ID.
 */
DlgMarkupSA::DlgMarkupSA(HiglId id, const QString& title, Highlighter * higl,
                         MainText * mainText, MainWin * mainWin)
    : m_id(id)
    , m_higl(higl)
    , m_mainText(mainText)
    , m_mainWin(mainWin)
{
    DlgHigl::initColorPalette();
    m_dlgWin = std::make_unique<DlgMarkup>(id, higl->getFmtSpecForId(id), title, higl, mainText, mainWin);

    connect(m_dlgWin.get(), &DlgMarkup::closeReq, this, &DlgMarkupSA::signalMarkupCloseReq);
    connect(m_dlgWin.get(), &DlgMarkup::applyReq, this, &DlgMarkupSA::signalMarkupApplyReq);
}


/**
 * This interface function is used to raise the window of the owned mark-up
 * dialog, when the user requests to open the dialog when it already exists.
 */
void DlgMarkupSA::raiseWindow()
{
    m_dlgWin->activateWindow();
    m_dlgWin->raise();
}

/**
 * This slot is connected to the signal sent when a mark-up editor dialog is
 * closed. The function releases the dialog resources.
 */
void DlgMarkupSA::signalMarkupCloseReq(HiglId id)
{
    Q_ASSERT(id == m_id);

    if (this == s_bookmarkFmtDlg)
    {
        delete s_bookmarkFmtDlg;
        s_bookmarkFmtDlg = nullptr;
    }
    else if (this == s_searchFmtDlg)
    {
        delete s_searchFmtDlg;
        s_searchFmtDlg = nullptr;
    }
}

/**
 * This slot is connected to the signal sent by the mark-up editor upon "Apply"
 * or "Ok". The function forwards the new format spec to the highlighter.
 */
void DlgMarkupSA::signalMarkupApplyReq(HiglId id, bool /*immediate*/)
{
    Q_ASSERT((id == Highlighter::HIGL_ID_BOOKMARK) || (id == Highlighter::HIGL_ID_SEARCH));

    m_higl->setFmtSpec(id, m_dlgWin->getFmtSpec());
    m_dlgWin->resetModified();
    ConfigFile::updateRcAfterIdle();
}

// ----------------------------------------------------------------------------
// Static interfaces

DlgMarkupSA * DlgMarkupSA::s_searchFmtDlg = nullptr;
DlgMarkupSA * DlgMarkupSA::s_searchIncFmtDlg = nullptr;
DlgMarkupSA * DlgMarkupSA::s_bookmarkFmtDlg = nullptr;

/**
 * This common sub-function is used by the static interfaces for actually
 * instantiating the wrapper class for a given mark-up ID. If the respective
 * instance already exists, its dialog window is raised instead.
 */
void DlgMarkupSA::openDialog(DlgMarkupSA* &ptr, HiglId id, const QString& title,
                             Highlighter * higl, MainText * mainText, MainWin * mainWin)  /*static*/
{
    if (ptr == nullptr)
    {
        ptr = new DlgMarkupSA(id, title, higl, mainText, mainWin);
    }
    else
    {
        ptr->raiseWindow();
    }
}

/**
 * Open or raise the mark-up editor for the mark-up used for highlighting
 * complete text lines that match a search.
 */
void DlgMarkupSA::editSearchFmt(Highlighter * higl, MainText * mainText, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_searchFmtDlg, Highlighter::HIGL_ID_SEARCH, "Search matches mark-up", higl, mainText, mainWin);
}

/**
 * Open or raise the mark-up editor for the mark-up used for highlighting the
 * exact range of text that matches a search. Note this mark-up is always
 * applied on top ot the above mark-up.
 */
void DlgMarkupSA::editSearchIncFmt(Highlighter * higl, MainText * mainText, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_searchIncFmtDlg, Highlighter::HIGL_ID_SEARCH_INC, "Search increment mark-up", higl, mainText, mainWin);
}

/**
 * Open or raise the mark-up editor for the mark-up used for highlighting
 * bookmarked lines.
 */
void DlgMarkupSA::editBookmarkFmt(Highlighter * higl, MainText * mainText, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_bookmarkFmtDlg, Highlighter::HIGL_ID_BOOKMARK, "Bookmarks mark-up", higl, mainText, mainWin);
}

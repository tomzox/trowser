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

#include <QWidget>

#include <cstdio>
#include <string>
#include <memory>

#include "main_win.h"
#include "highlighter.h"
#include "dlg_higl.h"
#include "dlg_markup.h"
#include "dlg_markup_sa.h"

// ----------------------------------------------------------------------------

DlgMarkupSA::DlgMarkupSA(HiglId id, const QString& name, Highlighter * higl, MainWin * mainWin)
    : m_id(id)
    , m_higl(higl)
    , m_mainWin(mainWin)
{
    DlgHigl::initColorPalette();
    m_dlgWin = std::make_unique<DlgMarkup>(id, name, higl->getFmtSpecForId(id), higl, mainWin);

    connect(m_dlgWin.get(), &DlgMarkup::closeReq, this, &DlgMarkupSA::signalMarkupCloseReq);
    connect(m_dlgWin.get(), &DlgMarkup::applyReq, this, &DlgMarkupSA::signalMarkupApplyReq);
}


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
    m_mainWin-> updateRcFile();
}

// ----------------------------------------------------------------------------
// Static interfaces

DlgMarkupSA * DlgMarkupSA::s_searchFmtDlg = nullptr;
DlgMarkupSA * DlgMarkupSA::s_searchIncFmtDlg = nullptr;
DlgMarkupSA * DlgMarkupSA::s_bookmarkFmtDlg = nullptr;

void DlgMarkupSA::openDialog(DlgMarkupSA* &ptr, HiglId id, const QString& name,
                             Highlighter * higl, MainWin * mainWin)  /*static*/
{
    if (ptr == nullptr)
    {
        ptr = new DlgMarkupSA(id, name, higl, mainWin);
    }
    else
    {
        ptr->raiseWindow();
    }
}

void DlgMarkupSA::editSearchFmt(Highlighter * higl, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_searchFmtDlg, Highlighter::HIGL_ID_SEARCH, "Search matches mark-up", higl, mainWin);
}

void DlgMarkupSA::editSearchIncFmt(Highlighter * higl, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_searchIncFmtDlg, Highlighter::HIGL_ID_SEARCH_INC, "Search increment mark-up", higl, mainWin);
}

void DlgMarkupSA::editBookmarkFmt(Highlighter * higl, MainWin * mainWin)  /*static*/
{
    openDialog(DlgMarkupSA::s_bookmarkFmtDlg, Highlighter::HIGL_ID_BOOKMARK, "Bookmarks mark-up", higl, mainWin);
}

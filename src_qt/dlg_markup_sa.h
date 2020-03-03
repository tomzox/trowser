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
#ifndef _DLG_MARKUP_SA_H
#define _DLG_MARKUP_SA_H

#include <QWidget>
#include <memory>
#include "highlighter.h"
#include "dlg_markup.h"

class QString;

class Highlighter;
class MainWin;
class DlgMarkup;

// ----------------------------------------------------------------------------

class DlgMarkupSA : public QWidget
{
    Q_OBJECT

public:
    static void editSearchFmt(Highlighter * higl, MainWin * mainWin);
    static void editSearchIncFmt(Highlighter * higl, MainWin * mainWin);
    static void editBookmarkFmt(Highlighter * higl, MainWin * mainWin);

private:
    static void openDialog(DlgMarkupSA* &ptr, HiglId id, const QString& title,
                           Highlighter * higl, MainWin * mainWin);
    // constructor can only be invoked via the static interface
    DlgMarkupSA(HiglId id, const QString& title, Highlighter * higl, MainWin * mainWin);
    virtual ~DlgMarkupSA() = default;
    void raiseWindow();
    void signalMarkupCloseReq(HiglId id);
    void signalMarkupApplyReq(HiglId id, bool immediate);

private:
    static DlgMarkupSA * s_searchFmtDlg;
    static DlgMarkupSA * s_searchIncFmtDlg;
    static DlgMarkupSA * s_bookmarkFmtDlg;

    const HiglId m_id;
    Highlighter * const m_higl;
    MainWin * const m_mainWin;
    std::unique_ptr<DlgMarkup> m_dlgWin;
};

#endif /* _DLG_MARKUP_SA_H */

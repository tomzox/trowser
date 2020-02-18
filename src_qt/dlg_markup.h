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
#ifndef _DLG_MARKUP_H
#define _DLG_MARKUP_H

#include <QWidget>
#include <QMainWindow>
#include "highlighter.h"

class QDialogButtonBox;
class QPlainTextEdit;
class QPushButton;
class QAbstractButton;

class HiglFmtSpec;
class Highlighter;
class MainWin;

class DlgMarkup : public QMainWindow
{
    Q_OBJECT

public:
    // constructor can only be invoked via the static interface
    DlgMarkup(const QString& name, const HiglFmtSpec * fmtSpec, Highlighter * higl, MainWin * mainWin);
    ~DlgMarkup();
    const HiglFmtSpec& getFmtSpec() const { return m_fmtSpec; }
    void resetModified();

signals:
    void applyReq(bool immediate);
    void closeReq();

private:
    virtual void closeEvent(QCloseEvent *) override;
    void mainFontChanged();
    void drawTextSample();
    void drawButtonPixmaps();
    void drawButtonPixmap(QPushButton *but, QRgb col, Qt::BrushStyle style, QPixmap& pix);
    void drawFontButtonText();
    void cmdResetColor(QRgb * col);
    void cmdSelectColor(QRgb * col, const QString& desc);
    void cmdSetFontOption(bool *option, bool value);
    void cmdSelectFont(bool doReset);
    void cmdButton(QAbstractButton * button);
#if 0
    void cmdChangeBgColor(bool);
    void cmdChangeFgColor(bool);
    void cmdToggleFontUnderline(const QModelIndex& index, bool checked);
    void cmdToggleFontBold(const QModelIndex& index, bool checked);
    void cmdToggleFontItalic(const QModelIndex& index, bool checked);
    void cmdToggleFontOverstrike(const QModelIndex& index, bool checked);
    void cmdResetFont(const QModelIndex& index);
    void cmdChangeFont(bool);
#endif

private:
    HiglFmtSpec         m_fmtSpec;
    Highlighter * const m_higl;
    MainWin * const     m_mainWin;
    bool                m_isModified = false;

    QPlainTextEdit    * m_sampleWid = nullptr;
    QDialogButtonBox  * m_cmdButs = nullptr;
    QPushButton       * m_bgColBut = nullptr;
    QPushButton       * m_bgStyleBut = nullptr;
    QPushButton       * m_fgColBut = nullptr;
    QPushButton       * m_fgStyleBut = nullptr;
    QPushButton       * m_olColBut = nullptr;
    QPushButton       * m_curFontBut = nullptr;
};

#endif /* _DLG_MARKUP_H */

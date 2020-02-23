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
class QComboBox;
class QSpinBox;
class QCheckBox;

class HiglFmtSpec;
class Highlighter;
class MainWin;
class BrushStyleListModel;

class DlgMarkup : public QMainWindow
{
    Q_OBJECT

public:
    DlgMarkup(HiglId id, const QString& name, const HiglFmtSpec * fmtSpec,
              Highlighter * higl, MainWin * mainWin);
    ~DlgMarkup();
    const HiglFmtSpec& getFmtSpec() const { return m_fmtSpec; }
    HiglId getFmtId() const { return m_id; }
    void resetModified();

signals:
    void applyReq(HiglId id, bool immediate);
    void closeReq(HiglId id);

private:
    virtual void closeEvent(QCloseEvent *) override;
    void mainFontChanged();
    void cmdButton(QAbstractButton * button);
    void cmdResetColor(QRgb * col);
    void cmdSelectColor(QRgb * col, const QString& desc);
    void cmdSetFontOption(bool *option, bool value);
    void cmdSetFontSizeOff(int value);
    void cmdResetFont();
    void cmdSelectFont();
    void cmdSelectStyle(Qt::BrushStyle * style, int idx);
    void drawTextSample();
    void drawButtonPixmaps();
    void drawButtonPixmap(QPushButton *but, QRgb col, Qt::BrushStyle style, QPixmap& pix);
    void drawFontButtonText();
    void setFontButtonState();

private:
    const HiglId        m_id;
    HiglFmtSpec         m_fmtSpecOrig;
    HiglFmtSpec         m_fmtSpec;
    Highlighter * const m_higl;
    MainWin * const     m_mainWin;
    bool                m_isModified = false;

    BrushStyleListModel * m_brushStyleModel = nullptr;
    QDialogButtonBox  * m_cmdButs = nullptr;
    QPlainTextEdit    * m_sampleWid = nullptr;
    QPushButton       * m_bgColBut = nullptr;
    QComboBox         * m_bgStyleBut = nullptr;
    QPushButton       * m_fgColBut = nullptr;
    QComboBox         * m_fgStyleBut = nullptr;
    QPushButton       * m_olColBut = nullptr;
    QPushButton       * m_curFontBut = nullptr;
    QSpinBox          * m_fontSizeBox = nullptr;
    QCheckBox         * m_boldChkb = nullptr;
    QCheckBox         * m_italicChkb = nullptr;
    QCheckBox         * m_underlineChkb = nullptr;
    QCheckBox         * m_overlineChkb = nullptr;
    QCheckBox         * m_strikeoutChkb = nullptr;
};

#endif /* _DLG_MARKUP_H */

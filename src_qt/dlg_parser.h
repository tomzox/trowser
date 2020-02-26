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
#ifndef _DLG_PARSER_H
#define _DLG_PARSER_H

#include <QWidget>
#include <QMainWindow>

#include "parse_frame.h"

class QAbstractButton;
class QCloseEvent;
class QDialogButtonBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;

class MainWin;
class MainText;

// ----------------------------------------------------------------------------

class DlgParser : public QMainWindow
{
    Q_OBJECT

public:
    DlgParser(MainWin * mainWin, MainText * mainText, const ParseSpec& spec);
    ~DlgParser();
    void getSpec(ParseSpec& spec) const { spec = m_spec; }

signals:
    void dlgClosed(bool dataChanged);

private:
    virtual void closeEvent(QCloseEvent *) override;
    void cmdOk(bool);
    void cmdTest(bool);
    void updateWidgetState();
    void getInputFromWidgets();
    QString validateInput(const ParseSpec& spec, bool checkLabels);

private:
    MainWin * const     m_mainWin;
    MainText * const    m_mainText;
    const ParseSpec     m_specOrig;
    ParseSpec           m_spec;
    bool                m_closed = false;

    QLineEdit	      * m_valPatEnt = nullptr;
    QLineEdit	      * m_valHeadEnt = nullptr;
    QCheckBox	      * m_valDeltaChk = nullptr;
    QSpinBox	      * m_rangeEnt = nullptr;
    QLineEdit	      * m_frmPatEnt = nullptr;
    QCheckBox	      * m_frmFwdChk = nullptr;
    QCheckBox	      * m_frmCaptureChk = nullptr;
    QLineEdit	      * m_frmHeadEnt = nullptr;
    QCheckBox	      * m_frmDeltaChk = nullptr;
};

#endif /* _DLG_PARSER_H */

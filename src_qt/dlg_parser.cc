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
 * This module implements a configuration dialog for "custom columns" in the
 * search list. This dialogs allows entering regular expressions ("patterns")
 * that are then applied for extracting data from adjacent text lines around
 * each line displayed in the search list. When enabled, extracted data is then
 * displayed in two optional columns within the search list.
 *
 * For example, this can be used to display a timestamp or timestamp delta for
 * each line, so that one can determine easily if two adjacent lines in the
 * search list actually are related. Or, for event or interrupt driven
 * applications, the parser can be used to search backwards for the last
 * message that indicates event reception and extract an event ID. Thus all
 * lines related to the same event can be identified easily.
 *
 * The module incorporates a small modal dialog for displaying test results.
 * This allows applying the parser to single selected lines before saving,
 * plus getting feed back from where the extracted data originates. This helps
 * the user in debugging the search patterns.
 */

#include <QWidget>
#include <QStyle>
#include <QApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextStream>

#include <cstdio>
#include <string>
#include <memory>

#include "main_win.h"
#include "main_text.h"
#include "parse_frame.h"
#include "dlg_parser.h"

// ----------------------------------------------------------------------------

/**
 * This helper class implements a modal dialog that displays results of a
 * test-run of the parser on a single line. The information consists of two
 * parts: At the top of the dialog the extracted data is shown as it would
 * appear in the search list columns. In the text box at the bottom, the
 * dialog shows the lines from where the data was extracted.
 */
class DlgParserTest : public QDialog
{
public:
    DlgParserTest(QWidget * parent);
    virtual ~DlgParserTest() = default;
    void showResults(const ParseSpec& spec, MainText * mainText);
private:
    void insertLine(MainText * mainText, int line, const QString& desc);
    void insertText(const QString& desc);
private:
    QLabel              * m_lab = nullptr;
    QPlainTextEdit      * m_txtWid = nullptr;
};

/**
 * This constructor function creates the dialog. Afterward the caller should
 * call the showResults(), which will fill in data and make it visible.
 */
DlgParserTest::DlgParserTest(QWidget * parent)
    : QDialog(parent, Qt::Dialog)
{
    this->setWindowTitle("trowser - column configuration test results");

    auto layout_grid = new QGridLayout(this);
    QIcon tmpIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = style()->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, this);

    auto icon = new QLabel(this);
        icon->setPixmap(tmpIcon.pixmap(iconSize, iconSize));
        layout_grid->addWidget(icon, 0, 0);
    m_lab = new QLabel("Test results", this);
        layout_grid->addWidget(m_lab, 0, 1);

    m_txtWid = new QPlainTextEdit(this);
        m_txtWid->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout_grid->addWidget(m_txtWid, 1, 0, 1, 2);

    auto cmdBut = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
        connect(cmdBut->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &QDialog::accept);
        layout_grid->addWidget(cmdBut, 2, 0, 1, 2);
}

/**
 * This interface function runs the parser with the given configuration on the
 * line currently selected in the main window (i.e. current cursor position).
 * Results are inserted into the text widgets in the dialog, then the dialog
 * is executed modally. The function returns only after the user has clicked "OK".
 */
void DlgParserTest::showResults(const ParseSpec& spec, MainText * mainText)
{
    int line = mainText->textCursor().blockNumber();

    ParseFramePtr parser = ParseFrame::create(mainText->document(), spec);
    QString matchVal = parser->parseFrame(line, 0).toHtmlEscaped();
    QString matchFrm = parser->parseFrame(line, 1).toHtmlEscaped();
    if (matchVal.isEmpty()) matchVal = "<EM>[empty string]</EM>";
    if (matchFrm.isEmpty()) matchFrm = "<EM>[empty string]</EM>";

    QString msg1;
    QTextStream bldMsg1(&msg1);
    if (spec.m_frmPat.isEmpty() || !spec.m_frmCapture)
        bldMsg1 << "<P>The result returned for the selected line "
                   "is:</P><UL><LI><B>" << matchVal << "</B></UL>\n";
    else
        bldMsg1 << "<P>The results returned for the selected line "
                   "are: <UL><LI><B>" << matchVal << "</B>\n"
                   "<LI><B>" << matchFrm << "</B></UL>\n";
    bldMsg1 << "<P>The text box below shows the line numbers "
               "and respective text from which these results were derived.</P>";

    m_lab->setText(msg1);

    int maxLine = mainText->document()->lastBlock().blockNumber();
    ParseFrameInfo info = parser->getMatchInfo();

    if (!spec.m_frmPat.isEmpty())
    {
        if (info.frmStart != -1)
            insertLine(mainText, info.frmStart, "Preceding frame boundary");
        else if (spec.m_frmCapture == false)
            insertText("Preceding frame boundary: not reached");
        else
            insertText("Preceding frame boundary: no match within range limit");
    }

    if (spec.m_frmFwd && (info.valMatch > line))
        insertLine(mainText, line, "Selected line (i.e. origin of search)");

    if (info.valMatch >= 0)
        insertLine(mainText, info.valMatch, "Value extraction");
    else if (info.frmStart != -1)
        insertText("Value extraction: no match before reaching frame boundary");
    else
        insertText("Value extraction: no match within range limit");

    if ( !(spec.m_frmFwd && (info.valMatch > line)) )  // if not inserted already above
        insertLine(mainText, line, "Selected line (i.e. origin of search)");

    if (!spec.m_frmPat.isEmpty() && spec.m_frmFwd)
    {
        if (info.frmEnd < 0)
            insertText("Following frame boundary: no match within range limit");
        else if (info.frmEnd > maxLine)
            insertText("Following frame boundary: no match until end of document");
        else
            insertLine(mainText, info.frmEnd, "Following frame boundary");
    }

    m_txtWid->setReadOnly(true);
    this->exec();
}

void DlgParserTest::insertLine(MainText * mainText, int line, const QString& desc)
{
    Q_ASSERT (line >= 0);
    QTextCursor c(mainText->document()->findBlockByNumber(line));

    m_txtWid->setCurrentCharFormat(QTextCharFormat());
    m_txtWid->appendPlainText(QString::number(line) + ": " + desc);

    m_txtWid->setCurrentCharFormat(c.charFormat());
    m_txtWid->appendPlainText(c.block().text() + "\n");
}

void DlgParserTest::insertText(const QString& desc)
{
    m_txtWid->setCurrentCharFormat(QTextCharFormat());
    m_txtWid->appendPlainText(desc + "\n");
}


// ----------------------------------------------------------------------------

/**
 * This constructor creates the frame parser configuration dialog.
 */
DlgParser::DlgParser(MainWin * mainWin, MainText * mainText, const ParseSpec& spec)
    : m_mainWin(mainWin)
    , m_mainText(mainText)
    , m_specOrig(spec)
    , m_spec(spec)
{
    this->setWindowTitle("Custom column configuration - trowser");
    auto central_wid = new QWidget();
    auto layout_top = new QVBoxLayout(central_wid);

    auto lab = new QLabel("<B>Define how to extract values from adjacent text lines "
                          "for display in a custom column of the search list.", central_wid);
        layout_top->addWidget(lab);

    //
    // Group #1: Simple backwards search (e.g. for previous timestamp)
    //
    auto frm = new QFrame(central_wid);
        frm->setFrameStyle(QFrame::Panel | QFrame::Raised);
    auto layout_f1 = new QGridLayout(frm);

    lab = new QLabel("Pattern for value extraction:", frm);
        lab->setToolTip("<P>Enter a regular expression in Perl syntax containing a capture "
                        "group (i.e. parenthesis). The value for the custom column is taken "
                        "from the first line matching the pattern, searching backwards.</P>"
                        "<P>Note the result is taken from the <B>first</B capture group. "
                        "Use non-capturing parenthesis <CODE>(?: ... )</CODE> for other "
                        "purposes.</P>");
        layout_f1->addWidget(lab, 0, 0);
    m_valPatEnt = new QLineEdit(frm);
        m_valPatEnt->setText(m_spec.m_valPat);
        layout_f1->addWidget(m_valPatEnt, 0, 1, 1, 2);

    lab = new QLabel("Label for column header:", frm);
        lab->setToolTip("<P>Defines the label shown in the header for your custom column.</P>");
        layout_f1->addWidget(lab, 1, 0);
    m_valHeadEnt = new QLineEdit(frm);
        m_valHeadEnt->setText(m_spec.m_valHeader);
        layout_f1->addWidget(m_valHeadEnt, 1, 1);

    m_valDeltaChk = new QCheckBox("Enable delta column", central_wid);
        m_valDeltaChk->setChecked(m_spec.m_valDelta);
        m_valDeltaChk->setToolTip("<P>Check this box if your pattern matches numerical values "
                                  "and you want to add a column showing the delta of each line's value "
                                  "to that of the same column of a manually selected line in the search list.</P>");
        layout_f1->addWidget(m_valDeltaChk, 1, 2);

    lab = new QLabel("Limit search range:", frm);
        lab->setToolTip("<P>If non-zero, the value limits the range of the lines that "
                        "are searched for a match on the given patterns.</P>");
        layout_f1->addWidget(lab, 3, 0);
    m_rangeEnt = new QSpinBox(central_wid);
        m_rangeEnt->setRange(1, 999999);
        m_rangeEnt->setSingleStep(100);
        m_rangeEnt->setValue(m_spec.m_range);
        layout_f1->addWidget(m_rangeEnt, 3, 1);
    layout_top->addWidget(frm);

    //
    // Group #2: Limited backwards & optionally forward search
    //
    frm = new QFrame(central_wid);
        frm->setFrameStyle(QFrame::Panel | QFrame::Raised);
    auto layout_f2 = new QGridLayout(frm);

    lab = new QLabel("Pattern for frame boundary:", frm);
        lab->setToolTip("<P>Enter a regular expression here if you want to limit the "
                        "backwards search for the value extraction pattern to a line "
                        "matching this pattern.</P><P>Leave this field entry to limit "
                        "only by the range value given above.</P>");
        layout_f2->addWidget(lab, 0, 0);
    m_frmPatEnt = new QLineEdit(frm);
        m_frmPatEnt->setText(m_spec.m_frmPat);
        connect(m_frmPatEnt, &QLineEdit::textChanged, [=](){ updateWidgetState(); });
        layout_f2->addWidget(m_frmPatEnt, 0, 1, 1, 2);

    m_frmFwdChk = new QCheckBox("Search back- and forward within frame for value extraction", frm);
        m_frmFwdChk->setChecked(m_spec.m_frmFwd);
        m_frmFwdChk->setToolTip("<P>Check this box for extending search for the value extraction "
                                "pattern from the complete range between two occurrences of "
                                "the frame boundary pattern.</P>");
        layout_f2->addWidget(m_frmFwdChk, 1, 1, 1, 2);

    m_frmCaptureChk = new QCheckBox("Extract value from boundary pattern", frm);
        m_frmCaptureChk->setChecked(m_spec.m_frmCapture);
        m_frmCaptureChk->setToolTip("<P>Check this box if your pattern contains a capture group "
                                    "(i.e. parenthesis) with a value that you want to show in a column.</P>");
        connect(m_frmCaptureChk, &QCheckBox::stateChanged, [=](){ updateWidgetState(); });
        layout_f2->addWidget(m_frmCaptureChk, 2, 1);
    lab = new QLabel("Label for column header:", frm);
        lab->setToolTip("<P>Defines the label shown in the header for your custom \"frame number\" column.</P>");
        layout_f2->addWidget(lab, 3, 0);
    m_frmHeadEnt = new QLineEdit(frm);
        m_frmHeadEnt->setText(m_spec.m_frmHeader);
        layout_f2->addWidget(m_frmHeadEnt, 3, 1);
    m_frmDeltaChk = new QCheckBox("Enable delta column", frm);
        m_frmDeltaChk->setChecked(m_spec.m_frmDelta);
        m_frmDeltaChk->setToolTip("<P>Check this box if your pattern matches numerical values "
                                  "and you want to add a column showing the delta of each line's value "
                                  "to that of the same column of a manually selected line in the search list.</P>");
        layout_f2->addWidget(m_frmDeltaChk, 3, 2);

    updateWidgetState();
    layout_top->addWidget(frm);

    auto cmdBut = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
                                       Qt::Horizontal, central_wid);
        auto but = cmdBut->addButton("Test", QDialogButtonBox::ActionRole);
        but->setToolTip("<P>Click here to test-run the pattern matching on the line "
                        "currently selected in the main window, so that you can see "
                        "the values that would be shown in custom column(s).</P>");
        connect(but, &QPushButton::clicked, this, &DlgParser::cmdTest);
        connect(cmdBut->button(QDialogButtonBox::Ok), &QPushButton::clicked,
                this, &DlgParser::cmdOk);
        connect(cmdBut->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
                [=](){ QCoreApplication::postEvent(this, new QCloseEvent()); });
        layout_top->addWidget(cmdBut);

    setCentralWidget(central_wid);
    this->show();
}


/**
 * This function is bound to destruction of the dialog window. The event is
 * also sent artificially upon the "Cancel" button. If the configuration was
 * modified the user is asked to confirm. Then the dialog is closed and the
 * owner of the dialog is notified; notification contains a flag indicating
 * that configuration changes shall not be applied.
 */
void DlgParser::closeEvent(QCloseEvent * event)
{
    if (!m_closed)
    {
        getInputFromWidgets();
        if ((m_spec != m_specOrig) && !m_closed)
        {
            auto answer = QMessageBox::question(this, "trowser", "Discard unsaved changes to parser configuration?",
                                                QMessageBox::Ok | QMessageBox::Cancel);
            if (answer != QMessageBox::Ok)
            {
                event->ignore();
                return;
            }
        }
        emit dlgClosed(false);
    }
    event->accept();
}


/**
 * This slot is connected to the OK button, which validates & saves the data
 * and if successful, closes the dialog. The dialog owner is notified that the
 * configuration can be extracted and the dialog be deleted.
 */
void DlgParser::cmdOk(bool)
{
    getInputFromWidgets();
    bool isModified = (m_spec != m_specOrig);

    if (isModified)
    {
        if (!validateInput(m_spec, true, "save"))
            return;
    }
    m_closed = true;
    this->close();

    emit dlgClosed(isModified);
}

/**
 * This slot is connected to the "Test" button, which opens a modal dialog
 * showing results of a test-run of the current configuration, using the
 * DlgParserTest helper class.
 */
void DlgParser::cmdTest(bool)
{
    getInputFromWidgets();
    if (validateInput(m_spec, false, "test"))
    {
        auto msgDlg = std::make_unique<DlgParserTest>(this);
        msgDlg->showResults(m_spec, m_mainText);
        // above function returns only after dialog is closed; dialog is deleted via scope
    }
}

/**
 * This internal helper function enables or disables entry widgets that depend
 * on content of other widgets.
 */
void DlgParser::updateWidgetState()
{
    bool enaFrm = false;
    bool enaCapt = false;

    if (!m_frmPatEnt->text().isEmpty())
    {
        enaFrm = true;

        if (m_frmCaptureChk->isChecked())
            enaCapt = true;
    }
    m_frmFwdChk->setEnabled(enaFrm);
    m_frmCaptureChk->setEnabled(enaFrm);
    m_frmHeadEnt->setEnabled(enaCapt);
    m_frmDeltaChk->setEnabled(enaCapt);
}

/**
 * This internal helper function polls all input widgets for current settings
 * and updates a configuration structure with these parameters. This function
 * has to be called by all functions that depend on current settings.
 */
void DlgParser::getInputFromWidgets()
{
    m_spec.m_valPat = m_valPatEnt->text();
    m_spec.m_valHeader = m_valHeadEnt->text();
    m_spec.m_valDelta = m_valDeltaChk->isChecked();
    m_spec.m_range = m_rangeEnt->value();

    m_spec.m_frmPat = m_frmPatEnt->text();
    m_spec.m_frmHeader = m_frmHeadEnt->text();
    m_spec.m_frmFwd = m_frmFwdChk->isChecked();
    m_spec.m_frmCapture = m_frmCaptureChk->isChecked();
    m_spec.m_frmDelta = m_frmDeltaChk->isChecked();
}

/**
 * This function checks validity and consistency of current parameters. If a
 * problem is found the user is notified via message and failure is indicated
 * to the calling function.
 */
bool DlgParser::validateInput(const ParseSpec& spec, bool checkLabels, const QString& opDesc)
{
    QString msg;
    QRegularExpression valExpr(spec.m_valPat);
    QRegularExpression frmExpr(spec.m_frmPat);

    if (spec.m_valPat.isEmpty())
    {
        msg = "The value extraction pattern field is empty.";
    }
    else if (!valExpr.isValid())
    {
        msg = QString("The value extraction pattern is invalid: ") + valExpr.errorString();
    }
    else if (!spec.m_frmPat.isEmpty() && !frmExpr.isValid())
    {
        msg = QString("The frame boundary pattern is invalid: ") + frmExpr.errorString();
    }
    else if (checkLabels)
    {
        if (spec.m_valHeader.isEmpty())
        {
            msg = QString("The column header label is empty.");
        }
        else if (   !spec.m_frmPat.isEmpty()
                 && spec.m_frmHeader.isEmpty() && spec.m_frmCapture)
        {
            msg = QString("The column header for frame label is empty.");
        }
    }

    if (!msg.isEmpty())
    {
        QMessageBox::critical(this, "trowser",
                              "Cannot " + opDesc +" this configuration due to validation error: " + msg,
                              QMessageBox::Ok);
        return false;
    }

    if (spec.m_valPat.indexOf('(') < 0)
    {
        auto answer = QMessageBox::warning(this, "trowser",
                              "Extraction pattern contains no parenthesis, so that capturing will fail most likely. "
                              "For capturing the entire matching text, please enclose the pattern in parenthesis.",
                              QMessageBox::Ignore | QMessageBox::Cancel);
        return (answer == QMessageBox::Ignore);
    }
    if (   !spec.m_frmPat.isEmpty() && spec.m_frmCapture
        && spec.m_frmPat.indexOf('(') < 0)
    {
        auto answer = QMessageBox::warning(this, "trowser",
                              "You have enabled value extraction from the frame boundary pattern, "
                              "yet it contains no parenthesis. Thus capturing will fail most likely. "
                              "For capturing the entire matching text, please enclose the pattern in parenthesis.",
                              QMessageBox::Ignore | QMessageBox::Cancel);
        return (answer == QMessageBox::Ignore);
    }
    return true;
}

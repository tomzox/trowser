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

#include <QApplication>
#include <QWidget>
#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFile>
#include <QTimer>

#include <cstdio>
#include <string>

#include "main_win.h"
#include "main_text.h"
#include "load_pipe.h"


// ----------------------------------------------------------------------------

LoadPipeWorker::LoadPipeWorker()
    : QObject(nullptr)
    , m_stream(this)
    , m_timer(this)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, [=](){ loadNextChunk(); });

    if (m_stream.open(0, QIODevice::ReadOnly | QIODevice::Text) == false)
    {
        emit dataComplete(true, m_stream.errorString());
    }
}

void LoadPipeWorker::startLoading(LoadMode loadMode, size_t loadSize)
{
    m_loadMode = loadMode;
    m_loadTarget = loadSize;
    m_loadDone = 0;
    m_isActive = true;
    m_timer.start();
}

void LoadPipeWorker::reconfLoading(LoadMode loadMode, size_t loadSize)
{
    if (m_isActive)
    {
        m_loadMode = loadMode;
        m_loadTarget = loadSize;

        if ((m_loadMode == LoadMode::Head) && (m_loadDone >= m_loadTarget))
        {
            m_isActive = false;
            m_timer.stop();
            emit dataComplete(false, "");
        }
    }
}

void LoadPipeWorker::pauseLoading()
{
    m_isActive = false;
    m_timer.stop();
}

void LoadPipeWorker::loadNextChunk()
{
    Q_ASSERT(m_isActive);
    int bufSize = LOAD_CHUNK_SIZE;
    if ((m_loadMode == LoadMode::Head) && (m_loadDone + LOAD_CHUNK_SIZE > m_loadTarget))
        bufSize = m_loadTarget - m_loadDone;

    char tmpBuf[LOAD_CHUNK_SIZE];

    qint64 rdSize = m_stream.read(tmpBuf, bufSize);
    if (rdSize > 0)
    {
        Q_ASSERT(rdSize <= bufSize);

        // FIXME this makes a copy of tmpBuf
        QByteArray buf(tmpBuf, rdSize);
        m_loadDone += rdSize;
        emit dataBuffered(buf);

        if ((m_loadMode == LoadMode::Head) && (m_loadDone >= m_loadTarget))
        {
            m_isActive = false;
            emit dataComplete(false, "");
        }
        else
            m_timer.start();
    }
    else
    {
        m_isActive = false;
        emit dataComplete(true, ((rdSize < 0) ? m_stream.errorString() : ""));
    }
}


// ----------------------------------------------------------------------------

LoadPipe::LoadPipe(MainWin * mainWin, MainText * mainText, LoadMode loadMode, size_t bufSize)
    : QDialog(mainWin, Qt::Dialog)
    , m_mainWin(mainWin)
    , m_mainText(mainText)
    , m_loadMode(loadMode)
    , m_bufSize(bufSize)
{
    if (setvbuf(stdin, nullptr, _IONBF, 0) != 0)
        qWarning("Failed to set STDIN unbuffered: %s", strerror(errno));

    createDialog();
    updateStatus();

    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<LoadMode>("LoadMode");

    // thread object needs to be on heap as it may not terminate prior to exit
    m_workerThread = new QThread();

    m_workerObj = new LoadPipeWorker();
    m_workerObj->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_workerObj, &QObject::deleteLater);
    connect(this, &LoadPipe::workerStart, m_workerObj, &LoadPipeWorker::startLoading);
    connect(this, &LoadPipe::workerReconfigure, m_workerObj, &LoadPipeWorker::reconfLoading);
    connect(this, &LoadPipe::workerPause, m_workerObj, &LoadPipeWorker::pauseLoading);
    connect(m_workerObj, &LoadPipeWorker::dataBuffered, this, &LoadPipe::signalDataBuffered);
    connect(m_workerObj, &LoadPipeWorker::dataComplete, this, &LoadPipe::signalDataComplete);
    m_workerThread->start();

    m_workerActive = true;
    emit workerStart(m_loadMode, m_bufSize);
}

LoadPipe::~LoadPipe()
{
    if (m_workerActive)
        emit workerPause();
    m_workerObj->deleteLater();
    //m_workerThread gets deleted via "finished" signal
}

void LoadPipe::continueReading()
{
    this->setVisible(true);
    updateStatus();
    updateWorker();
}


/**
 * This function opens the "Loading from STDIN" status dialog.
 */
void LoadPipe::createDialog()
{
    this->setWindowTitle("Loading from STDIN...");

    auto f1 = this;
    auto layout_grid = new QGridLayout(f1);
    int row = -1;

    auto lab = new QLabel("<B>Please wait while buffering data from the input stream...\n<br>"
                          "Select between loading from head or tail of the stream.", f1);
        layout_grid->addWidget(lab, ++row, 0, 1, 3);
    // spacer row
        layout_grid->setRowMinimumHeight(++row, 10);

    m_loadProgress = new QProgressBar(f1);
        m_loadProgress->setOrientation(Qt::Horizontal);
        m_loadProgress->setTextVisible(false);
        m_loadProgress->setMinimum(0);
        m_loadProgress->setMaximum(1000);
        layout_grid->addWidget(m_loadProgress, ++row, 1, 1, 3);

    lab = new QLabel("Loaded data:", f1);
        layout_grid->addWidget(lab, ++row, 0);
    m_labReadLen = new QLabel("", f1);
        m_labReadLen->setToolTip("<P>This value is the amount of data already read "
                                 "from the input stream. In \"tail\" mode this value "
                                 "may grow larger than the buffer size, as excess data "
                                 "is being discarded.");
        layout_grid->addWidget(m_labReadLen, row, 1, 1, 2);

    lab = new QLabel("Buffered data:", f1);
        layout_grid->addWidget(lab, ++row, 0);
    m_labBufLen = new QLabel("", f1);
        m_labBufLen->setToolTip("<P>This value is the amount of data currently in the "
                                "buffer. The value is limited to the buffer size "
                                "configured below.");
        layout_grid->addWidget(m_labBufLen, row, 1, 1, 2);

    lab = new QLabel("Buffer size:", f1);
        lab->setToolTip("<P>The buffer size determines the amount of text that will be "
                        "loaded into the main text window. If your input stream provides "
                        "more data, loading will either stop (\"head\" mode), or excess "
                        "data will be discarded (\"tail\" mode).");
        layout_grid->addWidget(lab, ++row, 0);
    auto spox = new QSpinBox(f1);
        spox->setRange(1, 999);
        spox->setSingleStep(1);
        spox->setSuffix(" MByte");
        spox->setAlignment(Qt::AlignRight);
        int bufMB = (m_bufSize + (1024*1024-1)) / (1024*1024);
        spox->setValue((bufMB != 0) ? bufMB : 1);
        connect(spox, QOverload<int>::of(&QSpinBox::valueChanged), this, &LoadPipe::bufSizeChanged);
        layout_grid->addWidget(spox, row, 1, 1, 2);

    lab = new QLabel("Buffering mode:", f1);
        layout_grid->addWidget(lab, ++row, 0);
    auto radb = new QRadioButton("Head", f1);
        radb->setToolTip("<P>Loading \"head\" of the input stream means loading"
                         "the <b>first</b> N MBytes of data read (where \"N\" is "
                         "the value configured as buffer size.)");
        radb->setChecked(m_loadMode == LoadMode::Head);
        connect(radb, &QRadioButton::toggled, [=](bool){ loadModeChanged(LoadMode::Head); });
        layout_grid->addWidget(radb, row, 1);
    radb = new QRadioButton("Tail", f1);
        radb->setToolTip("<P>Loading \"tail\" of the input stream means keep "
                         "reading until the end of the stream and then load "
                         "the <b>last</b> N MBytes of data read (where \"N\" is "
                         "the value configured as buffer size.)");
        radb->setChecked(m_loadMode == LoadMode::Tail);
        connect(radb, &QRadioButton::toggled, [=](bool){ loadModeChanged(LoadMode::Tail); });
        layout_grid->addWidget(radb, row, 2);

#if 0
    lab = new QLabel("Close file:", f1);
        layout_grid->addWidget(lab, ++row, 0);
    auto chkb = new QCheckButton("close after read", f1);
        //variable=opt_file_close
        layout_grid->addWidget(chkb, row, 1, 1, 2);
#endif

    // Note button texts are modified to Abort/Ok while reading from file is stopped
    auto cmdButs = new QDialogButtonBox(QDialogButtonBox::Cancel |
                                        QDialogButtonBox::Ok,
                                        Qt::Horizontal, this);
    m_butStop = cmdButs->button(QDialogButtonBox::Cancel);
    m_butOk = cmdButs->button(QDialogButtonBox::Ok);
        m_butStop->setText("Stop");
        m_butOk->setEnabled(false);
        connect(m_butStop, &QPushButton::clicked, this, &LoadPipe::cmdStop);
        connect(m_butOk, &QPushButton::clicked, this, &LoadPipe::cmdOk);
        layout_grid->addWidget(cmdButs, ++row, 0, 1, 3);

    this->setModal(true);
    this->show();
}


/**
 * This function is bound to destruction of the dialog window. The event is
 * also sent artificially upon the "Cancel" button.
 */
void LoadPipe::closeEvent(QCloseEvent * event)
{
    event->ignore();
}

/**
 */
void LoadPipe::cmdStop(bool)
{
    if (m_workerActive)
    {
        emit workerPause();
        m_workerActive = false;
    }

    this->setVisible(false);
    emit pipeLoaded();
}


/**
 */
void LoadPipe::cmdOk(bool)
{
    this->setVisible(false);
    emit pipeLoaded();
}


void LoadPipe::loadModeChanged(LoadMode newMode)
{
    m_loadMode = newMode;
    updateStatus();
    updateWorker();
}

void LoadPipe::bufSizeChanged(int newSizeMB)
{
    int64_t newSize = int64_t(newSizeMB) * 1024*1024;

    // apply possible change of buffer mode and limit by the user
    if (std::abs(int64_t(newSize - m_bufSize)) >= 1024*1024)
    {
        m_bufSize = newSize;
        limitData(false);
        updateStatus();
        updateWorker();
    }
}

void LoadPipe::signalDataBuffered(QByteArray data)
{
    m_readTotal += data.size();
    m_readBuffered += data.size();

    m_dataBuf.push_back(data);
    limitData(false);
    updateStatus();
}

void LoadPipe::signalDataComplete(bool isEof, QString errorMsg)
{
    m_isEof = isEof;
    m_workerActive = false;
    updateStatus();

    if (!errorMsg.isEmpty())
    {
        QMessageBox::critical(this, "trowser", "Error while reading input stream: " + errorMsg, QMessageBox::Ok);
    }
    // may be needed after crossing case with reconfguration
    updateWorker();
}

void LoadPipe::updateWorker()
{
    if (!m_isEof)
    {
        if (m_workerActive)
        {
            emit workerReconfigure(m_loadMode, m_bufSize);
        }
        else if (m_loadMode == LoadMode::Tail)
        {
            emit workerStart(m_loadMode, m_bufSize);
            m_workerActive = true;
        }
        else if (m_readBuffered < m_bufSize)
        {
            emit workerStart(m_loadMode, m_bufSize - m_readBuffered);
            m_workerActive = true;
        }
    }
}

void LoadPipe::updateStatus()
{
    for (auto& v : { std::make_pair(m_readTotal, m_labReadLen),
                     std::make_pair(m_readBuffered, m_labBufLen) })
    {
        if ((v.first >= 1000000) || (v.first > 4 * m_bufSize))
        {
            char strBuf[100];
            snprintf(strBuf, sizeof(strBuf), "%2.1f MByte", (double)v.first / 0x100000);
            strBuf[sizeof(strBuf) - 1] = 0;
            v.second->setText(strBuf);
        }
        else
            v.second->setText(QString::number(v.first) + " Bytes");
    }

    if (!m_isEof && (m_readBuffered < m_bufSize))  // not ==
    {
        m_loadProgress->setValue(double(1000) * m_readBuffered / m_bufSize);
        m_loadProgress->setMaximum(1000);
    }
    else if (!m_isEof && (m_loadMode == LoadMode::Tail))  // implies readBuffered >= bufSize
    {
        m_loadProgress->setValue(0);
        m_loadProgress->setMaximum(0);
    }
    else
    {
        m_loadProgress->setMaximum(1000);
        m_loadProgress->setValue(1000);
    }

    bool bufOk = (m_isEof || ((m_loadMode == LoadMode::Head) && (m_readBuffered >= m_bufSize)));
    m_butOk->setEnabled(bufOk);
    m_butStop->setEnabled(!bufOk);
}

/**
 * This function discards data in the load buffer queue if the length
 * limit is exceeded.  The buffer queue is an array of character strings
 * (each string the result of a "read" command.)  The function is called
 * after each read in tail mode, so it must be efficient (i.e. esp. avoid
 * copying large buffers.)
 */
void LoadPipe::limitData(bool exact)
{
    if ((m_loadMode == LoadMode::Tail) && (m_readTotal >= m_bufSize))
    {
        // calculate how much data must be discarded
        ssize_t rest = m_readBuffered - m_bufSize;

        // unhook complete data buffers from the queue
        while ((rest > 0) && !m_dataBuf.empty() && (m_dataBuf[0].size() <= rest))
        {
            size_t buflen = m_dataBuf[0].size();
            rest -= buflen;
            m_readBuffered -= buflen;
            m_dataBuf.erase(m_dataBuf.begin());
        }

        // truncate the last data buffer in the queue (only if exact limit is requested)
        if (exact && (rest > 0) && !m_dataBuf.empty())
        {
            m_dataBuf[0].remove(0, rest);
            m_readBuffered -= rest;
        }
    }
}


/**
 * This function closes the pipe-loading dialog window and inserts the loaded
 * text (if any) into the main window.
 */
void LoadPipe::getLoadedData(std::vector<QByteArray>& resultBuf)
{
    ssize_t rest = m_bufSize;

    // unhook complete data buffers from the queue
    while ((rest > 0) && !m_dataBuf.empty() && (m_dataBuf[0].size() <= rest))
    {
        size_t buflen = m_dataBuf[0].size();
        rest -= buflen;
        m_readBuffered -= buflen;

        resultBuf.push_back(m_dataBuf.front());
        m_dataBuf.erase(m_dataBuf.begin());
    }

    // partial copy of the last data buffer in the queue (only if exact limit is requested)
    if ((rest > 0) && !m_dataBuf.empty())  // implies size() > rest
    {
        QByteArray part(m_dataBuf[0]);
        part.remove(rest, m_dataBuf[0].size() - rest);

        m_dataBuf[0].remove(0, rest);
        m_readBuffered -= rest;
    }
}

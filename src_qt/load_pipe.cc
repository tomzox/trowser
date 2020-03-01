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
 * This module implements a dialog and background processing for reading data
 * from an input stream. Specifically the dialog is created when the user
 * specifies input file "-". The main purposes of the dialog are firstly
 * showing a progress bar during the delay of buffering data and secondly for
 * allowing selection between "head" and "tail" mode. In the first mode data is
 * read from the start of the stream up to a given limit. Then that portion of
 * text can be loaded into the main window; the used can later continue loading
 * more data via the File menu. In "tail" mode, the entire stream is read until
 * reaching EOF and then the specified amount of text from the end of the
 * stream is loaded into display. During the latter excess data is discarded
 * automatically so that not the entire data needs to be kept in memory.
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

/**
 * This helper class is instantiated in a separate thread for reading a text
 * document from an input stream (STDIN). A separate thread is required, as Qt
 * does not support non-blocking I/O for regular files.  The functionality in
 * this class is reduced the the minimum required, namely reading the stream in
 * chunks of 64kB and forwarding that data to the main thread via signals.
 * Loading stops automatically after a limit given by the controlling main
 * thread, or latest upon reaching EoF. The protocol between worker and
 * controller instances is entirely message based, i.e. not using shared
 * memory.
 *
 * Three signals are used for controlling the instance. Initially the instance
 * is in inactive mode and will not load any data. (1) The start message
 * contains mode and size parameters: In "tail" mode data is loaded until
 * reaching EOF; In Head mode the given amount of data is forwarded, then the
 * worker instance automatically switches into inactive mode. When entering
 * inactive mode, the controller is notified. (2) The reconfiguration message
 * allows changing the two parameters of the start message while still active.
 * The message is ignored when already inactive (crossing case; in that case
 * the controller will receive a completion message (see below) and shall
 * follow-up with a "start" request if still more data is needed). Thus in
 * "head" mode the newly given limit replaces the old limit. If already more
 * data was loaded, the worker becomes inactive immediately and notifies the
 * controller.  (3) The stop message immediately discontinues loading and
 * notifies the controller. NOTE: Since the worker uses blocking read on the
 * input stream, it cannot react on new messages until that read returns.
 * Specifically, new messages are evaluated only after forwarding the last read
 * block of data.
 *
 * Two signals are produced by the worker instance: (1) The data message is
 * sent for each read operation. The message contains a copy of the data. (2) A
 * complettion indication message is sent when entering inactive mode. This
 * message contains an indication of EOF and an error description when reading
 * failed for other reasons than EOF.
 */
LoadPipeWorker::LoadPipeWorker(QThread * thread)
    : QObject(nullptr)
    , m_thread(thread)
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

/**
 * This destructor has to be connected to the "finished" signal of the
 * associated thread, which is initiated by the controller invoking the quit()
 * slot. The destructor arranges for deletion of the thread object (which is
 * not possible before the thread has terminated). Note as the thread uses
 * blocking I/O, it is possible that the worker object is actually not
 * destroyed upon exit of the application; this causes no issue as the thread
 * will be terminated by the OS anyway.
 */
LoadPipeWorker::~LoadPipeWorker()
{
    m_thread->deleteLater();
}

/**
 * This slot handles the "start" message sent by the controller. The given
 * parameters are copied and reading of data is activated by starting a
 * zero-delay timer. (A timer needs to be used so that control messages can be
 * processed between reading chunks of data.)
 */
void LoadPipeWorker::startLoading(LoadMode loadMode, size_t loadSize)
{
    m_loadMode = loadMode;
    m_loadTarget = loadSize;
    m_loadDone = 0;
    m_isActive = true;
    m_timer.start();
}

/**
 * This slot handles the "reconfiguration" message sent by the controller. The
 * given parameters replace the ones of the start message. When the new
 * parameters indicate a lower limit for "head" mode, the timer for scheduling
 * data reading is stopped immediately and controller is notified. The message
 * is ignored if not active.
 */
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

/**
 * This slot handles the "pause" message sent by the controller. Reading data
 * is stopped immediately and the controller is notified. If not in active
 * mode, the message is ignored (as the controller will already have received a
 * completion indication message).
 */
void LoadPipeWorker::pauseLoading()
{
    if (m_isActive)
    {
        m_isActive = false;
        m_timer.stop();
        emit dataComplete(false, "");
    }
}

/**
 * This slot handles expire of the internal timer that drives reading of data.
 * The function simply reads a chunk of data and forwards it to the controller.
 * When the limit indicated by the controller is reached, or an I/O error is
 * detected, the worker enters inactive mode and notifies the controller. Else
 * the timer is restarted.
 */
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

/**
 * This class implements both a dialog window, and the controller instance for
 * the worker thread. The dialog is shown always when starting to read data
 * from an input stream. The dialog showns current status (i.e. progress and
 * amount of data loaded) and allows the user to modify buffer size and
 * head/tail modes.  The initial values for these parameters can be specified
 * on the command line.  When all data is loaded, the dialog waits for the user
 * to press "OK".  Alternatively the user can stop loading at any time using
 * the respective button; in that case only the data aready loaded from the
 * input strea, up to that time is forwarded into the text display and the
 * background worker is stopped. While loading is stopped, the dialog is made
 * invisible, but not destroyed. It will be shown again when the user requests
 * loading more data.
 */
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

    this->setModal(true);
    this->show();

    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<LoadMode>("LoadMode");

    // thread object needs to be on heap as it may not terminate prior to exit
    m_workerThread = new QThread();

    // create a worker instance
    m_workerObj = new LoadPipeWorker(m_workerThread);
    m_workerObj->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_workerObj, &QObject::deleteLater);
    // connect control messages sent to the worker
    connect(this, &LoadPipe::workerStart, m_workerObj, &LoadPipeWorker::startLoading);
    connect(this, &LoadPipe::workerReconfigure, m_workerObj, &LoadPipeWorker::reconfLoading);
    connect(this, &LoadPipe::workerPause, m_workerObj, &LoadPipeWorker::pauseLoading);
    // connect data and status indications sent to the controller
    connect(m_workerObj, &LoadPipeWorker::dataBuffered, this, &LoadPipe::signalDataBuffered);
    connect(m_workerObj, &LoadPipeWorker::dataComplete, this, &LoadPipe::signalDataComplete);
    m_workerThread->start();

    // start loading data
    m_workerActive = true;
    emit workerStart(m_loadMode, m_bufSize);
}

/**
 * This destructor is expected to be called only upon program termination, or
 * optionally after reaching EOF. The user frees all resources, except for the
 * worker thread
 */
LoadPipe::~LoadPipe()
{
    m_workerObj->disconnect(this);
    m_workerThread->quit();
    //m_workerObj gets deleted via QThread "finished" signal
    //m_workerThread gets deleted by destructor of m_workerObj
}

void LoadPipe::continueReading()
{
    this->setVisible(true);
    updateStatus();
    updateWorker();
}


/**
 * This function is used by the constructor for creating the "Loading from
 * STDIN" dialog.
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

    auto cmdButs = new QDialogButtonBox(QDialogButtonBox::Cancel |
                                        QDialogButtonBox::Ok,
                                        Qt::Horizontal, this);
    m_butStop = cmdButs->button(QDialogButtonBox::Cancel);
    m_butOk = cmdButs->button(QDialogButtonBox::Ok);
        m_butStop->setToolTip("<P>Abort loading and close the dialog, then load the already buffered text into the main window.</P>");
        m_butStop->setText("Stop");
        m_butOk->setEnabled(false);
        m_butOk->setToolTip("<P>Close this dialog and load the buffered text into the main window.</P>");
        connect(m_butStop, &QPushButton::clicked, this, &LoadPipe::cmdStop);
        connect(m_butOk, &QPushButton::clicked, this, &LoadPipe::cmdOk);
        layout_grid->addWidget(cmdButs, ++row, 0, 1, 3);
}


/**
 * This function is bound to destruction of the dialog window. As this is a
 * modal dialog, this should never happen, so the signal is ignored. The user
 * can instead close the dialog via "Stop" or "Ok" buttons.
 */
void LoadPipe::closeEvent(QCloseEvent * event)
{
    event->ignore();
}

/**
 * This slot is connected to clicks on the "Stop" button. The function aborts
 * loading of data, hides the dialog and notifies the owner of the dialog that
 * it is finished. See also description of the "OK" slot.
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
 * This slot is connected to clicks on the "OK" button. The function hides the
 * dialog and notifies the owner of the dialog that it is finished. The owner
 * then can retrieve the data from the buffer and forward it into the text
 * widget in the main window. The owner also should query the EOF status to
 * prevent further attempts of loading data when reaching EOF.
 */
void LoadPipe::cmdOk(bool)
{
    this->setVisible(false);
    emit pipeLoaded();
}


/**
 * This slot is connected to changes of the head/tail radio buttons. The
 * function reconfigures the worker object and updates display (which is needed
 * as representation of the progress bar may have to be adapted).
 */
void LoadPipe::loadModeChanged(LoadMode newMode)
{
    m_loadMode = newMode;
    updateStatus();
    updateWorker();
}

/**
 * This slot is connected to changes of the buffer size value via spinbox. The
 * function reconfigures the worker object and updates display (which is needed
 * as progress bar needs to be adjusted for the new target value).
 */
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

/**
 * This slot is connected to data indications sent by the worker object.  The
 * data is pushed to an internal queue and display status is updated.  The
 * latter may include enabling the "OK" button in case all requested data has
 * been loaded.
 */
void LoadPipe::signalDataBuffered(QByteArray data)
{
    m_readTotal += data.size();
    m_readBuffered += data.size();

    m_dataBuf.push_back(data);
    limitData(false);
    updateStatus();
}

/**
 * This slot is connected to completion indications sent by the worker object.
 * The main purpose is updating the "EOF" flag. Additionally, the function
 * handles crossing case of a preceding reconfiguration of buffer size or mode
 * and completion within the asynchronous worker thread. In such cases the
 * worker object may get restarted to load the remaining portion of data.
 */
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

/**
 * This internal helper function starts or reconfigures the worker object for
 * loading data according to current buffer size and mode parameters. It is called
 * upon initial start and whenever parameters are changed by the user.
 */
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

/**
 * This internal helper function updates the progress bar, statistics about
 * loaded data, and status of the mutually exclusive Stop/Ok buttons.
 */
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
 * This function is used in "tail" mode to discard excessive data at the
 * beginning of the load buffer queue.  The buffer queue is an array of raw
 * byte buffers of fixed size (i.e. each array element stores the result of one
 * "read" command.)
 */
void LoadPipe::limitData(bool exact)
{
    if ((m_loadMode == LoadMode::Tail) && (m_readTotal >= m_bufSize))
    {
        // calculate how much data must be discarded
        ssize_t rest = m_readBuffered - m_bufSize;

        // unhook complete data buffers from the queue
        while ((rest > 0) && !m_dataBuf.empty() && (rest >= m_dataBuf.front().size()))
        {
            size_t buflen = m_dataBuf.front().size();
            rest -= buflen;
            m_readBuffered -= buflen;
            m_dataBuf.pop_front();
        }

        // truncate the oldest data buffer in the queue to reach exact limit;
        // for performance reasons this is only done after end of loading
        if (exact && (rest > 0) && !m_dataBuf.empty())
        {
            m_dataBuf.front().remove(0, rest);
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

    // finalize trimming data for tail mode
    // done only now as data may still arrive from worker before reaching here
    limitData(true);
    resultBuf.reserve(m_dataBuf.size());

    // move complete data buffers from the queue into the result vector
    while ((rest > 0) && !m_dataBuf.empty() && (rest >= m_dataBuf.front().size()))
    {
        size_t buflen = m_dataBuf.front().size();
        rest -= buflen;
        m_readBuffered -= buflen;

        resultBuf.push_back(std::move(m_dataBuf.front()));
        m_dataBuf.pop_front();
    }

    // partial copy of the last data buffer in the queue (only if exact limit is requested)
    if ((rest > 0) && !m_dataBuf.empty())  // implies size() > rest
    {
        QByteArray part(m_dataBuf.front());
        part.remove(rest, m_dataBuf.front().size() - rest);

        m_dataBuf.front().remove(0, rest);
        m_readBuffered -= rest;
    }
}

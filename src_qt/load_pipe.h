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
#ifndef _LOAD_PIPE_H
#define _LOAD_PIPE_H

#include <QDialog>
#include <QThread>
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTimer>

#include <vector>

#include "main_win.h"

class QCloseEvent;
class QAbstractButton;
class QDialogButtonBox;
class QProgressBar;
class QLabel;
class QThread;

class MainWin;
class MainText;

// ----------------------------------------------------------------------------

class LoadPipeWorker : public QObject
{
    Q_OBJECT

public:
    LoadPipeWorker();

/* public slot */
    void startLoading(LoadMode loadMode, size_t loadSize);
    void reconfLoading(LoadMode loadMode, size_t loadSize);
    void pauseLoading();

signals:
    void dataBuffered(QByteArray data);
    void dataComplete(bool isEof, QString errorMsg);

private:
    void loadNextChunk();

private:
    static const size_t LOAD_CHUNK_SIZE = 64*1024;

    QFile               m_stream;
    QTimer              m_timer;
    LoadMode            m_loadMode = LoadMode::Head;
    size_t              m_loadTarget = 0;
    size_t              m_loadDone = 0;
    bool                m_isActive = false;
};

// ----------------------------------------------------------------------------

class LoadPipe : public QDialog
{
    Q_OBJECT

public:
    LoadPipe(MainWin * mainWin, MainText * mainText, LoadMode loadMode, size_t bufSize);
    ~LoadPipe();
    void continueReading();
    bool isEof() const { return m_isEof; }
    size_t getLoadBufferSize() const { return m_bufSize; }
    void getLoadedData(std::vector<QByteArray>& resultBuf);

private:
    virtual void closeEvent(QCloseEvent *) override;
    void createDialog();
    void cmdOk(bool);
    void cmdStop(bool);
    void loadModeChanged(LoadMode newMode);
    void bufSizeChanged(int newSize);

    void signalDataBuffered(QByteArray data);
    void signalDataComplete(bool isEof, QString errorMsg);
    void limitData(bool exact);
    void updateStatus();
    void updateWorker();

signals:
    void workerStart(LoadMode mode, size_t loadSize);
    void workerReconfigure(LoadMode mode, size_t loadSize);
    void workerPause();
    void pipeLoaded();

private:
    MainWin     * const m_mainWin;
    MainText    * const m_mainText;
    LoadMode            m_loadMode;
    size_t              m_bufSize;

    QProgressBar      * m_loadProgress = nullptr;
    QLabel            * m_labReadLen = nullptr;
    QLabel            * m_labBufLen = nullptr;
    QPushButton       * m_butStop = nullptr;
    QPushButton       * m_butOk = nullptr;

    QThread           * m_workerThread = nullptr;
    LoadPipeWorker    * m_workerObj = nullptr;
    bool                m_workerActive = false;

    std::vector<QByteArray> m_dataBuf;  // buffer for data read from file
    size_t m_readTotal = 0;       // number of bytes read from file; may grow beyond 32 bit!
    size_t m_readBuffered = 0;    // number of bytes read & still in buffer
    bool m_isEof = false;         // true once file.read() returned EOF
};

#endif // _LOAD_PIPE_H


#include "QtHttpClientWrapper.h"
#include "QtHttpRequest.h"
#include "QtHttpReply.h"
#include "QtHttpServer.h"
#include "QtHttpHeader.h"

#include <QCryptographicHash>
#include <QTcpSocket>
#include <QStringBuilder>
#include <QStringList>
#include <QDateTime>

const char       QtHttpClientWrapper::SPACE (' ');
const char       QtHttpClientWrapper::COLON (':');
const QByteArray QtHttpClientWrapper::CRLF  ("\r\n");

QtHttpClientWrapper::QtHttpClientWrapper (QTcpSocket * sock, QtHttpServer * parent)
    : QObject          (parent)
    , m_guid           ("")
    , m_parsingStatus  (AwaitingRequest)
    , m_sockClient     (sock)
    , m_currentRequest (Q_NULLPTR)
    , m_serverHandle   (parent)
{
    connect (m_sockClient, &QTcpSocket::readyRead, this, &QtHttpClientWrapper::onClientDataReceived);
}

QtHttpClientWrapper::~QtHttpClientWrapper (void) { }

QString QtHttpClientWrapper::getGuid (void) {
    if (m_guid.isEmpty ()) {
        m_guid = QString::fromLocal8Bit (
                     QCryptographicHash::hash (
                         QByteArray::number (quint64 (this)),
                         QCryptographicHash::Md5
                         ).toHex ()
                     );
    }
    return m_guid;
}

void QtHttpClientWrapper::onClientDataReceived (void) {
    if (m_sockClient != Q_NULLPTR) {
        while (m_sockClient->bytesAvailable ()) {
            const QByteArray line = m_sockClient->readLine ();
            switch (m_parsingStatus) { // handle parsing steps
                case AwaitingRequest: { // "command url version" × 1
                    const QString str = QString::fromUtf8 (line).trimmed ();
                    const QStringList parts = str.split (SPACE, Qt::SkipEmptyParts);
                    if (parts.size () == 3) {
                        const QString command = parts.at (0);
                        const QString url     = parts.at (1);
                        const QString version = parts.at (2);
                        if (version == QtHttpServer::HTTP_VERSION) {
                            //qDebug () << "Debug : HTTP"
                            //          << "command :" << command
                            //          << "url :"     << url
                            //          << "version :" << version;
                            m_currentRequest = new QtHttpRequest (this, m_serverHandle);
                            m_currentRequest->setUrl     (QUrl (url));
                            m_currentRequest->setCommand (command);
                            m_parsingStatus = AwaitingHeaders;
                        }
                        else {
                            m_parsingStatus = ParsingError;
                            //qWarning () << "Error : unhandled HTTP version :" << version;
                        }
                    }
                    else {
                        m_parsingStatus = ParsingError;
                        //qWarning () << "Error : incorrect HTTP command line :" << line;
                    }
                    break;
                }
                case AwaitingHeaders: { // "header: value" × N (until empty line)
                    QByteArray raw = line.trimmed ();
                    if (!raw.isEmpty ()) { // parse headers
                        const int pos = raw.indexOf (COLON);
                        if (pos > 0) {
                            const QByteArray header = raw.left (pos).trimmed ();
                            const QByteArray value  = raw.mid  (pos +1).trimmed ();
                            //qDebug () << "Debug : HTTP"
                            //          << "header :" << header
                            //          << "value :"  << value;
                            m_currentRequest->addHeader (header, value);
                            if (header == QtHttpHeader::ContentLength) {
                                bool ok = false;
                                const int len = value.toInt (&ok, 10);
                                if (ok) {
                                    m_currentRequest->addHeader (QtHttpHeader::ContentLength, QByteArray::number (len));
                                }
                            }
                        }
                        else {
                            m_parsingStatus = ParsingError;
                            qWarning () << "Error : incorrect HTTP headers line :" << line;
                        }
                    }
                    else { // end of headers
                        //qDebug () << "Debug : HTTP end of headers";
                        if (m_currentRequest->getHeader (QtHttpHeader::ContentLength).toInt () > 0) {
                            m_parsingStatus = AwaitingContent;
                        }
                        else {
                            m_parsingStatus = RequestParsed;
                        }
                    }
                    break;
                }
                case AwaitingContent: { // raw data × N (until EOF ??)
                    m_currentRequest->appendRawData (line);
                    //qDebug () << "Debug : HTTP"
                    //          << "content :" << m_currentRequest->getRawData ().toHex ()
                    //          << "size :"    << m_currentRequest->getRawData ().size  ();
                    if (m_currentRequest->getRawDataSize () == m_currentRequest->getHeader (QtHttpHeader::ContentLength).toInt ()) {
                        //qDebug () << "Debug : HTTP end of content";
                        m_parsingStatus = RequestParsed;
                    }
                    break;
                }
                default: { break; }
            }
            switch (m_parsingStatus) { // handle parsing status end/error
                case RequestParsed: { // a valid request has been fully parsed
                    QtHttpReply reply (m_serverHandle);
                    connect (&reply, &QtHttpReply::requestSendHeaders,
                             this, &QtHttpClientWrapper::onReplySendHeadersRequested);
                    connect (&reply, &QtHttpReply::requestSendData,
                             this, &QtHttpClientWrapper::onReplySendDataRequested);
                    emit m_serverHandle->requestNeedsReply (m_currentRequest, &reply); // allow app to handle request
                    m_parsingStatus = sendReplyToClient (&reply);
                    break;
                }
                case ParsingError: { // there was an error during one of parsing steps
                    m_sockClient->readAll (); // clear remaining buffer to ignore content
                    QtHttpReply reply (m_serverHandle);
                    reply.setStatusCode (QtHttpReply::BadRequest);
                    reply.appendRawData (QByteArrayLiteral ("<h1>Bad Request (HTTP parsing error) !</h1>"));
                    reply.appendRawData (CRLF);
                    m_parsingStatus = sendReplyToClient (&reply);
                    break;
                }
                default: { break; }
            }
        }
    }
}

void QtHttpClientWrapper::onReplySendHeadersRequested (void) {
    if (QtHttpReply * reply = qobject_cast<QtHttpReply *> (sender ())) {
        // HTTP Version + Status Code + Status Msg
        QByteArray data = (QtHttpServer::HTTP_VERSION %
                           SPACE %
                           QByteArray::number (reply->getStatusCode ()) %
                           SPACE %
                           QtHttpReply::getStatusTextForCode (reply->getStatusCode ()) %
                           CRLF);
        // Header name: header value
        if (reply->useChunked ()) {
            static const QByteArray CHUNKED ("chunked");
            reply->addHeader (QtHttpHeader::TransferEncoding, CHUNKED);
        }
        else {
            reply->addHeader (QtHttpHeader::ContentLength, QByteArray::number (reply->getRawDataSize ()));
        }
        const QList<QByteArray> & headersList = reply->getHeadersList ();
        foreach (const QByteArray & header, headersList) {
            data += (header %
                     COLON %
                     SPACE %
                     reply->getHeader (header) %
                     CRLF);
        }
        // empty line
        data += CRLF;
        m_sockClient->write (data);
        //m_sockClient->flush ();
    }
}

void QtHttpClientWrapper::onReplySendDataRequested (void) {
    if (QtHttpReply * reply = qobject_cast<QtHttpReply *> (sender ())) {
        // content raw data
        QByteArray data = reply->getRawData ();
        if (reply->useChunked ()) {
            data.prepend (QByteArray::number (data.size (), 16) % CRLF);
            data.append (CRLF);
            reply->resetRawData ();
        }
        // write to socket
        m_sockClient->write (data);
        //m_sockClient->flush ();
    }
}

QtHttpClientWrapper::ParsingStatus QtHttpClientWrapper::sendReplyToClient (QtHttpReply * reply) {
    if (reply != Q_NULLPTR) {
        if (!reply->useChunked ()) {
            reply->appendRawData (CRLF);
            // send all headers and all data in one shot
            reply->requestSendHeaders ();
            reply->requestSendData ();
        }
        else {
            // last chunk
            m_sockClient->write ("0");
            m_sockClient->write (CRLF);
            m_sockClient->write (CRLF);
            //m_sockClient->flush ();
        }
    }
    if (m_currentRequest != Q_NULLPTR) {
        static const QByteArray CLOSE ("close");
        if (m_currentRequest->getHeader (QtHttpHeader::Connection).toLower () == CLOSE) {
            // must close connection after this request
            m_sockClient->close ();
        }
        m_currentRequest->deleteLater ();
        m_currentRequest = Q_NULLPTR;
    }
    return AwaitingRequest;
}

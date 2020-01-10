/***************************************************************************
    begin                : Wed May 9 2012
    copyright            : (C) 2012 by Martin Sandsmark
    email                : martin.sandsmark@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDomDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <KAction>
#include <KLocalizedString>
#include <KActionCollection>
#include <KToggleAction>
#include <KConfigGroup>
#include <KDebug>

#include "lyricswidget.h"
#include "tag.h"
#include "actioncollection.h"


LyricsWidget::LyricsWidget(QWidget* parent): QTextBrowser(parent),
    m_networkAccessManager(new QNetworkAccessManager)
{
    setMinimumWidth(200);
    setReadOnly(true);
    setWordWrapMode(QTextOption::WordWrap);
    setOpenExternalLinks(true);
    
    KToggleAction *show = new KToggleAction(KIcon(QLatin1String("view-media-lyrics")),
                                            i18n("Show &Lyrics"), this);
    ActionCollection::actions()->addAction("showLyrics", show);
    connect(show, SIGNAL(toggled(bool)), this, SLOT(setShown(bool)));
    
    KConfigGroup config(KGlobal::config(), "LyricsWidget");
    bool shown = config.readEntry("Show", true);
    show->setChecked(shown);
    setVisible(shown);
}

LyricsWidget::~LyricsWidget()
{
    delete m_networkAccessManager;
    saveConfig();
}

void LyricsWidget::saveConfig()
{
    KConfigGroup config(KGlobal::config(), "LyricsWidget");
    config.writeEntry("Show", ActionCollection::action<KToggleAction>("showLyrics")->isChecked());
}


void LyricsWidget::playing(const FileHandle &file)
{
    setHtml("<i>Loading...</i>");
    
    QUrl listUrl("http://lyrics.wikia.com/api.php");
    listUrl.addQueryItem("action", "lyrics");
    listUrl.addQueryItem("func", "getSong");
    listUrl.addQueryItem("fmt", "xml");
    listUrl.addQueryItem("artist", file.tag()->artist());
    listUrl.addQueryItem("song", file.tag()->title());
    m_title = file.tag()->artist() + " &#8211; " + file.tag()->title();
    connect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveListReply(QNetworkReply*)));
    m_networkAccessManager->get(QNetworkRequest(listUrl));
}

void LyricsWidget::hideEvent(QHideEvent *)
{
    saveConfig();
}

void LyricsWidget::receiveListReply(QNetworkReply* reply)
{
    disconnect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveListReply(QNetworkReply*)));
    if (reply->error() != QNetworkReply::NoError) {
        kWarning() << "Error while fetching lyrics: " << reply->errorString();
        setHtml("<span style='color:red'>Error while retrieving lyrics!</span>");
        return;
    }

    QDomDocument document;
    document.setContent(reply);
    QString artist = document.elementsByTagName("artist").at(0).toElement().text();
    QString title = document.elementsByTagName("song").at(0).toElement().text();
    
    
    QUrl url("http://lyrics.wikia.com/api.php");
    url.addQueryItem("action", "query");
    url.addQueryItem("prop", "revisions");
    url.addQueryItem("rvprop", "content");
    url.addQueryItem("format", "xml");
    url.addQueryItem("titles", artist + ":" + title);
    connect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveLyricsReply(QNetworkReply*)));
    m_networkAccessManager->get(QNetworkRequest(url));
}

void LyricsWidget::receiveLyricsReply(QNetworkReply* reply)
{
    disconnect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(receiveLyricsReply(QNetworkReply*)));
    if (reply->error() != QNetworkReply::NoError) {
        kWarning() << "Error while fetching lyrics: " << reply->errorString();
        setHtml("<span style='color:red'>Error while retrieving lyrics!</span>");
        return;
    }
    
    QString content = QString::fromUtf8(reply->readAll());
    int lIndex = content.indexOf("&lt;lyrics&gt;");
    int rIndex = content.indexOf("&lt;/lyrics&gt;");
    if (lIndex == -1 || rIndex == -1) {
        kWarning() << Q_FUNC_INFO << "Unable to find lyrics in text";
        setText("No lyrics available.");
        return;
    }
    lIndex += 15; // We skip the tag
    content = content.mid(lIndex, rIndex - lIndex).trimmed();
    content.replace("\n", "<br />");
    //setText(content);
    setHtml("<h1>" + m_title + "</h1>" + 
            content + 
            "<br /><br /><i>Lyrics provided by <a href='http://lyrics.wikia.com/Lyrics_Wiki'>LyricWiki</a></i>");
}

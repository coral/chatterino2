#include "emotemanager.hpp"
#include "common.hpp"
#include "resources.hpp"
#include "util/urlfetch.hpp"
#include "windowmanager.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <memory>

#define TWITCH_EMOTE_TEMPLATE "https://static-cdn.jtvnw.net/emoticons/v1/{id}/{scale}.0"

using namespace chatterino::messages;

namespace chatterino {

EmoteManager::EmoteManager(WindowManager &_windowManager, Resources &_resources)
    : windowManager(_windowManager)
    , resources(_resources)
{
    // Note: Do not use this->resources in ctor
    pajlada::Settings::Setting<std::string> roomID(
        "/accounts/current/roomID", "", pajlada::Settings::SettingOption::DoNotWriteToJSON);

    roomID.getValueChangedSignal().connect([this](const std::string &roomID) {
        this->refreshTwitchEmotes(roomID);  //
    });
}

void EmoteManager::loadGlobalEmotes()
{
    this->loadEmojis();
    this->loadBTTVEmotes();
    this->loadFFZEmotes();
}

void EmoteManager::reloadBTTVChannelEmotes(const QString &channelName)
{
    printf("[EmoteManager] Reload BTTV Channel Emotes for channel %s\n", qPrintable(channelName));

    QString url("https://api.betterttv.net/2/channels/" + channelName);
    util::urlFetchJSON(url, [this, channelName](QJsonObject &rootNode) {
        EmoteMap &channelEmoteMap = this->bttvChannels[channelName];

        channelEmoteMap.clear();

        auto emotesNode = rootNode.value("emotes").toArray();

        QString linkTemplate = "https:" + rootNode.value("urlTemplate").toString();

        std::vector<std::string> codes;
        for (const QJsonValue &emoteNode : emotesNode) {
            QJsonObject emoteObject = emoteNode.toObject();

            QString id = emoteObject.value("id").toString();
            QString code = emoteObject.value("code").toString();
            // emoteObject.value("imageType").toString();

            QString link = linkTemplate;
            link.detach();

            link = link.replace("{{id}}", id).replace("{{image}}", "1x");

            auto emote = this->getBTTVChannelEmoteFromCaches().getOrAdd(id, [this, &code, &link] {
                return EmoteData(new LazyLoadedImage(*this, this->windowManager, link, 1, code,
                                                     code + "\nChannel BTTV Emote"));
            });

            this->bttvChannelEmotes.insert(code, emote);
            channelEmoteMap.insert(code, emote);
            codes.push_back(code.toStdString());
        }

        this->bttvChannelEmoteCodes[channelName.toStdString()] = codes;
    });
}

void EmoteManager::reloadFFZChannelEmotes(const QString &channelName)
{
    printf("[EmoteManager] Reload FFZ Channel Emotes for channel %s\n", qPrintable(channelName));

    QString url("http://api.frankerfacez.com/v1/room/" + channelName);

    util::urlFetchJSON(url, [this, channelName](QJsonObject &rootNode) {
        EmoteMap &channelEmoteMap = this->ffzChannels[channelName];

        channelEmoteMap.clear();

        auto setsNode = rootNode.value("sets").toObject();

        std::vector<std::string> codes;
        for (const QJsonValue &setNode : setsNode) {
            auto emotesNode = setNode.toObject().value("emoticons").toArray();

            for (const QJsonValue &emoteNode : emotesNode) {
                QJsonObject emoteObject = emoteNode.toObject();

                // margins
                int id = emoteObject.value("id").toInt();
                QString code = emoteObject.value("name").toString();

                QJsonObject urls = emoteObject.value("urls").toObject();
                QString url1 = "http:" + urls.value("1").toString();

                auto emote =
                    this->getFFZChannelEmoteFromCaches().getOrAdd(id, [this, &code, &url1] {
                        return EmoteData(new LazyLoadedImage(*this, this->windowManager, url1, 1,
                                                             code, code + "\nGlobal FFZ Emote"));
                    });

                this->ffzChannelEmotes.insert(code, emote);
                channelEmoteMap.insert(code, emote);
                codes.push_back(code.toStdString());
            }

            this->ffzChannelEmoteCodes[channelName.toStdString()] = codes;
        }
    });
}

ConcurrentMap<QString, twitch::EmoteValue *> &EmoteManager::getTwitchEmotes()
{
    return _twitchEmotes;
}

EmoteManager::EmoteMap &EmoteManager::getFFZEmotes()
{
    return ffzGlobalEmotes;
}

EmoteManager::EmoteMap &EmoteManager::getChatterinoEmotes()
{
    return _chatterinoEmotes;
}

EmoteManager::EmoteMap &EmoteManager::getBTTVChannelEmoteFromCaches()
{
    return _bttvChannelEmoteFromCaches;
}

ConcurrentMap<int, EmoteData> &EmoteManager::getFFZChannelEmoteFromCaches()
{
    return _ffzChannelEmoteFromCaches;
}

ConcurrentMap<long, EmoteData> &EmoteManager::getTwitchEmoteFromCache()
{
    return _twitchEmoteFromCache;
}

void EmoteManager::loadEmojis()
{
    QFile file(":/emojidata.txt");
    file.open(QFile::ReadOnly);
    QTextStream in(&file);

    uint unicodeBytes[4];

    while (!in.atEnd()) {
        // Line example: sunglasses 1f60e
        QString line = in.readLine();

        if (line.at(0) == '#') {
            // Ignore lines starting with # (comments)
            continue;
        }

        QStringList parts = line.split(' ');
        if (parts.length() < 2) {
            continue;
        }

        QString shortCode = parts[0];
        QString code = parts[1];

        QStringList unicodeCharacters = code.split('-');
        if (unicodeCharacters.length() < 1) {
            continue;
        }

        int numUnicodeBytes = 0;

        for (const QString &unicodeCharacter : unicodeCharacters) {
            unicodeBytes[numUnicodeBytes++] = QString(unicodeCharacter).toUInt(nullptr, 16);
        }

        EmojiData emojiData{
            QString::fromUcs4(unicodeBytes, numUnicodeBytes),  //
            code,                                              //
            shortCode,                                         //
        };

        this->emojiShortCodeToEmoji.insert(shortCode, emojiData);

        this->emojiFirstByte[emojiData.value.at(0)].append(emojiData);

        // TODO(pajlada): The vectors in emojiFirstByte need to be sorted by emojiData.code.length()
    }
}

void EmoteManager::parseEmojis(std::vector<std::tuple<EmoteData, QString>> &parsedWords,
                               const QString &text)
{
    int lastParsedEmojiEndIndex = 0;

    for (auto i = 0; i < text.length() - 1; i++) {
        const QChar character = text.at(i);

        if (character.isLowSurrogate()) {
            continue;
        }

        auto it = this->emojiFirstByte.find(character);
        if (it == this->emojiFirstByte.end()) {
            // No emoji starts with this character
            continue;
        }

        const QVector<EmojiData> possibleEmojis = it.value();

        int remainingCharacters = text.length() - i;

        EmojiData matchedEmoji;

        int matchedEmojiLength = 0;

        for (const EmojiData &emoji : possibleEmojis) {
            if (remainingCharacters < emoji.value.length()) {
                // It cannot be this emoji, there's not enough space for it
                continue;
            }

            bool match = true;

            for (int j = 1; j < emoji.value.length(); ++j) {
                if (text.at(i + j) != emoji.value.at(j)) {
                    match = false;

                    break;
                }
            }

            if (match) {
                matchedEmoji = emoji;
                matchedEmojiLength = emoji.value.length();

                break;
            }
        }

        if (matchedEmojiLength == 0) {
            continue;
        }

        int currentParsedEmojiFirstIndex = i;
        int currentParsedEmojiEndIndex = i + (matchedEmojiLength);

        int charactersFromLastParsedEmoji = currentParsedEmojiFirstIndex - lastParsedEmojiEndIndex;

        if (charactersFromLastParsedEmoji > 0) {
            // Add characters inbetween emojis
            parsedWords.push_back(std::tuple<messages::LazyLoadedImage *, QString>(
                nullptr, text.mid(lastParsedEmojiEndIndex, charactersFromLastParsedEmoji)));
        }

        QString url = "https://cdnjs.cloudflare.com/ajax/libs/"
                      "emojione/2.2.6/assets/png/" +
                      matchedEmoji.code + ".png";

        // Create or fetch cached emoji image
        auto emojiImage = this->emojiCache.getOrAdd(url, [this, &url] {
            return EmoteData(new LazyLoadedImage(*this, this->windowManager, url, 0.35));  //
        });

        // Push the emoji as a word to parsedWords
        parsedWords.push_back(std::tuple<EmoteData, QString>(emojiImage, QString()));

        lastParsedEmojiEndIndex = currentParsedEmojiEndIndex;

        i += matchedEmojiLength - 1;
    }

    if (lastParsedEmojiEndIndex < text.length()) {
        // Add remaining characters
        parsedWords.push_back(std::tuple<messages::LazyLoadedImage *, QString>(
            nullptr, text.mid(lastParsedEmojiEndIndex)));
    }
}

void EmoteManager::refreshTwitchEmotes(const std::string &roomID)
{
    std::string oauthClient =
        pajlada::Settings::Setting<std::string>::get("/accounts/" + roomID + "/oauthClient");
    std::string oauthToken =
        pajlada::Settings::Setting<std::string>::get("/accounts/" + roomID + "/oauthToken");

    TwitchAccountEmoteData &emoteData = this->twitchAccountEmotes[roomID];

    if (emoteData.filled) {
        qDebug() << "Already loaded for room id " << qS(roomID);
        return;
    }

    qDebug() << "Loading emotes for room id " << qS(roomID);

    if (oauthClient.empty() || oauthToken.empty()) {
        qDebug() << "Missing oauth client/token";
        return;
    }

    // api:v5
    QString url("https://api.twitch.tv/kraken/users/" + qS(roomID) +
                "/emotes?api_version=5&oauth_token=" + qS(oauthToken) +
                "&client_id=" + qS(oauthClient));

    qDebug() << url;

    util::urlFetchJSONTimeout(
        url,
        [=, &emoteData](QJsonObject &root) {
            emoteData.emoteSets.clear();
            emoteData.emoteCodes.clear();

            auto emoticonSets = root.value("emoticon_sets").toObject();
            for (QJsonObject::iterator it = emoticonSets.begin(); it != emoticonSets.end(); ++it) {
                std::string emoteSetString = it.key().toStdString();
                QJsonArray emoteSetList = it.value().toArray();

                for (QJsonValue emoteValue : emoteSetList) {
                    QJsonObject emoticon = emoteValue.toObject();
                    std::string id = emoticon["id"].toString().toStdString();
                    std::string code = emoticon["code"].toString().toStdString();
                    emoteData.emoteSets[emoteSetString].push_back({id, code});
                    emoteData.emoteCodes.push_back(code);
                }
            }

            emoteData.filled = true;
        },
        3000);
}

void EmoteManager::loadBTTVEmotes()
{
    QNetworkAccessManager *manager = new QNetworkAccessManager();

    QUrl url("https://api.betterttv.net/2/emotes");
    QNetworkRequest request(url);

    QNetworkReply *reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        if (reply->error() == QNetworkReply::NetworkError::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc(QJsonDocument::fromJson(data));
            QJsonObject root = jsonDoc.object();

            auto emotes = root.value("emotes").toArray();

            QString linkTemplate = "https:" + root.value("urlTemplate").toString();

            std::vector<std::string> codes;
            for (const QJsonValue &emote : emotes) {
                QString id = emote.toObject().value("id").toString();
                QString code = emote.toObject().value("code").toString();
                // emote.value("imageType").toString();

                QString tmp = linkTemplate;
                tmp.detach();
                QString url = tmp.replace("{{id}}", id).replace("{{image}}", "1x");

                this->bttvGlobalEmotes.insert(
                    code, new LazyLoadedImage(*this, this->windowManager, url, 1, code,
                                              code + "\nGlobal BTTV Emote"));
                codes.push_back(code.toStdString());
            }

            this->bttvGlobalEmoteCodes = codes;
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

void EmoteManager::loadFFZEmotes()
{
    QNetworkAccessManager *manager = new QNetworkAccessManager();

    QUrl url("https://api.frankerfacez.com/v1/set/global");
    QNetworkRequest request(url);

    QNetworkReply *reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        if (reply->error() == QNetworkReply::NetworkError::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument jsonDoc(QJsonDocument::fromJson(data));
            QJsonObject root = jsonDoc.object();

            auto sets = root.value("sets").toObject();

            std::vector<std::string> codes;
            for (const QJsonValue &set : sets) {
                auto emoticons = set.toObject().value("emoticons").toArray();

                for (const QJsonValue &emote : emoticons) {
                    QJsonObject object = emote.toObject();

                    // margins

                    // int id = object.value("id").toInt();
                    QString code = object.value("name").toString();

                    QJsonObject urls = object.value("urls").toObject();
                    QString url1 = "http:" + urls.value("1").toString();

                    this->ffzGlobalEmotes.insert(
                        code, new LazyLoadedImage(*this, this->windowManager, url1, 1, code,
                                                  code + "\nGlobal FFZ Emote"));
                    codes.push_back(code.toStdString());
                }

                this->ffzGlobalEmoteCodes = codes;
            }
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

// id is used for lookup
// emoteName is used for giving a name to the emote in case it doesn't exist
EmoteData EmoteManager::getTwitchEmoteById(long id, const QString &emoteName)
{
    return _twitchEmoteFromCache.getOrAdd(id, [this, &emoteName, &id] {
        qDebug() << "added twitch emote: " << id;
        qreal scale;
        QString url = getTwitchEmoteLink(id, scale);
        return new LazyLoadedImage(*this, this->windowManager, url, scale, emoteName,
                                   emoteName + "\nTwitch Emote");
    });
}

QString EmoteManager::getTwitchEmoteLink(long id, qreal &scale)
{
    scale = .5;

    QString value = TWITCH_EMOTE_TEMPLATE;

    value.detach();

    return value.replace("{id}", QString::number(id)).replace("{scale}", "2");
}

EmoteData EmoteManager::getCheerImage(long long amount, bool animated)
{
    // TODO: fix this xD
    return EmoteData();
}

boost::signals2::signal<void()> &EmoteManager::getGifUpdateSignal()
{
    if (!_gifUpdateTimerInitiated) {
        _gifUpdateTimerInitiated = true;

        _gifUpdateTimer.setInterval(30);
        _gifUpdateTimer.start();

        QObject::connect(&_gifUpdateTimer, &QTimer::timeout, [this] {
            _gifUpdateTimerSignal();
            this->windowManager.repaintGifEmotes();
        });
    }

    return _gifUpdateTimerSignal;
}

}  // namespace chatterino

#pragma once

#define GIF_FRAME_LENGTH 33

#include "concurrentmap.hpp"
#include "emojis.hpp"
#include "messages/lazyloadedimage.hpp"
#include "signalvector.hpp"
#include "twitch/emotevalue.hpp"

#include <QMap>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <boost/signals2.hpp>

namespace chatterino {

class WindowManager;
class Resources;

struct EmoteData {
    EmoteData()
    {
    }

    EmoteData(messages::LazyLoadedImage *_image)
        : image(_image)
    {
    }

    messages::LazyLoadedImage *image = nullptr;
};

class EmoteManager
{
public:
    using EmoteMap = ConcurrentMap<QString, EmoteData>;

    EmoteManager(WindowManager &_windowManager, Resources &_resources);

    void loadGlobalEmotes();

    void reloadBTTVChannelEmotes(const QString &channelName);
    void reloadFFZChannelEmotes(const QString &channelName);

    ConcurrentMap<QString, twitch::EmoteValue *> &getTwitchEmotes();
    EmoteMap &getFFZEmotes();
    EmoteMap &getChatterinoEmotes();
    EmoteMap &getBTTVChannelEmoteFromCaches();
    ConcurrentMap<int, EmoteData> &getFFZChannelEmoteFromCaches();
    ConcurrentMap<long, EmoteData> &getTwitchEmoteFromCache();

    EmoteData getCheerImage(long long int amount, bool animated);

    EmoteData getTwitchEmoteById(long int id, const QString &emoteName);

    int getGeneration()
    {
        return _generation;
    }

    void incGeneration()
    {
        _generation++;
    }

    boost::signals2::signal<void()> &getGifUpdateSignal();

    // Bit badge/emotes?
    ConcurrentMap<QString, messages::LazyLoadedImage *> miscImageCache;

private:
    WindowManager &windowManager;
    Resources &resources;

    /// Emojis
    // shortCodeToEmoji maps strings like "sunglasses" to its emoji
    QMap<QString, EmojiData> emojiShortCodeToEmoji;

    // Maps the first character of the emoji unicode string to a vector of possible emojis
    QMap<QChar, QVector<EmojiData>> emojiFirstByte;

    //            url      Emoji-one image
    EmoteMap emojiCache;

    void loadEmojis();

public:
    void parseEmojis(std::vector<std::tuple<EmoteData, QString>> &parsedWords, const QString &text);

    /// Twitch emotes
    void refreshTwitchEmotes(const std::string &roomID);

    struct TwitchAccountEmoteData {
        struct TwitchEmote {
            std::string id;
            std::string code;
        };

        //       emote set
        std::map<std::string, std::vector<TwitchEmote>> emoteSets;

        std::vector<std::string> emoteCodes;

        bool filled = false;
    };

    std::map<std::string, TwitchAccountEmoteData> twitchAccountEmotes;

private:
    //            emote code
    ConcurrentMap<QString, twitch::EmoteValue *> _twitchEmotes;

    //        emote id
    ConcurrentMap<long, EmoteData> _twitchEmoteFromCache;

    /// BTTV emotes
    EmoteMap bttvChannelEmotes;

public:
    ConcurrentMap<QString, EmoteMap> bttvChannels;
    EmoteMap bttvGlobalEmotes;
    SignalVector<std::string> bttvGlobalEmoteCodes;
    //       roomID
    std::map<std::string, SignalVector<std::string>> bttvChannelEmoteCodes;
    EmoteMap _bttvChannelEmoteFromCaches;

private:
    void loadBTTVEmotes();

    /// FFZ emotes
    EmoteMap ffzChannelEmotes;

public:
    ConcurrentMap<QString, EmoteMap> ffzChannels;
    EmoteMap ffzGlobalEmotes;
    SignalVector<std::string> ffzGlobalEmoteCodes;
    std::map<std::string, SignalVector<std::string>> ffzChannelEmoteCodes;

private:
    ConcurrentMap<int, EmoteData> _ffzChannelEmoteFromCaches;

    void loadFFZEmotes();

    /// Chatterino emotes
    EmoteMap _chatterinoEmotes;

    boost::signals2::signal<void()> _gifUpdateTimerSignal;
    QTimer _gifUpdateTimer;
    bool _gifUpdateTimerInitiated = false;

    int _generation = 0;

    // methods
    static QString getTwitchEmoteLink(long id, qreal &scale);
};

}  // namespace chatterino

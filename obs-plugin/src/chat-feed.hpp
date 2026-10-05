#pragma once
#include <QObject>
#include "chat-message.hpp"

class ChatFeed final : public QObject {
    Q_OBJECT
public:
    explicit ChatFeed(QObject *parent=nullptr) : QObject(parent) {}
    void publish(const ChatMessage &message) { emit messageReceived(message); }
signals:
    void messageReceived(ChatMessage message);
};

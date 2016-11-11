#ifndef BITCOIN_DUMMYCONNMAN_H
#define BITCOIN_DUMMYCONNMAN_H

#include "net.h"

#include <vector>
#include <map>

// Mockup of CConnman for use with unit tests.
class DummyConnman : public CConnman {
public:
    DummyConnman() : CConnman(11, 42) {
    }

    bool MsgWasSent(CNode& n, const std::string msg, int sequence = -1) {
        auto f = messages.find(&n);
        if (f == end(messages)) {
            return false;
        }
        auto& nodemsg = f->second;
        if (sequence == -1) {
            auto m = std::find(begin(nodemsg), end(nodemsg), msg);
            return m != end(nodemsg);
        }
        return nodemsg.at(sequence) == msg;
    }
    size_t NumMessagesSent(CNode& n) {
        auto f = messages.find(&n);
        if (f == end(messages)) {
            return 0;
        }
        return f->second.size();
    }

    void ClearMessages(CNode& n) {
        auto f = messages.find(&n);
        if (f != end(messages))
            f->second.clear();
    }
private:
    std::map<CNode*, std::vector<std::string> > messages;
    void PushMessage(CNode* pnode, CSerializedNetMsg&& msg) override {
        messages[pnode].push_back(msg.command);
        CConnman::PushMessage(pnode, std::forward<CSerializedNetMsg&&>(msg));
    }

};

#endif

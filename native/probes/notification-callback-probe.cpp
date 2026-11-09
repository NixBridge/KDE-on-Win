// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QTextStream>
#include <atomic>
#include <memory>
#include <thread>
#include "../notifications/asyncdispatch.h"

struct Receiver : QObject {
    std::shared_ptr<AsyncCallbackTarget> gate = std::make_shared<AsyncCallbackTarget>(this);
    ~Receiver() override { gate->invalidate(); }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int delivered = 0;
    auto receiver = std::make_unique<Receiver>();
    auto gate = receiver->gate;
    bool ok = gate->post([&] { ++delivered; });
    app.processEvents();
    ok &= delivered == 1;
    // Queued work must be cancelled by context destruction.
    gate->post([&] { delivered += 1000; });
    receiver.reset();
    app.processEvents();
    ok &= delivered == 1 && !gate->post([&] { delivered += 1000; });
    for (int trial = 0; trial < 100; ++trial) {
        receiver = std::make_unique<Receiver>();
        gate = receiver->gate;
        std::atomic<bool> ready { false };
        std::thread callback([&] {
            ready = true;
            for (int i = 0; i < 100; ++i)
                gate->post([&] { delivered += 1000; });
        });
        while (!ready)
            std::this_thread::yield();
        receiver.reset();
        callback.join();
        app.processEvents();
    }
    ok &= delivered == 1;
    QTextStream(stdout) << (ok ? "PASS" : "FAIL")
                        << ": queued and concurrent callbacks discarded after receiver teardown; no notification "
                           "permissions or listener used\n";
    return ok ? 0 : 1;
}

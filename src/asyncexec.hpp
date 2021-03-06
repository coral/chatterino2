#pragma once

#include <QCoreApplication>

#include <QRunnable>
#include <QThreadPool>

#include <functional>

#define async_exec(a) QThreadPool::globalInstance()->start(new LambdaRunnable(a));

class LambdaRunnable : public QRunnable
{
public:
    LambdaRunnable(std::function<void()> action)
    {
        this->action = action;
    }

    void run()
    {
        this->action();
    }

private:
    std::function<void()> action;
};

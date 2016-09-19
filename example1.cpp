#include <iostream>
#include <thread>
#include <stdint.h>
#include <chrono>
#include <string>
#include <sstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>


std::atomic<bool> AppShutdown(false);


// Wrap a thread to make it follow raii
class thread_raii
{
    std::thread t;
public:
    template< class Function, class ...Args>
    explicit thread_raii(std::string _threadName, Function&& f, Args&& ...args): threadName(_threadName)
    {
        // create and start thread..
        std::thread tl(f,args ...);
        // move thread owner ship to local thead object..
        t = std::move(tl);
    }

    ~thread_raii()
    {
        if (t.joinable())
        {
            std::cout << "Thread_raii: " << threadName;
            std::cout << " is active at garbage collection, waiting for thread to exit." << std::endl;
            t.join();
            std::cout << "Thread " << threadName << " Has completed.." << std::endl;
        }
    }
    thread_raii(thread_raii const&)=delete;
    thread_raii& operator=(thread_raii const&)=delete;
    std::string threadName;
};


// Thread safe queue
// Type T must be a copyable and assignable type..
template <typename T>
class Queue
{
 public:
 
  T pop()
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    auto item = queue_.front();
    queue_.pop();
    return item;
  }
 
  void pop(T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    item = queue_.front();
    queue_.pop();
  }
 
  void push(const T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(item);
    mlock.unlock();
    cond_.notify_one();
  }
 
  void push(T&& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(std::move(item));
    mlock.unlock();
    cond_.notify_one();
  }
 
 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};

typedef Queue<std::string> StringQueue;

// example worker object to be run by a thread..
// Generates messages and puts them into a queue.
class MessageGenerator 
{
public:
    void operator()(std::string taskName, int delayms, StringQueue *q )
    {
        int cnt = 0;
        std::cout << "Message Generator is starting.." << std::endl;
        while ( ! AppShutdown.load() )
        {
            std::string message("message id ");
            message += std::to_string( cnt );
            std::cout << "Enqueued message: \"" << message << "\"" << std::endl;
            q->push( message );
            cnt++;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
        }
        // signal we are done
        std::string message("shutdown");
        q->push( message );
        std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
        std::cout << "Message Generator has finished.." << std::endl;
    }
};

// example worker object to be run by a thread..
// Read messages from a queue and prints them on the screen.
class MessageReceiver
{
public:
    void operator()(std::string taskName, int delayms, StringQueue *q )
    {
        int cnt = 0;
        bool active = true;
        std::cout << "Message Received is starting.." << std::endl;
        while ( active )
        {
            std::string message( q->pop() );
            std::cout << "Received Message: \"" << message << "\"" << std::endl;
            if ( message.compare("shutdown") == 0 )
            {
                active = false;
                std::cout << "Received received a shutdown message.." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
        }
        std::cout << "Received Exit.." << std::endl;
    }
};

class application
{
    public:

    StringQueue q;

    application() { std::cout << "Application is initializing..\n"; }

    int run() {
        MessageGenerator GenWorker;
        MessageReceiver RecvWorker;
        thread_raii t1( std::string("GenWorker"), GenWorker, std::string("Generator"), 1, &q);
        thread_raii t2( std::string("RecvWorker"), RecvWorker, std::string("Receiver"), 5, &q);
        std::cout << "Threads spawned.. Let them run for 5 seconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "Signaling shutdown to threads.." << std::endl;
        AppShutdown.store(true);       
        std::cout << "Main Application Done.." << std::endl;
        return 0;
    }

    ~application() {
        std::cout << "Main Application Garbage Collection Complete.." << std::endl;
    }
};



int main() {
    application myApp;
    return myApp.run();
}


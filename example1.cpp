#include <iostream>
#include <thread>
#include <stdint.h>
#include <chrono>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
 

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
struct worker
{
    void operator()(std::string taskName, int delayms, StringQueue *q, bool isSrc )
    {
        // thread main
        std::cout << taskName << " starting operation.." << std::endl;
        for ( int i=0; i<10; i++ ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
            std::cout << taskName << " : " << i << std::endl; 
        }
        std::cout << taskName << " done.." << std::endl;
    }
};


class application
{
    public:
    application() { std::cout << "Application is initializing..\n"; }
    int run() {
        worker mySrcWorker;
        worker mySinkWorker;
        thread_raii t1( std::string("SrcWorker"), mySrcWorker, std::string("Generator"), 100 );
        thread_raii t2( std::string("SinkWorker"), mySinkWorker, std::string("Receiver"), 80 );
        std::cout << "Main Application Done..\n";
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


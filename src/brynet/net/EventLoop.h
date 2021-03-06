﻿#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>

#include <brynet/net/CurrentThread.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/timer/Timer.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Noexcept.h>

namespace brynet { namespace net {

    class Channel;
    class TcpConnection;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    class WakeupChannel;

    class EventLoop : public utils::NonCopyable
    {
    public:
        using Ptr = std::shared_ptr<EventLoop>;
        using UserFunctor = std::function<void(void)>;

#ifdef PLATFORM_WINDOWS
        enum class OverlappedType
        {
            OverlappedNone = 0,
            OverlappedRecv,
            OverlappedSend,
        };

        struct OverlappedExt
        {
            OVERLAPPED  base;
            const EventLoop::OverlappedType  OP;

            OverlappedExt(OverlappedType op) BRYNET_NOEXCEPT : OP(op)
            {
                memset(&base, 0, sizeof(base));
            }
        };
#endif

    public:
        EventLoop() BRYNET_NOEXCEPT;
        virtual ~EventLoop() BRYNET_NOEXCEPT;

        void                            loop(int64_t milliseconds);
        bool                            wakeup();

        void                            pushAsyncFunctor(UserFunctor f);
        void                            pushAfterLoopFunctor(UserFunctor f);

        /* return nullptr if not called in net thread*/
        timer::TimerMgr::Ptr            getTimerMgr();

        inline bool                     isInLoopThread() const
        {
            return mSelfThreadID == current_thread::tid();
        }

    private:
        void                            reallocEventSize(size_t size);
        void                            processAfterLoopFunctors();
        void                            processAsyncFunctors();

#ifndef PLATFORM_WINDOWS
        int                             getEpollHandle() const;
#endif
        bool                            linkChannel(sock fd, const Channel* ptr) BRYNET_NOEXCEPT;
        TcpConnectionPtr                getTcpConnection(sock fd);
        void                            addTcpConnection(sock fd, TcpConnectionPtr);
        void                            removeTcpConnection(sock fd);
        void                            tryInitThreadID();

    private:

#ifdef PLATFORM_WINDOWS
        std::vector<OVERLAPPED_ENTRY>   mEventEntries;

        typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);
        sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
        HANDLE                          mIOCP;
#else
        std::vector<epoll_event>        mEventEntries;
        int                             mEpollFd;
#endif
        std::unique_ptr<WakeupChannel>  mWakeupChannel;

        std::atomic_bool                mIsInBlock;
        std::atomic_bool                mIsAlreadyPostWakeup;

        std::mutex                      mAsyncFunctorsMutex;
        std::vector<UserFunctor>        mAsyncFunctors;
        std::vector<UserFunctor>        mCopyAsyncFunctors;

        std::vector<UserFunctor>        mAfterLoopFunctors;
        std::vector<UserFunctor>        mCopyAfterLoopFunctors;

        std::once_flag                  mOnceInitThreadID;
        current_thread::THREAD_ID_TYPE  mSelfThreadID;

        timer::TimerMgr::Ptr            mTimer;
        std::unordered_map<sock, TcpConnectionPtr> mTcpConnections;

        friend class TcpConnection;
    };

} }
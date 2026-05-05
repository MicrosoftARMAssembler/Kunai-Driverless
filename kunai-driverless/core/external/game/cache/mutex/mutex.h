#pragma once

namespace mutex {
    class c_shared_mutex {
    private:
        SRWLOCK m_srw = SRWLOCK_INIT;
    public:
        void lock( ) { AcquireSRWLockExclusive( &m_srw ); }
        void unlock( ) { ReleaseSRWLockExclusive( &m_srw ); }
        void lock_shared( ) { AcquireSRWLockShared( &m_srw ); }
        void unlock_shared( ) { ReleaseSRWLockShared( &m_srw ); }
    };

    struct exclusive_lock {
        c_shared_mutex& m;
        explicit exclusive_lock( c_shared_mutex& mx ) : m( mx ) { m.lock( ); }
        ~exclusive_lock( ) { m.unlock( ); }
    };

    struct shared_lock {
        c_shared_mutex& m;
        explicit shared_lock( c_shared_mutex& mx ) : m( mx ) { m.lock_shared( ); }
        ~shared_lock( ) { m.unlock_shared( ); }
    };
}
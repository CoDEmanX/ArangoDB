////////////////////////////////////////////////////////////////////////////////
/// @brief Read Locker
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReadLocker.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock
///
/// The constructors read-lock the lock, the destructors unlock the lock.
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock)
  : ReadLocker(readWriteLock, nullptr, 0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock
///
/// The constructors read-lock the lock, the destructors unlock the lock.
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock, char const* file, int line)
  : _readWriteLock(readWriteLock), _file(file), _line(line) {
  _readWriteLock->readLock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a read-lock, with periodic sleeps while not acquired
/// sleep time is specified in nanoseconds
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock, 
                          uint64_t sleepTime) 
  : ReadLocker(readWriteLock, sleepTime, nullptr, 0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a read-lock, with periodic sleeps while not acquired
/// sleep time is specified in nanoseconds
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock, 
                        uint64_t sleepTime,
                        char const* file,
                        int line) 
  : _readWriteLock(readWriteLock), _file(file), _line(line) {
  
  while (! _readWriteLock->tryReadLock()) {
    usleep(sleepTime);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock
////////////////////////////////////////////////////////////////////////////////

ReadLocker::~ReadLocker () {
  _readWriteLock->unlock();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

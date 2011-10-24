/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_JDWP_JDWP_H_
#define ART_JDWP_JDWP_H_

#include "jdwp/jdwp_bits.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_expand_buf.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct iovec;

namespace art {

namespace JDWP {

struct JdwpState;       /* opaque */

/*
 * Fundamental types.
 *
 * ObjectId and RefTypeId must be the same size.
 */
typedef uint32_t FieldId;     /* static or instance field */
typedef uint32_t MethodId;    /* any kind of method, including constructors */
typedef uint64_t ObjectId;    /* any object (threadID, stringID, arrayID, etc) */
typedef uint64_t RefTypeId;   /* like ObjectID, but unique for Class objects */
typedef uint64_t FrameId;     /* short-lived stack frame ID */

/*
 * Match these with the type sizes.  This way we don't have to pass
 * a value and a length.
 */
static inline FieldId ReadFieldId(const uint8_t** pBuf) { return read4BE(pBuf); }
static inline MethodId ReadMethodId(const uint8_t** pBuf) { return read4BE(pBuf); }
static inline ObjectId ReadObjectId(const uint8_t** pBuf) { return read8BE(pBuf); }
static inline RefTypeId ReadRefTypeId(const uint8_t** pBuf) { return read8BE(pBuf); }
static inline FrameId ReadFrameId(const uint8_t** pBuf) { return read8BE(pBuf); }
static inline void SetFieldId(uint8_t* buf, FieldId val) { return set4BE(buf, val); }
static inline void SetMethodId(uint8_t* buf, MethodId val) { return set4BE(buf, val); }
static inline void SetObjectId(uint8_t* buf, ObjectId val) { return set8BE(buf, val); }
static inline void SetRefTypeId(uint8_t* buf, RefTypeId val) { return set8BE(buf, val); }
static inline void SetFrameId(uint8_t* buf, FrameId val) { return set8BE(buf, val); }
static inline void expandBufAddFieldId(ExpandBuf* pReply, FieldId id) { expandBufAdd4BE(pReply, id); }
static inline void expandBufAddMethodId(ExpandBuf* pReply, MethodId id) { expandBufAdd4BE(pReply, id); }
static inline void expandBufAddObjectId(ExpandBuf* pReply, ObjectId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddRefTypeId(ExpandBuf* pReply, RefTypeId id) { expandBufAdd8BE(pReply, id); }
static inline void expandBufAddFrameId(ExpandBuf* pReply, FrameId id) { expandBufAdd8BE(pReply, id); }


/*
 * Holds a JDWP "location".
 */
struct JdwpLocation {
  uint8_t typeTag;        /* class or interface? */
  RefTypeId classId;        /* method->clazz */
  MethodId methodId;       /* method in which "idx" resides */
  uint64_t idx;            /* relative index into code block */
};

/*
 * How we talk to the debugger.
 */
enum JdwpTransportType {
  kJdwpTransportUnknown = 0,
  kJdwpTransportSocket,       /* transport=dt_socket */
  kJdwpTransportAndroidAdb,   /* transport=dt_android_adb */
};
std::ostream& operator<<(std::ostream& os, const JdwpTransportType& rhs);

/*
 * Holds collection of JDWP initialization parameters.
 */
struct JdwpStartupParams {
  JdwpTransportType transport;
  bool server;
  bool suspend;
  std::string host;
  short port;
};

/*
 * Perform one-time initialization.
 *
 * Among other things, this binds to a port to listen for a connection from
 * the debugger.
 *
 * Returns a newly-allocated JdwpState struct on success, or NULL on failure.
 */
JdwpState* JdwpStartup(const JdwpStartupParams* params);

/*
 * Shut everything down.
 */
void JdwpShutdown(JdwpState* state);

/*
 * Returns "true" if a debugger or DDM is connected.
 */
bool JdwpIsActive(JdwpState* state);

/*
 * Return the debugger thread's handle, or 0 if the debugger thread isn't
 * running.
 */
pthread_t GetDebugThread(JdwpState* state);

/*
 * Get time, in milliseconds, since the last debugger activity.
 */
int64_t LastDebuggerActivity(JdwpState* state);

/*
 * When we hit a debugger event that requires suspension, it's important
 * that we wait for the thread to suspend itself before processing any
 * additional requests.  (Otherwise, if the debugger immediately sends a
 * "resume thread" command, the resume might arrive before the thread has
 * suspended itself.)
 *
 * The thread should call the "set" function before sending the event to
 * the debugger.  The main JDWP handler loop calls "get" before processing
 * an event, and will wait for thread suspension if it's set.  Once the
 * thread has suspended itself, the JDWP handler calls "clear" and
 * continues processing the current event.  This works in the suspend-all
 * case because the event thread doesn't suspend itself until everything
 * else has suspended.
 *
 * It's possible that multiple threads could encounter thread-suspending
 * events at the same time, so we grab a mutex in the "set" call, and
 * release it in the "clear" call.
 */
//ObjectId GetWaitForEventThread(JdwpState* state);
void SetWaitForEventThread(JdwpState* state, ObjectId threadId);
void ClearWaitForEventThread(JdwpState* state);

/*
 * These notify the debug code that something interesting has happened.  This
 * could be a thread starting or ending, an exception, or an opportunity
 * for a breakpoint.  These calls do not mean that an event the debugger
 * is interested has happened, just that something has happened that the
 * debugger *might* be interested in.
 *
 * The item of interest may trigger multiple events, some or all of which
 * are grouped together in a single response.
 *
 * The event may cause the current thread or all threads (except the
 * JDWP support thread) to be suspended.
 */

/*
 * The VM has finished initializing.  Only called when the debugger is
 * connected at the time initialization completes.
 */
bool PostVMStart(JdwpState* state, bool suspend);

/*
 * A location of interest has been reached.  This is used for breakpoints,
 * single-stepping, and method entry/exit.  (JDWP requires that these four
 * events are grouped together in a single response.)
 *
 * In some cases "*pLoc" will just have a method and class name, e.g. when
 * issuing a MethodEntry on a native method.
 *
 * "eventFlags" indicates the types of events that have occurred.
 */
bool PostLocationEvent(JdwpState* state, const JdwpLocation* pLoc, ObjectId thisPtr, int eventFlags);

/*
 * An exception has been thrown.
 *
 * Pass in a zeroed-out "*pCatchLoc" if the exception wasn't caught.
 */
bool PostException(JdwpState* state, const JdwpLocation* pThrowLoc,
    ObjectId excepId, RefTypeId excepClassId, const JdwpLocation* pCatchLoc,
    ObjectId thisPtr);

/*
 * A thread has started or stopped.
 */
bool PostThreadChange(JdwpState* state, ObjectId threadId, bool start);

/*
 * Class has been prepared.
 */
bool PostClassPrepare(JdwpState* state, int tag, RefTypeId refTypeId,
    const char* signature, int status);

/*
 * The VM is about to stop.
 */
bool PostVMDeath(JdwpState* state);

/*
 * Send up a chunk of DDM data.
 */
void DdmSendChunkV(JdwpState* state, int type, const iovec* iov, int iovcnt);

}  // namespace JDWP

}  // namespace art

#endif  // ART_JDWP_JDWP_H_

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
/*
 * Handle registration of events, and debugger event notification.
 */
#ifndef DALVIK_JDWP_JDWPEVENT_H_
#define DALVIK_JDWP_JDWPEVENT_H_

#include "JdwpConstants.h"
#include "ExpandBuf.h"

/*
 * Event modifiers.  A JdwpEvent may have zero or more of these.
 */
union JdwpEventMod {
    u1      modKind;                /* JdwpModKind */
    struct {
        u1          modKind;
        int         count;
    } count;
    struct {
        u1          modKind;
        u4          exprId;
    } conditional;
    struct {
        u1          modKind;
        ObjectId    threadId;
    } threadOnly;
    struct {
        u1          modKind;
        RefTypeId   refTypeId;
    } classOnly;
    struct {
        u1          modKind;
        char*       classPattern;
    } classMatch;
    struct {
        u1          modKind;
        char*       classPattern;
    } classExclude;
    struct {
        u1          modKind;
        JdwpLocation loc;
    } locationOnly;
    struct {
        u1          modKind;
        u1          caught;
        u1          uncaught;
        RefTypeId   refTypeId;
    } exceptionOnly;
    struct {
        u1          modKind;
        RefTypeId   refTypeId;
        FieldId     fieldId;
    } fieldOnly;
    struct {
        u1          modKind;
        ObjectId    threadId;
        int         size;           /* JdwpStepSize */
        int         depth;          /* JdwpStepDepth */
    } step;
    struct {
        u1          modKind;
        ObjectId    objectId;
    } instanceOnly;
};

/*
 * One of these for every registered event.
 *
 * We over-allocate the struct to hold the modifiers.
 */
struct JdwpEvent {
    JdwpEvent* prev;           /* linked list */
    JdwpEvent* next;

    JdwpEventKind eventKind;      /* what kind of event is this? */
    JdwpSuspendPolicy suspendPolicy;  /* suspend all, none, or self? */
    int modCount;       /* #of entries in mods[] */
    u4 requestId;      /* serial#, reported to debugger */

    JdwpEventMod mods[1];        /* MUST be last field in struct */
};

/*
 * Allocate an event structure with enough space.
 */
JdwpEvent* dvmJdwpEventAlloc(int numMods);
void dvmJdwpEventFree(JdwpEvent* pEvent);

/*
 * Register an event by adding it to the event list.
 *
 * "*pEvent" must be storage allocated with jdwpEventAlloc().  The caller
 * may discard its pointer after calling this.
 */
JdwpError dvmJdwpRegisterEvent(JdwpState* state, JdwpEvent* pEvent);

/*
 * Unregister an event, given the requestId.
 */
void dvmJdwpUnregisterEventById(JdwpState* state, u4 requestId);

/*
 * Unregister all events.
 */
void dvmJdwpUnregisterAll(JdwpState* state);

/*
 * Send an event, formatted into "pReq", to the debugger.
 *
 * (Messages are sent asynchronously, and do not receive a reply.)
 */
bool dvmJdwpSendRequest(JdwpState* state, ExpandBuf* pReq);

#endif  // DALVIK_JDWP_JDWPEVENT_H_

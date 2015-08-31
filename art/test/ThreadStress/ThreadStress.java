/*
 * Copyright (C) 2011 The Android Open Source Project
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import libcore.io.*;

// Run on host with:
//   javac ThreadTest.java && java ThreadStress && rm *.class
class ThreadStress implements Runnable {

    public static final boolean DEBUG = false;

    enum Operation {
        OOM(1),
        SIGQUIT(19),
        ALLOC(60),
        STACKTRACE(20),
        EXIT(50),

        SLEEP(25),
        TIMED_WAIT(10),
        WAIT(15);

        private final int frequency;
        Operation(int frequency) {
            this.frequency = frequency;
        }
    }

    public static void main(String[] args) throws Exception {

        final int numberOfThreads = 5;
        final int totalOperations = 1000;
        final int operationsPerThread = totalOperations/numberOfThreads;

        // Lock used to notify threads performing Operation.WAIT
        final Object lock = new Object();

        // Each thread is going to do operationsPerThread
        // operations. The distribution of operations is determined by
        // the Operation.frequency values. We fill out an Operation[]
        // for each thread with the operations it is to perform. The
        // Operation[] is shuffled so that there is more random
        // interactions between the threads.

        // The simple-minded filling in of Operation[] based on
        // Operation.frequency below won't have even have close to a
        // reasonable distribution if the count of Operation
        // frequencies is greater than the total number of
        // operations. So here we do a quick sanity check in case
        // people tweak the constants above.
        int operationCount = 0;
        for (Operation op : Operation.values()) {
            operationCount += op.frequency;
        }
        if (operationCount > operationsPerThread) {
            throw new AssertionError(operationCount + " > " + operationsPerThread);
        }

        // Fill in the Operation[] array for each thread by laying
        // down references to operation according to their desired
        // frequency.
        final ThreadStress[] threadStresses = new ThreadStress[numberOfThreads];
        for (int t = 0; t < threadStresses.length; t++) {
            Operation[] operations = new Operation[operationsPerThread];
            int o = 0;
            LOOP:
            while (true) {
                for (Operation op : Operation.values()) {
                    for (int f = 0; f < op.frequency; f++) {
                        if (o == operations.length) {
                            break LOOP;
                        }
                        operations[o] = op;
                        o++;
                    }
                }
            }
            // Randomize the oepration order
            Collections.shuffle(Arrays.asList(operations));
            threadStresses[t] = new ThreadStress(lock, t, operations);
        }

        // Enable to dump operation counds per thread to make sure its
        // sane compared to Operation.frequency
        if (DEBUG) {
            for (int t = 0; t < threadStresses.length; t++) {
                Operation[] operations = new Operation[operationsPerThread];
                Map<Operation, Integer> distribution = new HashMap<Operation, Integer>();
                for (Operation operation : operations) {
                    Integer ops = distribution.get(operation);
                    if (ops == null) {
                        ops = 1;
                    } else {
                        ops++;
                    }
                    distribution.put(operation, ops);
                }
                System.out.println("Distribution for " + t);
                for (Operation op : Operation.values()) {
                    System.out.println(op + " = " + distribution.get(op));
                }
            }
        }

        // Create the runners for each thread. The runner Thread
        // ensures that thread that exit due to Operation.EXIT will be
        // restarted until they reach their desired
        // operationsPerThread.
        Thread[] runners = new Thread[numberOfThreads];
        for (int r = 0; r < runners.length; r++) {
            final ThreadStress ts = threadStresses[r];
            runners[r] = new Thread() {
                final ThreadStress threadStress = ts;
                public void run() {
                    int id = threadStress.id;
                    System.out.println("Starting runner for " + id);
                    while (threadStress.nextOperation < operationsPerThread) {
                        Thread thread = new Thread(ts);
                        thread.start();
                        try {
                            thread.join();
                        } catch (InterruptedException e) {
                        }
                        System.out.println("Thread exited for " + id + " with "
                                           + (operationsPerThread - threadStress.nextOperation)
                                           + " operations remaining.");
                    }
                    System.out.println("Finishing runner for " + id);
                }
            };
        }

        // The notifier thread is a daemon just loops forever to wake
        // up threads in Operation.WAIT
        Thread notifier = new Thread() {
            public void run() {
                while (true) {
                    synchronized (lock) {
                        lock.notifyAll();
                    }
                }
            }
        };
        notifier.setDaemon(true);
        notifier.start();

        for (int r = 0; r < runners.length; r++) {
            runners[r].start();
        }
        for (int r = 0; r < runners.length; r++) {
            runners[r].join();
        }
    }

    private final Operation[] operations;
    private final Object lock;
    private final int id;

    private int nextOperation;

    private ThreadStress(Object lock, int id, Operation[] operations) {
        this.lock = lock;
        this.id = id;
        this.operations = operations;
    }

    public void run() {
        try {
            if (DEBUG) {
                System.out.println("Starting ThreadStress " + id);
            }
            while (nextOperation < operations.length) {
                Operation operation = operations[nextOperation];
                if (DEBUG) {
                    System.out.println("ThreadStress " + id
                                       + " operation " + nextOperation
                                       + " is " + operation);
                }
                nextOperation++;
                switch (operation) {
                    case EXIT: {
                        return;
                    }
                    case SIGQUIT: {
                        try {
                            Libcore.os.kill(Libcore.os.getpid(), OsConstants.SIGQUIT);
                        } catch (ErrnoException ex) {
                        }
                    }
                    case SLEEP: {
                        try {
                            Thread.sleep(100);
                        } catch (InterruptedException ignored) {
                        }
                    }
                    case TIMED_WAIT: {
                        synchronized (lock) {
                            try {
                                lock.wait(100, 0);
                            } catch (InterruptedException ignored) {
                            }
                        }
                        break;
                    }
                    case WAIT: {
                        synchronized (lock) {
                            try {
                                lock.wait();
                            } catch (InterruptedException ignored) {
                            }
                        }
                        break;
                    }
                    case OOM: {
                        try {
                            List<byte[]> l = new ArrayList<byte[]>();
                            while (true) {
                                l.add(new byte[1024]);
                            }
                        } catch (OutOfMemoryError e) {
                        }
                        break;
                    }
                    case ALLOC: {
                        try {
                            List<byte[]> l = new ArrayList<byte[]>();
                            for (int i = 0; i < 1024; i++) {
                                l.add(new byte[1024]);
                            }
                        } catch (OutOfMemoryError e) {
                        }
                        break;
                    }
                    case STACKTRACE: {
                        Thread.currentThread().getStackTrace();
                        break;
                    }
                    default: {
                        throw new AssertionError(operation.toString());
                    }
                }
            }
        } finally {
            if (DEBUG) {
                System.out.println("Finishing ThreadStress for " + id);
            }
        }
    }
}

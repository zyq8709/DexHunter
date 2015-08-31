/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.dexgen.util;

/**
 * An exception type used to aggregate all the unexpected situations while
 * trying to save dex file with generated class and load the class afterwards.
 */
public class DexClassLoadingException extends Exception {

    /**
     * Encapsulates any checked exception being thrown in time between saving
     * generated dex class to a file and loading it via DexClassLoader with
     * an user-friendly message and passing the original exception as well.
     *
     * @param thr {@code non-null;} lower level exception with more detailed
     * error message
     */
    public DexClassLoadingException(Throwable thr) {
        super("Loading generated dex class has failed", thr);
    }
}
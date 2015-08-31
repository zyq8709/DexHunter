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

import dalvik.system.DexClassLoader;

/**
 * Class used indirectly for loading generated dex classes. It allows the caller
 * to obtain appropriate {@code DexClassLoader} instance, which can be then used for
 * loading classes.
 */
public class DexClassLoaderHelper {

    private static class DexClassLoaderHelperHolder {
        private static final DexClassLoaderHelper INSTANCE = new DexClassLoaderHelper();
    }

    private DexClassLoaderHelper() {
        // intentionally empty to disable direct instantiation
    }

    /**
     * Returns the sole instance of {@code DexClassLoaderHelper}.
     *
     * @return dex {@code DexClassLoaderHelper} sole instance
     */
    public static DexClassLoaderHelper getInstance() {
        return DexClassLoaderHelperHolder.INSTANCE;
    }

    /**
     * Creates and returns DexClassLoader instance with its classpath
     * set to {@code pathHolder}.
     *
     * @param pathHolder {@code non-null;} location of jar archive containing dex
     * classes canned into a working PathHolder instance.
     * @return dex class loader instance with its classpath set to location
     * indicated by {@code pathHolder}
     */
    public ClassLoader getDexClassLoader(PathHolder pathHolder) {
        ClassLoader myLoader = DexClassLoaderHelper.class.getClassLoader();
        return new DexClassLoader(pathHolder.getJarFilePath(), pathHolder.getDirLocation(),
                null, myLoader);
    }
}
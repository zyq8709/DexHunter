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

import java.io.File;

/**
 *  Helper class used primarily for holding path on the device of different
 *  files arising in the dex class generation process.
 */
public class PathHolder {

    public static final String DEX_FILE_EXTENSION = ".dex";

    public static final String JAR_FILE_EXTENSION = ".jar";

    /** {@code non-null;} directory location of the dex-related files */
    private final String dirLocation;

    /** {@code non-null;} common file name prefix of the created files */
    private final String fileNamePrefix;

    /**
     * Creates an instance of {@code PathHolder} initialized with the directory
     * location for storage of temporary files and common file name prefix for these
     * files.
     *
     * @param dirLocation {@code non-null;} path to directory used for storage of temporary files
     * @param fileNamePrefix {@code non-null;} common file name prefix across all the temporary
     * files involved in the dex class generation and loading process
     */
    public PathHolder(String dirLocation, String fileNamePrefix) {
        if (dirLocation == null) {
            throw new NullPointerException("dirLocation == null");
        }
        if (fileNamePrefix == null) {
            throw new NullPointerException("fileNamePrefix == null");
        }

        this.dirLocation = dirLocation;
        this.fileNamePrefix = fileNamePrefix;
    }

    public String getFileName() {
        return fileNamePrefix;
    }

    public String getDexFilePath() {
        return dirLocation + File.separator + fileNamePrefix + DEX_FILE_EXTENSION;
    }

    public String getDexFileName() {
        return fileNamePrefix + DEX_FILE_EXTENSION;
    }

    public String getJarFilePath() {
        return dirLocation + File.separator + fileNamePrefix + JAR_FILE_EXTENSION;
    }

    public String getJarFileName() {
        return fileNamePrefix + JAR_FILE_EXTENSION;
    }

    public String getDirLocation() {
        return dirLocation;
    }
}

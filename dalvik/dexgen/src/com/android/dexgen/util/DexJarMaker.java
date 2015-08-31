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

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;

/**
 * Helper class used to encapsulate generated .dex file into .jar
 * so that it fits {@code DexClassLoader} constructor.
 */
public class DexJarMaker {

    /** indicates name of the dex file added to jar */
    public static final String DEX_FILE_NAME_IN_JAR = "classes" + PathHolder.DEX_FILE_EXTENSION;

    /** {@code non-null;} storage for all the paths related to current dex file */
    private final PathHolder pathHolder;

    public DexJarMaker(PathHolder pathHolder) {
        this.pathHolder = pathHolder;
    }

    /** Packs previously added files into a single jar archive. */
    public void create() throws DexClassLoadingException {
        Manifest manifest = new Manifest();
        manifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
        JarOutputStream target = null;
        try {
            target = new JarOutputStream(
                    new BufferedOutputStream(new FileOutputStream(pathHolder.getJarFilePath())),
                    manifest);
            add(new File(pathHolder.getDexFilePath()), target);

        } catch (IOException e) {
            throw new DexClassLoadingException(e);
        }
        finally {
            try {
                if (target != null) {
                    target.close();
                }
            } catch(IOException e) {
                // Ignoring deliberately in order to keep the original exception clear.
            }
        }
    }

    /**
     * Adds indicated file to the requested archive.
     *
     * @param source {@code non-null;} dex file to add
     * @param target {@code non-null;} target jar archive
     * @throws IOException
     */
    private void add(File source, JarOutputStream target) throws IOException {

        if (!source.isFile()) {
            throw new IllegalArgumentException("Wrong source dex file provided");
        }

        BufferedInputStream in = new BufferedInputStream(new FileInputStream(source));
        JarEntry entry = new JarEntry(DEX_FILE_NAME_IN_JAR);
        entry.setTime(source.lastModified());
        target.putNextEntry(entry);

        int curr = -1;
        while ((curr = in.read()) != -1) {
            target.write(curr);
        }
        target.closeEntry();
    }
}
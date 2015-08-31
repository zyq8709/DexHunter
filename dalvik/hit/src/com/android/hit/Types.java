/*
 * Copyright (C) 2008 Google Inc.
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

package com.android.hit;

public class Types {
    private static int mIdSize = 4;

    public static final int OBJECT     =   2;
    public static final int BOOLEAN    =   4;
    public static final int CHAR       =   5;
    public static final int FLOAT      =   6;
    public static final int DOUBLE     =   7;
    public static final int BYTE       =   8;
    public static final int SHORT      =   9;
    public static final int INT        =   10;
    public static final int LONG       =   11;

    public static final void setIdSize(int size) {
        mIdSize = size;
    }

    public static final int getTypeSize(int type) {
        switch (type) {
            case '[':      return mIdSize; //  array object
            case 'L':      return mIdSize; //  object
            case 'Z':      return 1;       //  boolean
            case 'C':      return 2;       //  char
            case 'F':      return 4;       //  float
            case 'D':      return 8;       //  double
            case 'B':      return 1;       //  byte
            case 'S':      return 2;       //  short
            case 'I':      return 4;       //  int
            case 'J':      return 8;       //  long

            case OBJECT:   return mIdSize;
            case BOOLEAN:  return 1;
            case CHAR:     return 2;
            case FLOAT:    return 4;
            case DOUBLE:   return 8;
            case BYTE:     return 1;
            case SHORT:    return 2;
            case INT:      return 4;
            case LONG:     return 8;
        }

        throw new IllegalArgumentException("Illegal type signature: " + type);
    }

    public static final String getTypeName(int type) {
        switch (type) {
            case '[':      return "array";
            case 'L':      return "object";
            case 'Z':      return "boolean";
            case 'C':      return "char";
            case 'F':      return "float";
            case 'D':      return "double";
            case 'B':      return "byte";
            case 'S':      return "short";
            case 'I':      return "int";
            case 'J':      return "long";

            case OBJECT:   return "object";
            case BOOLEAN:  return "boolean";
            case CHAR:     return "char";
            case FLOAT:    return "float";
            case DOUBLE:   return "double";
            case BYTE:     return "byte";
            case SHORT:    return "short";
            case INT:      return "int";
            case LONG:     return "long";
        }

        throw new IllegalArgumentException("Illegal type signature: " + type);
    }
}

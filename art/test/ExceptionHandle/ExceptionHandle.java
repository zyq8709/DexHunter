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

import java.io.IOException;

public class ExceptionHandle {
    int f() throws Exception {
        try {
            g(1);
        } catch (IOException e) {
            return 1;
        } catch (Exception e) {
            return 2;
        }
        try {
            g(2);
        } catch (IOException e) {
            return 3;
        }
        return 0;
    }
    void g(int doThrow) throws Exception {
        if (doThrow == 1) {
            throw new Exception();
        } else if (doThrow == 2) {
            throw new IOException();
        }
    }
}

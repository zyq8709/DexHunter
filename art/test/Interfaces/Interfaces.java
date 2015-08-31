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

class Interfaces {
    interface I {
        public void i();
    }
    interface J {
        public String foo = "foo";
        public void j1();
        public void j2();
    }
    interface K extends J {
        public void k();
    }
    class A implements I, J {
        public void i() {};
        public void j1() {};
        public void j2() {};
    }
    class B implements K {
        public void j1() {};
        public void j2() {};
        public void k() {};
    }
}

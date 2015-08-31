This directory contains the Dalvik virtual machine and core class library,
as well as related tools, libraries, and tests.

A note about the licenses and header comments
---------------------------------------------

Much of the code under this directory originally came from the Apache
Harmony project, and as such contains the standard Apache header
comment. Some of the code was written originally for the Android
project, and as such contains the standard Android header comment.
Some files contain code from both projects. In these cases, the header
comment is a combination of the other two, and the portions of the
code from Harmony are identified as indicated in the comment.

Here is the combined header comment:

/*
 * Copyright (C) <year> The Android Open Source Project
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
 *
 * ----------
 *
 * Portions of the code surrounded by "// BEGIN Harmony code" and
 * "// END Harmony code" are copyrighted and licensed separately, as
 * follows:
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


Native SH call bridge
---------------------

Native SH call bridge is written by
Shin-ichiro KAWASAKI <shinichiro.kawasaki.mg@hitachi.com>
and Contributed to Android by Hitachi, Ltd. and Renesas Solutions Corp.

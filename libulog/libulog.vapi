/**
 * Copyright (C) 2013 Parrot S.A.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace ULog {

    [CCode (cname = "ULOG_CRIT", cheader_filename = "ulog.h")]
    public int CRIT;
    [CCode (cname = "ULOG_ERR", cheader_filename = "ulog.h")]
    public int ERR;
    [CCode (cname = "ULOG_WARN", cheader_filename = "ulog.h")]
    public int WARN;
    [CCode (cname = "ULOG_NOTICE", cheader_filename = "ulog.h")]
    public int NOTICE;
    [CCode (cname = "ULOG_INFO", cheader_filename = "ulog.h")]
    public int INFO;
    [CCode (cname = "ULOG_DEBUG", cheader_filename = "ulog.h")]
    public int DEBUG;

    [CCode (cname = "ulog_gst_redirect", has_target = false, cheader_filename = "ulog_gst.h")]
    public void gst_redirect();

    [CCode (cname = "ulog_gst_set_level", has_target = false, cheader_filename = "ulog_gst.h")]
    void gst_set_level(int level);


    [CCode (cname = "ulog_glib_redirect", has_target = false, cheader_filename = "ulog_glib.h")]
    public void glib_redirect();

    [CCode (cname = "ulog_glib_set_level", has_target = false, cheader_filename = "ulog_glib.h")]
    void glib_set_level(int level);


    [CCode (cname = "ulog_obus_redirect", has_target = false, cheader_filename = "ulog_obus.h")]
    public void obus_redirect();

    [CCode (cname = "ulog_obus_set_level", has_target = false, cheader_filename = "ulog_obus.h")]
    void obus_set_level(int level);
}

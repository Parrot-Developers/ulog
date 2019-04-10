/**
 * Copyright (C) 2017 Parrot S.A.
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
 *
 */

#ifndef _PARROT_ULOG_CTRL_H_
#define _PARROT_ULOG_CTRL_H_

void ulog_amba_early_init(void);
void ulog_amba_shd_init(void);
char ulog_prio2char(int prio);

int parse_level(int c);
int ulog_get_tag_level(const char *name);

#endif

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2016 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fluent-bit/flb_input.h>
#include "proc.h"

static char *human_readable_size(long size)
{
    long u = 1024, i, len = 128;
    char *buf = flb_malloc(len);
    static const char *__units[] = { "b", "K", "M", "G",
        "T", "P", "E", "Z", "Y", NULL
    };

    for (i = 0; __units[i] != NULL; i++) {
        if ((size / u) == 0) {
            break;
        }
        u *= 1024;
    }
    if (!i) {
        snprintf(buf, len, "%ld %s", size, __units[0]);
    }
    else {
        float fsize = (float) ((double) size / (u / 1024));
        snprintf(buf, len, "%.2f%s", fsize, __units[i]);
    }

    return buf;
}

/* Read file content into a memory buffer */
static char *file_to_buffer(const char *path)
{
    FILE *fp;
    char *buffer;
    long bytes;

    if (!(fp = fopen(path, "r"))) {
        flb_errno();
        return NULL;
    }

    buffer = flb_malloc(PROC_STAT_BUF_SIZE);
    if (!buffer) {
        fclose(fp);
        flb_errno();
        return NULL;
    }

    bytes = fread(buffer, PROC_STAT_BUF_SIZE, 1, fp);
    if (bytes < 0) {
        flb_free(buffer);
        fclose(fp);
        flb_errno();
        return NULL;
    }

    fclose(fp);
    return buffer;
}


struct proc_task *proc_stat(pid_t pid, int page_size)
{
    int ret;
    char *p, *q;
    char *buf;
    char pid_path[PROC_PID_SIZE];
    struct proc_task *t;

    t = flb_calloc(1, sizeof(struct proc_task));
    if (!t) {
        flb_errno();
        return NULL;
    }

    /* Compose path for /proc/PID/stat */
    ret = snprintf(pid_path, PROC_PID_SIZE, "/proc/%i/stat", pid);
    if (ret < 0) {
        flb_errno();
        return NULL;
    }

    buf = file_to_buffer(pid_path);
    if (!buf) {
        flb_free(t);
        return NULL;
    }

    sscanf(buf, "%d", &t->pid);

    /*
     * workaround for process with spaces in the name, so we dont screw up
     * sscanf(3).
     */
    p = buf;
    while (*p != '(') p++; p++;
    q = p;
    while (*q != ')') q++;
    strncpy(t->comm, p, q - p);
    q += 2;

    /* Read pending values */
    sscanf(q, PROC_STAT_FORMAT,
           &t->state,
           &t->ppid,
           &t->pgrp,
           &t->session,
           &t->tty_nr,
           &t->tpgid,
           &t->flags,
           &t->minflt,
           &t->cminflt,
           &t->majflt,
           &t->cmajflt,
           &t->utime,
           &t->stime,
           &t->cutime,
           &t->cstime,
           &t->priority,
           &t->nice,
           &t->num_threads,
           &t->itrealvalue,
           &t->starttime,
           &t->vsize,
           &t->rss);

    /* Internal conversion */
    t->proc_rss    = (t->rss * page_size);
    t->proc_rss_hr = human_readable_size(t->proc_rss);

    flb_free(buf);
    return t;
}

void proc_free(struct proc_task *t)
{
    flb_free(t->proc_rss_hr);
    flb_free(t);
}
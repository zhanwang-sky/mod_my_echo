/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Ji Chen <jicmail@icloud.com>
 *
 *
 * mod_my_echo.c -- Echo Demo Module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_LOAD_FUNCTION(mod_my_echo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_my_echo_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_my_echo_runtime);
/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_my_echo, mod_my_echo_load, mod_my_echo_shutdown, mod_my_echo_runtime);

typedef struct {
    switch_caller_profile_t *caller_profile;
    switch_channel_t *channel;
    switch_core_session_t *session;
    switch_media_handle_t *media_handle;
    switch_core_media_params_t mparams;
} private_object_t;

static struct {
    switch_memory_pool_t *pool;
    switch_mutex_t *mutex;
    switch_thread_cond_t *cond;
    int running;
    int exited;
} mod_my_echo_globals;

switch_endpoint_interface_t *my_echo_endpoint_interface;

/* Macro expands to: switch_status_t mod_my_echo_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_my_echo_load) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "start loading mod_my_echo\n");

    memset(&mod_my_echo_globals, 0, sizeof(mod_my_echo_globals));
    mod_my_echo_globals.pool = pool;
    switch_mutex_init(&mod_my_echo_globals.mutex, SWITCH_MUTEX_NESTED, mod_my_echo_globals.pool);
    switch_thread_cond_create(&mod_my_echo_globals.cond, mod_my_echo_globals.pool);
    mod_my_echo_globals.running = 1;
    mod_my_echo_globals.exited = 0;

    /* connect my internal structure to the blank pointer passed to me */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    my_echo_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
    my_echo_endpoint_interface->interface_name = "my_echo";

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "finish loading mod_my_echo\n");

    /* indicate that the module should continue to be loaded */
    return SWITCH_STATUS_SUCCESS;
}

/*
 * Called when the system shuts down
 * Macro expands to: switch_status_t mod_my_echo_shutdown()
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_my_echo_shutdown) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "start shutting down mod_my_echo\n");

    switch_mutex_lock(mod_my_echo_globals.mutex);
    mod_my_echo_globals.running = 0;
    while (!mod_my_echo_globals.exited) {
        switch_thread_cond_wait(mod_my_echo_globals.cond, mod_my_echo_globals.mutex);
    }
    switch_mutex_unlock(mod_my_echo_globals.mutex);

    switch_thread_cond_destroy(mod_my_echo_globals.cond);
    switch_mutex_destroy(mod_my_echo_globals.mutex);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "finish shutting down mod_my_echo\n");

    return SWITCH_STATUS_SUCCESS;
}

/*
 * If it exists, this is called in it's own thread when the module-load completes
 * If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
 * Macro expands to: switch_status_t mod_my_echo_runtime()
 */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_my_echo_runtime) {
    static int loop_cnt = 0;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_my_echo runtime started\n");

    do {
        switch_mutex_lock(mod_my_echo_globals.mutex);
        if (!mod_my_echo_globals.running) {
            break;
        }
        switch_mutex_unlock(mod_my_echo_globals.mutex);

        switch_yield(5000000);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_my_echo loop %d times\n", ++loop_cnt);
    } while (1);

    mod_my_echo_globals.exited = 1;
    switch_thread_cond_broadcast(mod_my_echo_globals.cond);
    switch_mutex_unlock(mod_my_echo_globals.mutex);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_my_echo runtime ended\n");

    return SWITCH_STATUS_TERM;
}

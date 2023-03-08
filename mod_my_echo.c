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

switch_call_cause_t my_echo_outgoing_channel(switch_core_session_t*, switch_event_t*, switch_caller_profile_t*,
                                             switch_core_session_t**, switch_memory_pool_t**,
                                             switch_originate_flag_t, switch_call_cause_t*);
switch_status_t my_echo_read_frame(switch_core_session_t*, switch_frame_t**, switch_io_flag_t, int);
switch_status_t my_echo_write_frame(switch_core_session_t*, switch_frame_t*, switch_io_flag_t, int);
switch_status_t my_echo_kill_channel(switch_core_session_t*, int);
switch_status_t my_echo_send_dtmf(switch_core_session_t*, const switch_dtmf_t*);
switch_status_t my_echo_receive_message(switch_core_session_t*, switch_core_session_message_t*);
switch_status_t my_echo_receive_event(switch_core_session_t*, switch_event_t*);
switch_status_t my_echo_read_video_frame(switch_core_session_t*, switch_frame_t**, switch_io_flag_t, int);
switch_status_t my_echo_write_video_frame(switch_core_session_t*, switch_frame_t*, switch_io_flag_t, int);
switch_jb_t* my_echo_get_jb(switch_core_session_t*, switch_media_type_t);

typedef struct {
    switch_caller_profile_t *caller_profile;
    switch_channel_t *channel;
    switch_core_session_t *session;
    switch_media_handle_t *media_handle;
    switch_core_media_params_t mparams;
} private_object_t;

/* Global Variables */
struct {
    switch_memory_pool_t *pool;
    switch_mutex_t *mutex;
    switch_thread_cond_t *cond;
    int running;
    int exited;
} mod_my_echo_globals;

switch_io_routines_t my_echo_io_routines = {
    /*.outgoing_channel */ my_echo_outgoing_channel,
    /*.read_frame */ my_echo_read_frame,
    /*.write_frame */ my_echo_write_frame,
    /*.kill_channel */ my_echo_kill_channel,
    /*.send_dtmf */ my_echo_send_dtmf,
    /*.receive_message */ my_echo_receive_message,
    /*.receive_event */ my_echo_receive_event,
    /*.state_change */ NULL,
    /*.read_video_frame */ my_echo_read_video_frame,
    /*.write_video_frame */ my_echo_write_video_frame,
    /*.read_text_frame */ NULL,
    /*.write_text_frame */ NULL,
    /*.state_run*/ NULL,
    /*.get_jb*/ my_echo_get_jb
};

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
    my_echo_endpoint_interface->io_routines = &my_echo_io_routines;

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

void my_echo_set_name(private_object_t *tech_pvt, const char *channame) {
    char name[256];
    switch_snprintf(name, sizeof(name), "my_echo/%s", channame);
    switch_channel_set_name(tech_pvt->channel, name);
}

void my_echo_attach_private(switch_core_session_t *session, private_object_t *tech_pvt, const char *channame) {
    switch_assert(session != NULL);
    switch_assert(tech_pvt != NULL);

    switch_core_session_add_stream(session, NULL);

    tech_pvt->session = session;
    tech_pvt->channel = switch_core_session_get_channel(session);

    // XXX TODO
    // switch_channel_set_cap(tech_pvt->channel, CC_FLAG_XXX);

    // XXX TODO
    // tech_pvt->mparams.rtp_timeout_sec = xxx;

    switch_media_handle_create(&tech_pvt->media_handle, session, &tech_pvt->mparams);
     // XXX TODO
    // switch_media_handle_set_media_flags(tech_pvt->media_handle, media_flags);

    switch_core_media_check_dtmf_type(session);

    switch_core_session_set_private(session, tech_pvt);

    if (channame) {
        my_echo_set_name(tech_pvt, channame);
    }
}

private_object_t *my_echo_new_pvt(switch_core_session_t *session) {
    private_object_t *tech_pvt = (private_object_t*) switch_core_session_alloc(session, sizeof(private_object_t));
    return tech_pvt;
}

switch_call_cause_t my_echo_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
                                             switch_caller_profile_t *outbound_profile,
                                             switch_core_session_t **new_session, switch_memory_pool_t **pool,
                                             switch_originate_flag_t flags, switch_call_cause_t *cancel_cause) {
    switch_core_session_t *nsession = NULL;
    private_object_t *tech_pvt = NULL;
    switch_channel_t *nchannel = NULL;
    switch_caller_profile_t *caller_profile = NULL;
    switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

    *new_session = NULL;

    if (!(nsession = switch_core_session_request_uuid(my_echo_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND,
                                                      flags, pool, switch_event_get_header(var_event, "origination_uuid")))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
        goto error;
    }

    tech_pvt = my_echo_new_pvt(nsession);

    nchannel = switch_core_session_get_channel(nsession);

    if (outbound_profile) {
        caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
        switch_channel_set_caller_profile(nchannel, caller_profile);
    }
    tech_pvt->caller_profile = caller_profile;

    my_echo_attach_private(nsession, tech_pvt, NULL);

    if (switch_channel_get_state(nchannel) == CS_NEW) {
        switch_channel_set_state(nchannel, CS_INIT);
    }

    *new_session = nsession;

    // XXX TODO
    // if (session) {
    //     switch_ivr_transfer_variable(session, nsession, "xxx");
    // }

    return SWITCH_CAUSE_SUCCESS;

error:
    if (nsession) {
        switch_core_session_destroy(&nsession);
    }

    if (pool) {
        *pool = NULL;
    }

    return cause;
}

switch_status_t my_echo_read_frame(switch_core_session_t* session, switch_frame_t** frame,
                                   switch_io_flag_t flags, int stream_id) {
    private_object_t* tech_pvt = (private_object_t*) switch_core_session_get_private(session);

    switch_assert(tech_pvt != NULL);

    if (!(switch_core_media_ready(session, SWITCH_MEDIA_TYPE_AUDIO))) {
        return SWITCH_STATUS_INUSE;
    }

    return switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO);
}

switch_status_t my_echo_write_frame(switch_core_session_t* session, switch_frame_t* frame,
                                    switch_io_flag_t flags, int stream_id) {
    switch_channel_t* channel = switch_core_session_get_channel(session);
    private_object_t* tech_pvt = (private_object_t*) switch_core_session_get_private(session);

    switch_assert(tech_pvt != NULL);

    if (!switch_core_media_ready(session, SWITCH_MEDIA_TYPE_AUDIO)) {
        if (switch_channel_up_nosig(channel)) {
            return SWITCH_STATUS_SUCCESS;
        }
        return SWITCH_STATUS_GENERR;
    }

    return switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO);
}

switch_status_t my_echo_kill_channel(switch_core_session_t* session, int sig) {
    private_object_t* tech_pvt = switch_core_session_get_private(session);

    if (!tech_pvt) {
        return SWITCH_STATUS_FALSE;
    }

    switch (sig) {
    case SWITCH_SIG_BREAK:
        if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
            switch_core_media_break(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
        }
        if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO)) {
            switch_core_media_break(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
        }
        break;

    case SWITCH_SIG_KILL:
    default:
        if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
            switch_core_media_kill_socket(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
        }
        if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO)) {
            switch_core_media_kill_socket(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
        }
        break;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t my_echo_send_dtmf(switch_core_session_t* session, const switch_dtmf_t* dtmf) {
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t my_echo_receive_message(switch_core_session_t* session, switch_core_session_message_t* msg) {
    switch_channel_t* channel = switch_core_session_get_channel(session);
    private_object_t* tech_pvt = switch_core_session_get_private(session);

    switch_assert(tech_pvt != NULL);

    if (switch_channel_down(channel)) {
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t my_echo_receive_event(switch_core_session_t* session, switch_event_t* event) {
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t my_echo_read_video_frame(switch_core_session_t* session, switch_frame_t** frame,
                                         switch_io_flag_t flags, int stream_id) {
    private_object_t* tech_pvt = (private_object_t*) switch_core_session_get_private(session);

    switch_assert(tech_pvt != NULL);

    return switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO);
}

switch_status_t my_echo_write_video_frame(switch_core_session_t* session, switch_frame_t* frame,
                                          switch_io_flag_t flags, int stream_id) {
    private_object_t* tech_pvt = (private_object_t*) switch_core_session_get_private(session);

    switch_assert(tech_pvt != NULL);

    return switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO);
}

switch_jb_t* my_echo_get_jb(switch_core_session_t* session, switch_media_type_t type) {
    private_object_t* tech_pvt = (private_object_t*) switch_core_session_get_private(session);
    return switch_core_media_get_jb(tech_pvt->session, type);
}
